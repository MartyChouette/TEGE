#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"

namespace Enjin {
namespace Gameplay {

class ENJIN_API FootstepSystem {
public:
    void Update(ECS::World* world, f32 deltaTime);
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    bool m_Enabled = false;
};

} // namespace Gameplay
} // namespace Enjin
