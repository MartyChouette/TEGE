#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Gameplay/RewindChannel.h"
#include "Enjin/Gameplay/StateRecorder.h"
#include "Enjin/Gameplay/RewindFeedback.h"

namespace Enjin {
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }
namespace Gameplay {

// RecordRewindSystem — the GAMEPLAY Rewind Ability, and only that.
//
// Processes RecordRewindComponent (per-entity, Braid-style) and
// SceneRewindComponent (whole-scene, Sands of Time-style): both are designed
// mechanics a person authors in the inspector, with a key, a cooldown, charges
// and a cost budget, and both ship inside the game.
//
// It is NOT the editor's Debug Recorder. That used to be a SceneRewindComponent
// on a hidden entity this system happened to also tick, which is why the Game
// View's rewind timeline read the debug buffer instead of the designer's. The
// Debug Recorder now owns a WorldStateRecorder of its own (Editor/DebugRecorder.h)
// and this system never sees it.
//
// The snapshot machinery both use lives in Gameplay/StateRecorder.h.
class ENJIN_API RecordRewindSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; m_Sampler.SetWorld(world); }
    void Update(f32 deltaTime);

    // Whether a gameplay rewind is running this frame.
    //
    // This reports; it does not pause anything. The comment here used to say
    // "for pausing other systems" and nothing anywhere paused -- the only reader
    // was the script binding. A game that wants the world to hold still during a
    // rewind reads this and does that itself, which is the honest arrangement:
    // what counts as "the world" is a game's decision, not the engine's.
    bool IsAnyRewinding() const { return m_AnyRewinding; }
    bool IsSceneRewinding() const { return m_SceneRewinding; }

    // What the rewind that is running wants the screen to look like, taken from
    // the component that is actually rewinding. RewindFeedbackApplier turns this
    // into post-process settings; until it existed, rewindTint and
    // rewindVignetteStrength were authored, serialized, documented as a screen
    // tint, and read by nothing but an editor progress bar.
    //
    // Inactive returns strengths of zero and a white tint -- an absence, not a
    // plausible default that would wash the screen when nothing is rewinding.
    const RewindFeedback& GetActiveFeedback() const { return m_Feedback; }

    // Programmatic rewind control (for runtime API / scripting)
    void StartEntityRewind(ECS::Entity entity);
    void StopEntityRewind(ECS::Entity entity);
    void StartSceneRewind();
    void StopSceneRewind();

    // Seek to specific time offset from latest (for editor scrubber)
    void SeekSceneToTime(f32 timeOffset);
    void SeekEntityToTime(ECS::Entity entity, f32 timeOffset);

    // Query recorded state
    f32 GetSceneRecordedDuration() const;
    f32 GetSceneCurrentTime() const;
    u32 GetSceneFrameCount() const;
    usize GetMemoryUsageBytes() const;

    // Physics backend pointers (for state restoration on rewind)
    void SetPhysics(Physics::IPhysicsBackend* physics) { m_Sampler.SetPhysics(physics); }
    void SetPhysics2D(Physics::IPhysicsBackend2D* physics2d) { m_Sampler.SetPhysics2D(physics2d); }

private:
    void UpdateEntityRewind(f32 deltaTime);
    void UpdateSceneRewind(f32 deltaTime);

    // Snapshot capture and restore are shared with the Debug Recorder and live in
    // EntityStateSampler. They were private members here and needed nothing from
    // the rewind components, which is both what made them extractable and what
    // let the Debug Recorder pass itself off as a gameplay component for so long.
    EntityStateSampler m_Sampler;

    ECS::World* m_World = nullptr;
    bool m_AnyRewinding = false;
    bool m_SceneRewinding = false;
    RewindFeedback m_Feedback;
};

} // namespace Gameplay
} // namespace Enjin
