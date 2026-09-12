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
    const f32 minX = std::min(s.a.x, s.b.x) - reach;
    const f32 minY = std::min(s.a.y, s.b.y) - reach;
    const f32 minZ = std::min(s.a.z, s.b.z) - reach;
    const f32 maxX = std::max(s.a.x, s.b.x) + reach;
    const f32 maxY = std::max(s.a.y, s.b.y) + reach;
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

} // namespace

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
                    const f32 d = SdfTaperedCapsule(p, stroke.a, stroke.b,
                                                    stroke.radiusA, stroke.radiusB);
                    if (d > 0.0f) continue;
                    const f32 w = std::min(1.0f, -d / std::max(volume.voxelSize, 0.001f));
                    const f32 blended = existing + (target - existing) * w;
                    if (std::fabs(blended - existing) > 1e-6f) {
                        volume.Set(x, y, z, blended);
                        changed = true;
                    }
                    continue;
                }

                f32 tool = SdfTaperedCapsule(p, stroke.a, stroke.b,
                                             stroke.radiusA, stroke.radiusB);
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
