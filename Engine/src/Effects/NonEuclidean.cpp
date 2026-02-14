#include "Enjin/Effects/NonEuclidean.h"
#include "Enjin/ECS/Components/Transform.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Effects {

// ===========================================================================
// PortalRenderer
// ===========================================================================

void PortalRenderer::RenderPortals(
    ECS::World* world,
    const Math::Vector3& cameraPos,
    const Math::Quaternion& cameraRot,
    const Math::Matrix4& viewMatrix,
    const Math::Matrix4& projectionMatrix,
    const PortalRenderCallback& renderCallback)
{
    if (!world) return;

    // Gather all portal entities
    m_VisiblePortals.clear();
    const auto& portalEntities = world->GetEntitiesWithComponent<PortalComponent>();
    for (ECS::Entity entity : portalEntities) {
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        auto* portal = world->GetComponent<PortalComponent>(entity);
        if (!transform || !portal || !transform->visible) continue;
        if (portal->linkedPortal == ECS::INVALID_ENTITY) continue;

        // Frustum/facing check: only render portals whose front face
        // is visible to the camera (dot product of view direction and
        // portal normal must be negative from camera's perspective)
        Math::Vector3 toPortal = transform->position - cameraPos;
        Math::Vector3 worldNormal = transform->rotation.Rotate(portal->portalNormal);
        f32 dot = toPortal.Dot(worldNormal);

        // Portal front face is visible when camera is on the normal side
        if (dot < 0.0f) {
            m_VisiblePortals.push_back(entity);
        }
    }

    // Sort by distance (nearest first for correct stencil layering)
    std::sort(m_VisiblePortals.begin(), m_VisiblePortals.end(),
        [&](ECS::Entity a, ECS::Entity b) {
            auto* ta = world->GetComponent<ECS::TransformComponent>(a);
            auto* tb = world->GetComponent<ECS::TransformComponent>(b);
            if (!ta || !tb) return false;
            f32 distA = (ta->position - cameraPos).LengthSquared();
            f32 distB = (tb->position - cameraPos).LengthSquared();
            return distA < distB;
        });

    // Render each visible portal with stencil recursion
    i32 stencilRef = 1;
    for (ECS::Entity entity : m_VisiblePortals) {
        auto* portal = world->GetComponent<PortalComponent>(entity);
        if (!portal) continue;

        RenderPortalRecursive(
            world, entity,
            cameraPos, cameraRot,
            viewMatrix, projectionMatrix,
            renderCallback,
            0, portal->maxRecursionDepth,
            stencilRef
        );
        stencilRef++;
    }
}

