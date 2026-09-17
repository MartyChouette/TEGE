#include "Enjin/Effects/FluidObstacles.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Physics/PhysicsTypes2D.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace Enjin {
namespace Effects {

bool PointInsideColliderShape(const ParticleColliderShape& shape,
                              const Math::Vector3& worldPos) {
    const Math::Vector3 centre(shape.posKind.x, shape.posKind.y, shape.posKind.z);
    const i32 kind = static_cast<i32>(shape.posKind.w);

    // Into the shape's local frame. Unit quaternion, so the conjugate is the
    // inverse rotation and there is no need for a general inverse.
    const Math::Quaternion rot(shape.rot.x, shape.rot.y, shape.rot.z, shape.rot.w);
    const Math::Vector3 local = rot.Conjugate().Rotate(worldPos - centre);

    switch (kind) {
        case 0:   // box: half extents in dims.xyz
            return std::fabs(local.x) <= shape.dims.x
                && std::fabs(local.y) <= shape.dims.y
                && std::fabs(local.z) <= shape.dims.z;

        case 1: { // sphere: radius in dims.x
            const f32 r = shape.dims.x;
            return local.x * local.x + local.y * local.y + local.z * local.z <= r * r;
        }

        case 2: { // capsule: radius dims.x, cylinder HALF height dims.y, axis = local Y
            const f32 r = shape.dims.x;
            const f32 halfH = shape.dims.y;
            // Distance to the axis segment: clamp onto the cylinder section,
            // which makes the hemispherical caps fall out for free.
            const f32 y = std::clamp(local.y, -halfH, halfH);
            const f32 dy = local.y - y;
            return local.x * local.x + dy * dy + local.z * local.z <= r * r;
        }

        default:
            return false;
    }
}

// Internal: the slab/segment complement to PointInsideColliderShape above.
static bool SegmentIntersectsColliderShape(const ParticleColliderShape& shape,
                                    const Math::Vector3& a, const Math::Vector3& b) {
    const Math::Vector3 centre(shape.posKind.x, shape.posKind.y, shape.posKind.z);
    const i32 kind = static_cast<i32>(shape.posKind.w);
    const Math::Quaternion inv = Math::Quaternion(shape.rot.x, shape.rot.y,
                                                  shape.rot.z, shape.rot.w).Conjugate();
    const Math::Vector3 p0 = inv.Rotate(a - centre);
    const Math::Vector3 p1 = inv.Rotate(b - centre);
    const Math::Vector3 d(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);

    switch (kind) {
        case 0: {   // box: the standard slab test, clamped to the segment
            const f32 ext[3] = {shape.dims.x, shape.dims.y, shape.dims.z};
            const f32 o[3] = {p0.x, p0.y, p0.z};
            const f32 dir[3] = {d.x, d.y, d.z};
            f32 tMin = 0.0f, tMax = 1.0f;
            for (int i = 0; i < 3; ++i) {
                if (std::fabs(dir[i]) < 1e-8f) {
                    // Parallel to this slab: inside it or nothing can help.
                    // `<=` matters -- a ZERO-extent collider (a flat quad,
                    // which is an ordinary thing to author in a 2D scene) is
                    // exactly the case a strict comparison drops.
                    if (std::fabs(o[i]) > ext[i]) return false;
                    continue;
                }
                f32 t1 = (-ext[i] - o[i]) / dir[i];
                f32 t2 = ( ext[i] - o[i]) / dir[i];
                if (t1 > t2) std::swap(t1, t2);
                tMin = std::max(tMin, t1);
                tMax = std::min(tMax, t2);
                if (tMin > tMax) return false;
            }
            return true;
        }

        case 1: {   // sphere: closest point on the segment to the centre
            const f32 dd = d.x * d.x + d.y * d.y + d.z * d.z;
            f32 t = 0.0f;
            if (dd > 1e-12f) {
                t = std::clamp(-(p0.x * d.x + p0.y * d.y + p0.z * d.z) / dd, 0.0f, 1.0f);
            }
            const f32 cx = p0.x + d.x * t, cy = p0.y + d.y * t, cz = p0.z + d.z * t;
            return cx * cx + cy * cy + cz * cz <= shape.dims.x * shape.dims.x;
        }

        case 2: {   // capsule: distance between the segment and the axis
            const f32 r = shape.dims.x;
            const f32 halfH = shape.dims.y;
            // Axis runs along local Y. Sample the segment rather than solving
            // the general segment-segment closest pair: the axis is a single
            // straight line here and a fixed sweep is exact to well under a
            // cell at any sane grid size, where the closed form is twenty
            // lines that have to be right.
            const int kSteps = 16;
            for (int i = 0; i <= kSteps; ++i) {
                const f32 t = static_cast<f32>(i) / static_cast<f32>(kSteps);
                const f32 x = p0.x + d.x * t;
                const f32 y = p0.y + d.y * t;
                const f32 z = p0.z + d.z * t;
                const f32 cy = std::clamp(y, -halfH, halfH);
                const f32 dy = y - cy;
                if (x * x + dy * dy + z * z <= r * r) return true;
            }
            return false;
        }

        default:
            return false;
    }
}

void BuildFluidObstacleMask(const std::vector<ParticleColliderShape>& shapes,
                            const Math::Vector3& volumeCentre,
                            const Math::Vector3& halfExtents,
                            u32 N, bool is3D,
                            std::vector<u8>& outSolid) {
    const usize stride = static_cast<usize>(N) + 2;
    const usize count = is3D ? stride * stride * stride : stride * stride;
    outSolid.assign(count, u8(0));
    if (N == 0 || shapes.empty()) return;

    // Same mapping FluidRenderer uses to place a cell's billboard. Kept in
    // step deliberately: a mismatch here draws the smoke offset from the
    // geometry it is flowing around, which reads as a physics bug.
    const Math::Vector3 origin = volumeCentre - halfExtents;
    const f32 cellX = (halfExtents.x * 2.0f) / static_cast<f32>(N);
    const f32 cellY = (halfExtents.y * 2.0f) / static_cast<f32>(N);
    const f32 cellZ = (halfExtents.z * 2.0f) / static_cast<f32>(N);

    // Interior only. The padding shell is the solver's existing wall; marking
    // it solid too would apply the reflection twice.
    const u32 kHi = is3D ? N : 1;
    for (u32 k = 1; k <= kHi; ++k) {
        for (u32 j = 1; j <= N; ++j) {
            for (u32 i = 1; i <= N; ++i) {
                const Math::Vector3 p(
                    origin.x + (static_cast<f32>(i) - 0.5f) * cellX,
                    origin.y + (static_cast<f32>(j) - 0.5f) * cellY,
                    is3D ? origin.z + (static_cast<f32>(k) - 0.5f) * cellZ : volumeCentre.z);

                bool solid = false;
                for (const ParticleColliderShape& s : shapes) {
                    if (PointInsideColliderShape(s, p)) { solid = true; break; }
                }
                if (!solid) continue;

                const usize idx = is3D
                    ? (static_cast<usize>(i) + stride * (static_cast<usize>(j) + stride * k))
                    : (static_cast<usize>(i) + stride * static_cast<usize>(j));
                outSolid[idx] = 1;
            }
        }
    }
}

namespace {

// One 2D collider, in the form the planar test wants.
//
// Body2DComponent rather than the Box/Sphere/CapsuleCollider components: those
// are the JOLT side, and a 2D scene has none of them. That is the whole reason
// a 2D volume saw no obstacles at all -- the gather came back empty and the
// mask was all zeroes, which is indistinguishable from the solver ignoring
// geometry.
struct Obstacle2D {
    Physics::Shape2DType kind = Physics::Shape2DType::Box;
    Math::Vector3 centre;                  // entity world position
    Math::Quaternion rotation;             // entity world rotation
    Math::Vector2 offset;                  // shape offset, body-local
    f32 shapeRotation = 0.0f;              // box only, body-local radians
    Math::Vector2 halfExtents;             // box
    f32 radius = 0.0f;                     // circle, capsule
    f32 height = 0.0f;                     // capsule, TOTAL including caps
    std::vector<Math::Vector2> polygon;    // polygon, body-local, convex
};

void GatherObstacles2D(ECS::World* world, std::vector<Obstacle2D>& out) {
    out.clear();
    if (!world) return;
    using namespace Enjin::ECS;

    for (Entity e : world->GetEntitiesWithComponent<Physics::Body2DComponent>()) {
        auto* body = world->GetComponent<Physics::Body2DComponent>(e);
        auto* xf = world->GetComponent<TransformComponent>(e);
        // A sensor is an overlap detector, not a wall -- the same exclusion the
        // 3D gather makes for isTrigger.
        if (!body || !xf || body->isSensor) continue;

        Obstacle2D o;
        o.kind = body->shapeType;
        o.centre = xf->position;
        o.rotation = xf->rotation;
        switch (body->shapeType) {
            case Physics::Shape2DType::Circle:
                o.offset = body->circle.offset;
                o.radius = body->circle.radius;
                break;
            case Physics::Shape2DType::Box:
                o.offset = body->box.offset;
                o.shapeRotation = body->box.rotation;
                o.halfExtents = body->box.halfExtents;
                break;
            case Physics::Shape2DType::Capsule:
                o.offset = body->capsule.offset;
                o.radius = body->capsule.radius;
                o.height = body->capsule.height;
                break;
            case Physics::Shape2DType::Polygon:
                o.offset = body->polygon.offset;
                o.polygon = body->polygon.vertices;
                if (o.polygon.size() < 3) continue;   // not an area
                break;
        }
        out.push_back(std::move(o));
    }
}

// Planar containment. Z is ignored on purpose: a 2D wall is a wall at every
// depth, and a 2D scene's entities sit wherever their sprite sorting put them.
bool PointInsideObstacle2D(const Obstacle2D& o, const Math::Vector3& worldPos) {
    // Into the body's frame through the full quaternion, so a rotated entity is
    // handled by the same maths as the 3D path rather than by extracting an
    // euler angle.
    const Math::Vector3 d(worldPos.x - o.centre.x, worldPos.y - o.centre.y, 0.0f);
    const Math::Vector3 r = o.rotation.Conjugate().Rotate(d);
    const f32 px = r.x - o.offset.x;
    const f32 py = r.y - o.offset.y;

    switch (o.kind) {
        case Physics::Shape2DType::Circle:
            return px * px + py * py <= o.radius * o.radius;

        case Physics::Shape2DType::Box: {
            // The shape's own rotation, on top of the entity's.
            const f32 c = std::cos(-o.shapeRotation);
            const f32 s = std::sin(-o.shapeRotation);
            const f32 lx = px * c - py * s;
            const f32 ly = px * s + py * c;
            return std::fabs(lx) <= o.halfExtents.x && std::fabs(ly) <= o.halfExtents.y;
        }

        case Physics::Shape2DType::Capsule: {
            // Box2D builds a capsule along Y, its two cap centres separated by
            // height - 2r. Clamping onto that segment gives the caps for free.
            const f32 halfSeg = std::max(0.0f, (o.height - 2.0f * o.radius) * 0.5f);
            const f32 y = std::clamp(py, -halfSeg, halfSeg);
            const f32 dy = py - y;
            return px * px + dy * dy <= o.radius * o.radius;
        }

        case Physics::Shape2DType::Polygon: {
            // Exact convex test, not the polygon's bounding box: an
            // approximation here shows as smoke stopping at nothing, which
            // reads as a solver bug rather than as a shape the mask could not
            // represent. Winding is whatever the author used, so accept
            // all-left OR all-right.
            bool anyNeg = false, anyPos = false;
            const usize n = o.polygon.size();
            for (usize i = 0; i < n; ++i) {
                const Math::Vector2& a = o.polygon[i];
                const Math::Vector2& b = o.polygon[(i + 1) % n];
                const f32 cross = (b.x - a.x) * (py - a.y) - (b.y - a.y) * (px - a.x);
                if (cross < 0.0f) anyNeg = true;
                if (cross > 0.0f) anyPos = true;
                if (anyNeg && anyPos) return false;
            }
            return true;
        }
    }
    return false;
}

void BuildMask2D(const std::vector<Obstacle2D>& shapes,
                 const Math::Vector3& volumeCentre,
                 const Math::Vector3& halfExtents,
                 u32 N, std::vector<u8>& outSolid) {
    const usize stride = static_cast<usize>(N) + 2;
    outSolid.assign(stride * stride, u8(0));
    if (N == 0 || shapes.empty()) return;

    // The same cell mapping the 3D path and FluidRenderer use. Kept in step
    // deliberately: a mismatch draws the smoke offset from the geometry it is
    // flowing around, which reads as a physics bug and is a coordinate bug.
    const Math::Vector3 origin = volumeCentre - halfExtents;
    const f32 cellX = (halfExtents.x * 2.0f) / static_cast<f32>(N);
    const f32 cellY = (halfExtents.y * 2.0f) / static_cast<f32>(N);

    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            const Math::Vector3 p(origin.x + (static_cast<f32>(i) - 0.5f) * cellX,
                                  origin.y + (static_cast<f32>(j) - 0.5f) * cellY,
                                  volumeCentre.z);
            for (const Obstacle2D& o : shapes) {
                if (!PointInsideObstacle2D(o, p)) continue;
                outSolid[static_cast<usize>(i) + stride * static_cast<usize>(j)] = 1;
                break;
            }
        }
    }
}

