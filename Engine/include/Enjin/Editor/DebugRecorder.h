#pragma once

// The Debug Recorder — an editor diagnostic, and nothing else.
//
// It keeps a rolling recording of the whole scene during play so you can pause,
// step back a snapshot at a time, scrub to a moment, and resume from there. It
// is a tool for finding out what happened; it is not a game mechanic and never
// reaches a build.
//
// It used to be a SceneRewindComponent on a hidden entity named
// "__DebugRecorder", created by PlayMode at play start. Three things went wrong
// with that disguise, and all three were unfixable while it lasted:
//
//   * The Game View's gameplay rewind timeline grabs the FIRST
//     SceneRewindComponent in the world. The hidden one usually came first, so
//     the timeline a designer used to tune their rewind mechanic was reporting
//     the debug buffer instead.
//   * The recorder is an entity, so it could be SAVED into the scene. PlayMode
//     carries a stray-recorder sweep on stop specifically to heal scenes that
//     had collected them.
//   * A debug tool and a shipped mechanic shared one inspector, one set of
//     fields and one system, so neither could be described accurately. The
//     component has a rewindKey, a cooldown and charges; the recorder set them
//     to -1, 0 and 0 and hoped nobody looked.
//
// What it DOES share with the gameplay mechanic is the snapshot machinery, and
// that is now explicit: both own a Gameplay::WorldStateRecorder.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Gameplay/StateRecorder.h"

namespace Enjin {
namespace ECS { class World; }
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }

namespace Editor {

class ENJIN_API DebugRecorder {
public:
    struct Settings {
        bool enabled = true;
        f32 bufferSeconds = 30.0f;
        // Finer than the gameplay default (15/s) on purpose: this is for stepping
        // through a bug, where a coarse step skips the frame you are looking for.
        f32 snapshotsPerSecond = 30.0f;
    };

    void Configure(const Settings& settings);
    const Settings& GetSettings() const { return m_Settings; }

    // Called by PlayMode at play start and stop. Begin clears any previous
    // session: a recording of the last play is worse than no recording, because
    // its timeline looks live.
    void Begin(ECS::World* world, Physics::IPhysicsBackend* physics,
               Physics::IPhysicsBackend2D* physics2d);
    void End();

    bool IsActive() const { return m_Active; }

    // Record a frame if one is due. Does nothing while scrubbing -- recording
    // over a scrub would append frames describing a past the session did not
    // actually have, and the timeline would grow while you dragged it.
    void Tick(f32 deltaTime);

    // ---- scrubbing ---------------------------------------------------------
    //
    // The offset is SECONDS BEFORE the pause point, so 0 is the live edge and
    // larger numbers are further back. That matches the slider, which reads
    // "-2.50s" rather than an absolute time nobody has a reference for.

    f32 GetScrubOffset() const { return m_ScrubOffset; }
    bool IsScrubbing() const { return m_Scrubbing; }

    // Seek the world to `secondsBack`. Clamped to what is actually recorded, so
    // dragging past the end of the buffer stops at the oldest frame rather than
    // silently restoring nothing.
    bool ScrubTo(f32 secondsBack);

    // One snapshot in either direction. Returns false when already at the end.
    bool StepBack();
    bool StepForward();

    // Return to the live edge. PlayMode calls this on resume: a session that
    // carried on from a scrubbed position would be recording against a clock
    // that had gone backwards.
    void ReturnToLiveEdge();

    // ---- queries -----------------------------------------------------------

    f32 RecordedDuration() const { return m_Recorder.RecordedDuration(); }
    f32 LatestTime() const { return m_Recorder.LatestTime(); }
    u32 FrameCount() const { return m_Recorder.FrameCount(); }
    usize MemoryBytes() const { return m_Recorder.MemoryBytes(); }
    f32 SnapshotInterval() const;

    // True when there is enough recorded to be worth offering a scrubber.
    bool HasUsableBuffer() const { return RecordedDuration() > 0.05f; }

private:
    Settings m_Settings;
    Gameplay::WorldStateRecorder m_Recorder;
    bool m_Active = false;
    bool m_Scrubbing = false;
    f32 m_ScrubOffset = 0.0f;
};

} // namespace Editor
} // namespace Enjin
