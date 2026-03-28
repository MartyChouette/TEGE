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
    m_SceneRewinding = false;

    // Scene rewind takes priority — if active, skip entity rewind
    UpdateSceneRewind(deltaTime);
    if (!m_SceneRewinding) {
        UpdateEntityRewind(deltaTime);
    }
}

// ============================================================================
// Entity Rewind (Braid-style — per-entity RecordRewindComponent)
// ============================================================================

void RecordRewindSystem::UpdateEntityRewind(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::RecordRewindComponent>()) {
        auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
        if (!rr || !rr->enabled) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Get velocity from whichever controller exists
        Math::Vector3 velocity(0.0f);
        if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity))
            velocity = ctrl->velocity;
        else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity))
            velocity = ctrl3->velocity;
        else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity))
            velocity = ctrl1->velocity;

        f32 health = 0.0f;
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (hp) health = hp->currentHealth;

        // Cooldown
        if (rr->cooldownTimer > 0.0f) rr->cooldownTimer -= deltaTime;

        // Check rewind input
        bool rewindHeld = Input::IsKeyDown(KeyCode::R) && rr->cooldownTimer <= 0.0f;

        if (rewindHeld && !rr->frames.empty()) {
            if (!rr->rewinding) {
                rr->rewinding = true;
                rr->rewindPlayhead = 0.0f;
            }

            rr->rewindPlayhead += deltaTime * rr->rewindSpeed;
            m_AnyRewinding = true;

            f32 latestTime = rr->frames.back().timestamp;
            f32 targetTime = latestTime - rr->rewindPlayhead;

            // Find surrounding frames for interpolation
            i32 frameA = 0, frameB = 0;
            for (i32 i = static_cast<i32>(rr->frames.size()) - 1; i >= 0; --i) {
                if (rr->frames[i].timestamp <= targetTime) {
                    frameA = i;
                    frameB = (i + 1 < static_cast<i32>(rr->frames.size())) ? i + 1 : i;
                    break;
                }
            }

            const auto& fA = rr->frames[frameA];
            const auto& fB = rr->frames[frameB];
            f32 t = 0.0f;
            if (frameA != frameB) {
                f32 span = fB.timestamp - fA.timestamp;
                if (span > 0.0001f) t = Math::Clamp((targetTime - fA.timestamp) / span, 0.0f, 1.0f);
            }

            transform->position = fA.position + (fB.position - fA.position) * t;
            transform->rotation = Math::Quaternion::Slerp(fA.rotation, fB.rotation, t);
            transform->scale = fA.scale + (fB.scale - fA.scale) * t;

            // Restore velocity
            Math::Vector3 interpVel = fA.velocity + (fB.velocity - fA.velocity) * t;
            if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity)) ctrl->velocity = interpVel;
            else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity)) ctrl3->velocity = interpVel;
            else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity)) ctrl1->velocity = interpVel;

            if (hp) {
                hp->currentHealth = fA.health + (fB.health - fA.health) * t;
                if (hp->isDead && hp->currentHealth > 0.0f) hp->isDead = false;
            }

            // Pop consumed frames
            while (!rr->frames.empty() && rr->frames.back().timestamp > targetTime + 0.001f)
                rr->frames.pop_back();

        } else {
            if (rr->rewinding) {
                rr->rewinding = false;
                rr->rewindPlayhead = 0.0f;
                rr->cooldownTimer = rr->cooldown;
            }

            // Record
            rr->recordTimer += deltaTime;
            if (rr->recordTimer >= rr->recordInterval) {
                rr->recordTimer -= rr->recordInterval;
                f32 timestamp = rr->frames.empty() ? 0.0f : rr->frames.back().timestamp + rr->recordInterval;

                ECS::RecordRewindComponent::Frame frame;
                frame.position = transform->position;
                frame.rotation = transform->rotation;
                frame.scale = transform->scale;
                frame.velocity = velocity;
                frame.health = health;
                frame.timestamp = timestamp;
                rr->frames.push_back(frame);

                u32 maxFrames = static_cast<u32>(rr->maxDuration / rr->recordInterval) + 1;
                while (rr->frames.size() > maxFrames) rr->frames.erase(rr->frames.begin());
            }
        }
    }
}

// ============================================================================
// Scene Rewind (Sands of Time-style — SceneRewindComponent)
// Records ALL entities with TransformComponent, rewinds entire world.
// ============================================================================

