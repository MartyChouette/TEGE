#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/CurlNoise.h"
#include "Enjin/Math/Math.h"
#include <cmath>

namespace Enjin {
namespace Effects {

CurlNoiseSystem::CurlNoiseSystem() = default;
CurlNoiseSystem::~CurlNoiseSystem() = default;

void CurlNoiseSystem::Initialize(ECS::World* world) {
    m_World = world;
}

void CurlNoiseSystem::Update(f32 deltaTime) {
    if (!m_World) return;

    auto fieldEntities = m_World->GetEntitiesWithComponent<ECS::CurlNoiseFieldComponent>();

    for (auto fieldEntity : fieldEntities) {
        auto* field = m_World->GetComponent<ECS::CurlNoiseFieldComponent>(fieldEntity);
        auto* fieldTransform = m_World->GetComponent<ECS::TransformComponent>(fieldEntity);
        if (!field || !fieldTransform) continue;

        // Accumulate time for field evolution
        field->accumulatedTime += deltaTime * field->timeScale;

        if (!field->affectParticles) continue;

        Math::Vector3 fieldCenter = fieldTransform->position;
        Math::Vector3 halfExt = field->halfExtents;

        // Iterate all particle emitters and apply forces to particles within this field
        auto particleEntities = m_World->GetEntitiesWithComponent<ECS::ParticleEmitterComponent>();
        for (auto particleEntity : particleEntities) {
            auto* emitter = m_World->GetComponent<ECS::ParticleEmitterComponent>(particleEntity);
            if (!emitter || !emitter->isPlaying) continue;

            auto& pool = emitter->pool;
            for (u32 i = 0; i < pool.activeCount; ++i) {
                auto& particle = pool.particles[i];

                // Check if particle is inside the field AABB
                Math::Vector3 localPos = particle.position - fieldCenter;
                if (std::abs(localPos.x) > halfExt.x ||
                    std::abs(localPos.y) > halfExt.y ||
                    std::abs(localPos.z) > halfExt.z)
                    continue;

                // Compute edge falloff
                f32 falloffMul = 1.0f;
                if (field->falloff != ECS::CurlNoiseFieldComponent::Falloff::None) {
                    f32 tx = halfExt.x > 0.0f ? std::abs(localPos.x) / halfExt.x : 0.0f;
                    f32 ty = halfExt.y > 0.0f ? std::abs(localPos.y) / halfExt.y : 0.0f;
                    f32 tz = halfExt.z > 0.0f ? std::abs(localPos.z) / halfExt.z : 0.0f;
                    f32 maxT = std::max({tx, ty, tz});

                    if (field->falloff == ECS::CurlNoiseFieldComponent::Falloff::Linear) {
                        falloffMul = 1.0f - maxT;
                    } else { // Smooth
                        f32 t = 1.0f - maxT;
                        falloffMul = t * t * (3.0f - 2.0f * t); // smoothstep
                    }
                }

                // Sample curl noise at particle position (with time evolution)
                Math::Vector3 curl = Math::CurlNoise3D(
                    particle.position.x, particle.position.y, particle.position.z + field->accumulatedTime,
                    field->octaves, field->lacunarity, field->persistence, field->frequency, field->seed);

                // Apply force as velocity addition (scaled by amplitude, falloff, and dt)
                particle.velocity.x += curl.x * field->amplitude * falloffMul * deltaTime;
                particle.velocity.y += curl.y * field->amplitude * falloffMul * deltaTime;
                particle.velocity.z += curl.z * field->amplitude * falloffMul * deltaTime;
            }
        }
    }
}

void CurlNoiseSystem::Shutdown() {
    m_World = nullptr;
}

Math::Vector3 CurlNoiseSystem::SampleField(const Math::Vector3& worldPos) const {
    if (!m_World) return Math::Vector3(0, 0, 0);

    Math::Vector3 totalForce(0, 0, 0);

    auto fieldEntities = m_World->GetEntitiesWithComponent<ECS::CurlNoiseFieldComponent>();
    for (auto fieldEntity : fieldEntities) {
        auto* field = m_World->GetComponent<ECS::CurlNoiseFieldComponent>(fieldEntity);
        auto* fieldTransform = m_World->GetComponent<ECS::TransformComponent>(fieldEntity);
        if (!field || !fieldTransform) continue;

        Math::Vector3 fieldCenter = fieldTransform->position;
        Math::Vector3 localPos = worldPos - fieldCenter;

        if (std::abs(localPos.x) > field->halfExtents.x ||
            std::abs(localPos.y) > field->halfExtents.y ||
            std::abs(localPos.z) > field->halfExtents.z)
            continue;

        f32 falloffMul = 1.0f;
        if (field->falloff != ECS::CurlNoiseFieldComponent::Falloff::None) {
            f32 tx = field->halfExtents.x > 0.0f ? std::abs(localPos.x) / field->halfExtents.x : 0.0f;
            f32 ty = field->halfExtents.y > 0.0f ? std::abs(localPos.y) / field->halfExtents.y : 0.0f;
            f32 tz = field->halfExtents.z > 0.0f ? std::abs(localPos.z) / field->halfExtents.z : 0.0f;
            f32 maxT = std::max({tx, ty, tz});

            if (field->falloff == ECS::CurlNoiseFieldComponent::Falloff::Linear) {
                falloffMul = 1.0f - maxT;
            } else {
                f32 t = 1.0f - maxT;
                falloffMul = t * t * (3.0f - 2.0f * t);
            }
        }

        Math::Vector3 curl = Math::CurlNoise3D(
            worldPos.x, worldPos.y, worldPos.z + field->accumulatedTime,
            field->octaves, field->lacunarity, field->persistence, field->frequency, field->seed);

        totalForce.x += curl.x * field->amplitude * falloffMul;
        totalForce.y += curl.y * field->amplitude * falloffMul;
        totalForce.z += curl.z * field->amplitude * falloffMul;
    }

    return totalForce;
}

} // namespace Effects
} // namespace Enjin
