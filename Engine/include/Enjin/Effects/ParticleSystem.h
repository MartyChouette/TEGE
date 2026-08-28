#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"

namespace Enjin {
namespace Effects {

// CPU particle simulation system
// Iterates entities with ParticleEmitterComponent + TransformComponent,
// spawns, updates, and kills particles per emitter each frame.
class ENJIN_API ParticleSystem {
public:
    ParticleSystem() = default;
    ~ParticleSystem() = default;

    // Update all particle emitters in the world
    void Update(f32 deltaTime, ECS::World* world);

    // Scene wind (xyz = direction * strength) — pushed by the runtime each frame so
    // emitters with useSceneWind drift with the world's wind. Set before Update().
    void SetSceneWind(const Math::Vector3& wind) { m_SceneWind = wind; }

    // Stats
    u32 GetTotalActiveParticles() const { return m_TotalActiveParticles; }
    u32 GetTotalEmitterCount() const { return m_TotalEmitterCount; }

private:
    void InitPool(ECS::ParticleEmitterComponent& emitter);
    void SpawnParticle(ECS::ParticleEmitterComponent& emitter, const Math::Vector3& emitterPos);
    void UpdateEmitter(ECS::ParticleEmitterComponent& emitter, const Math::Vector3& emitterPos, f32 deltaTime);

    u32 m_TotalActiveParticles = 0;
    u32 m_TotalEmitterCount = 0;
    Math::Vector3 m_SceneWind = Math::Vector3(0, 0, 0);
};

} // namespace Effects
} // namespace Enjin
