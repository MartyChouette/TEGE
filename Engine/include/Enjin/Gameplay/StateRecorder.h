#pragma once

// The part three features genuinely share.
//
// Rewind exists in this engine three times over, for three different jobs, and
// until now they were one implementation wearing three coats:
//
//   Rewind Ability   a designed gameplay mechanic. Authored per entity or per
//                    scene, serialized, keybound, tuned for cost, and it ships.
//   Debug Recorder   an editor diagnostic. Always on during play, every channel,
//                    no key, no cooldown, no cost ceiling, never ships. It used
//                    to BE a SceneRewindComponent on a hidden "__DebugRecorder"
//                    entity, which is why the Game View's rewind timeline read
//                    the debug buffer instead of the designer's -- it grabbed the
//                    first SceneRewindComponent in the world and the hidden one
//                    usually came first.
//   Replay           a .tegereplay input stream. Different mechanism entirely
//                    (re-injection, not state restore) and correctly separate
//                    already.
//
// What the first two share is exactly this file: how a snapshot is taken, how it
// is put back, and how a rolling window of them is kept. What they do NOT share
// is who turns them on, what they cost, or what UI they wear -- so those live
// with each feature instead of in one component with a mode flag.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Gameplay/RewindChannel.h"
#include "Enjin/Gameplay/StateRingBuffer.h"
#include "Enjin/ECS/Entity.h"
#include <unordered_map>

namespace Enjin {
namespace ECS { class World; }
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }

namespace Gameplay {

// ============================================================================
// EntityStateSampler -- one entity, one snapshot, both directions
// ============================================================================

// Reads an entity's state into an EntitySnapshot and writes it back. Every
// channel the engine can record lives here and nowhere else, so adding a channel
// is one edit rather than one per feature.
//
// Holds borrowed pointers only; it owns no storage and keeps no clock.
class ENJIN_API EntityStateSampler {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetPhysics(Physics::IPhysicsBackend* physics) { m_Physics = physics; }
    void SetPhysics2D(Physics::IPhysicsBackend2D* physics2d) { m_Physics2D = physics2d; }

    ECS::World* GetWorld() const { return m_World; }

    // Read the enabled channels of `entity` into `out`. Channels the entity has
    // no component for are left at their defaults rather than guessed at.
    void Capture(ECS::Entity entity, EntitySnapshot& out, u32 channelMask) const;

    // Put a snapshot back. Honours snap.channelMask, not the caller's -- a frame
    // recorded with fewer channels must not have the missing ones written as
    // defaults over live state.
    void Restore(ECS::Entity entity, const EntitySnapshot& snap) const;

    // Restore a position BETWEEN two recorded frames. Used by playback that runs
    // at a different rate from the recording, which is every playback.
    void RestoreInterpolated(ECS::Entity entity, const EntitySnapshot& a,
                             const EntitySnapshot& b, f32 t) const;

    // Whether a snapshot differs enough from the previous one to be worth
    // storing. This is the whole of delta compression's decision.
    bool Differs(const EntitySnapshot& current, const EntitySnapshot& prev,
                 u32 channelMask) const;

private:
    ECS::World* m_World = nullptr;
    Physics::IPhysicsBackend* m_Physics = nullptr;
    Physics::IPhysicsBackend2D* m_Physics2D = nullptr;
};

// ============================================================================
// WorldStateRecorder -- a rolling window of whole-world snapshots
// ============================================================================

// Keeps the last N seconds of the world in a ring buffer, with keyframes and
// delta frames between them. Owned by whoever needs one: the scene Rewind
// Ability owns one, and so does the editor's Debug Recorder.
//
// It records and it seeks. It knows nothing about keys, cooldowns, charges,
// tints or menus -- those belong to the feature, which is the distinction that
// was missing.
class ENJIN_API WorldStateRecorder {
public:
    struct Config {
        f32 maxDuration = 10.0f;            // seconds of window
        f32 recordInterval = 1.0f / 15.0f;  // seconds between snapshots
        u32 channels = static_cast<u32>(RewindChannelFlags::Default);
        bool deltaCompression = true;
        u32 keyframeInterval = 30;          // full frame every N deltas

        bool operator==(const Config& o) const {
            return maxDuration == o.maxDuration && recordInterval == o.recordInterval &&
                   channels == o.channels && deltaCompression == o.deltaCompression &&
                   keyframeInterval == o.keyframeInterval;
        }
        bool operator!=(const Config& o) const { return !(*this == o); }
    };

    // Re-applying the same config is free; a changed recordInterval or
    // maxDuration resizes the ring without dropping what is already in it.
    void Configure(const Config& config);
    const Config& GetConfig() const { return m_Config; }

    EntityStateSampler& Sampler() { return m_Sampler; }
    const EntityStateSampler& Sampler() const { return m_Sampler; }

    // Advance the record clock and capture a frame if an interval has elapsed.
    // `exclude` skips one entity, for a recorder that lives on an entity of its
    // own and should not record itself.
    //
    // Returns true if a frame was captured this call.
    bool Tick(f32 deltaTime, ECS::Entity exclude = ECS::INVALID_ENTITY);

    // Put the world back to `absoluteTime` on this recorder's clock, without
    // consuming anything. Seeking is non-destructive so a scrubber can go both
    // ways; a consuming rewind calls TrimAfter separately.
    //
    // Returns false when there is nothing recorded to seek to.
    bool RestoreAtTime(f32 absoluteTime) const;

    // Drop every frame newer than `absoluteTime`. This is what makes a gameplay
    // rewind consume its buffer, and what a scrubber calls once when a resume
    // branches the session away from the frames it had already recorded.
    //
    // Clears the delta cache, which described the dropped frames. Leaving it
    // would let the next delta frame decide an entity "has not changed" against a
    // value that is no longer anywhere in the buffer, and that entity would then
    // be missing from every frame until the next keyframe.
    void TrimAfter(f32 absoluteTime);

    // Move the record clock back to the newest surviving frame.
    //
    // Deliberately separate from TrimAfter, because the gameplay rewind needs the
    // opposite: it holds its clock FIXED as an anchor for the whole of a rewind
    // hold and only re-seats it on release. An offset measured against something
    // the pop loop moves compounds, which is what once made hold-to-rewind
    // accelerate instead of playing at its configured speed.
    void ResetClockToLatest();

    // Reset the clock and drop the frames, keeping the ring allocated for the
    // next session.
    void Clear();

    // Drop the frames AND give the memory back. For a recorder being switched
    // off, where keeping the allocation is the cost the switch exists to avoid.
    void Release();

    // ---- queries -----------------------------------------------------------

    bool Empty() const { return m_History.Empty(); }
    u32 FrameCount() const { return m_History.Count(); }
    f32 LatestTime() const { return m_RecordedTime; }
    f32 OldestTime() const;
    f32 RecordedDuration() const;
    usize MemoryBytes() const;

    const StateRingBuffer<DeltaFrame>& History() const { return m_History; }

private:
    // Rebuild full state at a frame index by walking back to its keyframe.
    void RestoreFrameAndAncestors(i32 index) const;

    Config m_Config;
    EntityStateSampler m_Sampler;

    StateRingBuffer<DeltaFrame> m_History;
    std::unordered_map<ECS::Entity, EntitySnapshot> m_PrevFrameCache;
    f32 m_RecordTimer = 0.0f;
    f32 m_RecordedTime = 0.0f;
    u32 m_FramesSinceKeyframe = 0;
};

} // namespace Gameplay
} // namespace Enjin
