#include "Enjin/Effects/FluidObstacles.h"
#include "Enjin/Math/Quaternion.h"

#include <algorithm>
#include <cmath>

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

void BuildFluidObstacleMask(ECS::World* world,
                            const Math::Vector3& volumeCentre,
                            const Math::Vector3& halfExtents,
                            u32 N, bool is3D,
                            std::vector<u8>& outSolid) {
    std::vector<ParticleColliderShape> shapes;
    // No cap: this runs once on the CPU at bake time, where the GPU sim's 32
    // would silently drop most of a room.
    GatherParticleColliders(world, shapes, static_cast<usize>(-1));
    BuildFluidObstacleMask(shapes, volumeCentre, halfExtents, N, is3D, outSolid);
}

} // namespace Effects
} // namespace Enjin
