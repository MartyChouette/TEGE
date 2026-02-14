#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <vector>
#include <string>
#include <tuple>

namespace Enjin {
namespace Renderer {

// ---------------------------------------------------------------------------
// SDFVolume — Voxelized signed distance field stored on a regular 3D grid
// ---------------------------------------------------------------------------
struct ENJIN_API SDFVolume {
    std::vector<f32> data;           // Flat 3D array: data[z * resY * resX + y * resX + x]
    i32 resX = 0, resY = 0, resZ = 0;
    Math::Vector3 boundsMin;
    Math::Vector3 boundsMax;
    f32 voxelSize = 0.0f;

    // Index helper (clamped)
    f32 Sample(i32 x, i32 y, i32 z) const;

    bool IsValid() const {
        return resX > 0 && resY > 0 && resZ > 0
            && static_cast<usize>(resX) * resY * resZ == data.size();
    }
};

// ---------------------------------------------------------------------------
// SDFGlyph — A single glyph stored as a 2D signed distance field
// ---------------------------------------------------------------------------
struct ENJIN_API SDFGlyph {
    std::vector<f32> sdfData;        // Flat 2D array: sdfData[y * width + x]
    i32 width = 0, height = 0;
    f32 baseline = 0.0f;             // Distance from top to baseline (pixels)
    f32 advance = 0.0f;              // Horizontal advance (pixels)
};

// ---------------------------------------------------------------------------
// SDFTextConfig — Controls SDF text rendering appearance
// ---------------------------------------------------------------------------
struct SDFTextConfig {
    f32 smoothing = 0.05f;                               // Alpha smoothing width
    f32 outlineWidth = 0.0f;                             // Outline thickness (0 = none)
    Math::Vector3 outlineColor = {0.0f, 0.0f, 0.0f};    // Outline color
    Math::Vector2 shadowOffset = {0.0f, 0.0f};           // Drop shadow offset
    Math::Vector3 shadowColor = {0.0f, 0.0f, 0.0f};     // Drop shadow color
    f32 shadowSoftness = 0.1f;                           // Drop shadow blur
};

// ---------------------------------------------------------------------------
// MeshData — Lightweight output mesh (vertices + indices)
// ---------------------------------------------------------------------------
struct MeshData {
    std::vector<ECS::MeshComponent::Vertex> vertices;
    std::vector<u32> indices;
};

// ---------------------------------------------------------------------------
// SDFRenderComponent — ECS component for SDF-based rendering
// ---------------------------------------------------------------------------
struct SDFRenderComponent {
    enum class RenderMode : u8 { SphereTrace, MeshExtract };

    RenderMode mode = RenderMode::MeshExtract;
    i32 resolution = 64;             // SDF volume resolution per axis
    f32 smoothness = 0.01f;          // Isosurface smoothing
    f32 outlineWidth = 0.0f;         // SDF outline effect (world units)
    Math::Vector3 outlineColor = {0.0f, 0.0f, 0.0f};
    bool autoRebuild = true;         // Rebuild SDF when mesh changes
};

// ---------------------------------------------------------------------------
// MeshToSDF — Converts polygon meshes to signed distance fields
// ---------------------------------------------------------------------------
class ENJIN_API MeshToSDF {
public:
    // Convert a triangle mesh to a voxelized SDF.
    // vertices/indices describe a triangle soup; resolution controls grid size per axis.
    static SDFVolume ConvertMesh(const std::vector<ECS::MeshComponent::Vertex>& vertices,
                                 const std::vector<u32>& indices,
                                 i32 resolution);

    // Trilinear-interpolated SDF evaluation at an arbitrary world position.
    static f32 EvaluateSDF(const SDFVolume& volume, const Math::Vector3& worldPos);

    // Gradient-based normal at a world position (central differences).
    static Math::Vector3 ComputeNormalSDF(const SDFVolume& volume, const Math::Vector3& worldPos);

private:
    // Closest point on triangle ABC to point P, returning squared distance.
    static f32 PointTriangleDistSq(const Math::Vector3& p,
                                    const Math::Vector3& a,
                                    const Math::Vector3& b,
                                    const Math::Vector3& c,
                                    Math::Vector3& closestPoint);

    // Sign determination via pseudo-normal method.
    static f32 ComputeSign(const Math::Vector3& p,
                           const Math::Vector3& closestPoint,
                           const Math::Vector3& triNormal);
};

// ---------------------------------------------------------------------------
// SDFTextRenderer — Resolution-independent text via SDF glyphs
// ---------------------------------------------------------------------------
class ENJIN_API SDFTextRenderer {
public:
    // Generate an SDF glyph from a binary (alpha) bitmap.
    // padding: extra border pixels around the glyph.
    // spreadRadius: max distance (pixels) encoded in the SDF.
    static SDFGlyph GenerateGlyphSDF(const u8* glyphBitmap,
                                      i32 width, i32 height,
                                      i32 padding, f32 spreadRadius);

    // Build a quad-strip mesh for a string of text using pre-built SDF glyphs.
    // fontSDFAtlas maps character codes to SDFGlyph entries.
    static MeshData RenderText(const std::string& text,
                               const std::vector<SDFGlyph>& fontSDFAtlas,
                               const Math::Vector3& position,
                               f32 fontSize,
                               const Math::Vector3& color,
                               const SDFTextConfig& config = {});

private:
    // 8SSEDT internal pass (forward + backward sweep).
    static void ComputeDistanceField8SSEDT(const std::vector<bool>& inside,
                                            i32 w, i32 h,
                                            std::vector<f32>& outDist);
};

// ---------------------------------------------------------------------------
// SDFMeshRenderer — Renders SDF volumes as meshes via sphere tracing
//                   or marching cubes isosurface extraction
// ---------------------------------------------------------------------------
class ENJIN_API SDFMeshRenderer {
public:
    // Sphere-trace (ray march) a single ray through an SDF volume.
    struct TraceResult {
        bool hit = false;
        f32 distance = 0.0f;
        Math::Vector3 hitPoint;
    };

    static TraceResult SphereTrace(const SDFVolume& volume,
                                   const Math::Vector3& rayOrigin,
                                   const Math::Vector3& rayDir,
                                   i32 maxSteps = 128,
                                   f32 epsilon = 0.001f);

    // Marching cubes isosurface extraction from an SDF volume.
    static MeshData ExtractIsosurface(const SDFVolume& volume, f32 isovalue = 0.0f);

    // Extract at a reduced effective resolution for smooth LOD.
    // detailLevel: 1.0 = full resolution, 0.5 = half, etc.
    static MeshData ComputeSmoothLOD(const SDFVolume& volume, f32 detailLevel);

    // Smooth blending of two SDF volumes (must share the same grid dimensions).
    static SDFVolume BlendSDFVolumes(const SDFVolume& volumeA,
                                     const SDFVolume& volumeB,
                                     f32 blendFactor);

private:
    // Marching cubes helpers
    static Math::Vector3 InterpolateEdge(const Math::Vector3& p1,
                                          const Math::Vector3& p2,
                                          f32 v1, f32 v2, f32 isovalue);
};

} // namespace Renderer
} // namespace Enjin
