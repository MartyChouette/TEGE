#include "Enjin/Editor/DebugRecorder.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

namespace Enjin::Editor {

f32 DebugRecorder::SnapshotInterval() const {
    const f32 rate = m_Settings.snapshotsPerSecond > 0.1f ? m_Settings.snapshotsPerSecond : 30.0f;
    return 1.0f / rate;
}

void DebugRecorder::Configure(const Settings& settings) {
    m_Settings = settings;

    // Switched off means no buffer and no cost. A recorder that allocates its
    // ring and then declines to fill it is still holding the memory the checkbox
    // was unticked to avoid -- at 30 snapshots/s over 30s that is a 900-frame
    // vector of nothing.
    if (!m_Settings.enabled) {
        m_Active = false;
        m_Scrubbing = false;
        m_ScrubOffset = 0.0f;
        m_Recorder.Release();
        return;
    }

    Gameplay::WorldStateRecorder::Config cfg;
    cfg.maxDuration = m_Settings.bufferSeconds;
    cfg.recordInterval = SnapshotInterval();
    // Every channel. This is a diagnostic: the one field you did not record is
    // the one the bug is in, and nothing here has to fit a shipping budget.
    cfg.channels = static_cast<u32>(Gameplay::RewindChannelFlags::All);
    cfg.deltaCompression = true;
    cfg.keyframeInterval = 30;
    m_Recorder.Configure(cfg);
}

void DebugRecorder::Begin(ECS::World* world, Physics::IPhysicsBackend* physics,
                          Physics::IPhysicsBackend2D* physics2d) {
    m_Active = false;
    m_Scrubbing = false;
    m_ScrubOffset = 0.0f;
    m_Recorder.Clear();

    if (!m_Settings.enabled || !world) return;

    m_Recorder.Sampler().SetWorld(world);
    m_Recorder.Sampler().SetPhysics(physics);
    m_Recorder.Sampler().SetPhysics2D(physics2d);
    Configure(m_Settings);   // re-applies the ring size against a cleared buffer
    m_Active = true;

    ENJIN_LOG_INFO(Editor, "Debug Recorder: armed (%.0fs buffer, %.0f snapshots/s)",
                   m_Settings.bufferSeconds, m_Settings.snapshotsPerSecond);
}

void DebugRecorder::End() {
    if (m_Active) {
        ENJIN_LOG_INFO(Editor, "Debug Recorder: stopped (%u frames, %.1f KB)",
                       m_Recorder.FrameCount(),
                       static_cast<f32>(m_Recorder.MemoryBytes()) / 1024.0f);
    }
    m_Active = false;
    m_Scrubbing = false;
    m_ScrubOffset = 0.0f;
    // The buffer itself is kept until the next Begin, so the timeline is still
    // readable in the moment after a stop.
    m_Recorder.Sampler().SetWorld(nullptr);
    m_Recorder.Sampler().SetPhysics(nullptr);
    m_Recorder.Sampler().SetPhysics2D(nullptr);
}

void DebugRecorder::Tick(f32 deltaTime) {
    if (!m_Active || m_Scrubbing) return;
    m_Recorder.Tick(deltaTime);
}

bool DebugRecorder::ScrubTo(f32 secondsBack) {
    if (!m_Active || !HasUsableBuffer()) return false;

    // Clamp to what is actually in the buffer. Dragging past the oldest frame
    // used to restore the oldest frame anyway while the slider carried on, so the
    // scene stopped moving and the control did not -- which reads as a broken
    // scrubber rather than as the end of the recording.
    const f32 maxBack = m_Recorder.LatestTime() - m_Recorder.OldestTime();
    m_ScrubOffset = Math::Clamp(secondsBack, 0.0f, maxBack);
    m_Scrubbing = m_ScrubOffset > 0.0001f;

    return m_Recorder.RestoreAtTime(m_Recorder.LatestTime() - m_ScrubOffset);
}

bool DebugRecorder::StepBack() {
    const f32 before = m_ScrubOffset;
    ScrubTo(m_ScrubOffset + SnapshotInterval());
    return m_ScrubOffset > before + 0.0001f;
}

bool DebugRecorder::StepForward() {
    if (m_ScrubOffset <= 0.0001f) return false;
    const f32 before = m_ScrubOffset;
    ScrubTo(m_ScrubOffset - SnapshotInterval());
    return m_ScrubOffset < before - 0.0001f;
}

void DebugRecorder::ReturnToLiveEdge() {
    if (!m_Scrubbing) { m_ScrubOffset = 0.0f; return; }

    // Resuming from a scrubbed position branches the session: the frames after
    // the scrub point describe a future that no longer happens. Drop them, so the
    // buffer stays a record of what actually occurred rather than two timelines
    // spliced together.
    m_Recorder.TrimAfter(m_Recorder.LatestTime() - m_ScrubOffset);
    m_Recorder.ResetClockToLatest();
    m_Scrubbing = false;
    m_ScrubOffset = 0.0f;
}

} // namespace Enjin::Editor