void PortalRenderer::RenderPortalRecursive(
    ECS::World* world,
    ECS::Entity portalEntity,
    const Math::Vector3& cameraPos,
    const Math::Quaternion& cameraRot,
    const Math::Matrix4& viewMatrix,
    const Math::Matrix4& projectionMatrix,
    const PortalRenderCallback& renderCallback,
    i32 currentDepth,
    i32 maxDepth,
    i32 stencilRef)
{
    if (currentDepth >= maxDepth) {
        // At max recursion depth: render a solid color or portal surface
        // The callback with negative stencil ref signals "final depth"
        renderCallback(viewMatrix, projectionMatrix, -stencilRef);
        return;
    }

    auto* srcTransform = world->GetComponent<ECS::TransformComponent>(portalEntity);
    auto* srcPortal = world->GetComponent<PortalComponent>(portalEntity);
    if (!srcTransform || !srcPortal) return;

    ECS::Entity linkedEntity = srcPortal->linkedPortal;
    auto* dstTransform = world->GetComponent<ECS::TransformComponent>(linkedEntity);
    auto* dstPortal = world->GetComponent<PortalComponent>(linkedEntity);
    if (!dstTransform || !dstPortal) return;

    // Step 1: Compute virtual camera position and rotation through the portal pair
    Math::Vector3 srcWorldNormal = srcTransform->rotation.Rotate(srcPortal->portalNormal);
    Math::Vector3 dstWorldNormal = dstTransform->rotation.Rotate(dstPortal->portalNormal);

    PortalCameraResult virtualCamera = ComputePortalTransform(
        srcTransform->position, srcTransform->rotation, srcWorldNormal,
        dstTransform->position, dstTransform->rotation, dstWorldNormal,
        cameraPos, cameraRot
    );

    // Step 2: Build view matrix from virtual camera
    Math::Vector3 forward = virtualCamera.rotation.GetForward();
    Math::Vector3 up = virtualCamera.rotation.GetUp();
    Math::Matrix4 virtualView = Math::Matrix4::LookAt(
        virtualCamera.position,
        virtualCamera.position + forward,
        up
    );

    // Step 3: Compute oblique projection to clip behind the destination portal
    // Portal plane in world space: normal = -dstWorldNormal (facing into destination),
    // d = dot(dstWorldNormal, dstTransform->position)
    Math::Vector3 clipNormal = -dstWorldNormal;
    f32 clipD = clipNormal.Dot(dstTransform->position);

    // Transform portal plane into view space
    Math::Matrix4 viewInvTranspose = virtualView.Inverse().Transposed();
    Math::Vector4 worldPlane(clipNormal.x, clipNormal.y, clipNormal.z, -clipD);
    Math::Vector4 viewSpacePlane = viewInvTranspose * worldPlane;

    Math::Matrix4 virtualProjection = ComputeObliqueProjection(projectionMatrix, viewSpacePlane);

    // Step 4: Render the scene from the virtual camera, masked by stencil
    renderCallback(virtualView, virtualProjection, stencilRef);

    // Step 5: Recurse for portals visible through this portal
    const auto& portalEntities = world->GetEntitiesWithComponent<PortalComponent>();
    for (ECS::Entity entity : portalEntities) {
        if (entity == portalEntity || entity == linkedEntity) continue;
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        auto* portal = world->GetComponent<PortalComponent>(entity);
        if (!transform || !portal || !transform->visible) continue;
        if (portal->linkedPortal == ECS::INVALID_ENTITY) continue;

        // Check if this portal is visible from the virtual camera
        Math::Vector3 toPortal = transform->position - virtualCamera.position;
        Math::Vector3 worldNormal = transform->rotation.Rotate(portal->portalNormal);
        if (toPortal.Dot(worldNormal) < 0.0f) {
            RenderPortalRecursive(
                world, entity,
                virtualCamera.position, virtualCamera.rotation,
                virtualView, virtualProjection,
                renderCallback,
                currentDepth + 1, maxDepth,
                stencilRef + 100 // Unique stencil ref per recursion level
            );
        }
    }
}

PortalCameraResult PortalRenderer::ComputePortalTransform(
    const Math::Vector3& srcPos,
    const Math::Quaternion& srcRot,
    const Math::Vector3& srcNormal,
    const Math::Vector3& dstPos,
    const Math::Quaternion& dstRot,
    const Math::Vector3& dstNormal,
    const Math::Vector3& cameraPos,
    const Math::Quaternion& cameraRot)
{
    PortalCameraResult result;

    // 1. Transform camera position from source portal's local space to
    //    destination portal's local space with a 180-degree flip.

    // Compute the relative transform from source to destination:
    // The portal pair should show "through" the portal, which means
    // the camera needs to be rotated 180 degrees around the up axis
    // at the destination (you're looking "into" the source but "out of"
    // the destination).

    // Get camera position relative to source portal
    Math::Vector3 relativePos = cameraPos - srcPos;

    // Transform to source portal's local space
    Math::Quaternion srcInverse = srcRot.Inverse();
    Math::Vector3 localPos = srcInverse.Rotate(relativePos);

    // 180-degree rotation around Y axis (flip to look through destination)
    // This inverts the X and Z local coordinates
    localPos.x = -localPos.x;
    localPos.z = -localPos.z;

    // Transform from destination portal's local space to world space
    result.position = dstPos + dstRot.Rotate(localPos);

    // 2. Transform camera rotation through the portal
    // Relative rotation from source to destination portal
    Math::Quaternion srcInvRot = srcRot.Inverse();
    Math::Quaternion portalRotDelta = dstRot * srcInvRot;

    // 180-degree rotation around up axis
    Math::Quaternion halfTurn(Math::Vector3(0, 1, 0), Math::PI);

    result.rotation = portalRotDelta * halfTurn * cameraRot;
    result.rotation.Normalize();

    return result;
}

