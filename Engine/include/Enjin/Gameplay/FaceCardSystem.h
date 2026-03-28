#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"

namespace Enjin {
namespace ECS { class RenderSystem; }

namespace Gameplay {

class ENJIN_API FaceCardSystem {
public:
    void SetWorld(ECS::World* world) { m_World = world; }
    void SetRenderSystem(ECS::RenderSystem* rs) { m_RenderSystem = rs; }
    void Update(f32 deltaTime);

private:
    ECS::World* m_World = nullptr;
    ECS::RenderSystem* m_RenderSystem = nullptr;
};

} // namespace Gameplay
} // namespace Enjin
