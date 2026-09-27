#include "Enjin/ECS/Billboards.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Quaternion.h"
#include <cmath>

namespace Enjin {
namespace ECS {

namespace {

void DirtySubtree(World* world, Entity e, u32 depth) {
    if (depth >= kMaxHierarchyDepth) return;
    if (auto* t = world->GetComponent<TransformComponent>(e)) t->worldMatrixDirty = true;
    if (auto* cc = world->GetComponent<ChildrenComponent>(e)) {
        for (Entity c : cc->children) DirtySubtree(world, c, depth + 1);
    }
}

bool SameRotation(const Math::Quaternion& a, const Math::Quaternion& b) {
    // q and -q are the same rotation
    const f32 d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    return std::abs(d) > 0.999999f;
}

} // namespace

u32 FaceBillboards(World* world, const Math::Vector3& cameraPosition) {
    if (!world) return 0;
    u32 changed = 0;
    for (auto raw : world->GetEntitiesWithComponent<BillboardComponent>()) {
        const Entity e = static_cast<Entity>(raw);
        const auto* bb = world->GetComponent<BillboardComponent>(e);
        auto* t = world->GetComponent<TransformComponent>(e);
        if (!bb || !t || !bb->faceCamera) continue;

        Math::Vector3 worldPos;
        Math::Quaternion worldRot;
        GetWorldTransform(world, e, worldPos, worldRot);

        Math::Vector3 toCamera = cameraPosition - worldPos;
        if (bb->lockY) toCamera.y = 0.0f;
        // Camera exactly on the billboard (or straight above a Y-locked one):
        // there is no direction to face, so leave it as it was.
        if (toCamera.LengthSquared() < 1e-8f) continue;

        Math::Quaternion facing = Math::Quaternion::LookRotation(toCamera, Math::Vector3(0.0f, 1.0f, 0.0f));
        if (bb->rotationOffset != 0.0f) {
            facing = facing * Math::Quaternion::FromEuler(
                Math::Vector3(0.0f, Math::Radians(bb->rotationOffset), 0.0f));
        }

        Math::Vector3 localPos;
        Math::Quaternion localRot;
        WorldToLocalTransform(world, e, worldPos, facing, localPos, localRot);
        localRot = localRot.Normalized();
        if (SameRotation(localRot, t->rotation)) continue;

        t->rotation = localRot;
        DirtySubtree(world, e, 0);
        ++changed;
    }
    return changed;
}

} // namespace ECS
} // namespace Enjin