void RecordRewindSystem::UpdateSceneRewind(f32 deltaTime) {
    for (auto manager : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(manager);
        if (!sr || !sr->enabled) continue;

        // Cooldown
        if (sr->cooldownTimer > 0.0f) sr->cooldownTimer -= deltaTime;

        // Check rewind input (T key by default)
        bool canRewind = sr->cooldownTimer <= 0.0f &&
            (sr->charges == 0 || sr->chargesUsed < sr->charges);
        bool rewindHeld = Input::IsKeyDown(KeyCode::T) && canRewind;

        if (rewindHeld && !sr->frames.empty()) {
            // --- REWINDING ---
            if (!sr->rewinding) {
                sr->rewinding = true;
                sr->rewindPlayhead = 0.0f;
                sr->chargesUsed++;
                ENJIN_LOG_INFO(Game, "Scene rewind started (%zu frames, charge %d/%d)",
                    sr->frames.size(), sr->chargesUsed, sr->charges);
            }

            sr->rewindPlayhead += deltaTime * sr->rewindSpeed;
            m_AnyRewinding = true;
            m_SceneRewinding = true;

            f32 latestTime = sr->frames.back().timestamp;
            f32 targetTime = latestTime - sr->rewindPlayhead;

            // Find the closest frame
            i32 bestFrame = 0;
            f32 bestDist = 999999.0f;
            for (i32 i = 0; i < static_cast<i32>(sr->frames.size()); ++i) {
                f32 dist = Math::Abs(sr->frames[i].timestamp - targetTime);
                if (dist < bestDist) { bestDist = dist; bestFrame = i; }
            }

            // Apply the frame to ALL entities
            const auto& frame = sr->frames[bestFrame];
            for (const auto& ef : frame.entities) {
                if (!m_World->IsValid(ef.entity)) continue;

                auto* t = m_World->GetComponent<ECS::TransformComponent>(ef.entity);
                if (t) {
                    t->position = ef.position;
                    t->rotation = ef.rotation;
                    t->scale = ef.scale;
                    t->visible = ef.visible;
                }

                // Restore velocity on controllers
                if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(ef.entity))
                    ctrl->velocity = ef.velocity;
                else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(ef.entity))
                    ctrl3->velocity = ef.velocity;
                else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(ef.entity))
                    ctrl1->velocity = ef.velocity;

                // Restore health
                auto* hp = m_World->GetComponent<ECS::HealthComponent>(ef.entity);
                if (hp) {
                    hp->currentHealth = ef.health;
                    if (hp->isDead && ef.health > 0.0f) hp->isDead = false;
                    if (!ef.isDead) hp->isDead = false;
                }
            }

            // Pop consumed frames
            while (!sr->frames.empty() && sr->frames.back().timestamp > targetTime + 0.001f)
                sr->frames.pop_back();

        } else {
            // --- RECORDING ---
            if (sr->rewinding) {
                sr->rewinding = false;
                sr->rewindPlayhead = 0.0f;
                sr->cooldownTimer = sr->cooldown;
                ENJIN_LOG_INFO(Game, "Scene rewind ended (cooldown %.1fs)", sr->cooldown);
            }

            sr->recordTimer += deltaTime;
            if (sr->recordTimer >= sr->recordInterval) {
                sr->recordTimer -= sr->recordInterval;
                f32 timestamp = sr->frames.empty() ? 0.0f : sr->frames.back().timestamp + sr->recordInterval;

                ECS::SceneRewindComponent::SceneFrame frame;
                frame.timestamp = timestamp;

                // Snapshot ALL entities with TransformComponent
                for (auto entity : m_World->GetEntitiesWithComponent<ECS::TransformComponent>()) {
                    if (entity == manager) continue; // Skip the manager entity itself

                    auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
                    if (!t) continue;

                    ECS::SceneRewindComponent::EntityFrame ef;
                    ef.entity = entity;
                    ef.position = t->position;
                    ef.rotation = t->rotation;
                    ef.scale = t->scale;
                    ef.visible = t->visible;

                    // Capture velocity
                    if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity))
                        ef.velocity = ctrl->velocity;
                    else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity))
                        ef.velocity = ctrl3->velocity;
                    else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity))
                        ef.velocity = ctrl1->velocity;

                    // Capture health
                    auto* hp = m_World->GetComponent<ECS::HealthComponent>(entity);
                    if (hp) {
                        ef.health = hp->currentHealth;
                        ef.isDead = hp->isDead;
                    }

                    frame.entities.push_back(ef);
                }

                sr->frames.push_back(std::move(frame));

                // Cap buffer
                u32 maxFrames = static_cast<u32>(sr->maxDuration / sr->recordInterval) + 1;
                while (sr->frames.size() > maxFrames) sr->frames.erase(sr->frames.begin());
            }
        }
    }
}

} // namespace Enjin::Gameplay