Math::Matrix4 PortalRenderer::ComputeObliqueProjection(
    const Math::Matrix4& projection,
    const Math::Vector4& portalPlane)
{
    Math::Matrix4 result = projection;

    // Oblique near-plane clipping (Lengyel's method):
    // Replace the third row of the projection matrix so that the near
    // plane aligns with the portal plane. This prevents geometry behind
    // the portal destination from being rendered.

    // Compute the clip-space corner point opposite the portal plane
    Math::Vector4 q(
        (Math::Sign(portalPlane.x) + projection(0, 2)) / projection(0, 0),
        (Math::Sign(portalPlane.y) + projection(1, 2)) / projection(1, 1),
        -1.0f,
        (1.0f + projection(2, 2)) / projection(2, 3)
    );

    // Scale the portal plane so that dot(plane, q) = 2
    f32 dotPQ = portalPlane.x * q.x + portalPlane.y * q.y +
                portalPlane.z * q.z + portalPlane.w * q.w;

    if (Math::Abs(dotPQ) < Math::EPSILON) {
        // Portal plane is nearly perpendicular to view; skip oblique clipping
        return result;
    }

    Math::Vector4 c = portalPlane * (2.0f / dotPQ);

    // Replace the third row of the projection matrix
    result(2, 0) = c.x;
    result(2, 1) = c.y;
    result(2, 2) = c.z + 1.0f;
    result(2, 3) = c.w;

    return result;
}

// ===========================================================================
// NonEuclideanSystem
// ===========================================================================

void NonEuclideanSystem::Update(ECS::World* world, f32 dt)
{
    (void)dt; // Reserved for future time-based warping effects
    if (!world) return;

    const auto& warpEntities = world->GetEntitiesWithComponent<SpaceWarpComponent>();
    if (warpEntities.empty()) return;

    // Apply warping to all entities with transforms
    const auto& transformEntities = world->GetEntitiesWithComponent<ECS::TransformComponent>();
    for (ECS::Entity entity : transformEntities) {
        // Skip entities that are themselves warp zones
        if (world->GetComponent<SpaceWarpComponent>(entity)) continue;

        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!transform) continue;

        // Check against all warp zones
        for (ECS::Entity warpEntity : warpEntities) {
            auto* warp = world->GetComponent<SpaceWarpComponent>(warpEntity);
            if (!warp || warp->type == SpaceType::Euclidean) continue;

            if (IsInsideWarpZone(transform->position, *warp)) {
                transform->position = WarpPosition(transform->position, *warp);
            }
        }
    }
}

Math::Vector3 NonEuclideanSystem::WarpPosition(
    const Math::Vector3& position,
    const SpaceWarpComponent& warp)
{
    switch (warp.type) {
        case SpaceType::Euclidean:
            return position;

        case SpaceType::Hyperbolic: {
            // Apply hyperbolic warping: positions curve away from center
            Math::Vector3 offset = position - warp.center;
            f32 dist = offset.Length();
            if (dist < Math::EPSILON) return position;

            Math::Vector3 dir = offset / dist;
            Math::Vector3 warped = ComputeHyperbolicTranslation(
                warp.center, dir, dist, warp.curvature
            );
            return Math::Lerp(position, warped, warp.warpStrength);
        }

        case SpaceType::Spherical: {
            // Apply spherical warping: positions curve toward center
            Math::Vector3 offset = position - warp.center;
            f32 dist = offset.Length();
            if (dist < Math::EPSILON) return position;

            Math::Vector3 dir = offset / dist;
            Math::Vector3 warped = ComputeSphericalTranslation(
                warp.center, dir, dist, warp.curvature
            );
            return Math::Lerp(position, warped, warp.warpStrength);
        }

        case SpaceType::Toroidal: {
            // Toroidal wrap within the warp zone bounding box
            Math::Vector3 halfExtent(warp.radius);
            Math::Vector3 boundsMin = warp.center - halfExtent;
            Math::Vector3 boundsMax = warp.center + halfExtent;
            Math::Vector3 wrapped = ComputeToroidalWrap(position, boundsMin, boundsMax);
            return Math::Lerp(position, wrapped, warp.warpStrength);
        }

        default:
            return position;
    }
}

