#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

namespace Enjin::Gameplay {

// Epsilon thresholds for delta change detection
static constexpr f32 POS_EPSILON = 0.001f;
static constexpr f32 ROT_EPSILON = 0.001f;
static constexpr f32 SCALE_EPSILON = 0.0001f;
static constexpr f32 HEALTH_EPSILON = 0.01f;

// ============================================================================
// Main Update
// ============================================================================

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
// Snapshot Capture / Restore
// ============================================================================

void RecordRewindSystem::CaptureEntitySnapshot(ECS::Entity entity, EntitySnapshot& out, u32 channelMask) {
    out.entity = entity;
    out.channelMask = channelMask;

    if (HasChannel(channelMask, RewindChannelFlags::Transform)) {
        auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (t) {
            out.position = t->position;
            out.rotation = t->rotation;
            out.scale = t->scale;
            out.visible = t->visible;
        }
    }

    if (HasChannel(channelMask, RewindChannelFlags::Velocity)) {
        if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity))
            out.velocity = ctrl->velocity;
        else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity))
            out.velocity = ctrl3->velocity;
        else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity))
            out.velocity = ctrl1->velocity;
    }

    if (HasChannel(channelMask, RewindChannelFlags::Health)) {
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (hp) {
            out.health = hp->currentHealth;
            out.shield = hp->currentShield;
            out.isDead = hp->isDead;
        }
    }

    if (HasChannel(channelMask, RewindChannelFlags::Animation)) {
        auto* anim = m_World->GetComponent<ECS::AnimatorComponent>(entity);
        if (anim) {
            out.animNormalizedTime = anim->animator.GetNormalizedTime();
        }
    }
}

void RecordRewindSystem::RestoreEntitySnapshot(ECS::Entity entity, const EntitySnapshot& snap) {
    if (!m_World->IsValid(entity)) return;

    if (HasChannel(snap.channelMask, RewindChannelFlags::Transform)) {
        auto* t = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (t) {
            t->position = snap.position;
            t->rotation = snap.rotation;
            t->scale = snap.scale;
            t->visible = snap.visible;
        }
    }

    if (HasChannel(snap.channelMask, RewindChannelFlags::Velocity)) {
        if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity))
            ctrl->velocity = snap.velocity;
        else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity))
            ctrl3->velocity = snap.velocity;
        else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity))
            ctrl1->velocity = snap.velocity;
    }

    if (HasChannel(snap.channelMask, RewindChannelFlags::Health)) {
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (hp) {
            hp->currentHealth = snap.health;
            hp->currentShield = snap.shield;
            if (hp->isDead && !snap.isDead) hp->isDead = false;
        }
    }

    // Animation restore: seek the animator to the recorded time
    // (SkeletalAnimator doesn't expose a time setter, so we skip restore for now —
    //  the animator will naturally resume from its current state after rewind stops)

    // Sync physics body state if physics channel or transform was restored
    if (HasChannel(snap.channelMask, RewindChannelFlags::Transform)) {
        if (m_Physics) {
            m_Physics->ForceSetBodyState(entity, snap.position, snap.rotation,
                                          snap.linearVelocity, snap.angularVelocity);
        }
        if (m_Physics2D) {
            m_Physics2D->ForceSetBodyState(entity, snap.position, snap.velocity);
        }
    }
}

