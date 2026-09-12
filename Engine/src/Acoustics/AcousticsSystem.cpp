#include "Enjin/Acoustics/AcousticsSystem.h"

#include "Enjin/ECS/World.h"

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
    m_MeasuredAt = listener;
    m_HasTraced = true;
    m_SinceTrace = 0.0f;
    ++m_Traces;
}

} // namespace Acoustics
} // namespace Enjin
