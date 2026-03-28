#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/ECS/World.h"

namespace Enjin::Gameplay {

// RecordRewindSystem — processes RecordRewindComponent entities each frame.
// During normal gameplay: records transform + velocity + health snapshots.
// During rewind: interpolates entity back through recorded frames.
class ENJIN_API RecordRewindSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void Update(f32 deltaTime);

    // Check if any entity is currently rewinding (for pausing other systems)
    bool IsAnyRewinding() const { return m_AnyRewinding; }

private:
    ECS::World* m_World = nullptr;
    bool m_AnyRewinding = false;
};

} // namespace Enjin::Gameplay
