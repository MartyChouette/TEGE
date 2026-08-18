#pragma once

#include "Enjin/Platform/Platform.h"   // ENJIN_FORCE_INLINE (Math headers) + ENJIN_API
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Entity.h"
#include <unordered_map>
#include <string>

namespace Enjin {
namespace ECS { class World; class RenderSystem; }
namespace Physics { class IPhysicsBackend; }

namespace Gameplay {

// TotK-style surface response. Reads the MaterialComponent of the surface a
// character walks on (footstep) or the surface a collision strikes (impact), and
// plays that material's footstepSound / impactSound and bursts its surfaceParticle.
//
// Footsteps are automatic (distance-based, faster cadence when running). An
// animation clip can override the cadence by calling OnFootstepEvent from a
// "footstep" event, which fires a step immediately and suppresses the auto step
// briefly so the two don't double up.
class ENJIN_API SurfaceResponseSystem {
public:
    void Initialize(Audio::SimpleAudio* audio, ECS::RenderSystem* render,
                    Physics::IPhysicsBackend* physics) {
        m_Audio = audio; m_Render = render; m_Physics = physics;
    }

    void Update(ECS::World* world, f32 deltaTime);

    // Animation-event override: a "footstep" event fired on this entity's clip.
    void OnFootstepEvent(ECS::World* world, ECS::Entity entity);

    void Reset() { m_State.clear(); }

private:
    struct WalkerState {
        Math::Vector3 lastPos{0, 0, 0};
        f32 distanceSinceStep = 0.0f;
        f32 eventSuppress = 0.0f;   // seconds the auto step stays muted after an anim event
        bool haveLast = false;
    };

    // Play the ground material's footstep sound + particle at footPos for `walker`.
    void EmitFootstep(ECS::World* world, ECS::Entity walker, const Math::Vector3& footPos);
    void EmitSurface(ECS::World* world, ECS::Entity surface, const Math::Vector3& at,
                     const Math::Vector3& normal, bool impact);
    Audio::AudioClipHandle GetClip(const std::string& path);

    Audio::SimpleAudio* m_Audio = nullptr;
    ECS::RenderSystem* m_Render = nullptr;
    Physics::IPhysicsBackend* m_Physics = nullptr;

    std::unordered_map<u32, WalkerState> m_State;                  // EntityIndex -> walker state
    std::unordered_map<u32, f32> m_ImpactNextAllowed;              // EntityIndex -> next allowed play time
    f32 m_Clock = 0.0f;
    std::unordered_map<std::string, Audio::AudioClipHandle> m_ClipCache;
};

} // namespace Gameplay
} // namespace Enjin