// 3D colliders against a 2D volume's slab: one vertical segment per cell,
// spanning the volume's Z thickness.
void BuildMask3DShapesForSheet(const std::vector<ParticleColliderShape>& shapes,
                               const Math::Vector3& volumeCentre,
                               const Math::Vector3& halfExtents,
                               u32 N, std::vector<u8>& outSolid) {
    const usize stride = static_cast<usize>(N) + 2;
    outSolid.assign(stride * stride, u8(0));
    if (N == 0 || shapes.empty()) return;

    const Math::Vector3 origin = volumeCentre - halfExtents;
    const f32 cellX = (halfExtents.x * 2.0f) / static_cast<f32>(N);
    const f32 cellY = (halfExtents.y * 2.0f) / static_cast<f32>(N);
    const f32 zMin = volumeCentre.z - halfExtents.z;
    const f32 zMax = volumeCentre.z + halfExtents.z;

    for (u32 j = 1; j <= N; ++j) {
        for (u32 i = 1; i <= N; ++i) {
            const f32 x = origin.x + (static_cast<f32>(i) - 0.5f) * cellX;
            const f32 y = origin.y + (static_cast<f32>(j) - 0.5f) * cellY;
            for (const ParticleColliderShape& sh : shapes) {
                if (!SegmentIntersectsColliderShape(sh, Math::Vector3(x, y, zMin),
                                                        Math::Vector3(x, y, zMax))) {
                    continue;
                }
                outSolid[static_cast<usize>(i) + stride * static_cast<usize>(j)] = 1;
                break;
            }
        }
    }
}

