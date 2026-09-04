#include "Enjin/Gameplay/CameraPose.h"

#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"

#include "Enjin/Renderer/Camera.h"

namespace Enjin {
namespace Gameplay {

void ApplyCameraPose(ECS::World* world, Renderer::Camera* gameCamera,
                     const Math::Vector3& position,
                     const Math::Vector3& lookPoint,
                     f32 fov) {
    if (!world) return;

    const Math::Vector3 dir = lookPoint - position;
    const bool haveDir = dir.Length() > 1e-5f;

    // The render camera.
    if (gameCamera) {
        gameCamera->SetPosition(position);
        if (haveDir) {
            gameCamera->SetLookAt(position, lookPoint, Math::Vector3(0, 1, 0));
        }
        gameCamera->SetPerspective(fov, 16.0f / 9.0f, 0.1f, 1000.0f);
    }

    // The active camera ENTITY. This is the half that actually reaches the
    // screen: every runtime rebuilds the render camera from this transform
    // after gameplay ticks, so without this write the pose above is discarded
    // in the same frame.
    //
    // A camera looks down its local -Z, and LookRotation puts its `forward`
    // argument on local +Z, so the direction is negated.
    const ECS::Entity camEnt = ECS::CameraManager::GetActiveCamera(world);
    if (camEnt == ECS::INVALID_ENTITY) return;

    if (auto* ct = world->GetComponent<ECS::TransformComponent>(camEnt)) {
        ct->position = position;
        if (haveDir) {
            const Math::Vector3 fwd = dir.Normalized();
            ct->rotation = Math::Quaternion::LookRotation(fwd * -1.0f, Math::Vector3(0, 1, 0));
        }
    }
    if (auto* cc = world->GetComponent<ECS::CameraComponent>(camEnt)) {
        cc->fieldOfView = fov;
    }
}

} // namespace Gameplay
} // namespace Enjin
