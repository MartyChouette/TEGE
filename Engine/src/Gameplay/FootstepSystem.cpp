#include "Enjin/Gameplay/FootstepSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Gameplay {

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
            isMoving = fps->velocity.Length() > 0.5f;
            isRunning = fps->isSprinting;
        }

        // Check TPS controller
        if (!isMoving) {
            auto* tps = world->GetComponent<ECS::ThirdPersonController>(entity);
            if (tps) {
                isMoving = tps->velocity.Length() > 0.5f;
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
                    audio->pitch = 1.0f + ((static_cast<f32>(rand()) / RAND_MAX) * 2.0f - 1.0f) * footstep->pitchVariance;
                    audio->isPlaying = true;
                }
            }
        }
    }
}

} // namespace Gameplay
} // namespace Enjin