void HashF32(u64& h, f32 v) {
    u32 bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    h = (h ^ static_cast<u64>(bits)) * 1099511628211ull;   // FNV-1a
}

} // namespace

void BuildFluidObstacleMask(ECS::World* world,
                            const Math::Vector3& volumeCentre,
                            const Math::Vector3& halfExtents,
                            u32 N, bool is3D,
                            std::vector<u8>& outSolid) {
    // No cap on either gather: this runs on the CPU, where the GPU sim's limit
    // of 32 would silently drop most of a room.
    std::vector<ParticleColliderShape> shapes3D;
    GatherParticleColliders(world, shapes3D, static_cast<usize>(-1));

    if (is3D) {
        BuildFluidObstacleMask(shapes3D, volumeCentre, halfExtents, N, is3D, outSolid);
        return;
    }

    // A 2D volume is a SLAB, not a plane, so a 3D collider is tested against
    // the whole thickness rather than sampled at the middle of it. The case
    // that forces this is an ordinary one: a flat quad with a zero-thickness
    // box collider, which a point test catches only when the sample plane
    // lands exactly on it -- so it would work in one scene, vanish in the next,
    // and look like the obstacle feature being unreliable.
    BuildMask3DShapesForSheet(shapes3D, volumeCentre, halfExtents, N, outSolid);

    // A 2D volume takes BOTH kinds, and the union is the point.
    //
    // Strictly, 2D physics is Box2D and 3D physics is Jolt and the two never
    // mix -- but obstacles are not physics here, they are geometry, and a
    // scene is routinely mixed: a 2D fluid volume in front of 3D meshes with
    // ordinary BoxColliders on them is the normal case, not an abuse. Gathering
    // only Body2D there reproduced the exact bug this set out to fix, silently,
    // in the scene most likely to be used to check it.
    //
    // The 3D shapes above were already tested against the volume's Z plane by
    // the planar sampler, so a box that intersects the sheet counts and one
    // that passes in front of or behind it does not. The 2D bodies below have
    // no Z at all and count everywhere.
    std::vector<Obstacle2D> shapes2D;
    GatherObstacles2D(world, shapes2D);
    if (shapes2D.empty()) return;


    std::vector<u8> planar;
    BuildMask2D(shapes2D, volumeCentre, halfExtents, N, planar);
    if (planar.size() != outSolid.size()) return;   // cannot happen; not worth a crash
    for (usize i = 0; i < planar.size(); ++i) {
        if (planar[i]) outSolid[i] = 1;
    }
}

