// Tree trunk collider generation - CPU-only, compiled on ALL platforms.
// Lives outside TreeRenderer.cpp because that file is Vulkan-only (excluded
// from the web build), while the web renderer draws the same hash-scattered
// trees (WebGPUVegetationSystem ports the identical integer hash), so web
// play needs the matching colliders too.
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <string>

namespace Enjin {
namespace Effects {

void TreeRenderer::GenerateColliders(ECS::World* world, ECS::Entity volumeEntity) {
    if (!world) return;

    auto* tree = world->GetComponent<ECS::TreeVolumeComponent>(volumeEntity);
    auto* transform = world->GetComponent<ECS::TransformComponent>(volumeEntity);
    if (!tree || !transform) return;

    Math::Vector3 volumeCenter = transform->position;
    f32 halfX = tree->halfExtents.x;
    f32 halfZ = tree->halfExtents.z;

    // Replicate the XorShift hash from the vertex shader on CPU
    auto cpuHash = [](u32 n) -> f32 {
        n = (n << 13u) ^ n;
        n = n * (n * n * 15731u + 789221u) + 1376312589u;
        return static_cast<f32>(n & 0x7fffffffu) / static_cast<f32>(0x7fffffff);
    };

    for (u32 i = 0; i < tree->density; ++i) {
        f32 px = cpuHash(i * 3u + 0u) * 2.0f - 1.0f;
        f32 pz = cpuHash(i * 3u + 1u) * 2.0f - 1.0f;
        f32 minS = tree->minHeightScale, maxS = tree->maxHeightScale;
        f32 sizeVar = minS + cpuHash(i * 3u + 2u) * (maxS - minS);

        Math::Vector3 treePos = volumeCenter + Math::Vector3(px * halfX, 0.0f, pz * halfZ);

        f32 tHeight = tree->trunkHeight * sizeVar;
        f32 tWidth = tree->trunkWidth * sizeVar;

        // Create a collider entity for each tree trunk
        ECS::Entity collider = world->CreateEntity();

        ECS::NameComponent nameComp;
        nameComp.name = "TreeTrunk_" + std::to_string(i);
        world->AddComponent<ECS::NameComponent>(collider, nameComp);

        ECS::TransformComponent xform;
        xform.position = treePos + Math::Vector3(0, tHeight * 0.5f, 0);
        world->AddComponent<ECS::TransformComponent>(collider, xform);

        // Capsule (the "cylinder" ask): rounder pushback than a box, and the
        // physics backends have a real capsule shape. height = cylinder
        // section only (engine convention: total = height + 2*radius).
        ECS::CapsuleColliderComponent cap;
        cap.center = Math::Vector3(0, 0, 0);
        cap.direction = ECS::CapsuleColliderComponent::Direction::Y;
        cap.radius = tWidth;
        cap.height = std::max(0.1f, tHeight - 2.0f * tWidth);
        cap.isTrigger = false;
        world->AddComponent<ECS::CapsuleColliderComponent>(collider, cap);

        // Static body so the physics backend actually builds it (a bare
        // collider component without a rigidbody never became a body - the
        // reason this function sat dead and unwired).
        ECS::RigidbodyComponent rb;
        rb.bodyType = ECS::RigidbodyComponent::BodyType::Static;
        rb.useGravity = false;
        world->AddComponent<ECS::RigidbodyComponent>(collider, rb);

        // Runtime-generated: a mid-play save must not write these into the
        // scene file (this exact leak polluted the Playground scene once).
        world->AddComponent<ECS::TransientComponent>(collider, ECS::TransientComponent{});
    }

    ENJIN_LOG_INFO(Renderer, "Generated %u tree trunk colliders", tree->density);
}

void TreeRenderer::GenerateAllColliders(ECS::World* world) {
    if (!world) return;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::TreeVolumeComponent>()) {
        auto* tv = world->GetComponent<ECS::TreeVolumeComponent>(e);
        if (tv && tv->generateColliders) GenerateColliders(world, e);
    }
}

} // namespace Effects
} // namespace Enjin
