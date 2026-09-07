#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"

namespace Enjin {
namespace Effects {

// The emitter entity's WORLD transform, resolved once per frame.
//
// Particle sizes and emission shapes are authored in world units, the same
// convention as collider sizes. That used to mean the emitter's own transform
// was ignored outside of position: scaling the entity changed nothing, and
// rotating it changed nothing, so the only way to aim an emitter was to type
// numbers into `direction`. Both now come from the entity, which is what
// everyone tries first.
//
// `scale` is a single factor because the billboard is square. It is the largest
// component of the world scale, so a non-uniform scale still gives a sensible
// answer instead of collapsing.
struct EmitterTransform {
    Math::Vector3 position = Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Quaternion rotation;
    f32 scale = 1.0f;
};

// Resolve an emitter entity's world transform. Shared by the CPU emitters here
// and the GPU emitters fed from RenderSystem, so both answer the same way to a
// scaled or rotated entity.
ENJIN_API EmitterTransform ResolveEmitterTransform(ECS::World* world, ECS::Entity entity);

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
    void SpawnParticle(ECS::ParticleEmitterComponent& emitter, const EmitterTransform& xf);
    void UpdateEmitter(ECS::ParticleEmitterComponent& emitter, const EmitterTransform& xf, f32 deltaTime);

    u32 m_TotalActiveParticles = 0;
    u32 m_TotalEmitterCount = 0;
    Math::Vector3 m_SceneWind = Math::Vector3(0, 0, 0);
};

} // namespace Effects
} // namespace Enjin