void RecordRewindSystem::RestoreEntitySnapshotInterpolated(ECS::Entity entity,
                                                            const EntitySnapshot& a,
                                                            const EntitySnapshot& b, f32 t) {
    if (!m_World->IsValid(entity)) return;
    u32 mask = a.channelMask;

    if (HasChannel(mask, RewindChannelFlags::Transform)) {
        auto* tr = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (tr) {
            tr->position = a.position + (b.position - a.position) * t;
            tr->rotation = Math::Quaternion::Slerp(a.rotation, b.rotation, t);
            tr->scale = a.scale + (b.scale - a.scale) * t;
            tr->visible = (t < 0.5f) ? a.visible : b.visible;
        }
    }

    if (HasChannel(mask, RewindChannelFlags::Velocity)) {
        Math::Vector3 interpVel = a.velocity + (b.velocity - a.velocity) * t;
        if (auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity))
            ctrl->velocity = interpVel;
        else if (auto* ctrl3 = m_World->GetComponent<ECS::ThirdPersonController>(entity))
            ctrl3->velocity = interpVel;
        else if (auto* ctrl1 = m_World->GetComponent<ECS::FirstPersonController>(entity))
            ctrl1->velocity = interpVel;
    }

    if (HasChannel(mask, RewindChannelFlags::Health)) {
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (hp) {
            hp->currentHealth = a.health + (b.health - a.health) * t;
            hp->currentShield = a.shield + (b.shield - a.shield) * t;
            if (hp->isDead && hp->currentHealth > 0.0f) hp->isDead = false;
        }
    }

    // Animation interpolation: SkeletalAnimator doesn't expose a time setter,
    // so we skip animation restore during interpolated rewind.

    // Sync physics
    if (HasChannel(mask, RewindChannelFlags::Transform)) {
        Math::Vector3 pos = a.position + (b.position - a.position) * t;
        Math::Quaternion rot = Math::Quaternion::Slerp(a.rotation, b.rotation, t);
        Math::Vector3 vel = a.velocity + (b.velocity - a.velocity) * t;
        if (m_Physics) {
            m_Physics->ForceSetBodyState(entity, pos, rot, vel, Math::Vector3(0.0f));
        }
        if (m_Physics2D) {
            m_Physics2D->ForceSetBodyState(entity, pos, vel);
        }
    }
}

// ============================================================================
// Delta Change Detection
// ============================================================================

bool RecordRewindSystem::HasEntityChanged(const EntitySnapshot& current, const EntitySnapshot& prev, u32 channelMask) const {
    if (HasChannel(channelMask, RewindChannelFlags::Transform)) {
        if ((current.position - prev.position).LengthSquared() > POS_EPSILON * POS_EPSILON) return true;
        // Quaternion distance
        f32 dot = Math::Abs(current.rotation.Dot(prev.rotation));
        if (dot < 1.0f - ROT_EPSILON) return true;
        if ((current.scale - prev.scale).LengthSquared() > SCALE_EPSILON * SCALE_EPSILON) return true;
        if (current.visible != prev.visible) return true;
    }
    if (HasChannel(channelMask, RewindChannelFlags::Health)) {
        if (Math::Abs(current.health - prev.health) > HEALTH_EPSILON) return true;
        if (current.isDead != prev.isDead) return true;
    }
    if (HasChannel(channelMask, RewindChannelFlags::Velocity)) {
        if ((current.velocity - prev.velocity).LengthSquared() > POS_EPSILON * POS_EPSILON) return true;
    }
    return false;
}

// ============================================================================
// Entity Rewind (Braid-style — per-entity RecordRewindComponent)
// ============================================================================

