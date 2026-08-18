#include "Enjin/Effects/ParticleColliders.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

namespace Enjin {
namespace Effects {

void GatherParticleColliders(ECS::World* world, std::vector<ParticleColliderShape>& out) {
    out.clear();
    if (!world) return;
    using namespace Enjin::ECS;

    auto quat = [](const Math::Quaternion& q) {
        return Math::Vector4(q.x, q.y, q.z, q.w);
    };

    for (Entity e : world->GetEntitiesWithComponent<BoxColliderComponent>()) {
        if (out.size() >= kMaxParticleColliders) return;
        auto* col = world->GetComponent<BoxColliderComponent>(e);
        auto* xf = world->GetComponent<TransformComponent>(e);
        if (!col || !xf || col->isTrigger) continue;
        ParticleColliderShape s;
        Math::Vector3 p = xf->position + xf->rotation.Rotate(col->center);
        s.posKind = Math::Vector4(p.x, p.y, p.z, 0.0f);
        s.rot = quat(xf->rotation);
        s.dims = Math::Vector4(col->size.x * 0.5f, col->size.y * 0.5f, col->size.z * 0.5f, 0.0f);
        out.push_back(s);
    }
    for (Entity e : world->GetEntitiesWithComponent<SphereColliderComponent>()) {
        if (out.size() >= kMaxParticleColliders) return;
        auto* col = world->GetComponent<SphereColliderComponent>(e);
        auto* xf = world->GetComponent<TransformComponent>(e);
        if (!col || !xf || col->isTrigger) continue;
        ParticleColliderShape s;
        Math::Vector3 p = xf->position + xf->rotation.Rotate(col->center);
        s.posKind = Math::Vector4(p.x, p.y, p.z, 1.0f);
        s.rot = quat(xf->rotation);
        s.dims = Math::Vector4(col->radius, 0.0f, 0.0f, 0.0f);
        out.push_back(s);
    }
    for (Entity e : world->GetEntitiesWithComponent<CapsuleColliderComponent>()) {
        if (out.size() >= kMaxParticleColliders) return;
        auto* col = world->GetComponent<CapsuleColliderComponent>(e);
        auto* xf = world->GetComponent<TransformComponent>(e);
        if (!col || !xf || col->isTrigger) continue;
        ParticleColliderShape s;
        Math::Vector3 p = xf->position + xf->rotation.Rotate(col->center);
        s.posKind = Math::Vector4(p.x, p.y, p.z, 2.0f);
        s.rot = quat(xf->rotation);
        s.dims = Math::Vector4(col->radius, col->height * 0.5f, 0.0f, 0.0f);
        out.push_back(s);
    }
}

} // namespace Effects
} // namespace Enjin
