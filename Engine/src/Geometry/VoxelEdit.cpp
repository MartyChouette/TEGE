#include "Enjin/Geometry/VoxelEdit.h"
#include "Enjin/Geometry/Sdf.h"
#include "Enjin/Math/Noise.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Geometry {

namespace {

// The sample range a stroke can possibly reach, clamped to the volume.
//
// Bounded rather than walking the whole volume, because a stroke is a couple of
// metres across and a volume is tens of metres: touching every sample per frame
// of a drag is most of the difference between carving and waiting. The margin
// covers the blend fillet and the roughness displacement, both of which reach
// further than the stroke's own radius.
EditRegion StrokeBounds(const ECS::VoxelVolumeComponent& v,
                        const Math::Vector3& origin, const VoxelStroke& s) {
    const f32 reach = std::max(s.radiusA, s.radiusB) + s.blend + s.roughness + v.voxelSize * 2.0f;
    // A squash below 1 makes the stroke reach FURTHER vertically than its own
    // radius, so the vertical bound is divided by it. Without this a low wide
    // slot is clipped flat top and bottom by the bounds of a shape that was
    // never that tall.
    const f32 vReach = reach / std::max(s.heightScale, 0.05f);
    const f32 minX = std::min(s.a.x, s.b.x) - reach;
    const f32 minY = std::min(s.a.y, s.b.y) - vReach;
    const f32 minZ = std::min(s.a.z, s.b.z) - reach;
    const f32 maxX = std::max(s.a.x, s.b.x) + reach;
    const f32 maxY = std::max(s.a.y, s.b.y) + vReach;
    const f32 maxZ = std::max(s.a.z, s.b.z) + reach;

    auto lo = [&](f32 world, f32 o, u32 dim) -> u32 {
        const f32 f = std::floor((world - o) / v.voxelSize);
        if (f <= 0.0f) return 0u;
        if (f >= static_cast<f32>(dim)) return dim;
        return static_cast<u32>(f);
    };
    auto hi = [&](f32 world, f32 o, u32 dim) -> u32 {
        const f32 f = std::ceil((world - o) / v.voxelSize) + 1.0f;
        if (f <= 0.0f) return 0u;
        if (f >= static_cast<f32>(dim)) return dim;
        return static_cast<u32>(f);
    };

    EditRegion r;
    r.x0 = lo(minX, origin.x, v.dimX);  r.x1 = hi(maxX, origin.x, v.dimX);
    r.y0 = lo(minY, origin.y, v.dimY);  r.y1 = hi(maxY, origin.y, v.dimY);
    r.z0 = lo(minZ, origin.z, v.dimZ);  r.z1 = hi(maxZ, origin.z, v.dimZ);
    return r;
}

// Fractal value noise, used to push a stroke's surface around.
//
// Three octaves is enough to read as rock and cheap enough to run per voxel of
// a live drag. It is added to the DISTANCE, which stops the result being a true
// distance field -- so the amplitude is the caller's to keep small against the
// voxel size, and ApplyStroke clamps it rather than trusting that.
f32 Roughness(const Math::Vector3& p, f32 scale, u32 seed) {
    f32 sum = 0.0f, amp = 1.0f, freq = scale, norm = 0.0f;
    for (u32 o = 0; o < 3; ++o) {
        sum += amp * Math::ValueNoise3D(p.x * freq, p.y * freq, p.z * freq, seed + o * 7919u);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.13f;   // not exactly 2, so octaves do not line up into grids
    }
    return (norm > 0.0f) ? (sum / norm) : 0.0f;
}

// The stroke shape at a point, squash included.
//
// One definition, so the shape a stroke CARVES and the shape its bounds are
// computed from can never disagree. A stroke judged to fit inside a box it then
// carves outside of would clip itself against its own edge.
f32 StrokeField(const VoxelStroke& s, const Math::Vector3& p) {
    if (s.heightScale == 1.0f) {
        return SdfTaperedCapsule(p, s.a, s.b, s.radiusA, s.radiusB);
    }
    const f32 midY = (s.a.y + s.b.y) * 0.5f;
    const Math::Vector3 squashed(p.x, midY + (p.y - midY) * s.heightScale, p.z);
    const Math::Vector3 a(s.a.x, midY, s.a.z);
    const Math::Vector3 b(s.b.x, midY, s.b.z);
    return SdfTaperedCapsule(squashed, a, b, s.radiusA, s.radiusB);
}

} // namespace

