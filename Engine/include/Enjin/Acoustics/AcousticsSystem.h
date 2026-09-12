#pragma once

// Keeping a measurement of the room the listener is standing in.
//
// The tracing is not cheap -- a few thousand rays with several bounces each --
// and it does not need to be frequent. A room does not change while you walk
// across it, and the only things that invalidate a measurement are moving
// somewhere meaningfully different or the geometry itself changing.
//
// So this is mostly a set of decisions about WHEN to spend that cost, and those
// decisions are the part worth testing: a system that retraced every frame
// would be correct and unusable, and one that never retraced would be cheap and
// wrong. Both look identical from the outside until you profile or walk into
// another room.
//
// The measurement replaces the authored ReverbZone values rather than sitting
// beside them. Where there is no geometry to measure -- an empty scene, a level
// still loading -- the authored zone still answers, so every existing scene
// keeps working and a person can still overrule the simulation.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Audio/AcousticScene.h"
#include "Enjin/Acoustics/AcousticBVH.h"
#include "Enjin/Acoustics/RoomResponse.h"

namespace Enjin {
namespace ECS { class World; }

namespace Acoustics {

struct ENJIN_API AcousticsSettings {
    // How far the listener has to move before the room is worth measuring
    // again. Small enough that walking through a doorway retraces; large
    // enough that pacing does not.
    f32 retraceDistance = 3.0f;

    // And a ceiling on how stale a measurement may get, for the cases distance
    // cannot see -- a door opening, a wall being carved away.
    f32 retraceInterval = 4.0f;

    // Geometry is rebuilt no more than this often. Collecting every collider in
    // a level and building a BVH is the expensive half, and something that
    // marks it dirty every frame (a physics object settling, say) must not be
    // able to turn that into a per-frame cost.
    f32 rebuildInterval = 1.0f;

    RoomTraceSettings trace;
};

class ENJIN_API AcousticsSystem {
public:
    void SetWorld(ECS::World* world);
    void SetSettings(const AcousticsSettings& settings) { m_Settings = settings; }
    const AcousticsSettings& Settings() const { return m_Settings; }

    // The geometry changed. Cheap to call often -- the rebuild it schedules is
    // rate limited.
    void MarkGeometryDirty() { m_GeometryDirty = true; }

    // Called once per frame with where the listener is. Does at most one
    // expensive thing per call: either a rebuild or a trace, never both, so a
    // frame that has to do the work is never the frame that has to do all of it.
    void Update(const Math::Vector3& listener, f32 deltaTime);

    // The most recent measurement, and whether there is one at all. No
    // measurement is an honest answer: an empty scene has no room in it.
    const RoomResponse& Measurement() const { return m_Measurement; }
    bool HasMeasurement() const { return m_Measurement.Valid(); }

    // Where the measurement was taken, which is what tells a caller whether it
    // still describes where the listener is now.
    Math::Vector3 MeasuredAt() const { return m_MeasuredAt; }

    const AcousticBVH& BVH() const { return m_BVH; }
    const Audio::AcousticScene& Scene() const { return m_Scene; }

    // Diagnostics, so a person can see what the system is spending and why a
    // room sounds the way it does.
    u32 RebuildCount() const { return m_Rebuilds; }
    u32 TraceCount() const { return m_Traces; }

    void Clear();

private:
    bool ShouldRetrace(const Math::Vector3& listener) const;

    ECS::World* m_World = nullptr;
    AcousticsSettings m_Settings;

    Audio::AcousticScene m_Scene;
    AcousticBVH m_BVH;

    RoomResponse m_Measurement;
    Math::Vector3 m_MeasuredAt = Math::Vector3(0.0f, 0.0f, 0.0f);
    bool m_HasTraced = false;

    bool m_GeometryDirty = true;
    f32 m_SinceRebuild = 0.0f;
    f32 m_SinceTrace = 0.0f;

    u32 m_Rebuilds = 0;
    u32 m_Traces = 0;
};

} // namespace Acoustics
} // namespace Enjin
