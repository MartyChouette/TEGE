#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include <functional>
#include <vector>

namespace Enjin {
namespace Effects {

// ---------------------------------------------------------------------------
// PortalComponent -- ECS component for portal rendering
// ---------------------------------------------------------------------------
struct ENJIN_API PortalComponent {
    ECS::Entity linkedPortal = 0;     // The other portal entity
    f32 portalWidth = 2.0f;           // Portal opening dimensions
    f32 portalHeight = 3.0f;
    i32 maxRecursionDepth = 4;        // Stencil recursion depth
    bool seamless = true;             // Seamless transition (no visible border)
    bool preserveMomentum = true;     // Maintain velocity through portal
    Math::Vector3 portalNormal = {0, 0, 1};  // Forward direction of portal face
};

// ---------------------------------------------------------------------------
// PortalRenderer -- stencil-buffer recursive portal rendering
// ---------------------------------------------------------------------------

// Result of computing a virtual camera through a portal pair
struct PortalCameraResult {
    Math::Vector3 position;
    Math::Quaternion rotation;
};

// Render callback signature: renders the scene from the given view/projection
using PortalRenderCallback = std::function<void(
    const Math::Matrix4& view,
    const Math::Matrix4& projection,
    i32 stencilRef
)>;

class ENJIN_API PortalRenderer {
public:
    PortalRenderer() = default;
    ~PortalRenderer() = default;

    // Main portal render entry point.
    // For each visible portal, performs stencil-buffer recursion to render
    // the scene from the linked portal's perspective.
    void RenderPortals(
        ECS::World* world,
        const Math::Vector3& cameraPos,
        const Math::Quaternion& cameraRot,
        const Math::Matrix4& viewMatrix,
        const Math::Matrix4& projectionMatrix,
        const PortalRenderCallback& renderCallback
    );

    // Transform camera position/rotation through a portal pair.
    // Applies 180-degree rotation around up axis + relative offset between
    // source and destination portals.
    static PortalCameraResult ComputePortalTransform(
        const Math::Vector3& srcPos,
        const Math::Quaternion& srcRot,
        const Math::Vector3& srcNormal,
        const Math::Vector3& dstPos,
        const Math::Quaternion& dstRot,
        const Math::Vector3& dstNormal,
        const Math::Vector3& cameraPos,
        const Math::Quaternion& cameraRot
    );

    // Compute oblique near-plane clipping matrix to prevent seeing behind
    // the portal destination. Modifies the projection matrix so the near
    // plane aligns with the portal plane.
    static Math::Matrix4 ComputeObliqueProjection(
        const Math::Matrix4& projection,
        const Math::Vector4& portalPlane  // (nx, ny, nz, d) in view space
    );

private:
    // Recursive portal render helper
    void RenderPortalRecursive(
        ECS::World* world,
        ECS::Entity portalEntity,
        const Math::Vector3& cameraPos,
        const Math::Quaternion& cameraRot,
        const Math::Matrix4& viewMatrix,
        const Math::Matrix4& projectionMatrix,
        const PortalRenderCallback& renderCallback,
        i32 currentDepth,
        i32 maxDepth,
        i32 stencilRef
    );

    // Per-frame visible portal cache
    std::vector<ECS::Entity> m_VisiblePortals;
};

// ---------------------------------------------------------------------------
// SpaceWarpComponent -- non-Euclidean spatial warping ECS component
// ---------------------------------------------------------------------------
enum class SpaceType : u8 {
    Euclidean,
    Hyperbolic,
    Spherical,
    Toroidal
};

struct ENJIN_API SpaceWarpComponent {
    SpaceType type = SpaceType::Euclidean;
    f32 curvature = 0.0f;          // Negative = hyperbolic, positive = spherical
    Math::Vector3 center = {0, 0, 0};
    f32 radius = 10.0f;            // Affected region radius
    f32 warpStrength = 1.0f;       // Blend factor (0 = no warp, 1 = full warp)
};

// ---------------------------------------------------------------------------
// NonEuclideanSystem -- processes SpaceWarp components to warp positions
// ---------------------------------------------------------------------------
class ENJIN_API NonEuclideanSystem {
public:
    NonEuclideanSystem() = default;
    ~NonEuclideanSystem() = default;

    // Process all SpaceWarp components in the world
    void Update(ECS::World* world, f32 dt);

    // Apply spatial warping from a single warp component to a position
    static Math::Vector3 WarpPosition(
        const Math::Vector3& position,
        const SpaceWarpComponent& warp
    );

    // Hyperbolic translation: moves along a geodesic in hyperbolic space
    // (Poincare disk model). Curvature < 0 means hyperbolic.
    static Math::Vector3 ComputeHyperbolicTranslation(
        const Math::Vector3& pos,
        const Math::Vector3& direction,
        f32 distance,
        f32 curvature
    );

    // Spherical translation: moves along a great circle in spherical space.
    // Curvature > 0 means spherical (radius = 1/sqrt(curvature)).
    static Math::Vector3 ComputeSphericalTranslation(
        const Math::Vector3& pos,
        const Math::Vector3& direction,
        f32 distance,
        f32 curvature
    );

    // Toroidal wrapping: position wraps seamlessly at bounds edges
    static Math::Vector3 ComputeToroidalWrap(
        const Math::Vector3& pos,
        const Math::Vector3& boundsMin,
        const Math::Vector3& boundsMax
    );

    // Check if a position is inside a warp zone's influence radius
    static bool IsInsideWarpZone(
        const Math::Vector3& pos,
        const SpaceWarpComponent& warp
    );

private:
    // Cached entity list to avoid per-frame allocation
    std::vector<ECS::Entity> m_WarpEntities;
};

} // namespace Effects
} // namespace Enjin