const char* VoxelBrushName(VoxelBrush brush) {
    switch (brush) {
        case VoxelBrush::Passage: return "Passage";
        case VoxelBrush::Chamber: return "Chamber";
        case VoxelBrush::Shaft:   return "Shaft";
        case VoxelBrush::Ramp:    return "Ramp";
        case VoxelBrush::Crack:   return "Crack";
        case VoxelBrush::Smooth:  return "Smooth";
        default:                  return "Unknown";
    }
}

VoxelStroke MakeBrushStroke(VoxelBrush brush, const BrushGesture& g) {
    VoxelStroke s;
    // Smooth ignores Dig/Fill: softening a wall is not a direction, and a
    // "Fill Smooth" that did something different from a "Dig Smooth" would be
    // two behaviours hiding behind one name.
    s.mode = (brush == VoxelBrush::Smooth)
                 ? VoxelEditMode::Smooth
                 : (g.filling ? VoxelEditMode::Fill : VoxelEditMode::Carve);
    s.blend = g.blend;
    s.roughness = g.roughness;
    s.seed = g.seed;

    const f32 bore = std::max(g.bore, 0.25f);
    const f32 depth = std::max(g.depth, bore);
    const f32 dx = g.to.x - g.from.x;
    const f32 dy = g.to.y - g.from.y;
    const f32 dz = g.to.z - g.from.z;
    const f32 run = std::sqrt(dx * dx + dy * dy + dz * dz);

    switch (brush) {
        case VoxelBrush::Chamber: {
            // A room has no direction, so the drag sets its SIZE. Centred on
            // where the gesture started rather than on the midpoint: you point
            // at the spot you want the room and drag to say how big, which is
            // how every other radius-by-drag control in the editor behaves.
            s.a = g.from;
            s.b = g.from;
            s.radiusA = s.radiusB = std::max(bore, run);
            break;
        }
        case VoxelBrush::Shaft: {
            // Straight down from the aim point. A vertical drag on a ground
            // plane is impossible to express, which is why sinking a shaft
            // used to need the camera moved and the whole cave re-approached.
            s.a = g.from;
            s.b = Math::Vector3(g.from.x, g.from.y - depth, g.from.z);
            s.radiusA = s.radiusB = bore;
            break;
        }
        case VoxelBrush::Ramp: {
            // Descends as it runs. Tapered slightly so the lower end is the
            // narrower one, which is what makes a ramp read as going somewhere
            // rather than as a tilted corridor.
            s.a = g.from;
            s.b = Math::Vector3(g.to.x, g.from.y - depth, g.to.z);
            s.radiusA = bore;
            s.radiusB = bore * 0.85f;
            break;
        }
        case VoxelBrush::Crack: {
            // Tall and narrow. The squash is what does it; the radius stays the
            // bore so the setting still means the same thing across brushes.
            s.a = g.from;
            s.b = g.to;
            s.radiusA = s.radiusB = bore;
            s.heightScale = 0.45f;   // reaches about twice the bore vertically
            break;
        }
        case VoxelBrush::Smooth: {
            // Swept like a passage, because you smooth ALONG a wall rather than
            // at a point, and a wider reach than the bore so one pass covers
            // the shape rather than pitting it.
            s.a = g.from;
            s.b = g.to;
            s.radiusA = s.radiusB = bore * 1.25f;
            // No noise: adding roughness while smoothing is a contradiction.
            s.roughness = 0.0f;
            break;
        }
        case VoxelBrush::Passage:
        default: {
            s.a = g.from;
            s.b = g.to;
            s.radiusA = s.radiusB = bore;
            break;
        }
    }
    return s;
}