u64 FluidObstacleFingerprint(ECS::World* world, bool is3D) {
    u64 h = 14695981039346656037ull;   // FNV-1a offset basis
    if (!world) return h;

    // Both gathers for a 2D volume, because both feed its mask. A fingerprint
    // that covered only half of them would leave the mask stale exactly when
    // the other half moved.
    {
        std::vector<ParticleColliderShape> shapes;
        GatherParticleColliders(world, shapes, static_cast<usize>(-1));
        for (const ParticleColliderShape& s : shapes) {
            HashF32(h, s.posKind.x); HashF32(h, s.posKind.y);
            HashF32(h, s.posKind.z); HashF32(h, s.posKind.w);
            HashF32(h, s.rot.x); HashF32(h, s.rot.y);
            HashF32(h, s.rot.z); HashF32(h, s.rot.w);
            HashF32(h, s.dims.x); HashF32(h, s.dims.y); HashF32(h, s.dims.z);
        }
    }
    if (is3D) return h;

    std::vector<Obstacle2D> shapes;
    GatherObstacles2D(world, shapes);
    for (const Obstacle2D& o : shapes) {
        HashF32(h, static_cast<f32>(static_cast<i32>(o.kind)));
        HashF32(h, o.centre.x); HashF32(h, o.centre.y); HashF32(h, o.centre.z);
        HashF32(h, o.rotation.x); HashF32(h, o.rotation.y);
        HashF32(h, o.rotation.z); HashF32(h, o.rotation.w);
        HashF32(h, o.offset.x); HashF32(h, o.offset.y);
        HashF32(h, o.shapeRotation);
        HashF32(h, o.halfExtents.x); HashF32(h, o.halfExtents.y);
        HashF32(h, o.radius); HashF32(h, o.height);
        for (const Math::Vector2& v : o.polygon) { HashF32(h, v.x); HashF32(h, v.y); }
    }
    return h;
}

} // namespace Effects
} // namespace Enjin
