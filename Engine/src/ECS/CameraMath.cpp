#include "Enjin/ECS/CameraMath.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Renderer/Camera.h"

#include <cmath>

namespace Enjin {
namespace ECS {

namespace {

// Guards a divide by the perspective w and by a plane-parallel ray.
constexpr f32 kEpsilon = 1e-6f;

bool ViewProjectionFor(World* world, Entity cameraEntity, f32 aspect, Math::Matrix4& outVP) {
    Renderer::Camera cam;
    if (!BuildCameraFromEntity(world, cameraEntity, aspect, cam)) return false;
    outVP = cam.GetProjectionMatrix() * cam.GetViewMatrix();
    return true;
}

} // namespace

bool BuildCameraFromEntity(World* world, Entity cameraEntity, f32 aspect,
                           Renderer::Camera& outCamera) {
    if (!world || aspect <= 0.0f) return false;
    auto* cc = world->GetComponent<CameraComponent>(cameraEntity);
    auto* ct = world->GetComponent<TransformComponent>(cameraEntity);
    if (!cc || !ct) return false;

    if (cc->projectionType == ProjectionType::Perspective) {
        outCamera.SetPerspective(cc->fieldOfView, aspect, cc->nearPlane, cc->farPlane);
    } else {
        // orthoSize is the HALF-height, so the half-width follows the aspect.
        // A script that copies this by hand is the duplication this exists to
        // remove.
        const f32 halfH = cc->orthoSize;
        const f32 halfW = halfH * aspect;
        outCamera.SetOrthographic(-halfW, halfW, -halfH, halfH, cc->nearPlane, cc->farPlane);
    }

    const Math::Vector3 fwd = ct->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
    const Math::Vector3 up = ct->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
    outCamera.SetPosition(ct->position);
    outCamera.SetLookAt(ct->position, ct->position + fwd, up);
    return true;
}

bool ScreenToRayVP(const Math::Matrix4& viewProjection, const Math::Vector2& screen,
                   f32 viewportWidth, f32 viewportHeight,
                   Math::Vector3& outOrigin, Math::Vector3& outDirection) {
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return false;

    // Screen pixels (top-left origin) to NDC. Y is NOT flipped: in Vulkan clip
    // space Y already runs downward, matching screen Y, which is the same
    // convention the editor's picker uses.
    const f32 ndcX = (2.0f * screen.x) / viewportWidth - 1.0f;
    const f32 ndcY = (2.0f * screen.y) / viewportHeight - 1.0f;

    const Math::Matrix4 invVP = viewProjection.Inverse();
    Math::Vector4 nearH = invVP * Math::Vector4(ndcX, ndcY, 0.0f, 1.0f);
    Math::Vector4 farH  = invVP * Math::Vector4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::fabs(nearH.w) < kEpsilon || std::fabs(farH.w) < kEpsilon) return false;

    const Math::Vector3 nearP(nearH.x / nearH.w, nearH.y / nearH.w, nearH.z / nearH.w);
    const Math::Vector3 farP(farH.x / farH.w, farH.y / farH.w, farH.z / farH.w);

    Math::Vector3 dir = farP - nearP;
    const f32 len = dir.Length();
    if (len < kEpsilon) return false;

    outOrigin = nearP;
    outDirection = dir * (1.0f / len);
    return true;
}

bool ScreenToRay(World* world, Entity cameraEntity, const Math::Vector2& screen,
                 f32 viewportWidth, f32 viewportHeight,
                 Math::Vector3& outOrigin, Math::Vector3& outDirection) {
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return false;
    Math::Matrix4 vp;
    if (!ViewProjectionFor(world, cameraEntity, viewportWidth / viewportHeight, vp)) return false;
    return ScreenToRayVP(vp, screen, viewportWidth, viewportHeight, outOrigin, outDirection);
}

bool ScreenToWorldOnPlane(World* world, Entity cameraEntity, const Math::Vector2& screen,
                          f32 viewportWidth, f32 viewportHeight,
                          f32 planeZ, Math::Vector3& outWorld) {
    Math::Vector3 origin, dir;
    if (!ScreenToRay(world, cameraEntity, screen, viewportWidth, viewportHeight, origin, dir)) {
        return false;
    }
    // Parallel to the plane: there is no crossing, and reporting one would put
    // a click somewhere arbitrary.
    if (std::fabs(dir.z) < kEpsilon) return false;

    const f32 t = (planeZ - origin.z) / dir.z;
    outWorld = origin + dir * t;
    return true;
}

bool WorldToScreen(World* world, Entity cameraEntity, const Math::Vector3& world_,
                   f32 viewportWidth, f32 viewportHeight, Math::Vector2& outScreen) {
    if (viewportWidth <= 0.0f || viewportHeight <= 0.0f) return false;

    Math::Matrix4 vp;
    if (!ViewProjectionFor(world, cameraEntity, viewportWidth / viewportHeight, vp)) return false;

    const Math::Vector4 clip = vp * Math::Vector4(world_.x, world_.y, world_.z, 1.0f);
    // w <= 0 is behind the camera. Dividing anyway yields a plausible-looking
    // pixel for something that is not on screen at all.
    if (clip.w <= kEpsilon) return false;

    const f32 ndcX = clip.x / clip.w;
    const f32 ndcY = clip.y / clip.w;
    outScreen.x = (ndcX * 0.5f + 0.5f) * viewportWidth;
    outScreen.y = (ndcY * 0.5f + 0.5f) * viewportHeight;
    return true;
}

} // namespace ECS
} // namespace Enjin