bool RaycastVolume(const ECS::VoxelVolumeComponent& volume, const Math::Vector3& volumeOrigin,
                   const Math::Vector3& rayOrigin, const Math::Vector3& rayDirection,
                   f32 maxDistance, Math::Vector3& outPoint) {
    if (volume.field.size() != volume.Count()) return false;

    const f32 dirLen = std::sqrt(rayDirection.x * rayDirection.x +
                                 rayDirection.y * rayDirection.y +
                                 rayDirection.z * rayDirection.z);
    if (dirLen < 1e-6f) return false;
    const Math::Vector3 dir(rayDirection.x / dirLen, rayDirection.y / dirLen,
                            rayDirection.z / dirLen);

    // Trilinear, so a hit lands on the surface rather than on a voxel corner. A
    // stroke that snapped to the grid would start every dig up to half a voxel
    // from where it was aimed, which reads as the tool being imprecise.
    auto sample = [&](const Math::Vector3& p) -> f32 {
        const f32 fx = (p.x - volumeOrigin.x) / volume.voxelSize;
        const f32 fy = (p.y - volumeOrigin.y) / volume.voxelSize;
        const f32 fz = (p.z - volumeOrigin.z) / volume.voxelSize;
        if (fx < 0.0f || fy < 0.0f || fz < 0.0f) return volume.Band();
        const u32 x0 = static_cast<u32>(fx), y0 = static_cast<u32>(fy), z0 = static_cast<u32>(fz);
        if (x0 + 1 >= volume.dimX || y0 + 1 >= volume.dimY || z0 + 1 >= volume.dimZ)
            return volume.Band();
        const f32 tx = fx - static_cast<f32>(x0);
        const f32 ty = fy - static_cast<f32>(y0);
        const f32 tz = fz - static_cast<f32>(z0);
        auto V = [&](u32 dx, u32 dy, u32 dz) { return volume.At(x0 + dx, y0 + dy, z0 + dz); };
        const f32 c00 = V(0,0,0) * (1 - tx) + V(1,0,0) * tx;
        const f32 c10 = V(0,1,0) * (1 - tx) + V(1,1,0) * tx;
        const f32 c01 = V(0,0,1) * (1 - tx) + V(1,0,1) * tx;
        const f32 c11 = V(0,1,1) * (1 - tx) + V(1,1,1) * tx;
        const f32 c0 = c00 * (1 - ty) + c10 * ty;
        const f32 c1 = c01 * (1 - ty) + c11 * ty;
        return c0 * (1 - tz) + c1 * tz;
    };

    const f32 step = volume.voxelSize * 0.5f;
    f32 previous = sample(rayOrigin);
    for (f32 travelled = step; travelled <= maxDistance; travelled += step) {
        const Math::Vector3 p(rayOrigin.x + dir.x * travelled,
                              rayOrigin.y + dir.y * travelled,
                              rayOrigin.z + dir.z * travelled);
        const f32 current = sample(p);
        // An air-to-rock crossing only. Starting already inside rock is
        // legitimate -- a camera can be buried in a hillside -- and the useful
        // answer there is the next surface along, not a hit at the eye.
        if (current < 0.0f && previous >= 0.0f) {
            const f32 denom = previous - current;
            const f32 frac = (std::fabs(denom) > 1e-8f) ? (previous / denom) : 0.5f;
            const f32 hitAt = travelled - step + step * std::max(0.0f, std::min(1.0f, frac));
            outPoint = Math::Vector3(rayOrigin.x + dir.x * hitAt,
                                     rayOrigin.y + dir.y * hitAt,
                                     rayOrigin.z + dir.z * hitAt);
            return true;
        }
        previous = current;
    }
    return false;
}

