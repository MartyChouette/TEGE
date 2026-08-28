#pragma once
// SimulationClock — fixed-timestep accumulator with render interpolation
// (ADR-0005). Owns WHEN physics steps; the runtimes own WHAT steps (they pass
// a callback that updates their physics backends).
//
// Disabled (legacy) mode: Tick() calls stepFn(frameDt) once and touches
// nothing else — byte-identical to the old variable-step path.
//
// Enabled: an accumulator drains in fixed increments (default 1/60s), capped
// at kMaxStepsPerFrame per frame (overload = slow motion, never a spiral).
// Dynamic 3D rigidbody transforms are interpolated between the last two tick
// poses for rendering; the raw tick pose is restored before the next step so
// the simulation never integrates an interpolated value. If something else
// (script, teleport, editor gizmo) moves a body's transform between frames,
// the stored pose is dropped and the new value is accepted as truth.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <functional>
#include <unordered_map>

namespace Enjin {
namespace ECS { class World; using Entity = u64; }
namespace Gameplay {

class ENJIN_API SimulationClock {
public:
    static constexpr u32 kMaxStepsPerFrame = 4;

    // enabled = the project's fixedTimestep setting. ticksPerSecond is clamped
    // to [15, 240]; 60 is the default and the recommended value.
    void Configure(bool enabled, f32 ticksPerSecond);

    // Drop all accumulated time and stored poses (scene load, play start).
    void Reset();

    // Advance one rendered frame. frameDt should already include the global
    // time scale (the runtimes multiply it at the top of their update).
    // stepFn(fixedDt) is invoked 0..kMaxStepsPerFrame times when enabled, or
    // exactly once with frameDt when disabled.
    void Tick(ECS::World* world, f32 frameDt, const std::function<void(f32)>& stepFn);

    bool IsEnabled() const { return m_Enabled; }
    f32  GetFixedDeltaTime() const { return m_FixedDt; }
    // 0..1 fraction of a tick left in the accumulator after the last Tick —
    // the interpolation factor the last pose write used.
    f32  GetAlpha() const { return m_Alpha; }
    u32  GetLastStepCount() const { return m_LastSteps; }

private:
    struct BodyPose {
        Math::Vector3    prevPos,  simPos,  writtenPos;
        Math::Quaternion prevRot,  simRot,  writtenRot;
    };

    void RestoreSimPoses(ECS::World* world);
    void SnapshotPrevPoses(ECS::World* world);
    void CaptureSimPoses(ECS::World* world);
    void WriteInterpolatedPoses(ECS::World* world);

    bool m_Enabled = false;
    f32  m_FixedDt = 1.0f / 60.0f;
    f32  m_Accumulator = 0.0f;
    f32  m_Alpha = 0.0f;
    u32  m_LastSteps = 0;
    std::unordered_map<u64, BodyPose> m_Poses;
};

} // namespace Gameplay
} // namespace Enjin
