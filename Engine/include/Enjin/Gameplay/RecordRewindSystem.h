#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Gameplay/RewindChannel.h"

namespace Enjin {
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }
namespace Gameplay {

// RecordRewindSystem — processes RecordRewindComponent (per-entity, Braid-style)
// AND SceneRewindComponent (whole-scene, Sands of Time-style) each frame.
// Supports delta compression, channel-based capture, and programmatic seek.
class ENJIN_API RecordRewindSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void Update(f32 deltaTime);

    // Check if any entity or scene rewind is active (for pausing other systems)
    bool IsAnyRewinding() const { return m_AnyRewinding; }
    bool IsSceneRewinding() const { return m_SceneRewinding; }

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
    void SetPhysics(Physics::IPhysicsBackend* physics) { m_Physics = physics; }
    void SetPhysics2D(Physics::IPhysicsBackend2D* physics2d) { m_Physics2D = physics2d; }

private:
    void UpdateEntityRewind(f32 deltaTime);
    void UpdateSceneRewind(f32 deltaTime);

    void CaptureEntitySnapshot(ECS::Entity entity, EntitySnapshot& out, u32 channelMask);
    void RestoreEntitySnapshot(ECS::Entity entity, const EntitySnapshot& snap);
    void RestoreEntitySnapshotInterpolated(ECS::Entity entity,
                                            const EntitySnapshot& a, const EntitySnapshot& b, f32 t);

    // Delta compression helpers
    bool HasEntityChanged(const EntitySnapshot& current, const EntitySnapshot& prev, u32 channelMask) const;

    ECS::World* m_World = nullptr;
    Physics::IPhysicsBackend* m_Physics = nullptr;
    Physics::IPhysicsBackend2D* m_Physics2D = nullptr;
    bool m_AnyRewinding = false;
    bool m_SceneRewinding = false;
};

} // namespace Gameplay
} // namespace Enjin