EditRegion ApplyStroke(ECS::VoxelVolumeComponent& volume, const Math::Vector3& volumeOrigin,
                       const VoxelStroke& stroke) {
    EditRegion region = StrokeBounds(volume, volumeOrigin, stroke);
    if (region.Empty()) return EditRegion{};

    const f32 band = volume.Band();

    // Roughness is capped against the voxel size. Displacing a field by more
    // than the mesher can see across does not make it rougher, it makes it
    // wrong: thin walls disappear and the surface develops holes.
    const f32 rough = std::min(stroke.roughness, volume.voxelSize * 1.5f);

    // Smooth is not a shape edit, so it runs on a copy: averaging in place
    // would read values this same pass had already changed, which turns one
    // pass of smoothing into a directional smear.
    std::vector<f32> source;
    if (stroke.mode == VoxelEditMode::Smooth && !volume.field.empty()) {
        source = volume.field;
    }

    bool changed = false;

    for (u32 z = region.z0; z < region.z1; ++z) {
        for (u32 y = region.y0; y < region.y1; ++y) {
            for (u32 x = region.x0; x < region.x1; ++x) {
                const Math::Vector3 p(
                    volumeOrigin.x + static_cast<f32>(x) * volume.voxelSize,
                    volumeOrigin.y + static_cast<f32>(y) * volume.voxelSize,
                    volumeOrigin.z + static_cast<f32>(z) * volume.voxelSize);

                const f32 existing = volume.At(x, y, z);

                if (stroke.mode == VoxelEditMode::Smooth) {
                    // Average with the six neighbours, weighted towards the
                    // centre so one pass softens rather than melts.
                    if (source.empty()) continue;
                    auto src = [&](u32 sx, u32 sy, u32 sz) {
                        if (!volume.InBounds(sx, sy, sz)) return band;
                        return source[volume.Index(sx, sy, sz)];
                    };
                    const f32 neighbours =
                        src(x + 1, y, z) + (x > 0 ? src(x - 1, y, z) : band) +
                        src(x, y + 1, z) + (y > 0 ? src(x, y - 1, z) : band) +
                        src(x, y, z + 1) + (z > 0 ? src(x, y, z - 1) : band);
                    const f32 target = (existing * 2.0f + neighbours) / 8.0f;

                    // Only inside the stroke, and faded at its edge, so a
                    // smoothing pass does not leave a visible disc.
                    const f32 d = StrokeField(stroke, p);
                    if (d > 0.0f) continue;
                    const f32 w = std::min(1.0f, -d / std::max(volume.voxelSize, 0.001f));
                    const f32 blended = existing + (target - existing) * w;
                    if (std::fabs(blended - existing) > 1e-6f) {
                        volume.Set(x, y, z, blended);
                        changed = true;
                    }
                    continue;
                }

                f32 tool = StrokeField(stroke, p);
                if (rough > 0.0f) {
                    tool = SdfDisplace(tool, Roughness(p, stroke.roughnessScale,
                                                       stroke.seed) * rough);
                }

                // Anything well outside the tool is left alone, so a stroke
                // cannot quietly nudge the whole volume by a rounding error.
                if (tool > band) continue;

                const f32 result = (stroke.mode == VoxelEditMode::Carve)
                                       ? SdfSmoothSubtract(existing, tool, stroke.blend)
                                       : SdfSmoothUnion(existing, tool, stroke.blend);

                if (std::fabs(result - existing) > 1e-6f) {
                    volume.Set(x, y, z, result);
                    changed = true;
                }
            }
        }
    }

    if (!changed) return EditRegion{};
    volume.meshDirty = true;
    return region;
}

