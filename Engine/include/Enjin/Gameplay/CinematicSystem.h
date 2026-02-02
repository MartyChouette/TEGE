#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Renderer/Camera.h"

namespace Enjin {
namespace Gameplay {

class ENJIN_API CinematicSystem {
public:
    void Update(ECS::World* world, Renderer::Camera* gameCamera, f32 deltaTime);
    void Play(ECS::World* world, ECS::Entity cinematicEntity);
    void Stop(ECS::World* world, ECS::Entity cinematicEntity);
    bool IsAnyCinematicPlaying(ECS::World* world) const;
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

private:
    f32 ApplyEasing(f32 t, ECS::CinematicCameraComponent::Waypoint::Easing easing);
    bool m_Enabled = false;
};

} // namespace Gameplay
} // namespace Enjin
