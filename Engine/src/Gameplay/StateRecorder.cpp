// Implementation of the shared recording core. See StateRecorder.h for why these
// three features are separate and what they share.
//
// The four EntityStateSampler methods moved here verbatim from
// RecordRewindSystem, which is where they used to live as private members. They
// never needed anything from the rewind components -- only the world and the two
// physics backends -- which is what made them extractable and what made the
// Debug Recorder's old disguise possible in the first place.
#include "Enjin/Gameplay/StateRecorder.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

namespace Enjin::Gameplay {

// Epsilon thresholds for delta change detection
static constexpr f32 POS_EPSILON = 0.001f;
static constexpr f32 ROT_EPSILON = 0.001f;
static constexpr f32 SCALE_EPSILON = 0.0001f;
static constexpr f32 HEALTH_EPSILON = 0.01f;

// ============================================================================
// EntityStateSampler
// ============================================================================

void EntityStateSampler::Capture(ECS::Entity entity, EntitySnapshot& out, u32 channelMask) const {
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

    // Physics and Material were declared in RewindChannelFlags, drawn in the
    // inspector and serialized, and never captured by anything -- so the
    // Physics checkbox restored zeroed velocity (RestoreEntitySnapshot reads
    // these two fields) and the Material checkbox did nothing at all.
    if (HasChannel(channelMask, RewindChannelFlags::Physics)) {
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        if (rb) {
            out.linearVelocity = rb->velocity;
            out.angularVelocity = rb->angularVelocity;
        }
    }

    if (HasChannel(channelMask, RewindChannelFlags::Material)) {
        auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) {
            out.opacity = mat->opacity;
            out.baseColor = mat->baseColor;
        }
    }
}


void EntityStateSampler::Restore(ECS::Entity entity, const EntitySnapshot& snap) const {
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

    if (HasChannel(snap.channelMask, RewindChannelFlags::Animation)) {
        // Seek the animator to the recorded time.
        //
        // This used to be a comment saying "SkeletalAnimator doesn't expose a time
        // setter, so we skip restore for now". It does expose one, and has for
        // longer than this comment has been here: SetNormalizedTime seeks, resamples
        // the clip, and recalculates the world transforms and skinning matrices.
        // Capture was writing animNormalizedTime into every snapshot the whole time,
        // so the Animation checkbox recorded a channel that was then thrown away --
        // a rewound character walked backwards through its own footsteps with its
        // legs still cycling forward.
        //
        // Nothing pauses the animator during a rewind, so it also ticks forward each
        // frame. That drift is bounded by one frame and re-corrected by the next
        // restore, because this runs every frame of the rewind rather than once at
        // the end.
        if (auto* anim = m_World->GetComponent<ECS::AnimatorComponent>(entity)) {
            anim->animator.SetNormalizedTime(snap.animNormalizedTime);
        }
    }

    if (HasChannel(snap.channelMask, RewindChannelFlags::Material)) {
        auto* mat = m_World->GetComponent<ECS::MaterialComponent>(entity);
        if (mat) {
            mat->opacity = snap.opacity;
            mat->baseColor = snap.baseColor;
        }
    }

    if (HasChannel(snap.channelMask, RewindChannelFlags::Physics)) {
        auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity);
        if (rb) {
            rb->velocity = snap.linearVelocity;
            rb->angularVelocity = snap.angularVelocity;
        }
    }

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