void RecordRewindSystem::UpdateEntityRewind(f32 deltaTime) {
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::RecordRewindComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
        if (!rr || !rr->enabled) continue;

        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Ensure ring buffer is sized
        u32 maxFrames = static_cast<u32>(rr->maxDuration / rr->recordInterval) + 2;
        if (rr->history.Capacity() < maxFrames) {
            rr->history.Reserve(maxFrames);
        }

        // Cooldown
        if (rr->cooldownTimer > 0.0f) rr->cooldownTimer -= deltaTime;

        // Check rewind input
        bool rewindHeld = Input::IsKeyDown(static_cast<KeyCode>(rr->rewindKey)) && rr->cooldownTimer <= 0.0f;

        if (rewindHeld && !rr->history.Empty()) {
            if (!rr->rewinding) {
                rr->rewinding = true;
                rr->rewindPlayhead = 0.0f;
            }

            rr->rewindPlayhead += deltaTime * rr->rewindSpeed;
            m_AnyRewinding = true;

            f32 latestTime = rr->history.Back().position.x; // timestamp stored in EntitySnapshot
            // We store timestamp as currentRecordedTime at capture time
            // Find surrounding frames for interpolation
            f32 targetTime = rr->currentRecordedTime - rr->rewindPlayhead;

            i32 frameA = 0, frameB = 0;
            for (i32 i = static_cast<i32>(rr->history.Count()) - 1; i >= 0; --i) {
                // We need timestamps — stored in a separate way. Use index * interval as approximation.
                f32 frameTime = rr->currentRecordedTime - (rr->history.Count() - 1 - i) * rr->recordInterval;
                if (frameTime <= targetTime) {
                    frameA = i;
                    frameB = (i + 1 < static_cast<i32>(rr->history.Count())) ? i + 1 : i;
                    break;
                }
            }

            if (frameA == frameB) {
                RestoreEntitySnapshot(entity, rr->history.At(frameA));
            } else {
                f32 timeA = rr->currentRecordedTime - (rr->history.Count() - 1 - frameA) * rr->recordInterval;
                f32 timeB = rr->currentRecordedTime - (rr->history.Count() - 1 - frameB) * rr->recordInterval;
                f32 span = timeB - timeA;
                f32 t = (span > 0.0001f) ? Math::Clamp((targetTime - timeA) / span, 0.0f, 1.0f) : 0.0f;
                RestoreEntitySnapshotInterpolated(entity, rr->history.At(frameA), rr->history.At(frameB), t);
            }

            // Pop consumed frames from the back
            while (!rr->history.Empty()) {
                f32 backTime = rr->currentRecordedTime - 0 * rr->recordInterval;  // newest
                if (backTime > targetTime + 0.001f) {
                    rr->history.PopBack();
                    rr->currentRecordedTime -= rr->recordInterval;
                } else {
                    break;
                }
            }

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
                rr->currentRecordedTime += rr->recordInterval;

                EntitySnapshot snap;
                CaptureEntitySnapshot(entity, snap, rr->channels);
                rr->history.Push(std::move(snap));
            }
        }
    }
}

// ============================================================================
// Scene Rewind (Sands of Time-style — SceneRewindComponent)
// Records ALL entities with TransformComponent, supports delta compression.
// ============================================================================