GrowthRequest StrokeOverflow(const ECS::VoxelVolumeComponent& v,
                             const Math::Vector3& origin, const VoxelStroke& s) {
    // The same reach StrokeBounds uses, so "does it fit" and "what gets
    // written" can never disagree. If they did, a stroke could be judged to fit
    // and then be clipped by the edge it was judged against.
    const f32 reach = std::max(s.radiusA, s.radiusB) + s.blend + s.roughness + v.voxelSize * 2.0f;
    const f32 vReach = reach / std::max(s.heightScale, 0.05f);
    const f32 minW[3] = { std::min(s.a.x, s.b.x) - reach,
                          std::min(s.a.y, s.b.y) - vReach,
                          std::min(s.a.z, s.b.z) - reach };
    const f32 maxW[3] = { std::max(s.a.x, s.b.x) + reach,
                          std::max(s.a.y, s.b.y) + vReach,
                          std::max(s.a.z, s.b.z) + reach };
    const f32 o[3] = { origin.x, origin.y, origin.z };
    const u32 dim[3] = { v.dimX, v.dimY, v.dimZ };

    u32 neg[3] = {0, 0, 0}, pos[3] = {0, 0, 0};
    for (u32 axis = 0; axis < 3; ++axis) {
        const f32 lo = (minW[axis] - o[axis]) / v.voxelSize;
        const f32 hi = (maxW[axis] - o[axis]) / v.voxelSize;
        if (lo < 0.0f) neg[axis] = static_cast<u32>(std::ceil(-lo));
        const f32 last = static_cast<f32>(dim[axis] - 1);
        if (hi > last) pos[axis] = static_cast<u32>(std::ceil(hi - last));
    }

    GrowthRequest r;
    r.negX = neg[0]; r.negY = neg[1]; r.negZ = neg[2];
    r.posX = pos[0]; r.posY = pos[1]; r.posZ = pos[2];
    return r;
}

