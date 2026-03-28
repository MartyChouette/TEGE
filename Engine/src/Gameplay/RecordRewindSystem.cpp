#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

namespace Enjin::Gameplay {

void RecordRewindSystem::Update(f32 deltaTime) {
    if (!m_World) return;

    m_AnyRewinding = false;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::RecordRewindComponent>()) {
        auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
        if (!rr || !rr->enabled) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Get velocity from whichever controller exists
        Math::Vector3 velocity(0.0f);
        if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity)) {
            velocity = ctrl->velocity;
        } else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity)) {
            velocity = ctrl3->velocity;
        } else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity)) {
            velocity = ctrl1->velocity;
        }

        // Get health
        f32 health = 0.0f;
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (hp) health = hp->currentHealth;

        // --- Cooldown ---
        if (rr->cooldownTimer > 0.0f) {
            rr->cooldownTimer -= deltaTime;
        }

        // --- Check rewind input (hold to rewind) ---
        bool rewindHeld = Input::IsKeyDown(KeyCode::R) && rr->cooldownTimer <= 0.0f;

        if (rewindHeld && !rr->frames.empty()) {
            // Enter or continue rewinding
            if (!rr->rewinding) {
                rr->rewinding = true;
                rr->rewindPlayhead = 0.0f;
                ENJIN_LOG_INFO(Game, "Rewind started (entity %llu, %zu frames buffered)",
                    (unsigned long long)entity, rr->frames.size());
            }

            rr->rewindPlayhead += deltaTime * rr->rewindSpeed;
            m_AnyRewinding = true;

            // Find the frame to restore (playhead seconds from the end)
            f32 latestTime = rr->frames.back().timestamp;
            f32 targetTime = latestTime - rr->rewindPlayhead;

            // Find the two frames surrounding targetTime for interpolation
            i32 frameA = -1, frameB = -1;
            for (i32 i = static_cast<i32>(rr->frames.size()) - 1; i >= 0; --i) {
                if (rr->frames[i].timestamp <= targetTime) {
                    frameA = i;
                    frameB = (i + 1 < static_cast<i32>(rr->frames.size())) ? i + 1 : i;
                    break;
                }
            }

            if (frameA < 0) {
                // Reached the beginning of the buffer
                frameA = 0;
                frameB = 0;
            }

            const auto& fA = rr->frames[frameA];
            const auto& fB = rr->frames[frameB];

            // Interpolation factor between frames
            f32 t = 0.0f;
            if (frameA != frameB) {
                f32 span = fB.timestamp - fA.timestamp;
                if (span > 0.0001f) {
                    t = (targetTime - fA.timestamp) / span;
                    t = Math::Clamp(t, 0.0f, 1.0f);
                }
            }

            // Interpolate and apply
            transform->position = fA.position + (fB.position - fA.position) * t;
            transform->rotation = Math::Quaternion::Slerp(fA.rotation, fB.rotation, t);
            transform->scale = fA.scale + (fB.scale - fA.scale) * t;

            // Restore velocity
            if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity)) {
                ctrl->velocity = fA.velocity + (fB.velocity - fA.velocity) * t;
            } else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity)) {
                ctrl3->velocity = fA.velocity + (fB.velocity - fA.velocity) * t;
            } else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity)) {
                ctrl1->velocity = fA.velocity + (fB.velocity - fA.velocity) * t;
            }

            // Restore health
            if (hp) {
                f32 interpHealth = fA.health + (fB.health - fA.health) * t;
                hp->currentHealth = interpHealth;
                if (hp->isDead && interpHealth > 0.0f) {
                    hp->isDead = false; // Revive on rewind
                }
            }

            // Pop consumed frames (everything after targetTime)
            while (!rr->frames.empty() && rr->frames.back().timestamp > targetTime + 0.001f) {
                rr->frames.pop_back();
            }

        } else {
            // Not rewinding — record state
            if (rr->rewinding) {
                // Just released rewind
                rr->rewinding = false;
                rr->rewindPlayhead = 0.0f;
                rr->cooldownTimer = rr->cooldown;
                ENJIN_LOG_INFO(Game, "Rewind ended (entity %llu, cooldown %.1fs)",
                    (unsigned long long)entity, rr->cooldown);
            }

            rr->recordTimer += deltaTime;
            if (rr->recordTimer >= rr->recordInterval) {
                rr->recordTimer -= rr->recordInterval;

                // Compute timestamp
                f32 timestamp = rr->frames.empty() ? 0.0f :
                    rr->frames.back().timestamp + rr->recordInterval;

                // Record frame
                ECS::RecordRewindComponent::Frame frame;
                frame.position = transform->position;
                frame.rotation = transform->rotation;
                frame.scale = transform->scale;
                frame.velocity = velocity;
                frame.health = health;
                frame.timestamp = timestamp;
                rr->frames.push_back(frame);

                // Cap buffer size based on maxDuration
                u32 maxFrames = static_cast<u32>(rr->maxDuration / rr->recordInterval) + 1;
                while (rr->frames.size() > maxFrames) {
                    rr->frames.erase(rr->frames.begin());
                }
            }
        }
    }
}

} // namespace Enjin::Gameplay
