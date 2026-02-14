#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/World.h"
#include <vector>

namespace Enjin {
namespace Renderer {

// ---------------------------------------------------------------------------
// VXGIConfig -- configuration for the voxel cone tracing system
// ---------------------------------------------------------------------------
struct VXGIConfig {
    i32 resolution = 64;          // Voxel grid resolution (64/128/256)
    f32 worldExtent = 50.0f;      // World-space extent of the grid (cube)
    i32 diffuseCones = 6;         // Number of cones for diffuse GI
    f32 coneSpread = 0.5f;        // Cone aperture half-angle (radians)
    f32 traceDistance = 30.0f;    // Max trace distance
    f32 indirectMultiplier = 1.0f; // Scales indirect lighting contribution
    bool enableDiffuse = true;
    bool enableSpecular = true;
    bool enableAO = true;
    bool enableGodRays = false;
    i32 godRaySteps = 64;         // Number of volumetric march steps
};

// ---------------------------------------------------------------------------
// VoxelData -- data stored per voxel cell
// ---------------------------------------------------------------------------
struct VoxelData {
    Math::Vector3 color = {0, 0, 0};  // Radiance/albedo
    f32 opacity = 0.0f;                // 0 = empty, 1 = fully solid
    f32 emission = 0.0f;               // Emissive intensity
};

// ---------------------------------------------------------------------------
// ConeTraceResult -- result of a single cone trace
// ---------------------------------------------------------------------------
struct ConeTraceResult {
    Math::Vector3 color = {0, 0, 0};
    f32 occlusion = 0.0f;  // 0 = no occlusion, 1 = fully occluded
};

// ---------------------------------------------------------------------------
// Light data passed to the voxelizer for direct lighting injection
// ---------------------------------------------------------------------------
struct VXGILight {
    Math::Vector3 position;
    Math::Vector3 direction;    // Normalized; used for directional lights
    Math::Vector3 color;
    f32 intensity = 1.0f;
    f32 range = 50.0f;
    bool isDirectional = false;
};

// ---------------------------------------------------------------------------
// VoxelGrid -- 3D voxel grid with mip chain for cone tracing
// ---------------------------------------------------------------------------
class ENJIN_API VoxelGrid {
public:
    VoxelGrid() = default;
    ~VoxelGrid() = default;

    // Initialize the grid with the given resolution and world extent.
    // Resolution is clamped to {64, 128, 256}.
    void Initialize(i32 resolution, f32 worldExtent);

    // Clear all voxels to empty
    void Clear();

    // Voxelize the scene: scan all mesh entities and rasterize triangles
    // into the voxel grid using conservative AABB-triangle tests.
    void Voxelize(ECS::World* world);

    // Inject direct lighting into voxels from the given light sources.
    // For each lit voxel face, stores the incoming radiance.
    void InjectDirectLighting(const std::vector<VXGILight>& lights);

    // Generate mipmap chain by averaging 2x2x2 voxel blocks.
    // Required for cone tracing at varying apertures (coarser mip = wider cone).
    void GenerateMipmaps();

    // Sample a voxel at a given position and mip level.
    // mipLevel 0 = finest (original resolution), higher = coarser.
    VoxelData SampleVoxel(const Math::Vector3& worldPos, i32 mipLevel) const;

    // Trilinear interpolated sample at a world position and fractional mip level
    VoxelData SampleVoxelTrilinear(const Math::Vector3& worldPos, f32 mipLevel) const;

    // Convert world position to grid coordinates at a given mip level
    bool WorldToGrid(const Math::Vector3& worldPos, i32 mipLevel, i32& gx, i32& gy, i32& gz) const;

    // Accessors
    i32 GetResolution() const { return m_Resolution; }
    f32 GetWorldExtent() const { return m_WorldExtent; }
    f32 GetVoxelSize() const { return m_VoxelSize; }
    i32 GetMipLevels() const { return m_MipLevels; }
    const Math::Vector3& GetGridOrigin() const { return m_GridOrigin; }