void EntityStateSampler::RestoreInterpolated(ECS::Entity entity,
                                                            const EntitySnapshot& a,
                                                            const EntitySnapshot& b, f32 t) const {
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

    if (HasChannel(mask, RewindChannelFlags::Animation)) {
        if (auto* anim = m_World->GetComponent<ECS::AnimatorComponent>(entity)) {
            anim->animator.SetNormalizedTime(
                LerpNormalizedAnimTime(a.animNormalizedTime, b.animNormalizedTime, t));
        }
    }

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


bool EntityStateSampler::Differs(const EntitySnapshot& current, const EntitySnapshot& prev, u32 channelMask) const {
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
// WorldStateRecorder
// ============================================================================

void WorldStateRecorder::Configure(const Config& config) {
    if (m_Config == config) return;

    const bool windowChanged = m_Config.maxDuration != config.maxDuration ||
                               m_Config.recordInterval != config.recordInterval;
    m_Config = config;

    if (windowChanged) {
        // +2 so the newest frame and the one being written never fight over the
        // same slot at the exact moment the window is full.
        const f32 interval = m_Config.recordInterval > 0.0001f ? m_Config.recordInterval
                                                              : 1.0f / 60.0f;
        const u32 frames = static_cast<u32>(m_Config.maxDuration / interval) + 2;
        if (m_History.Capacity() < frames) m_History.Reserve(frames);
    }
}

bool WorldStateRecorder::Tick(f32 deltaTime, ECS::Entity exclude) {
    ECS::World* world = m_Sampler.GetWorld();
    if (!world) return false;

    const f32 interval = m_Config.recordInterval > 0.0001f ? m_Config.recordInterval
                                                           : 1.0f / 60.0f;
    if (m_History.Capacity() == 0) {
        m_History.Reserve(static_cast<u32>(m_Config.maxDuration / interval) + 2);
    }

    m_RecordTimer += deltaTime;
    if (m_RecordTimer < interval) return false;

    m_RecordTimer -= interval;
    m_RecordedTime += interval;

    const bool isKeyframe = !m_Config.deltaCompression ||
                            m_FramesSinceKeyframe >= m_Config.keyframeInterval ||
                            m_History.Empty();

    DeltaFrame frame;
    frame.timestamp = m_RecordedTime;
    frame.isKeyframe = isKeyframe;

    // Snapshot every entity that has a transform, ONCE each. An earlier version
    // captured every entity a second time purely to refill the delta cache and
    // discarded the first result, which with deltaCompression off built a cache
    // nothing read -- a second full copy of the scene that the memory readout did
    // not count.
    for (auto entity : world->GetEntitiesWithComponent<ECS::TransformComponent>()) {
        if (entity == exclude) continue;

        EntitySnapshot snap;
        m_Sampler.Capture(entity, snap, m_Config.channels);
        snap.timestamp = m_RecordedTime;

        bool keep = isKeyframe;
        if (!keep) {
            auto it = m_PrevFrameCache.find(entity);
            keep = (it == m_PrevFrameCache.end() ||
                    m_Sampler.Differs(snap, it->second, m_Config.channels));
        }
        if (keep) frame.snapshots.push_back(snap);

        // Only the delta path reads this cache.
        if (m_Config.deltaCompression) m_PrevFrameCache[entity] = std::move(snap);
    }

    m_History.Push(std::move(frame));
    m_FramesSinceKeyframe = isKeyframe ? 0 : m_FramesSinceKeyframe + 1;
    return true;
}

void WorldStateRecorder::RestoreFrameAndAncestors(i32 index) const {
    const auto& frame = m_History.At(static_cast<u32>(index));
    for (const auto& snap : frame.snapshots) {
        m_Sampler.Restore(snap.entity, snap);
    }
    if (frame.isKeyframe) return;

    // A delta frame only carries what changed, so anything it does not mention
    // has to come from the nearest keyframe behind it -- unless a frame BETWEEN
    // that keyframe and this one already supplied a newer value for it.
    for (i32 k = index - 1; k >= 0; --k) {
        const auto& kf = m_History.At(static_cast<u32>(k));
        if (!kf.isKeyframe) continue;

        for (const auto& snap : kf.snapshots) {
            bool superseded = false;
            for (i32 d = k + 1; d <= index && !superseded; ++d) {
                for (const auto& ds : m_History.At(static_cast<u32>(d)).snapshots) {
                    if (ds.entity == snap.entity) { superseded = true; break; }
                }
            }
            if (!superseded) m_Sampler.Restore(snap.entity, snap);
        }
        return;
    }
}

bool WorldStateRecorder::RestoreAtTime(f32 absoluteTime) const {
    if (m_History.Empty() || !m_Sampler.GetWorld()) return false;

    // Newest-first walk to the last frame at or before the target.
    i32 best = 0;
    for (i32 i = static_cast<i32>(m_History.Count()) - 1; i >= 0; --i) {
        if (m_History.At(static_cast<u32>(i)).timestamp <= absoluteTime) { best = i; break; }
    }
    RestoreFrameAndAncestors(best);
    return true;
}

void WorldStateRecorder::TrimAfter(f32 absoluteTime) {
    while (!m_History.Empty() && m_History.Back().timestamp > absoluteTime + 0.001f) {
        m_History.PopBack();
    }
    // The cache described frames that are now gone. Clearing it makes the next
    // frame capture everything, which is the safe direction: a stale cache would
    // decide an entity had not changed against a value no longer in the buffer,
    // and that entity would go missing until the next keyframe.
    m_PrevFrameCache.clear();
    m_FramesSinceKeyframe = m_Config.keyframeInterval;   // next frame is a keyframe
}

void WorldStateRecorder::ResetClockToLatest() {
    m_RecordedTime = m_History.Empty() ? 0.0f : m_History.Back().timestamp;
    m_RecordTimer = 0.0f;
}

void WorldStateRecorder::Release() {
    m_History.Release();
    std::unordered_map<ECS::Entity, EntitySnapshot>().swap(m_PrevFrameCache);
    m_RecordTimer = 0.0f;
    m_RecordedTime = 0.0f;
    m_FramesSinceKeyframe = 0;
    m_Config = Config{};
}

void WorldStateRecorder::Clear() {
    m_History.Clear();
    m_PrevFrameCache.clear();
    m_RecordTimer = 0.0f;
    m_RecordedTime = 0.0f;
    m_FramesSinceKeyframe = 0;
}

f32 WorldStateRecorder::OldestTime() const {
    return m_History.Empty() ? 0.0f : m_History.Front().timestamp;
}

f32 WorldStateRecorder::RecordedDuration() const {
    if (m_History.Count() < 2) return 0.0f;
    return m_History.Back().timestamp - m_History.Front().timestamp;
}

usize WorldStateRecorder::MemoryBytes() const {
    usize bytes = m_History.MemoryUsage();
    // The ring's own MemoryUsage only counts the DeltaFrame structs; the
    // snapshots hang off them in heap vectors and are the bulk of the cost.
    for (u32 i = 0; i < m_History.Count(); ++i) {
        bytes += m_History.At(i).snapshots.capacity() * sizeof(EntitySnapshot);
    }
    bytes += m_PrevFrameCache.size() *
             (sizeof(EntitySnapshot) + sizeof(ECS::Entity));
    return bytes;
}

} // namespace Enjin::Gameplay