void RecordRewindSystem::UpdateSceneRewind(f32 deltaTime) {
    for (auto manager : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        if (!m_World->IsValid(manager)) continue;
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(manager);
        if (!sr || !sr->enabled) continue;

        // Ensure ring buffer is sized
        u32 maxFrames = static_cast<u32>(sr->maxDuration / sr->recordInterval) + 2;
        if (sr->history.Capacity() < maxFrames) {
            sr->history.Reserve(maxFrames);
        }

        // Cooldown
        if (sr->cooldownTimer > 0.0f) sr->cooldownTimer -= deltaTime;

        // Check rewind input
        bool canRewind = sr->cooldownTimer <= 0.0f &&
            (sr->charges == 0 || sr->chargesUsed < sr->charges);
        bool rewindHeld = Input::IsKeyDown(static_cast<KeyCode>(sr->rewindKey)) && canRewind;

        if (rewindHeld && !sr->history.Empty()) {
            // --- REWINDING ---
            if (!sr->rewinding) {
                sr->rewinding = true;
                sr->rewindPlayhead = 0.0f;
                sr->chargesUsed++;
                ENJIN_LOG_INFO(Game, "Scene rewind started (%u frames, charge %d/%d)",
                    sr->history.Count(), sr->chargesUsed, sr->charges);
            }

            sr->rewindPlayhead += deltaTime * sr->rewindSpeed;
            m_AnyRewinding = true;
            m_SceneRewinding = true;

            f32 targetTime = sr->currentRecordedTime - sr->rewindPlayhead;

            // Find the two surrounding frames for interpolation
            i32 bestA = 0, bestB = 0;
            for (i32 i = static_cast<i32>(sr->history.Count()) - 1; i >= 0; --i) {
                if (sr->history.At(i).timestamp <= targetTime) {
                    bestA = i;
                    bestB = (i + 1 < static_cast<i32>(sr->history.Count())) ? i + 1 : i;
                    break;
                }
            }

            const auto& frameA = sr->history.At(bestA);

            // For delta frames, we need to reconstruct full state from nearest keyframe
            // For simplicity in v1, scene rewind snaps to nearest frame and restores all entities
            for (const auto& snap : frameA.snapshots) {
                RestoreEntitySnapshot(snap.entity, snap);
            }

            // If this is a delta frame, also apply any entities from the nearest keyframe
            // that aren't in this delta (they haven't changed)
            if (!frameA.isKeyframe) {
                // Walk back to find the nearest keyframe
                for (i32 k = bestA - 1; k >= 0; --k) {
                    const auto& kf = sr->history.At(k);
                    if (kf.isKeyframe) {
                        for (const auto& snap : kf.snapshots) {
                            // Only restore entities not already covered by the delta chain
                            bool alreadyRestored = false;
                            // Check all frames between keyframe and target
                            for (i32 d = k + 1; d <= bestA; ++d) {
                                for (const auto& ds : sr->history.At(d).snapshots) {
                                    if (ds.entity == snap.entity) { alreadyRestored = true; break; }
                                }
                                if (alreadyRestored) break;
                            }
                            if (!alreadyRestored) {
                                RestoreEntitySnapshot(snap.entity, snap);
                            }
                        }
                        break;
                    }
                }
            }

            // Pop consumed frames
            while (!sr->history.Empty() && sr->history.Back().timestamp > targetTime + 0.001f) {
                sr->history.PopBack();
            }

        } else {
            // --- RECORDING ---
            if (sr->rewinding) {
                sr->rewinding = false;
                sr->rewindPlayhead = 0.0f;
                sr->cooldownTimer = sr->cooldown;
                sr->prevFrameCache.clear();
                sr->framesSinceKeyframe = 0;
                ENJIN_LOG_INFO(Game, "Scene rewind ended (cooldown %.1fs)", sr->cooldown);
            }

            sr->recordTimer += deltaTime;
            if (sr->recordTimer >= sr->recordInterval) {
                sr->recordTimer -= sr->recordInterval;
                sr->currentRecordedTime += sr->recordInterval;

                bool isKeyframe = !sr->deltaCompression ||
                                   sr->framesSinceKeyframe >= sr->keyframeInterval ||
                                   sr->history.Empty();

                DeltaFrame frame;
                frame.timestamp = sr->currentRecordedTime;
                frame.isKeyframe = isKeyframe;

                // Snapshot ALL entities with TransformComponent
                for (auto entity : m_World->GetEntitiesWithComponent<ECS::TransformComponent>()) {
                    if (entity == manager) continue;

                    EntitySnapshot snap;
                    CaptureEntitySnapshot(entity, snap, sr->channels);

                    if (isKeyframe) {
                        // Keyframe: store all entities
                        frame.snapshots.push_back(std::move(snap));
                    } else {
                        // Delta: only store changed entities
                        auto it = sr->prevFrameCache.find(entity);
                        if (it == sr->prevFrameCache.end() ||
                            HasEntityChanged(snap, it->second, sr->channels)) {
                            frame.snapshots.push_back(std::move(snap));
                        }
                    }
                }

                // Update previous-frame cache in-place (avoids clear+rebuild rehashing).
                // Overwrite existing entries; new entities get inserted; stale entries are harmless.
                for (auto entity : m_World->GetEntitiesWithComponent<ECS::TransformComponent>()) {
                    if (entity == manager) continue;
                    auto& cached = sr->prevFrameCache[entity]; // insert-or-access, no rehash if exists
                    CaptureEntitySnapshot(entity, cached, sr->channels);
                }

                sr->history.Push(std::move(frame));

                if (isKeyframe) {
                    sr->framesSinceKeyframe = 0;
                } else {
                    sr->framesSinceKeyframe++;
                }
            }
        }
    }
}

// ============================================================================
// Programmatic Control (for scripting / editor)
// ============================================================================

void RecordRewindSystem::StartEntityRewind(ECS::Entity entity) {
    if (!m_World) return;
    auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
    if (rr && !rr->rewinding && !rr->history.Empty()) {
        rr->rewinding = true;
        rr->rewindPlayhead = 0.0f;
    }
}

void RecordRewindSystem::StopEntityRewind(ECS::Entity entity) {
    if (!m_World) return;
    auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
    if (rr && rr->rewinding) {
        rr->rewinding = false;
        rr->rewindPlayhead = 0.0f;
        rr->cooldownTimer = rr->cooldown;
    }
}