    // Direct voxel access (mip level 0)
    VoxelData& GetVoxel(i32 x, i32 y, i32 z);
    const VoxelData& GetVoxel(i32 x, i32 y, i32 z) const;

    // Mip level access
    VoxelData& GetMipVoxel(i32 mipLevel, i32 x, i32 y, i32 z);
    const VoxelData& GetMipVoxel(i32 mipLevel, i32 x, i32 y, i32 z) const;

private:
    // Rasterize a single triangle into the voxel grid (conservative)
    void RasterizeTriangle(
        const Math::Vector3& v0,
        const Math::Vector3& v1,
        const Math::Vector3& v2,
        const Math::Vector3& color
    );

    // Check if a triangle's AABB overlaps a voxel cell
    static bool TriangleAABBOverlap(
        const Math::Vector3& v0,
        const Math::Vector3& v1,
        const Math::Vector3& v2,
        const Math::Vector3& boxCenter,
        const Math::Vector3& boxHalfSize
    );

    i32 m_Resolution = 0;
    f32 m_WorldExtent = 0.0f;
    f32 m_VoxelSize = 0.0f;
    i32 m_MipLevels = 0;
    Math::Vector3 m_GridOrigin;

    // Mip chain: m_MipChain[0] = full resolution, m_MipChain[N] = coarsest
    // Each entry is a flat 3D array of VoxelData (res^3 elements)
    std::vector<std::vector<VoxelData>> m_MipChain;

    // Dummy voxel for out-of-bounds access
    static VoxelData s_EmptyVoxel;
};

// ---------------------------------------------------------------------------
// ConeTracer -- traces cones through the voxel mip pyramid
// ---------------------------------------------------------------------------
class ENJIN_API ConeTracer {
public:
    ConeTracer() = default;
    ~ConeTracer() = default;

    // Set the voxel grid to trace through
    void SetGrid(const VoxelGrid* grid) { m_Grid = grid; }

    // Set configuration
    void SetConfig(const VXGIConfig& config) { m_Config = config; }
    const VXGIConfig& GetConfig() const { return m_Config; }

    // Trace a single cone through the voxel grid.
    // origin: start position (world space)
    // direction: normalized cone axis
    // aperture: cone half-angle in radians (0 = ray, PI/4 = wide cone)
    // maxDistance: maximum trace distance
    ConeTraceResult TraceCone(
        const Math::Vector3& origin,
        const Math::Vector3& direction,
        f32 aperture,
        f32 maxDistance
    ) const;

    // Compute diffuse global illumination at a surface point.
    // Traces ~diffuseCones cones in a hemisphere around the normal.
    Math::Vector3 ComputeDiffuseGI(
        const Math::Vector3& position,
        const Math::Vector3& normal
    ) const;

    // Compute specular/glossy global illumination.
    // Traces 1 cone in the reflection direction with aperture based on roughness.
    Math::Vector3 ComputeSpecularGI(
        const Math::Vector3& position,
        const Math::Vector3& normal,
        const Math::Vector3& viewDir,
        f32 roughness
    ) const;

    // Compute ambient occlusion by tracing short cones and measuring occlusion.
    f32 ComputeAmbientOcclusion(
        const Math::Vector3& position,
        const Math::Vector3& normal
    ) const;

    // Compute volumetric god rays by ray marching through voxels toward light.
    Math::Vector3 ComputeGodRays(
        const Math::Vector3& lightPos,
        const Math::Vector3& viewPos,
        i32 numSteps
    ) const;

private:
    // Build an orthonormal basis (tangent, bitangent) from a normal
    static void BuildOrthonormalBasis(
        const Math::Vector3& normal,
        Math::Vector3& tangent,
        Math::Vector3& bitangent
    );

    const VoxelGrid* m_Grid = nullptr;
    VXGIConfig m_Config;
};

} // namespace Renderer
} // namespace Enjin
