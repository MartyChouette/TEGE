#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Math/Vector.h"

namespace Enjin {

namespace Renderer { class Camera; }

namespace Gameplay {

/**
 * @brief Point the game camera at something. The one way to do it.
 *
 * Writing Renderer::Camera on its own is not enough and never was. Every
 * runtime rebuilds the render camera from the active CameraComponent entity's
 * transform after gameplay has ticked -- Player/src/main.cpp, web_main.cpp and
 * EditorLayer all do it -- so a system that sets only the Renderer::Camera has
 * its work thrown away in the same frame, silently and with no error.
 *
 * CameraDirector learned this and mirrored onto the active camera entity.
 * CinematicSystem did not: it wrote the Renderer::Camera and then the CINEMATIC
 * entity's own transform, which nothing renders from. So authored cutscenes ran
 * their full timeline, fired their events, and the view never moved.
 *
 * Both now call this.
 *
 * @param world       Scene world. Required.
 * @param gameCamera  Render camera. May be null (headless, or a tick with no
 *                    camera yet); the entity mirror still happens.
 * @param position    Where the camera sits, world space.
 * @param lookPoint   What it looks at, world space.
 * @param fov         Vertical field of view, degrees.
 */
ENJIN_API void ApplyCameraPose(ECS::World* world, Renderer::Camera* gameCamera,
                               const Math::Vector3& position,
                               const Math::Vector3& lookPoint,
                               f32 fov);

} // namespace Gameplay
} // namespace Enjin
