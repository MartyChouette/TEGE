#include "Enjin/Acoustics/AcousticsSystem.h"

#include "Enjin/ECS/World.h"
#include "Enjin/Logging/Log.h"

#include <cmath>

namespace Enjin {
namespace Acoustics {

void AcousticsSystem::SetWorld(ECS::World* world) {
    if (m_World == world) return;
    m_World = world;
    Clear();
}

void AcousticsSystem::Clear() {
    m_Scene = Audio::AcousticScene{};
    m_BVH.Clear();
    m_Measurement = RoomResponse{};
    m_Reflections = EarlyReflectionResult{};
    m_HasTraced = false;
    m_GeometryDirty = true;
    m_SinceRebuild = 0.0f;
    m_SinceTrace = 0.0f;
}

bool AcousticsSystem::ShouldRetrace(const Math::Vector3& listener) const {
    if (!m_HasTraced) return true;

    const f32 dx = listener.x - m_MeasuredAt.x;
    const f32 dy = listener.y - m_MeasuredAt.y;
    const f32 dz = listener.z - m_MeasuredAt.z;
    const f32 moved = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (moved >= m_Settings.retraceDistance) return true;

    // Distance cannot see a door opening or a wall being carved away, so a
    // measurement also ages out.
    return m_SinceTrace >= m_Settings.retraceInterval;
}

void AcousticsSystem::Update(const Math::Vector3& listener, f32 deltaTime) {
    if (!m_World) return;

    m_SinceRebuild += deltaTime;
    m_SinceTrace += deltaTime;

    // Rebuild OR trace, never both in one call.
    //
    // Collecting every collider in a level and then tracing thousands of rays
    // through the result is two expensive things, and doing them on the same
    // frame makes one visible hitch out of two invisible ones.
    if (m_GeometryDirty && m_SinceRebuild >= m_Settings.rebuildInterval) {
        m_Scene = Audio::BuildAcousticScene(m_World);
        m_BVH.Build(m_Scene);
        m_GeometryDirty = false;
        m_SinceRebuild = 0.0f;
        ++m_Rebuilds;

        // The room has changed shape, so whatever was measured describes
        // somewhere that no longer exists.
        m_HasTraced = false;
        return;
    }

    if (!m_BVH.IsBuilt()) {
        // No geometry to measure. Not an error -- an empty scene has no room in
        // it, and the caller falls back to whatever a person authored.
        m_Measurement = RoomResponse{};
        return;
    }

    if (!ShouldRetrace(listener)) return;

    m_Measurement = TraceRoomResponse(m_BVH, m_Scene, listener, m_Settings.trace);

    // The discrete reflections, traced at the same moment and from the same
    // place. They answer the half of "what does this room sound like" that a
    // decay time cannot: the tail says how live the room is, the taps say where
    // its walls are and what they are made of. Tracing them together means they
    // can never describe two different positions.
    m_Reflections = TraceEarlyReflections(m_BVH, m_Scene, listener, listener);

    m_MeasuredAt = listener;
    m_HasTraced = true;
    m_SinceTrace = 0.0f;
    ++m_Traces;

    // Say what was measured.
    //
    // Not noise: a trace happens every few seconds at most, and this is the one
    // line that tells a person whether the room they are standing in is being
    // heard as the room they are standing in. Without it, "the reverb sounds
    // wrong" and "the reverb is not running at all" are the same observation.
    if (m_Measurement.Valid()) {
        ENJIN_LOG_INFO(Audio,
                       "Acoustics: RT60 %.2f / %.2f / %.2f s, mean free path %.1f m, "
                       "reflected %.2f (%u tris)",
                       m_Measurement.rt60[0], m_Measurement.rt60[1], m_Measurement.rt60[2],
                       m_Measurement.meanFreePath, m_Measurement.reflectedEnergy,
                       static_cast<u32>(m_Scene.TriangleCount()));
        ENJIN_LOG_INFO(Audio, "Acoustics: %zu early reflections, first at %.0f ms",
                       m_Reflections.taps.size(),
                       m_Measurement.firstReflection * 1000.0f);
    } else if (m_Measurement.raysTraced > 0) {
        // A measurement that failed is worth more than silence: it says which
        // way it failed, and both ways are things a person can act on.
        ENJIN_LOG_INFO(Audio,
                       "Acoustics: could not measure this space (%u of %u rays escaped, "
                       "%u ran out of bounces) -- authored reverb still applies",
                       m_Measurement.raysEscaped, m_Measurement.raysTraced,
                       m_Measurement.raysTruncated);
    }
}

EarlyReflectionResult AcousticsSystem::TraceSource(const Math::Vector3& source,
                                                   const Math::Vector3& listener) const {
    if (!m_BVH.IsBuilt()) return EarlyReflectionResult{};
    return TraceEarlyReflections(m_BVH, m_Scene, source, listener);
}

} // namespace Acoustics
} // namespace Enjin