void RecordRewindSystem::StartSceneRewind() {
    if (!m_World) return;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (sr && !sr->rewinding && !sr->history.Empty()) {
            bool canRewind = sr->charges == 0 || sr->chargesUsed < sr->charges;
            if (canRewind) {
                sr->rewinding = true;
                sr->rewindPlayhead = 0.0f;
                sr->chargesUsed++;
            }
        }
    }
}

void RecordRewindSystem::StopSceneRewind() {
    if (!m_World) return;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (sr && sr->rewinding) {
            sr->rewinding = false;
            sr->rewindPlayhead = 0.0f;
            sr->cooldownTimer = sr->cooldown;
            sr->prevFrameCache.clear();
            sr->framesSinceKeyframe = 0;
        }
    }
}

void RecordRewindSystem::SeekSceneToTime(f32 timeOffset) {
    if (!m_World) return;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (!sr || sr->history.Empty()) continue;

        f32 targetTime = sr->currentRecordedTime - timeOffset;
        if (targetTime < 0.0f) targetTime = 0.0f;

        // Find nearest frame and restore
        i32 best = 0;
        f32 bestDist = 999999.0f;
        for (u32 i = 0; i < sr->history.Count(); ++i) {
            f32 dist = Math::Abs(sr->history.At(i).timestamp - targetTime);
            if (dist < bestDist) { bestDist = dist; best = static_cast<i32>(i); }
        }

        const auto& frame = sr->history.At(best);
        for (const auto& snap : frame.snapshots) {
            RestoreEntitySnapshot(snap.entity, snap);
        }

        // Handle delta reconstruction from keyframe
        if (!frame.isKeyframe) {
            for (i32 k = best - 1; k >= 0; --k) {
                const auto& kf = sr->history.At(k);
                if (kf.isKeyframe) {
                    for (const auto& snap : kf.snapshots) {
                        bool covered = false;
                        for (i32 d = k + 1; d <= best; ++d) {
                            for (const auto& ds : sr->history.At(d).snapshots) {
                                if (ds.entity == snap.entity) { covered = true; break; }
                            }
                            if (covered) break;
                        }
                        if (!covered) RestoreEntitySnapshot(snap.entity, snap);
                    }
                    break;
                }
            }
        }
    }
}

void RecordRewindSystem::SeekEntityToTime(ECS::Entity entity, f32 timeOffset) {
    if (!m_World) return;
    auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
    if (!rr || rr->history.Empty()) return;

    f32 targetTime = rr->currentRecordedTime - timeOffset;
    if (targetTime < 0.0f) targetTime = 0.0f;

    // Find nearest frame
    i32 best = 0;
    f32 bestDist = 999999.0f;
    for (u32 i = 0; i < rr->history.Count(); ++i) {
        f32 frameTime = rr->currentRecordedTime - (rr->history.Count() - 1 - i) * rr->recordInterval;
        f32 dist = Math::Abs(frameTime - targetTime);
        if (dist < bestDist) { bestDist = dist; best = static_cast<i32>(i); }
    }

    RestoreEntitySnapshot(entity, rr->history.At(best));
}

// ============================================================================
// Query
// ============================================================================

f32 RecordRewindSystem::GetSceneRecordedDuration() const {
    if (!m_World) return 0.0f;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (sr) return sr->currentRecordedTime;
    }
    return 0.0f;
}

f32 RecordRewindSystem::GetSceneCurrentTime() const {
    if (!m_World) return 0.0f;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (sr) return sr->rewinding ? (sr->currentRecordedTime - sr->rewindPlayhead) : sr->currentRecordedTime;
    }
    return 0.0f;
}

u32 RecordRewindSystem::GetSceneFrameCount() const {
    if (!m_World) return 0;
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (sr) return sr->history.Count();
    }
    return 0;
}

usize RecordRewindSystem::GetMemoryUsageBytes() const {
    usize total = 0;
    if (!m_World) return total;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::RecordRewindComponent>()) {
        auto* rr = m_World->GetComponent<ECS::RecordRewindComponent>(entity);
        if (rr) total += rr->history.MemoryUsage();
    }
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::SceneRewindComponent>()) {
        auto* sr = m_World->GetComponent<ECS::SceneRewindComponent>(entity);
        if (sr) total += sr->history.MemoryUsage();
    }
    return total;
}

} // namespace Enjin::Gameplay
