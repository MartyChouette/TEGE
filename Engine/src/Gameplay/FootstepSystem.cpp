#include "Enjin/Gameplay/FootstepSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Gameplay {

// S14: Fast xorshift32 PRNG replaces weak rand()
static u32 s_FootstepRandState = 3741582947u;
static f32 FootstepRandFloat() {
    s_FootstepRandState ^= s_FootstepRandState << 13;
    s_FootstepRandState ^= s_FootstepRandState >> 17;
    s_FootstepRandState ^= s_FootstepRandState << 5;
    return static_cast<f32>(s_FootstepRandState & 0x7FFFFFFF) * (1.0f / 2147483647.0f);
}

void FootstepSystem::Update(ECS::World* world, f32 deltaTime) {
    if (!m_Enabled || !world) return;

    auto entities = world->GetEntitiesWithComponent<ECS::FootstepComponent>();
    for (auto entity : entities) {
        auto* footstep = world->GetComponent<ECS::FootstepComponent>(entity);
        if (!footstep) continue;

        // Detect movement from character controllers
        bool isMoving = false;
        bool isRunning = false;

        // Check FPS controller
        auto* fps = world->GetComponent<ECS::FirstPersonController>(entity);
        if (fps) {
            isMoving = fps->velocity.Length() > footstep->movementThreshold;
            isRunning = fps->isSprinting;
        }

        // Check TPS controller
        if (!isMoving) {
            auto* tps = world->GetComponent<ECS::ThirdPersonController>(entity);
            if (tps) {
                isMoving = tps->velocity.Length() > footstep->movementThreshold;
                isRunning = tps->isSprinting;
            }
        }

        footstep->isMoving = isMoving;
        footstep->isRunning = isRunning;

        if (!isMoving) {
            footstep->stepTimer = 0.0f;
            continue;
        }

        // Update step timer
        f32 interval = isRunning ? footstep->runStepInterval : footstep->walkStepInterval;
        footstep->stepTimer += deltaTime;

        if (footstep->stepTimer >= interval) {
            footstep->stepTimer -= interval;

            // Find the appropriate sound for current surface
            std::string soundPath;
            for (const auto& surface : footstep->surfaceSounds) {
                if (surface.surfaceTag == footstep->currentSurface) {
                    soundPath = isRunning ? surface.runSound : surface.walkSound;
                    break;
                }
            }
            if (soundPath.empty()) {
                soundPath = isRunning ? footstep->defaultRunSound : footstep->defaultWalkSound;
            }

            // Trigger audio via AudioSourceComponent if present
            if (!soundPath.empty()) {
                auto* audio = world->GetComponent<ECS::AudioSourceComponent>(entity);
                if (audio) {
                    audio->clipPath = soundPath;
                    audio->volume = footstep->volume;
                    // Apply random pitch variance
                    audio->pitch = 1.0f + (FootstepRandFloat() * 2.0f - 1.0f) * footstep->pitchVariance;
                    audio->isPlaying = true;
                }
            }
        }
    }
}

} // namespace Gameplay
} // namespace Enjin
