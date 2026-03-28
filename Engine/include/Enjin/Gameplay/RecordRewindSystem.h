#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"

namespace Enjin::Gameplay {

// RecordRewindSystem — processes RecordRewindComponent (per-entity, Braid-style)
// AND SceneRewindComponent (whole-scene, Sands of Time-style) each frame.
class ENJIN_API RecordRewindSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void Update(f32 deltaTime);

    // Check if any entity or scene rewind is active (for pausing other systems)
    bool IsAnyRewinding() const { return m_AnyRewinding; }
    bool IsSceneRewinding() const { return m_SceneRewinding; }

private:
    void UpdateEntityRewind(f32 deltaTime);
    void UpdateSceneRewind(f32 deltaTime);

    ECS::World* m_World = nullptr;
    bool m_AnyRewinding = false;
    bool m_SceneRewinding = false;
};

} // namespace Enjin::Gameplay