Math::Vector3 NonEuclideanSystem::ComputeHyperbolicTranslation(
    const Math::Vector3& pos,
    const Math::Vector3& direction,
    f32 distance,
    f32 curvature)
{
    // Hyperbolic geometry (Poincare disk model approximation):
    // In hyperbolic space, parallel lines diverge. We model this by
    // stretching distances exponentially based on curvature.
    //
    // For curvature K < 0, the effective distance grows as:
    //   d_hyp = (1/sqrt(-K)) * sinh(sqrt(-K) * d_euclidean)
    // This makes "rooms bigger on the inside" -- objects at the periphery
    // appear farther away than they would in Euclidean space.

    f32 absK = Math::Abs(curvature);
    if (absK < Math::EPSILON) {
        // Near-zero curvature = Euclidean
        return pos + direction * distance;
    }

    f32 sqrtK = Math::Sqrt(absK);
    f32 hypDist = (1.0f / sqrtK) * std::sinh(sqrtK * distance);

    return pos + direction * hypDist;
}

Math::Vector3 NonEuclideanSystem::ComputeSphericalTranslation(
    const Math::Vector3& pos,
    const Math::Vector3& direction,
    f32 distance,
    f32 curvature)
{
    // Spherical geometry:
    // In spherical space, parallel lines converge. The effective distance
    // is compressed:
    //   d_sph = (1/sqrt(K)) * sin(sqrt(K) * d_euclidean)
    // This creates corridors that appear to shrink as you walk.

    f32 absK = Math::Abs(curvature);
    if (absK < Math::EPSILON) {
        return pos + direction * distance;
    }

    f32 sqrtK = Math::Sqrt(absK);
    f32 arg = sqrtK * distance;

    // Clamp to avoid wrapping past the antipodal point
    if (arg > Math::PI) {
        arg = Math::PI;
    }

    f32 sphDist = (1.0f / sqrtK) * Math::Sin(arg);

    return pos + direction * sphDist;
}

Math::Vector3 NonEuclideanSystem::ComputeToroidalWrap(
    const Math::Vector3& pos,
    const Math::Vector3& boundsMin,
    const Math::Vector3& boundsMax)
{
    // Seamless toroidal wrap: position wraps around when exiting bounds.
    // The result is a pac-man/asteroids style wraparound in 3D.
    Math::Vector3 result = pos;
    Math::Vector3 size = boundsMax - boundsMin;

    // Wrap each axis independently
    for (i32 i = 0; i < 3; ++i) {
        if (size[i] <= Math::EPSILON) continue;

        // Normalize to [0, 1] within bounds, then fmod wrap
        f32 normalized = (result[i] - boundsMin[i]) / size[i];
        normalized = std::fmod(normalized, 1.0f);
        if (normalized < 0.0f) normalized += 1.0f;
        result[i] = boundsMin[i] + normalized * size[i];
    }

    return result;
}

bool NonEuclideanSystem::IsInsideWarpZone(
    const Math::Vector3& pos,
    const SpaceWarpComponent& warp)
{
    Math::Vector3 offset = pos - warp.center;
    f32 distSq = offset.LengthSquared();
    return distSq <= (warp.radius * warp.radius);
}

} // namespace Effects
} // namespace Enjin