Math::Vector3 GrowVolume(ECS::VoxelVolumeComponent& v, const Math::Vector3& origin,
                         const GrowthRequest& request, u32 maxDimension, usize maxSamples,
                         const std::function<f32(const Math::Vector3&)>& seed) {
    if (!request.Any()) return Math::Vector3(0.0f, 0.0f, 0.0f);

    // Growth is capped per axis, and the cap is spent on the side that asked
    // for it. A volume that refused to grow at all once it hit the cap would
    // stop a dig dead; one that grew both ways equally would waste half the
    // budget on rock nobody is digging towards.
    auto clampAxis = [&](u32 dim, u32 neg, u32 pos, u32& outNeg, u32& outPos) {
        outNeg = neg;
        outPos = pos;
        if (dim + neg + pos <= maxDimension) return;
        const u32 room = (maxDimension > dim) ? (maxDimension - dim) : 0u;
        const u32 asked = neg + pos;
        if (asked == 0 || room == 0) { outNeg = 0; outPos = 0; return; }
        outNeg = static_cast<u32>((static_cast<u64>(neg) * room) / asked);
        outPos = room - outNeg;
    };

    u32 nx0 = 0, nx1 = 0, ny0 = 0, ny1 = 0, nz0 = 0, nz1 = 0;
    clampAxis(v.dimX, request.negX, request.posX, nx0, nx1);
    clampAxis(v.dimY, request.negY, request.posY, ny0, ny1);
    clampAxis(v.dimZ, request.negZ, request.posZ, nz0, nz1);
    if (nx0 + nx1 + ny0 + ny1 + nz0 + nz1 == 0) return Math::Vector3(0.0f, 0.0f, 0.0f);

    // The total budget. A per-axis cap cannot bound this: 174 x 53 x 148 sits
    // inside a 192 cap on every axis and is still 1.4 million samples, which is
    // a remesh long enough to feel after every stroke.
    //
    // Refused WHOLE rather than trimmed to fit, because a partial grow leaves
    // the dig still running off an edge, and the next stroke would ask again
    // and be refused again -- a tool that gets slower and still says no. The
    // caller reports the refusal instead.
    {
        const usize grownCount = static_cast<usize>(v.dimX + nx0 + nx1) *
                                 (v.dimY + ny0 + ny1) * (v.dimZ + nz0 + nz1);
        if (maxSamples > 0 && grownCount > maxSamples) return Math::Vector3(0.0f, 0.0f, 0.0f);
    }

    const u32 oldX = v.dimX, oldY = v.dimY, oldZ = v.dimZ;
    const f32 band = v.Band();

    // Old samples must keep their WORLD positions, so the new origin moves back
    // by however many samples were added on the negative side.
    const Math::Vector3 newOrigin(origin.x - static_cast<f32>(nx0) * v.voxelSize,
                                  origin.y - static_cast<f32>(ny0) * v.voxelSize,
                                  origin.z - static_cast<f32>(nz0) * v.voxelSize);

    std::vector<f32> grown(static_cast<usize>(oldX + nx0 + nx1) *
                           (oldY + ny0 + ny1) * (oldZ + nz0 + nz1), band);

    const u32 newX = oldX + nx0 + nx1;
    const u32 newY = oldY + ny0 + ny1;
    const u32 newZ = oldZ + nz0 + nz1;
    auto newIndex = [&](u32 x, u32 y, u32 z) {
        return (static_cast<usize>(z) * newY + y) * newX + x;
    };

    // Seed the new space first, then copy the old field over the top of it. In
    // that order the copy always wins, so nothing a person carved can be
    // overwritten by the seed function.
    if (seed) {
        for (u32 z = 0; z < newZ; ++z) {
            for (u32 y = 0; y < newY; ++y) {
                for (u32 x = 0; x < newX; ++x) {
                    const Math::Vector3 p(newOrigin.x + static_cast<f32>(x) * v.voxelSize,
                                          newOrigin.y + static_cast<f32>(y) * v.voxelSize,
                                          newOrigin.z + static_cast<f32>(z) * v.voxelSize);
                    grown[newIndex(x, y, z)] = std::max(-band, std::min(band, seed(p)));
                }
            }
        }
    }

    if (v.field.size() == v.Count()) {
        for (u32 z = 0; z < oldZ; ++z) {
            for (u32 y = 0; y < oldY; ++y) {
                for (u32 x = 0; x < oldX; ++x) {
                    grown[newIndex(x + nx0, y + ny0, z + nz0)] =
                        v.field[(static_cast<usize>(z) * oldY + y) * oldX + x];
                }
            }
        }
    }

    v.dimX = newX;
    v.dimY = newY;
    v.dimZ = newZ;
    v.field = std::move(grown);
    v.meshDirty = true;

    // The transform has to move by HALF the growth, because the volume is
    // centred on it: adding samples only on one side shifts where the centre
    // of the grid is.
    return Math::Vector3(
        (static_cast<f32>(nx1) - static_cast<f32>(nx0)) * v.voxelSize * 0.5f,
        (static_cast<f32>(ny1) - static_cast<f32>(ny0)) * v.voxelSize * 0.5f,
        (static_cast<f32>(nz1) - static_cast<f32>(nz0)) * v.voxelSize * 0.5f);
}

void BakeField(ECS::VoxelVolumeComponent& volume, const Math::Vector3& volumeOrigin,
               const std::function<f32(const Math::Vector3&)>& field) {
    if (!field) return;
    const f32 band = volume.Band();
    volume.field.assign(volume.Count(), band);
    for (u32 z = 0; z < volume.dimZ; ++z) {
        for (u32 y = 0; y < volume.dimY; ++y) {
            for (u32 x = 0; x < volume.dimX; ++x) {
                const Math::Vector3 p(
                    volumeOrigin.x + static_cast<f32>(x) * volume.voxelSize,
                    volumeOrigin.y + static_cast<f32>(y) * volume.voxelSize,
                    volumeOrigin.z + static_cast<f32>(z) * volume.voxelSize);
                const f32 d = field(p);
                volume.field[volume.Index(x, y, z)] = std::max(-band, std::min(band, d));
            }
        }
    }
    volume.meshDirty = true;
}

} // namespace Geometry
} // namespace Enjin
