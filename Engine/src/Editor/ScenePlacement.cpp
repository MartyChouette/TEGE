#include "Enjin/Editor/ScenePlacement.h"

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/Renderer/MeshFactory.h"

#include <algorithm>

namespace Enjin::Editor {

namespace {

// The per-kind curve. Instances per square metre, and the floor and ceiling.
//
// These three rows are the whole difference between grass, shrubs and trees, and
// they were the numbers duplicated between the two surfaces.
struct PlantCurve {
    f32 perUnit;
    u32 lo;
    u32 hi;
};

constexpr PlantCurve kPlantCurves[] = {
    { 4.00f, 32u, 6000u },   // Grass
    { 1.00f,  8u, 1500u },   // Shrubs
    { 0.08f,  1u,  120u },   // Trees
};

static_assert(sizeof(kPlantCurves) / sizeof(kPlantCurves[0]) ==
              static_cast<usize>(PlantKind::Count),
              "every PlantKind needs a density curve, or a new kind silently "
              "inherits whichever row it happens to index");

} // namespace

const char* PlantKindName(PlantKind kind) {
    switch (kind) {
        case PlantKind::Grass:  return "Grass Patch";
        case PlantKind::Shrubs: return "Shrub Patch";
        case PlantKind::Trees:  return "Tree Grove";
        default:                return "Plants";
    }
}

u32 PlantDensity(PlantKind kind, f32 halfX, f32 halfZ, f32 densityScale) {
    const usize i = static_cast<usize>(kind);
    if (i >= static_cast<usize>(PlantKind::Count)) return 0;
    const PlantCurve& c = kPlantCurves[i];

    // Negative half-extents are a caller bug, not a tiny patch; abs keeps a
    // mirrored drag from producing zero instances.
    const f32 area = std::abs(halfX) * std::abs(halfZ) * 4.0f;
    const f32 scaled = area * c.perUnit * std::max(0.0f, densityScale);

    const u32 d = static_cast<u32>(scaled);
    return std::max(c.lo, std::min(d, c.hi));
}

void AddPlantVolume(ECS::World* world, ECS::Entity entity, PlantKind kind) {
    if (!world) return;
    switch (kind) {
        case PlantKind::Grass:
            if (!world->HasComponent<ECS::GrassVolumeComponent>(entity))
                world->AddComponent<ECS::GrassVolumeComponent>(entity);
            break;
        case PlantKind::Shrubs:
            if (!world->HasComponent<ECS::ShrubVolumeComponent>(entity))
                world->AddComponent<ECS::ShrubVolumeComponent>(entity);
            break;
        case PlantKind::Trees:
            if (!world->HasComponent<ECS::TreeVolumeComponent>(entity))
                world->AddComponent<ECS::TreeVolumeComponent>(entity);
            break;
        default: break;
    }
}

void SizePlantVolume(ECS::World* world, ECS::Entity entity, PlantKind kind,
                     f32 halfX, f32 halfZ, f32 densityScale) {
    if (!world) return;

    const Math::Vector3 half(halfX, 0.0f, halfZ);
    const u32 density = PlantDensity(kind, halfX, halfZ, densityScale);

    switch (kind) {
        case PlantKind::Grass:
            if (auto* v = world->GetComponent<ECS::GrassVolumeComponent>(entity)) {
                v->halfExtents = half;
                v->density = density;
            }
            break;
        case PlantKind::Shrubs:
            if (auto* v = world->GetComponent<ECS::ShrubVolumeComponent>(entity)) {
                v->halfExtents = half;
                v->density = density;
            }
            break;
        case PlantKind::Trees:
            if (auto* v = world->GetComponent<ECS::TreeVolumeComponent>(entity)) {
                v->halfExtents = half;
                v->density = density;
            }
            break;
        default: break;
    }
}

const char* PropKindName(PropKind kind) {
    switch (kind) {
        case PropKind::Block:      return "Block";
        case PropKind::Ball:       return "Ball";
        case PropKind::Light:      return "Light";
        case PropKind::PhysicsBox: return "Physics Box";
        case PropKind::Barrel:     return "Barrel";
        case PropKind::SpawnPoint: return "Spawn Point";
        default:                   return "Prop";
    }
}

void CreateProp(ECS::World* world, ECS::Entity entity, PropKind kind,
                const Math::Vector3& groundPoint) {
    if (!world) return;

    auto* xf = world->GetComponent<ECS::TransformComponent>(entity);
    if (!xf) return;
    xf->position = groundPoint;

    switch (kind) {
        case PropKind::Block:
            world->AddComponent<ECS::MeshComponent>(
                entity, Renderer::MeshFactory::CreateCube(1.0f));
            world->AddComponent<ECS::MaterialComponent>(entity);
            xf->position.y += 0.5f;
            break;

        case PropKind::Ball:
            world->AddComponent<ECS::MeshComponent>(
                entity, Renderer::MeshFactory::CreateSphere(0.5f));
            world->AddComponent<ECS::MaterialComponent>(entity);
            xf->position.y += 0.5f;
            break;

        case PropKind::Light: {
            auto& l = world->AddComponent<ECS::LightComponent>(entity);
            l.type = ECS::LightType::Point;
            // Above head height. A point light sitting on the floor lights the
            // floor and nothing else.
            xf->position.y += 2.0f;
            break;
        }

        case PropKind::PhysicsBox: {
            world->AddComponent<ECS::MeshComponent>(
                entity, Renderer::MeshFactory::CreateCube(1.0f));
            world->AddComponent<ECS::MaterialComponent>(entity);
            auto& rb = world->AddComponent<ECS::RigidbodyComponent>(entity);
            rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
            rb.useGravity = true;
            // Collider sizes are WORLD space here and entity scale does not
            // multiply them, so this is the cube's real size.
            auto& bc = world->AddComponent<ECS::BoxColliderComponent>(entity);
            bc.size = Math::Vector3(1.0f, 1.0f, 1.0f);
            // Three metres up, so it has somewhere to fall from.
            xf->position.y += 3.0f;
            break;
        }

        case PropKind::Barrel: {
            world->AddComponent<ECS::MeshComponent>(
                entity, Renderer::MeshFactory::CreateCylinder(0.5f, 1.2f));
            world->AddComponent<ECS::MaterialComponent>(entity);
            auto& d = world->AddComponent<ECS::DestructibleComponent>(entity);
            d.health = 1.0f;
            d.destroyOnHit = true;
            auto& bc = world->AddComponent<ECS::BoxColliderComponent>(entity);
            bc.size = Math::Vector3(1.0f, 1.2f, 1.0f);
            xf->position.y += 0.6f;
            break;
        }

        case PropKind::SpawnPoint: {
            world->AddComponent<ECS::MeshComponent>(
                entity, Renderer::MeshFactory::CreateCone(0.4f, 1.0f));
            auto& mat = world->AddComponent<ECS::MaterialComponent>(entity);
            mat.baseColor = Math::Vector3(0.2f, 0.9f, 0.4f);
            mat.emissiveColor = Math::Vector3(0.1f, 0.5f, 0.2f);
            xf->position.y += 0.5f;
            break;
        }

        default: break;
    }
}

} // namespace Enjin::Editor
