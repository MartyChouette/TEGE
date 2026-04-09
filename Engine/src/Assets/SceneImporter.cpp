#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Assets/PLYLoader.h"
#include "Enjin/Assets/VOXLoader.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/MorphTarget.h"
#include "Enjin/Animation/Animation.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Logging/Log.h"
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <unordered_map>

namespace Enjin {
namespace Assets {

// ============================================================================
// Mesh Validation — detect and auto-fix common import problems
// ============================================================================

static void ValidateImportedMeshes(ECS::World* world, const ImportResult& result,
                                     std::vector<std::string>& warnings) {
    u32 nanVertices = 0;
    u32 degenerateTriangles = 0;
    u32 badBoneWeights = 0;
    u32 fixedBoneWeights = 0;
    f32 maxExtent = 0.0f;

    for (ECS::Entity entity : result.entities) {
        auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
        if (!mesh) continue;

        // Check vertices for NaN/Inf and fix
        for (auto& vert : mesh->vertices) {
            bool bad = false;
            if (std::isnan(vert.position.x) || std::isinf(vert.position.x) ||
                std::isnan(vert.position.y) || std::isinf(vert.position.y) ||
                std::isnan(vert.position.z) || std::isinf(vert.position.z)) {
                vert.position = Math::Vector3(0.0f);
                bad = true;
            }
            if (std::isnan(vert.normal.x) || std::isinf(vert.normal.x) ||
                std::isnan(vert.normal.y) || std::isinf(vert.normal.y) ||
                std::isnan(vert.normal.z) || std::isinf(vert.normal.z)) {
                vert.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
                bad = true;
            }
            if (bad) nanVertices++;

            // Track max extent for scale sanity check
            f32 absX = std::fabs(vert.position.x);
            f32 absY = std::fabs(vert.position.y);
            f32 absZ = std::fabs(vert.position.z);
            if (absX > maxExtent) maxExtent = absX;
            if (absY > maxExtent) maxExtent = absY;
            if (absZ > maxExtent) maxExtent = absZ;

            // Bone weight validation — check sum and normalize if needed
            f32 wSum = vert.boneWeights.x + vert.boneWeights.y + vert.boneWeights.z + vert.boneWeights.w;
            if (wSum > 0.001f && std::fabs(wSum - 1.0f) > 0.01f) {
                badBoneWeights++;
                // Auto-fix: normalize
                f32 inv = 1.0f / wSum;
                vert.boneWeights.x *= inv;
                vert.boneWeights.y *= inv;
                vert.boneWeights.z *= inv;
                vert.boneWeights.w *= inv;
                fixedBoneWeights++;
            }
        }

        // Check for degenerate triangles (zero-area)
        for (usize i = 0; i + 2 < mesh->indices.size(); i += 3) {
            u32 i0 = mesh->indices[i], i1 = mesh->indices[i + 1], i2 = mesh->indices[i + 2];
            if (i0 >= mesh->vertices.size() || i1 >= mesh->vertices.size() || i2 >= mesh->vertices.size()) continue;
            const auto& v0 = mesh->vertices[i0].position;
            const auto& v1 = mesh->vertices[i1].position;
            const auto& v2 = mesh->vertices[i2].position;
            Math::Vector3 edge1 = v1 - v0;
            Math::Vector3 edge2 = v2 - v0;
            Math::Vector3 cross = Math::Vector3(
                edge1.y * edge2.z - edge1.z * edge2.y,
                edge1.z * edge2.x - edge1.x * edge2.z,
                edge1.x * edge2.y - edge1.y * edge2.x);
            f32 area2 = cross.x * cross.x + cross.y * cross.y + cross.z * cross.z;
            if (area2 < 1e-12f) degenerateTriangles++;
        }
    }

    // Report findings
    if (nanVertices > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Fixed %u vertices with NaN/Inf values (clamped to zero)", nanVertices);
        warnings.push_back(buf);
        ENJIN_LOG_WARN(Asset, "Mesh validation: %s", buf);
    }
    if (degenerateTriangles > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Found %u degenerate triangles (zero area)", degenerateTriangles);
        warnings.push_back(buf);
        ENJIN_LOG_WARN(Asset, "Mesh validation: %s", buf);
    }
    if (fixedBoneWeights > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Normalized %u vertices with bone weights not summing to 1.0", fixedBoneWeights);
        warnings.push_back(buf);
        ENJIN_LOG_WARN(Asset, "Mesh validation: %s", buf);
    }
    if (maxExtent > 1000.0f) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Model is very large (%.0f units). Consider scaling down.", maxExtent);
        warnings.push_back(buf);
        ENJIN_LOG_WARN(Asset, "Mesh validation: %s", buf);
    } else if (maxExtent > 0.0f && maxExtent < 0.001f) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Model is very small (%.6f units). Consider scaling up.", maxExtent);
        warnings.push_back(buf);
        ENJIN_LOG_WARN(Asset, "Mesh validation: %s", buf);
    }
}

// --- Source App Presets ---

SourceAppPreset GetSourceAppPreset(SourceApp app) {
    switch (app) {
        case SourceApp::Blender:           return { 1.0f,    true,  false, "Blender" };
        case SourceApp::Maya:              return { 0.01f,   false, false, "Maya" };
        case SourceApp::Max3ds:            return { 1.0f,    true,  false, "3ds Max" };
        case SourceApp::Houdini:           return { 1.0f,    false, false, "Houdini" };
        case SourceApp::Cinema4D:          return { 0.01f,   false, false, "Cinema 4D" };
        case SourceApp::ZBrush:            return { 1.0f,    true,  false, "ZBrush" };
        case SourceApp::SubstancePainter:  return { 1.0f,    false, false, "Substance Painter" };
        case SourceApp::Unreal:            return { 0.01f,   false, true,  "Unreal Engine" };
        case SourceApp::Unity:             return { 1.0f,    false, true,  "Unity" };
        case SourceApp::SketchUp:          return { 0.0254f, true,  false, "SketchUp" };
        case SourceApp::Custom:            return { 1.0f,    false, false, "Custom" };
        default:                           return { 1.0f,    false, false, "Auto" };
    }
}

const char* GetSourceAppName(SourceApp app) {
    switch (app) {
        case SourceApp::Auto:              return "Auto";
        case SourceApp::Blender:           return "Blender";
        case SourceApp::Maya:              return "Maya";
        case SourceApp::Max3ds:            return "3ds Max";
        case SourceApp::Houdini:           return "Houdini";
        case SourceApp::Cinema4D:          return "Cinema 4D";
        case SourceApp::ZBrush:            return "ZBrush";
        case SourceApp::SubstancePainter:  return "Substance Painter";
        case SourceApp::Unreal:            return "Unreal Engine";
        case SourceApp::Unity:             return "Unity";
        case SourceApp::SketchUp:          return "SketchUp";
        case SourceApp::Custom:            return "Custom";
        default:                           return "Unknown";
    }
}

// --- Auto-detection ---

static std::string ToLowerStr(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

static SourceApp DetectSourceApp(const std::string& generator, const std::string& creator) {
    auto check = [](const std::string& haystack, const char* needle) {
        return haystack.find(needle) != std::string::npos;
    };

    // Check generator string (glTF)
    std::string g = ToLowerStr(generator);
    if (check(g, "blender"))    return SourceApp::Blender;
    if (check(g, "maya"))       return SourceApp::Maya;
    if (check(g, "3ds max") || check(g, "3dsmax")) return SourceApp::Max3ds;
    if (check(g, "houdini"))    return SourceApp::Houdini;
    if (check(g, "cinema 4d") || check(g, "cinema4d") || check(g, "c4d")) return SourceApp::Cinema4D;
    if (check(g, "zbrush"))     return SourceApp::ZBrush;
    if (check(g, "substance"))  return SourceApp::SubstancePainter;
    if (check(g, "unreal"))     return SourceApp::Unreal;
    if (check(g, "unity"))      return SourceApp::Unity;
    if (check(g, "sketchup"))   return SourceApp::SketchUp;

    // Check creator string (FBX via Assimp)
    std::string c = ToLowerStr(creator);
    if (check(c, "blender"))    return SourceApp::Blender;
    if (check(c, "maya"))       return SourceApp::Maya;
    if (check(c, "3ds max") || check(c, "3dsmax")) return SourceApp::Max3ds;
    if (check(c, "houdini"))    return SourceApp::Houdini;
    if (check(c, "cinema 4d") || check(c, "cinema4d") || check(c, "c4d")) return SourceApp::Cinema4D;
    if (check(c, "zbrush"))     return SourceApp::ZBrush;
    if (check(c, "substance"))  return SourceApp::SubstancePainter;
    if (check(c, "unreal"))     return SourceApp::Unreal;
    if (check(c, "unity"))      return SourceApp::Unity;
    if (check(c, "sketchup"))   return SourceApp::SketchUp;

    return SourceApp::Auto; // Unknown — no conversion applied
}

// --- Axis Conversion Utilities ---

static Math::Vector3 ConvertPosition(const Math::Vector3& v, bool zToY, bool lToR,
                                      bool flipX, bool flipY, bool flipZ) {
    Math::Vector3 r = v;
    if (zToY) {
        // Z-up -> Y-up: (x, y, z) -> (x, z, -y)
        r = Math::Vector3(v.x, v.z, -v.y);
    }
    if (lToR) {
        // Left-hand -> Right-hand: negate X
        r.x = -r.x;
    }
    // Per-axis flip overrides
    if (flipX) r.x = -r.x;
    if (flipY) r.y = -r.y;
    if (flipZ) r.z = -r.z;
    return r;
}

static Math::Vector3 ConvertNormal(const Math::Vector3& n, bool zToY, bool lToR) {
    Math::Vector3 r = n;
    if (zToY) {
        r = Math::Vector3(n.x, n.z, -n.y);
    }
    if (lToR) {
        r.x = -r.x;
    }
    return r;
}

static Math::Quaternion ConvertRotation(const Math::Quaternion& q, bool zToY, bool lToR) {
    Math::Quaternion r = q;
    if (zToY) {
        // Z-up -> Y-up: (x, y, z, w) -> (x, z, -y, w)
        r = Math::Quaternion(q.x, q.z, -q.y, q.w);
        // Renormalize
        f32 len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
        if (len > 0.0001f) {
            f32 inv = 1.0f / len;
            r.x *= inv; r.y *= inv; r.z *= inv; r.w *= inv;
        }
    }
    if (lToR) {
        // Left-hand -> Right-hand: negate x and w
        r.x = -r.x;
        r.w = -r.w;
    }
    return r;
}

// --- Texture search path helper ---

static std::string TryTextureSearchPaths(const std::string& filename,
                                          const std::vector<std::string>& searchPaths) {
    namespace fs = std::filesystem;
    for (const auto& dir : searchPaths) {
        fs::path resolved = fs::path(dir) / filename;
        if (fs::exists(resolved)) {
            return resolved.string();
        }
    }
    return "";
}

// Helper: copy accumulated stats into ImportResult
static void CopyStatsToResult(ImportResult& result, const SceneImporter::ImportStats& stats) {
    result.meshCount = stats.meshCount;
    result.materialCount = stats.materialCount;
    result.totalVertexCount = stats.totalVertexCount;
    result.totalIndexCount = stats.totalIndexCount;
    result.entityNames = stats.entityNames;
    result.texturePathsResolved = stats.texturePathsResolved;
    result.texturePathsMissing = stats.texturePathsMissing;
    result.warnings = stats.warnings;
}

ImportResult SceneImporter::ImportGLTF(const std::string& filepath, ECS::World* world,
                                        const ImportOptions& options) {
    ImportResult result;

    if (!world) {
        result.errorMessage = "World is null";
        return result;
    }

    // Load the glTF file
    GLTFScene scene;
    if (!GLTFLoader::Load(filepath, scene)) {
        result.errorMessage = GLTFLoader::GetLastError();
        return result;
    }

    // Resolve source app and axis conversion
    ImportOptions effectiveOptions = options;
    SourceApp resolvedApp = options.sourceApp;
    if (resolvedApp == SourceApp::Auto) {
        resolvedApp = DetectSourceApp(scene.generator, "");
    }

    // For glTF files, the glTF spec mandates Y-up right-handed.
    // Well-behaved exporters (e.g. Blender's glTF exporter) already convert axes.
    // So for glTF/GLB we skip axis conversion even if a source app is detected,
    // unless the user explicitly chose a preset (non-Auto).
    bool skipAxisForGltf = (options.sourceApp == SourceApp::Auto);

    if (effectiveOptions.convertAxes && !skipAxisForGltf && resolvedApp != SourceApp::Auto) {
        SourceAppPreset preset = GetSourceAppPreset(resolvedApp);
        effectiveOptions.scale *= preset.scale;
        ENJIN_LOG_INFO(Asset, "Applying %s preset (scale=%.4f, zToY=%d, lToR=%d) for %s",
                       preset.name, effectiveOptions.scale, preset.zUpToYUp, preset.leftToRight,
                       filepath.c_str());
    }

    ImportStats stats;
    stats.sourceFilePath = filepath;

    // Count scene-level totals
    stats.meshCount = static_cast<u32>(scene.meshes.size());
    stats.materialCount = static_cast<u32>(scene.materials.size());

    // Count animations
    result.animationCount = static_cast<u32>(scene.animations.size());

    // Create entities from root nodes
    for (i32 rootIndex : scene.rootNodes) {
        ECS::Entity entity = CreateEntityFromNode(scene, rootIndex, world, effectiveOptions, result.entities, stats);
        if (result.rootEntity == ECS::INVALID_ENTITY) {
            result.rootEntity = entity;
        }
    }

    // --- Mesh Validation (auto-fix NaN, normalize weights, detect degenerate triangles) ---
    ValidateImportedMeshes(world, result, stats.warnings);

    // --- Import Validation Report ---
    // Detect skipped features and log a summary so users know what was lost.
    {
        bool hasVertexColors = false;
        bool hasMultipleUVSets = false;
        bool hasMorphTargets = false;
        u32 totalMorphTargets = 0;

        // Scan glTF data for features we don't fully support
        for (const auto& mesh : scene.meshes) {
            for (const auto& prim : mesh.primitives) {
                for (const auto& v : prim.vertices) {
                    if (v.hasColor) { hasVertexColors = true; break; }
                }
            }
        }

        // Check raw cgltf data for morph targets and extra UV sets (via warning count)
        // We can't access cgltf directly here, so detect by checking if any mesh
        // name suggests morph targets, or just note as unsupported.
        // For now, add a general note about unsupported features.

        if (!stats.texturePathsMissing.empty()) {
            ENJIN_LOG_WARN(Asset, "Import: %zu texture(s) could not be resolved", stats.texturePathsMissing.size());
            for (const auto& tp : stats.texturePathsMissing) {
                ENJIN_LOG_WARN(Asset, "  Missing texture: %s", tp.c_str());
            }
        }

        if (hasVertexColors) {
            stats.warnings.push_back("Vertex colors imported (COLOR_0)");
        }

        // Features not supported — add warnings so users know
        stats.warnings.push_back("Note: Morph targets / blend shapes are not imported (not yet supported)");
        stats.warnings.push_back("Note: Only UV set 0 is imported (additional UV sets are dropped)");
        stats.warnings.push_back("Note: glTF extensions (transmission, clearcoat, sheen) are not imported");

        // Log summary
        ENJIN_LOG_INFO(Asset, "--- Import Report: %s ---", filepath.c_str());
        ENJIN_LOG_INFO(Asset, "  Meshes: %u, Materials: %u, Vertices: %u, Indices: %u",
            stats.meshCount, stats.materialCount, stats.totalVertexCount, stats.totalIndexCount);
        ENJIN_LOG_INFO(Asset, "  Entities created: %zu", result.entities.size());
        ENJIN_LOG_INFO(Asset, "  Textures resolved: %zu, missing: %zu",
            stats.texturePathsResolved.size(), stats.texturePathsMissing.size());
        if (!stats.warnings.empty()) {
            ENJIN_LOG_INFO(Asset, "  Warnings (%zu):", stats.warnings.size());
            for (const auto& w : stats.warnings) {
                ENJIN_LOG_INFO(Asset, "    - %s", w.c_str());
            }
        }
        ENJIN_LOG_INFO(Asset, "--- End Import Report ---");
    }

    CopyStatsToResult(result, stats);
    result.success = true;
    return result;
}

ECS::Entity SceneImporter::CreateEntityFromNode(const GLTFScene& scene, i32 nodeIndex,
                                                  ECS::World* world, const ImportOptions& options,
                                                  std::vector<ECS::Entity>& outEntities,
                                                  ImportStats& stats) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(scene.nodes.size())) {
        return ECS::INVALID_ENTITY;
    }

    // Skip excluded nodes (from import preview deselection)
    for (i32 excluded : options.excludedNodeIndices) {
        if (excluded == nodeIndex) {
            // Still recurse into children so hierarchy isn't broken
            const GLTFNode& skippedNode = scene.nodes[nodeIndex];
            for (i32 childIdx : skippedNode.children) {
                CreateEntityFromNode(scene, childIdx, world, options, outEntities, stats);
            }
            return ECS::INVALID_ENTITY;
        }
    }

    const GLTFNode& node = scene.nodes[nodeIndex];

    // Create entity
    ECS::Entity entity = world->CreateEntity();
    outEntities.push_back(entity);

    // Add name component
    std::string name = node.name.empty() ? "Node_" + std::to_string(nodeIndex) : node.name;
    world->AddComponent<ECS::NameComponent>(entity, name);
    stats.entityNames.push_back(name);

    // Determine axis conversion flags
    // For glTF with Auto source app, skip axis conversion (glTF is already Y-up)
    bool doAxisConvert = options.convertAxes && options.sourceApp != SourceApp::Auto;
    SourceAppPreset preset = GetSourceAppPreset(options.sourceApp);
    bool zToY = doAxisConvert && preset.zUpToYUp;
    bool lToR = doAxisConvert && preset.leftToRight;

    // Add transform component
    auto& transform = world->AddComponent<ECS::TransformComponent>(entity);
    Math::Vector3 pos = node.translation;
    Math::Quaternion rot = node.rotation;
    if (zToY || lToR || options.flipX || options.flipY || options.flipZ) {
        pos = ConvertPosition(pos, zToY, lToR, options.flipX, options.flipY, options.flipZ);
        rot = ConvertRotation(rot, zToY, lToR);
    }
    transform.position = pos * options.scale;
    transform.rotation = rot;
    transform.scale = node.scale * options.scale;

    // Add mesh component if node has a mesh
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<i32>(scene.meshes.size())) {
        const GLTFMesh& gltfMesh = scene.meshes[node.meshIndex];

        // Combine all primitives into one mesh component, tracking sub-mesh
        // boundaries for multi-material support.
        ECS::MeshComponent meshComp;

        u32 vertexOffset = 0;
        std::unordered_map<i32, i32> uniqueMaterials;  // glTF materialIndex -> slot index
        struct GltfSubMesh { u32 indexOffset; u32 indexCount; i32 gltfMaterialIndex; };
        std::vector<GltfSubMesh> gltfSubMeshes;

        for (const auto& primitive : gltfMesh.primitives) {
            u32 subMeshIndexOffset = static_cast<u32>(meshComp.indices.size());

            // Add vertices
            for (const auto& gltfVert : primitive.vertices) {
                ECS::MeshComponent::Vertex vertex;
                vertex.position = gltfVert.position;
                vertex.normal = gltfVert.normal;
                vertex.tangent = gltfVert.tangent;
                // Apply axis conversion to vertex positions, normals, tangents
                if (zToY || lToR) {
                    vertex.position = ConvertNormal(vertex.position, zToY, lToR);
                    vertex.normal = ConvertNormal(vertex.normal, zToY, lToR);
                    Math::Vector3 tang3(vertex.tangent.x, vertex.tangent.y, vertex.tangent.z);
                    tang3 = ConvertNormal(tang3, zToY, lToR);
                    vertex.tangent = Math::Vector4(tang3.x, tang3.y, tang3.z, vertex.tangent.w);
                }
                vertex.uv = gltfVert.texCoord;
                if (gltfVert.hasColor) vertex.color = gltfVert.color;
                vertex.boneWeights = gltfVert.boneWeights;
                vertex.boneIndices[0] = gltfVert.boneIndices[0];
                vertex.boneIndices[1] = gltfVert.boneIndices[1];
                vertex.boneIndices[2] = gltfVert.boneIndices[2];
                vertex.boneIndices[3] = gltfVert.boneIndices[3];
                meshComp.vertices.push_back(vertex);
            }

            // Add indices (offset by current vertex count)
            for (u32 index : primitive.indices) {
                meshComp.indices.push_back(index + vertexOffset);
            }

            u32 subMeshIndexCount = static_cast<u32>(meshComp.indices.size()) - subMeshIndexOffset;

            // Track sub-mesh for this primitive
            i32 primMatIdx = primitive.materialIndex >= 0 ? primitive.materialIndex : 0;
            gltfSubMeshes.push_back({ subMeshIndexOffset, subMeshIndexCount, primMatIdx });
            if (uniqueMaterials.find(primMatIdx) == uniqueMaterials.end()) {
                uniqueMaterials[primMatIdx] = static_cast<i32>(uniqueMaterials.size());
            }

            vertexOffset += static_cast<u32>(primitive.vertices.size());
        }

        if (!meshComp.vertices.empty()) {
            // Accumulate vertex/index stats
            stats.totalVertexCount += static_cast<u32>(meshComp.vertices.size());
            stats.totalIndexCount += static_cast<u32>(meshComp.indices.size());

            // Compute AABB from vertex positions for auto-generated box collider
            Math::Vector3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
            Math::Vector3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            for (const auto& v : meshComp.vertices) {
                if (v.position.x < minBounds.x) minBounds.x = v.position.x;
                if (v.position.y < minBounds.y) minBounds.y = v.position.y;
                if (v.position.z < minBounds.z) minBounds.z = v.position.z;
                if (v.position.x > maxBounds.x) maxBounds.x = v.position.x;
                if (v.position.y > maxBounds.y) maxBounds.y = v.position.y;
                if (v.position.z > maxBounds.z) maxBounds.z = v.position.z;
            }

            ENJIN_LOG_INFO(Asset, "Mesh '%s': %zu vertices, %zu indices, bounds [%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f] (size %.1fx%.1fx%.1f)",
                name.c_str(), meshComp.vertices.size(), meshComp.indices.size(),
                minBounds.x, minBounds.y, minBounds.z,
                maxBounds.x, maxBounds.y, maxBounds.z,
                maxBounds.x - minBounds.x, maxBounds.y - minBounds.y, maxBounds.z - minBounds.z);

            // Build sub-meshes if multiple materials were found
            bool isMultiMaterial = uniqueMaterials.size() > 1;
            if (isMultiMaterial) {
                for (const auto& gsm : gltfSubMeshes) {
                    ECS::MeshComponent::SubMesh sm;
                    sm.indexOffset = gsm.indexOffset;
                    sm.indexCount = gsm.indexCount;
                    sm.materialSlot = uniqueMaterials[gsm.gltfMaterialIndex];
                    meshComp.subMeshes.push_back(sm);
                }
                ENJIN_LOG_INFO(Asset, "  Multi-material mesh: %zu sub-meshes, %zu material slots",
                    meshComp.subMeshes.size(), uniqueMaterials.size());
            }

            world->AddComponent<ECS::MeshComponent>(entity, std::move(meshComp));
// Create MorphTargetComponent if any primitive has morph targets            {                bool hasMorphTargets = false;                for (const auto& prim : mesh.primitives) {                    if (!prim.morphTargets.empty()) { hasMorphTargets = true; break; }                }                if (hasMorphTargets) {                    u32 totalVerts = static_cast<u32>(world->GetComponent<ECS::MeshComponent>(entity)->vertices.size());                    auto& morphComp = world->AddComponent<ECS::MorphTargetComponent>(entity);                    u32 vOff = 0;                    for (const auto& prim : mesh.primitives) {                        for (usize t = 0; t < prim.morphTargets.size(); ++t) {                            const auto& src = prim.morphTargets[t];                            i32 existingIdx = morphComp.FindTarget(src.name);                            if (existingIdx < 0) {                                ECS::MorphTarget tgt;                                tgt.name = src.name;                                tgt.deltas.resize(totalVerts, ECS::MorphTargetDelta{});                                morphComp.targets.push_back(std::move(tgt));                                existingIdx = static_cast<i32>(morphComp.targets.size()) - 1;                            }                            auto& dst = morphComp.targets[existingIdx];                            for (usize v = 0; v < src.positionDeltas.size() && (vOff + v) < dst.deltas.size(); ++v) {                                dst.deltas[vOff + v].positionDelta = src.positionDeltas[v];                                if (v < src.normalDeltas.size()) dst.deltas[vOff + v].normalDelta = src.normalDeltas[v];                            }                        }                        vOff += static_cast<u32>(prim.vertices.size());                    }                    morphComp.weights.resize(morphComp.targets.size(), 0.0f);                    if (!mesh.defaultMorphWeights.empty()) {                        for (usize w = 0; w < morphComp.weights.size() && w < mesh.defaultMorphWeights.size(); ++w)                            morphComp.weights[w] = mesh.defaultMorphWeights[w];                    }                    stats.warnings.push_back("Morph targets imported: " + std::to_string(morphComp.targets.size()) + " targets");                }            }

            // Add box collider from mesh AABB
            if (options.generateColliders) {
                auto& collider = world->AddComponent<ECS::BoxColliderComponent>(entity);
                collider.center = (minBounds + maxBounds) * 0.5f;
                collider.size = maxBounds - minBounds;
            }

            // Resolve texture paths from glTF image URIs with fallback directories
            auto resolveGltfTex = [&](i32 texIdx) -> std::string {
                if (texIdx < 0 || texIdx >= static_cast<i32>(scene.images.size())) return "";
                const std::string& uri = scene.images[texIdx].uri;
                if (uri.empty()) return "";
                namespace fs = std::filesystem;
                fs::path resolved = fs::path(scene.basePath) / uri;
                if (fs::exists(resolved)) {
                    stats.texturePathsResolved.push_back(resolved.string());
                    return resolved.string();
                }
                resolved = fs::path(scene.basePath) / "textures" / fs::path(uri).filename();
                if (fs::exists(resolved)) {
                    stats.texturePathsResolved.push_back(resolved.string());
                    return resolved.string();
                }
                resolved = fs::path(scene.basePath) / "Textures" / fs::path(uri).filename();
                if (fs::exists(resolved)) {
                    stats.texturePathsResolved.push_back(resolved.string());
                    return resolved.string();
                }
                if (!options.textureSearchPaths.empty()) {
                    std::string found = TryTextureSearchPaths(
                        fs::path(uri).filename().string(), options.textureSearchPaths);
                    if (!found.empty()) {
                        stats.texturePathsResolved.push_back(found);
                        return found;
                    }
                }
                stats.texturePathsMissing.push_back(uri);
                return uri;
            };

            // Helper: build MaterialComponent from glTF material
            auto buildGltfMat = [&](const GLTFMaterial& gmat) -> ECS::MaterialComponent {
                ECS::MaterialComponent mat;
                mat.baseColor = Math::Vector3(gmat.baseColorFactor.x, gmat.baseColorFactor.y, gmat.baseColorFactor.z);
                mat.opacity = gmat.baseColorFactor.w;
                mat.metallic = gmat.metallicFactor;
                mat.roughness = gmat.roughnessFactor;
                mat.emissiveColor = gmat.emissiveFactor;
                mat.emissiveStrength = (gmat.emissiveFactor.x + gmat.emissiveFactor.y + gmat.emissiveFactor.z > 0.01f) ? 1.0f : 0.0f;
                mat.doubleSided = gmat.doubleSided;
                mat.alphaCutoff = gmat.alphaCutoff;
                if (gmat.alphaMode == GLTFMaterial::AlphaMode::Mask) {
                    mat.alphaMode = ECS::MaterialComponent::AlphaMode::Mask;
                } else if (gmat.alphaMode == GLTFMaterial::AlphaMode::Blend) {
                    mat.alphaMode = ECS::MaterialComponent::AlphaMode::Blend;
                }
                mat.baseColorTexturePath = resolveGltfTex(gmat.baseColorTextureIndex);
                if (!mat.baseColorTexturePath.empty()) {
                    mat.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
                }
                mat.normalTexturePath = resolveGltfTex(gmat.normalTextureIndex);
                mat.metallicRoughnessTexturePath = resolveGltfTex(gmat.metallicRoughnessTextureIndex);
                mat.emissiveTexturePath = resolveGltfTex(gmat.emissiveTextureIndex);
                return mat;
            };

            if (options.importMaterials) {
                if (isMultiMaterial) {
                    // Multi-material: create MaterialSlotsComponent
                    ECS::MaterialSlotsComponent slotsComp;
                    slotsComp.slots.resize(uniqueMaterials.size());
                    i32 firstMatIdx = -1;

                    for (const auto& [gltfIdx, slotIdx] : uniqueMaterials) {
                        if (gltfIdx >= 0 && gltfIdx < static_cast<i32>(scene.materials.size())) {
                            slotsComp.slots[slotIdx] = buildGltfMat(scene.materials[gltfIdx]);
                            if (firstMatIdx < 0) firstMatIdx = gltfIdx;
                        }
                    }

                    world->AddComponent<ECS::MaterialSlotsComponent>(entity, std::move(slotsComp));

                    // Add primary MaterialComponent from first material for backward compat
                    if (firstMatIdx >= 0 && firstMatIdx < static_cast<i32>(scene.materials.size())) {
                        world->AddComponent<ECS::MaterialComponent>(entity,
                            buildGltfMat(scene.materials[firstMatIdx]));
                    }
                } else {
                    // Single material (original path)
                    i32 matIdx = -1;
                    for (const auto& primitive : gltfMesh.primitives) {
                        if (primitive.materialIndex >= 0) { matIdx = primitive.materialIndex; break; }
                    }
                    if (matIdx >= 0 && matIdx < static_cast<i32>(scene.materials.size())) {
                        world->AddComponent<ECS::MaterialComponent>(entity,
                            buildGltfMat(scene.materials[matIdx]));
                    }
                }
            }

            // Auto-generate LODs for imported meshes with enough geometry.
            // IMPORTANT: AddComponent can reallocate component storage, invalidating
            // any raw pointers from GetComponent. Re-fetch after AddComponent.
            if (options.generateLODs) {
                auto* meshCheck = world->GetComponent<ECS::MeshComponent>(entity);
                if (meshCheck && meshCheck->vertices.size() > 64) {
                    auto& lod = world->AddComponent<ECS::LODComponent>(entity);
                    auto* meshForLOD = world->GetComponent<ECS::MeshComponent>(entity);
                    if (meshForLOD) {
                        Renderer::MeshSimplifier::GenerateLODs(*meshForLOD, lod);
                    }
                }
            }
        }
    }

    // Set up skeleton and animator if this node has a skin (and animations are enabled)
    if (options.importAnimations && node.skinIndex >= 0 && node.skinIndex < static_cast<i32>(scene.skins.size())) {
        const GLTFSkin& skin = scene.skins[node.skinIndex];

        // Build skeleton from skin data
        auto skeleton = std::make_shared<Animation::Skeleton>();
        skeleton->name = skin.name.empty() ? "Skeleton" : skin.name;
        skeleton->bones.resize(skin.jointNodeIndices.size());

        // Build a lookup: node index -> joint index (for finding parent bones)
        std::unordered_map<i32, i32> nodeToJoint;
        for (i32 j = 0; j < static_cast<i32>(skin.jointNodeIndices.size()); ++j) {
            nodeToJoint[skin.jointNodeIndices[j]] = j;
        }

        for (usize j = 0; j < skin.jointNodeIndices.size(); ++j) {
            i32 jointNodeIdx = skin.jointNodeIndices[j];
            Animation::Bone& bone = skeleton->bones[j];

            if (jointNodeIdx >= 0 && jointNodeIdx < static_cast<i32>(scene.nodes.size())) {
                const GLTFNode& jointNode = scene.nodes[jointNodeIdx];
                bone.name = jointNode.name;
                bone.bindPosition = jointNode.translation;
                bone.bindRotation = jointNode.rotation;
                bone.bindScale = jointNode.scale;
            }

            if (j < skin.inverseBindMatrices.size()) {
                bone.inverseBindMatrix = skin.inverseBindMatrices[j];
            }

            // Find parent: walk the glTF node hierarchy to find which joint is the parent
            bone.parentIndex = -1;
            if (jointNodeIdx >= 0) {
                // Search all nodes for one whose children list contains this joint node
                for (usize n = 0; n < scene.nodes.size(); ++n) {
                    const GLTFNode& potentialParent = scene.nodes[n];
                    for (i32 childIdx : potentialParent.children) {
                        if (childIdx == jointNodeIdx) {
                            auto parentIt = nodeToJoint.find(static_cast<i32>(n));
                            if (parentIt != nodeToJoint.end()) {
                                bone.parentIndex = parentIt->second;
                            }
                            break;
                        }
                    }
                    if (bone.parentIndex >= 0) break;
                }
            }
        }

        // Add skeleton component
        auto& skelComp = world->AddComponent<ECS::SkeletonComponent>(entity);
        skelComp.skeleton = skeleton;
        skelComp.sourceAssetPath = stats.sourceFilePath;

        // Add animator component
        auto& animComp = world->AddComponent<ECS::AnimatorComponent>(entity);
        animComp.Initialize(skeleton);

        // Convert glTF animations to skeletal animations
        for (const auto& gltfAnim : scene.animations) {
            Animation::SkeletalAnimation skelAnim;
            skelAnim.name = gltfAnim.name;
            skelAnim.duration = gltfAnim.duration;

            for (const auto& channel : gltfAnim.channels) {
                if (channel.targetNode < 0) continue;

                // Find which bone this channel targets
                auto jointIt = nodeToJoint.find(channel.targetNode);
                if (jointIt == nodeToJoint.end()) continue;
                i32 boneIndex = jointIt->second;

                // Find or create a track for this bone
                Animation::BoneTrack* track = nullptr;
                for (auto& t : skelAnim.tracks) {
                    if (t.boneIndex == boneIndex) {
                        track = &t;
                        break;
                    }
                }
                if (!track) {
                    skelAnim.tracks.push_back({});
                    track = &skelAnim.tracks.back();
                    track->boneIndex = boneIndex;
                    if (boneIndex >= 0 && boneIndex < static_cast<i32>(skeleton->bones.size())) {
                        track->boneName = skeleton->bones[boneIndex].name;
                    }
                }

                switch (channel.path) {
                    case GLTFAnimationChannel::Path::Translation: {
                        usize keyCount = channel.times.size();
                        if (channel.values.size() < keyCount * 3) keyCount = channel.values.size() / 3;
                        track->positionTimes.assign(channel.times.begin(), channel.times.begin() + keyCount);
                        track->positions.resize(keyCount);
                        for (usize k = 0; k < keyCount; ++k) {
                            Math::Vector3 p(channel.values[k * 3 + 0],
                                            channel.values[k * 3 + 1],
                                            channel.values[k * 3 + 2]);
                            if (zToY || lToR || options.flipX || options.flipY || options.flipZ) {
                                p = ConvertPosition(p, zToY, lToR, options.flipX, options.flipY, options.flipZ);
                            }
                            track->positions[k] = p;
                        }
                        break;
                    }
                    case GLTFAnimationChannel::Path::Rotation: {
                        usize keyCount = channel.times.size();
                        if (channel.values.size() < keyCount * 4) keyCount = channel.values.size() / 4;
                        track->rotationTimes.assign(channel.times.begin(), channel.times.begin() + keyCount);
                        track->rotations.resize(keyCount);
                        for (usize k = 0; k < keyCount; ++k) {
                            Math::Quaternion q(channel.values[k * 4 + 0],
                                               channel.values[k * 4 + 1],
                                               channel.values[k * 4 + 2],
                                               channel.values[k * 4 + 3]);
                            if (zToY || lToR) {
                                q = ConvertRotation(q, zToY, lToR);
                            }
                            track->rotations[k] = q;
                        }
                        break;
                    }
                    case GLTFAnimationChannel::Path::Scale: {
                        usize keyCount = channel.times.size();
                        if (channel.values.size() < keyCount * 3) keyCount = channel.values.size() / 3;
                        track->scaleTimes.assign(channel.times.begin(), channel.times.begin() + keyCount);
                        track->scales.resize(keyCount);
                        for (usize k = 0; k < keyCount; ++k) {
                            track->scales[k] = Math::Vector3(
                                channel.values[k * 3 + 0],
                                channel.values[k * 3 + 1],
                                channel.values[k * 3 + 2]);
                        }
                        break;
                    }
                    case GLTFAnimationChannel::Path::Weights: {
                        // Morph target weight animation — one weight per target per keyframe
                        // Determine number of targets from value count / key count
                        usize keyCount = channel.times.size();
                        if (keyCount == 0) break;
                        usize targetCount = channel.values.size() / keyCount;
                        if (targetCount == 0) break;

                        // Create one MorphWeightTrack per target
                        for (usize t = 0; t < targetCount; ++t) {
                            Animation::SkeletalAnimation::MorphWeightTrack mwt;
                            mwt.targetIndex = static_cast<i32>(t);
                            mwt.targetName = "target_" + std::to_string(t);
                            mwt.times = channel.times;
                            mwt.weights.resize(keyCount);
                            for (usize k = 0; k < keyCount; ++k) {
                                mwt.weights[k] = channel.values[k * targetCount + t];
                            }
                            skelAnim.morphTracks.push_back(std::move(mwt));
                        }
                        break;
                    }
                }
            }

            if (!skelAnim.tracks.empty() || !skelAnim.morphTracks.empty()) {
                animComp.animator.AddAnimation(skelAnim);
            }
        }

        // Auto-play first animation
        if (!scene.animations.empty()) {
            animComp.animator.Play(scene.animations[0].name);
            ENJIN_LOG_INFO(Asset, "Auto-playing animation '%s' on entity '%s'",
                scene.animations[0].name.c_str(), node.name.c_str());
        }
    }

    // Recursively create child entities with parent-child hierarchy
    for (i32 childIndex : node.children) {
        ECS::Entity childEntity = CreateEntityFromNode(scene, childIndex, world, options, outEntities, stats);
        if (childEntity != ECS::INVALID_ENTITY) {
            ECS::SetParent(world, childEntity, entity);
        }
    }

    return entity;
}

ImportResult SceneImporter::ImportAssimp(const std::string& filepath, ECS::World* world,
                                          const ImportOptions& options) {
    ImportResult result;

    if (!world) {
        result.errorMessage = "World is null";
        return result;
    }

    // Load the file using Assimp
    AssimpScene scene;
    if (!AssimpLoader::Load(filepath, scene)) {
        result.errorMessage = AssimpLoader::GetLastError();
        return result;
    }

    // Resolve source app and axis conversion
    ImportOptions effectiveOptions = options;
    SourceApp resolvedApp = options.sourceApp;
    if (resolvedApp == SourceApp::Auto) {
        resolvedApp = DetectSourceApp("", scene.creator);
        effectiveOptions.sourceApp = resolvedApp;
    }

    if (effectiveOptions.convertAxes && resolvedApp != SourceApp::Auto) {
        SourceAppPreset preset = GetSourceAppPreset(resolvedApp);
        effectiveOptions.scale *= preset.scale;
        ENJIN_LOG_INFO(Asset, "Applying %s preset (scale=%.4f, zToY=%d, lToR=%d) for %s",
                       preset.name, effectiveOptions.scale, preset.zUpToYUp, preset.leftToRight,
                       filepath.c_str());
    }

    ImportStats stats;
    stats.sourceFilePath = filepath;

    // Count scene-level totals
    stats.meshCount = static_cast<u32>(scene.meshes.size());
    stats.materialCount = static_cast<u32>(scene.materials.size());
    result.animationCount = static_cast<u32>(scene.animations.size());

    // Build skeleton context if the scene has skinning data
    AssimpSkeletonContext skelCtx;
    if (scene.hasSkinning && effectiveOptions.importAnimations && !scene.bones.empty()) {
        // Determine axis conversion flags
        bool doAxisConvert = effectiveOptions.convertAxes && effectiveOptions.sourceApp != SourceApp::Auto;
        SourceAppPreset preset = GetSourceAppPreset(effectiveOptions.sourceApp);
        bool zToY = doAxisConvert && preset.zUpToYUp;
        bool lToR = doAxisConvert && preset.leftToRight;

        auto skeleton = std::make_shared<Animation::Skeleton>();
        skeleton->name = "Skeleton";
        skeleton->bones.resize(scene.bones.size());

        // Build a lookup: bone name -> node index
        std::unordered_map<std::string, i32> boneNameToNodeIdx;
        for (i32 n = 0; n < static_cast<i32>(scene.nodes.size()); ++n) {
            boneNameToNodeIdx[scene.nodes[n].name] = n;
        }

        // Build a lookup: bone name -> bone index
        std::unordered_map<std::string, i32> boneNameToBoneIdx;
        for (i32 b = 0; b < static_cast<i32>(scene.bones.size()); ++b) {
            boneNameToBoneIdx[scene.bones[b].name] = b;
        }

        for (usize b = 0; b < scene.bones.size(); ++b) {
            Animation::Bone& bone = skeleton->bones[b];
            bone.name = scene.bones[b].name;
            bone.inverseBindMatrix = scene.bones[b].offsetMatrix;

            // Find matching node for bind pose
            auto nodeIt = boneNameToNodeIdx.find(bone.name);
            if (nodeIt != boneNameToNodeIdx.end()) {
                i32 nodeIdx = nodeIt->second;
                const AssimpNode& boneNode = scene.nodes[nodeIdx];
                bone.bindPosition = boneNode.translation;
                bone.bindRotation = boneNode.rotation;
                bone.bindScale = boneNode.scale;

                // Apply axis conversion to bind pose AND inverse bind matrix.
                // The inverse bind matrix must be converted to the same coordinate
                // system as the bind pose, otherwise skinning matrices will mix
                // coordinate systems (e.g., Z-up IBMs with Y-up world transforms).
                if (zToY || lToR || effectiveOptions.flipX || effectiveOptions.flipY || effectiveOptions.flipZ) {
                    bone.bindPosition = ConvertPosition(bone.bindPosition, zToY, lToR,
                        effectiveOptions.flipX, effectiveOptions.flipY, effectiveOptions.flipZ);
                    bone.bindRotation = ConvertRotation(bone.bindRotation, zToY, lToR);

                    // Convert inverse bind matrix: IBM_new = C * IBM * C^(-1)
                    // where C is the axis conversion matrix.
                    // For orthogonal C, C^(-1) = C^T.
                    Math::Matrix4 C = Math::Matrix4::Identity();
                    Math::Matrix4 Cinv = Math::Matrix4::Identity();
                    if (zToY) {
                        // Z-up -> Y-up: (x,y,z) -> (x,z,-y)
                        // Row 0: x' = x  → [1, 0, 0, 0]
                        // Row 1: y' = z  → [0, 0, 1, 0]
                        // Row 2: z' = -y → [0,-1, 0, 0]
                        // C^-1 reverses: (x,y,z) -> (x,-z,y)
                        C = Math::Matrix4(
                            1, 0, 0, 0,
                            0, 0, 1, 0,
                            0,-1, 0, 0,
                            0, 0, 0, 1);
                        Cinv = Math::Matrix4(
                            1, 0, 0, 0,
                            0, 0,-1, 0,
                            0, 1, 0, 0,
                            0, 0, 0, 1);
                    }
                    if (lToR) {
                        // Left->Right: negate X
                        Math::Matrix4 Clr = Math::Matrix4(
                           -1, 0, 0, 0,
                            0, 1, 0, 0,
                            0, 0, 1, 0,
                            0, 0, 0, 1);
                        C = Clr * C;
                        Cinv = Cinv * Clr; // Clr is self-inverse
                    }
                    bone.inverseBindMatrix = C * bone.inverseBindMatrix * Cinv;
                }

                // Find parent bone: walk up the node's parentIndex chain
                bone.parentIndex = -1;
                i32 parentNodeIdx = boneNode.parentIndex;
                while (parentNodeIdx >= 0) {
                    auto parentBoneIt = boneNameToBoneIdx.find(scene.nodes[parentNodeIdx].name);
                    if (parentBoneIt != boneNameToBoneIdx.end()) {
                        bone.parentIndex = parentBoneIt->second;
                        break;
                    }
                    parentNodeIdx = scene.nodes[parentNodeIdx].parentIndex;
                }
            }
        }

        // Topological sort: ensure parent bones always have lower indices than
        // their children. CalculateWorldTransforms iterates bones by index and
        // reads worldTransforms[parentIndex] — if a parent hasn't been processed
        // yet, the child reads uninitialized data (garbage transforms).
        // Also build a remapping table so vertex bone indices stay valid.
        {
            std::vector<i32> sortedOrder;
            sortedOrder.reserve(skeleton->bones.size());
            std::vector<bool> placed(skeleton->bones.size(), false);

            // Iteratively place bones whose parents are already placed (or root)
            bool progress = true;
            while (progress && sortedOrder.size() < skeleton->bones.size()) {
                progress = false;
                for (usize i = 0; i < skeleton->bones.size(); ++i) {
                    if (placed[i]) continue;
                    i32 parent = skeleton->bones[i].parentIndex;
                    if (parent < 0 || placed[parent]) {
                        sortedOrder.push_back(static_cast<i32>(i));
                        placed[i] = true;
                        progress = true;
                    }
                }
            }
            // If any bones remain (cycles), append them
            for (usize i = 0; i < skeleton->bones.size(); ++i) {
                if (!placed[i]) sortedOrder.push_back(static_cast<i32>(i));
            }

            // Check if reordering is actually needed
            bool needsReorder = false;
            for (usize i = 0; i < sortedOrder.size(); ++i) {
                if (sortedOrder[i] != static_cast<i32>(i)) { needsReorder = true; break; }
            }

            if (needsReorder) {
                // Build old->new index mapping
                std::vector<i32> oldToNew(skeleton->bones.size());
                for (usize i = 0; i < sortedOrder.size(); ++i) {
                    oldToNew[sortedOrder[i]] = static_cast<i32>(i);
                }

                // Reorder bones
                std::vector<Animation::Bone> reordered(skeleton->bones.size());
                for (usize i = 0; i < sortedOrder.size(); ++i) {
                    reordered[i] = skeleton->bones[sortedOrder[i]];
                    // Remap parent index
                    if (reordered[i].parentIndex >= 0) {
                        reordered[i].parentIndex = oldToNew[reordered[i].parentIndex];
                    }
                }
                skeleton->bones = std::move(reordered);

                // Update boneNameToBoneIdx for animation track lookup
                boneNameToBoneIdx.clear();
                for (i32 b = 0; b < static_cast<i32>(skeleton->bones.size()); ++b) {
                    boneNameToBoneIdx[skeleton->bones[b].name] = b;
                }

                // Remap vertex bone indices in ALL meshes
                for (auto& mesh : scene.meshes) {
                    for (auto& prim : mesh.primitives) {
                        for (auto& vert : prim.vertices) {
                            f32* weights = &vert.boneWeights.x;
                            for (int s = 0; s < 4; ++s) {
                                if (weights[s] > 0.0f && vert.boneIndices[s] < oldToNew.size()) {
                                    vert.boneIndices[s] = static_cast<u32>(oldToNew[vert.boneIndices[s]]);
                                }
                            }
                        }
                    }
                }

                ENJIN_LOG_INFO(Asset, "Reordered %zu bones for parent-before-child traversal",
                    skeleton->bones.size());
            }
        }

        skelCtx.skeleton = skeleton;

        ENJIN_LOG_INFO(Asset, "Built skeleton: %zu bones, %zu animations from %s",
            scene.bones.size(), scene.animations.size(), filepath.c_str());

    }

    // Create a single root entity named after the file to keep the hierarchy clean.
    // All imported mesh entities are nested under this root.
    std::filesystem::path importPath(filepath);
    std::string rootName = importPath.stem().string();
    ECS::Entity importRoot = world->CreateEntity();
    world->AddComponent<ECS::NameComponent>(importRoot, rootName);
    auto& rootTransform = world->AddComponent<ECS::TransformComponent>(importRoot);

    // Auto-detect FBX unit scale from mesh bounding box.
    // Mixamo/Maya FBX uses cm (170cm character), engine grid ≈ 1 unit per meter.
    // Scale so the tallest dimension maps to ~1.8 units (matches default capsule controller height).
    f32 unitScale = effectiveOptions.scale;
    Math::Vector3 boundsMin(FLT_MAX, FLT_MAX, FLT_MAX);
    Math::Vector3 boundsMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // For SKINNED meshes, the entity build path bakes the mesh node's accumulated
    // world transform into vertices, so the post-bake scale matches the bone bind
    // data (cm). Use the BONE world bind positions for the auto-scale check —
    // they're already in the post-bake unit and represent the actual character
    // size, regardless of which mesh-local space the raw FBX vertices were in.
    if (skelCtx.skeleton && !skelCtx.skeleton->bones.empty()) {
        // Compute world bind transforms by walking parent chain (parents always
        // come before children thanks to the topological sort step).
        std::vector<Math::Vector3> boneWorldPos(skelCtx.skeleton->bones.size());
        for (usize i = 0; i < skelCtx.skeleton->bones.size(); ++i) {
            const auto& b = skelCtx.skeleton->bones[i];
            Math::Vector3 localPos = b.bindPosition;
            if (b.parentIndex >= 0 && b.parentIndex < static_cast<i32>(i)) {
                // World pos = parent.world + parent.rotation * local (approximate;
                // good enough for size detection without full matrix math)
                boneWorldPos[i] = boneWorldPos[b.parentIndex] + localPos;
            } else {
                boneWorldPos[i] = localPos;
            }
            boundsMin.x = Math::Min(boundsMin.x, boneWorldPos[i].x);
            boundsMin.y = Math::Min(boundsMin.y, boneWorldPos[i].y);
            boundsMin.z = Math::Min(boundsMin.z, boneWorldPos[i].z);
            boundsMax.x = Math::Max(boundsMax.x, boneWorldPos[i].x);
            boundsMax.y = Math::Max(boundsMax.y, boneWorldPos[i].y);
            boundsMax.z = Math::Max(boundsMax.z, boneWorldPos[i].z);
        }
    } else {
        for (const auto& mesh : scene.meshes) {
            for (const auto& prim : mesh.primitives) {
                for (const auto& v : prim.vertices) {
                    boundsMin.x = Math::Min(boundsMin.x, v.position.x);
                    boundsMin.y = Math::Min(boundsMin.y, v.position.y);
                    boundsMin.z = Math::Min(boundsMin.z, v.position.z);
                    boundsMax.x = Math::Max(boundsMax.x, v.position.x);
                    boundsMax.y = Math::Max(boundsMax.y, v.position.y);
                    boundsMax.z = Math::Max(boundsMax.z, v.position.z);
                }
            }
        }
    }
    f32 height = boundsMax.y - boundsMin.y; // Y-up height
    if (height < 1.0f) height = boundsMax.z - boundsMin.z; // Try Z-up
    if (height > 10.0f) {
        // Target ~1.8 units (standard capsule/character controller height).
        unitScale *= 1.8f / height;
        ENJIN_LOG_INFO(Asset, "FBX auto-scale: height=%.1f, bounds=[%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f], scale=%.4f",
            height, boundsMin.x, boundsMin.y, boundsMin.z, boundsMax.x, boundsMax.y, boundsMax.z, unitScale);
    }
    rootTransform.scale = Math::Vector3(unitScale, unitScale, unitScale);
    skelCtx.unitScale = unitScale; // Pass to skinned mesh entities
    result.entities.push_back(importRoot);
    result.rootEntity = importRoot;

    // Create entities from root nodes, parented under the import root
    for (i32 rootIndex : scene.rootNodes) {
        ECS::Entity entity = CreateEntityFromAssimpNode(scene, rootIndex, world, effectiveOptions,
                                                         result.entities, stats, skelCtx);
        if (entity != ECS::INVALID_ENTITY) {
            ECS::SetParent(world, entity, importRoot);
        }
    }

    // --- Mesh Validation (auto-fix NaN, normalize weights, detect degenerate triangles) ---
    ValidateImportedMeshes(world, result, stats.warnings);

    // --- Import Validation Report (Assimp) ---
    {
        if (!stats.texturePathsMissing.empty()) {
            ENJIN_LOG_WARN(Asset, "Import: %zu texture(s) could not be resolved", stats.texturePathsMissing.size());
            for (const auto& tp : stats.texturePathsMissing) {
                ENJIN_LOG_WARN(Asset, "  Missing texture: %s", tp.c_str());
            }
        }

        bool hasVertexColors = false;
        for (const auto& mesh : scene.meshes) {
            for (const auto& prim : mesh.primitives) {
                for (const auto& v : prim.vertices) {
                    if (v.hasColor) { hasVertexColors = true; break; }
                }
                if (hasVertexColors) break;
            }
            if (hasVertexColors) break;
        }
        if (hasVertexColors) {
            stats.warnings.push_back("Vertex colors imported");
        }

        if (scene.hasSkinning) {
            stats.warnings.push_back("Skeletal mesh: " + std::to_string(scene.bones.size()) + " bones, " +
                                      std::to_string(scene.animations.size()) + " animations");
        }

        stats.warnings.push_back("Note: Morph targets / blend shapes are not imported (not yet supported)");
        stats.warnings.push_back("Note: Only UV set 0 is imported (additional UV sets are dropped)");

        // Log summary
        ENJIN_LOG_INFO(Asset, "--- Import Report: %s ---", filepath.c_str());
        ENJIN_LOG_INFO(Asset, "  Source: %s | Format: Assimp", scene.creator.empty() ? "Unknown" : scene.creator.c_str());
        ENJIN_LOG_INFO(Asset, "  Meshes: %u, Materials: %u, Vertices: %u, Indices: %u",
            stats.meshCount, stats.materialCount, stats.totalVertexCount, stats.totalIndexCount);
        ENJIN_LOG_INFO(Asset, "  Entities created: %zu", result.entities.size());
        ENJIN_LOG_INFO(Asset, "  Textures resolved: %zu, missing: %zu",
            stats.texturePathsResolved.size(), stats.texturePathsMissing.size());
        if (!stats.warnings.empty()) {
            ENJIN_LOG_INFO(Asset, "  Warnings (%zu):", stats.warnings.size());
            for (const auto& w : stats.warnings) {
                ENJIN_LOG_INFO(Asset, "    - %s", w.c_str());
            }
        }
        ENJIN_LOG_INFO(Asset, "--- End Import Report ---");
    }

    CopyStatsToResult(result, stats);
    result.success = true;
    return result;
}

ImportResult SceneImporter::Import(const std::string& filepath, ECS::World* world,
                                   const ImportOptions& options) {
    std::filesystem::path path(filepath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Use native glTF loader for glTF files
    if (ext == ".gltf" || ext == ".glb") {
        return ImportGLTF(filepath, world, options);
    }

    // Resolve axis conversion for simple formats (PLY, VOX)
    bool doAxisConvert = options.convertAxes && options.sourceApp != SourceApp::Auto;
    SourceAppPreset simplePreset = GetSourceAppPreset(options.sourceApp);
    bool simpleZToY = doAxisConvert && simplePreset.zUpToYUp;
    bool simpleLToR = doAxisConvert && simplePreset.leftToRight;
    f32 effectiveScale = options.scale;
    if (doAxisConvert) {
        effectiveScale *= simplePreset.scale;
    }

    // PLY point cloud/mesh format
    if (ext == ".ply") {
        ImportResult result;
        PLYMesh plyMesh;
        if (!PLYLoader::Load(filepath, plyMesh)) {
            result.errorMessage = "PLY load failed: " + PLYLoader::GetLastError();
            return result;
        }
        ECS::Entity entity = world->CreateEntity();
        world->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{path.stem().string()});
        ECS::TransformComponent transform;
        transform.scale = Math::Vector3(effectiveScale, effectiveScale, effectiveScale);
        world->AddComponent<ECS::TransformComponent>(entity, transform);
        ECS::MeshComponent mesh;
        mesh.vertices.reserve(plyMesh.vertices.size());
        for (const auto& v : plyMesh.vertices) {
            ECS::MeshComponent::Vertex vert;
            vert.position = v.position;
            vert.normal = v.hasNormal ? v.normal : Math::Vector3(0, 1, 0);
            if (simpleZToY || simpleLToR) {
                vert.position = ConvertNormal(vert.position, simpleZToY, simpleLToR);
                vert.normal = ConvertNormal(vert.normal, simpleZToY, simpleLToR);
            }
            vert.uv = v.hasTexCoord ? v.texCoord : Math::Vector2(0, 0);
            vert.color = v.hasColor ? Math::Vector4(v.color.x, v.color.y, v.color.z, 1.0f) : Math::Vector4(1, 1, 1, 1);
            mesh.vertices.push_back(vert);
        }
        mesh.indices = plyMesh.indices;
        world->AddComponent<ECS::MeshComponent>(entity, std::move(mesh));
        if (options.importMaterials) {
            world->AddComponent<ECS::MaterialComponent>(entity, ECS::MaterialComponent{});
        }
        result.success = true;
        result.rootEntity = entity;
        result.entities.push_back(entity);
        result.meshCount = 1;
        ENJIN_LOG_INFO(Asset, "Imported PLY: %zu vertices, %zu indices",
                       plyMesh.vertices.size(), plyMesh.indices.size());
        return result;
    }

    // MagicaVoxel VOX format
    if (ext == ".vox") {
        ImportResult result;
        VOXMesh voxMesh;
        if (!VOXLoader::LoadAsMesh(filepath, voxMesh)) {
            result.errorMessage = "VOX load failed: " + VOXLoader::GetLastError();
            return result;
        }
        ECS::Entity entity = world->CreateEntity();
        world->AddComponent<ECS::NameComponent>(entity, ECS::NameComponent{path.stem().string()});
        ECS::TransformComponent transform;
        transform.scale = Math::Vector3(effectiveScale, effectiveScale, effectiveScale);
        world->AddComponent<ECS::TransformComponent>(entity, transform);
        ECS::MeshComponent mesh;
        mesh.vertices.reserve(voxMesh.vertices.size());
        for (const auto& v : voxMesh.vertices) {
            ECS::MeshComponent::Vertex vert;
            vert.position = v.position;
            vert.normal = v.normal;
            if (simpleZToY || simpleLToR) {
                vert.position = ConvertNormal(vert.position, simpleZToY, simpleLToR);
                vert.normal = ConvertNormal(vert.normal, simpleZToY, simpleLToR);
            }
            vert.color = Math::Vector4(v.color.x, v.color.y, v.color.z, 1.0f);
            mesh.vertices.push_back(vert);
        }
        mesh.indices = voxMesh.indices;
        world->AddComponent<ECS::MeshComponent>(entity, std::move(mesh));
        if (options.importMaterials) {
            world->AddComponent<ECS::MaterialComponent>(entity, ECS::MaterialComponent{});
        }
        result.success = true;
        result.rootEntity = entity;
        result.entities.push_back(entity);
        result.meshCount = 1;
        ENJIN_LOG_INFO(Asset, "Imported VOX: %zu vertices, %zu indices",
                       voxMesh.vertices.size(), voxMesh.indices.size());
        return result;
    }

    // Use Assimp for all other supported formats
    if (AssimpLoader::IsSupported(filepath)) {
        return ImportAssimp(filepath, world, options);
    }

    ImportResult result;
    result.errorMessage = "Unsupported file format: " + ext;
    return result;
}

// Backward-compatible overload (no skeleton context)
ECS::Entity SceneImporter::CreateEntityFromAssimpNode(const AssimpScene& scene, i32 nodeIndex,
                                                       ECS::World* world, const ImportOptions& options,
                                                       std::vector<ECS::Entity>& outEntities,
                                                       ImportStats& stats) {
    AssimpSkeletonContext skelCtx;
    return CreateEntityFromAssimpNode(scene, nodeIndex, world, options, outEntities, stats, skelCtx);
}

ECS::Entity SceneImporter::CreateEntityFromAssimpNode(const AssimpScene& scene, i32 nodeIndex,
                                                       ECS::World* world, const ImportOptions& options,
                                                       std::vector<ECS::Entity>& outEntities,
                                                       ImportStats& stats,
                                                       AssimpSkeletonContext& skelCtx) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(scene.nodes.size())) {
        return ECS::INVALID_ENTITY;
    }

    // Skip excluded nodes (from import preview deselection)
    for (i32 excluded : options.excludedNodeIndices) {
        if (excluded == nodeIndex) {
            const AssimpNode& skippedNode = scene.nodes[nodeIndex];
            for (i32 childIdx : skippedNode.children) {
                CreateEntityFromAssimpNode(scene, childIdx, world, options, outEntities, stats, skelCtx);
            }
            return ECS::INVALID_ENTITY;
        }
    }

    const AssimpNode& node = scene.nodes[nodeIndex];

    // Skip nodes that shouldn't become scene entities:
    // 1. Assimp FBX helper nodes ($AssimpFbx$_Translation, _PreRotation, etc.)
    // 2. Bone nodes in skinned models — bones are internal skeleton data, not scene entities.
    //    Only mesh-bearing nodes and the root node need entities.
    bool isAssimpHelper = node.name.find("$AssimpFbx$") != std::string::npos;
    bool isBoneNode = false;
    if (skelCtx.skeleton && !node.name.empty()) {
        isBoneNode = skelCtx.skeleton->FindBoneIndex(node.name) >= 0;
    }
    // Keep the node if it has meshes (even if it's also a bone)
    bool hasMeshes = !node.meshIndices.empty() || node.meshIndex >= 0;
    // Skip nodes that shouldn't become entities:
    // - Assimp helper nodes ($AssimpFbx$)
    // - Bone nodes without meshes
    // - For skinned models: ALL nodes without meshes (end effectors, empty transforms)
    bool skipNode = isAssimpHelper || (isBoneNode && !hasMeshes);
    if (!skipNode && skelCtx.skeleton && !hasMeshes) {
        skipNode = true; // Skip empty nodes in skinned models
    }
    if (skipNode) {
        // Don't create an entity — just recurse into children
        for (i32 childIdx : node.children) {
            CreateEntityFromAssimpNode(scene, childIdx, world, options, outEntities, stats, skelCtx);
        }
        return ECS::INVALID_ENTITY;
    }

    // For skinned models: each mesh part becomes its own entity (Body, Eyes,
    // Clothing, etc.) with its own material. This lets users select, hide, and
    // re-material individual parts. Bone-only nodes are already filtered above.
    // All mesh entities share the same skeleton and animator.

    // Create entity
    ECS::Entity entity = world->CreateEntity();
    outEntities.push_back(entity);

    // Add name component — clean up common DCC prefixes for readability
    std::string name = node.name.empty() ? "Node_" + std::to_string(nodeIndex) : node.name;
    const char* cleanPrefixes[] = { "mixamorig:", "mixamorig_", "Armature|" };
    for (const char* prefix : cleanPrefixes) {
        if (name.find(prefix) == 0) {
            name = name.substr(std::strlen(prefix));
            break;
        }
    }
    world->AddComponent<ECS::NameComponent>(entity, name);
    stats.entityNames.push_back(name);

    // Determine axis conversion flags
    bool doAxisConvert = options.convertAxes && options.sourceApp != SourceApp::Auto;
    SourceAppPreset preset = GetSourceAppPreset(options.sourceApp);
    bool zToY = doAxisConvert && preset.zUpToYUp;
    bool lToR = doAxisConvert && preset.leftToRight;

    // Add transform component
    auto& transform = world->AddComponent<ECS::TransformComponent>(entity);
    Math::Vector3 pos = node.translation;
    Math::Quaternion rot = node.rotation;
    if (zToY || lToR || options.flipX || options.flipY || options.flipZ) {
        pos = ConvertPosition(pos, zToY, lToR, options.flipX, options.flipY, options.flipZ);
        rot = ConvertRotation(rot, zToY, lToR);
    }
    // Scale is applied on the import root entity, so children use
    // their node-local transforms without additional scale multiplication.
    if (skelCtx.skeleton && hasMeshes) {
        // Skinned mesh: zero position/rotation (skinning handles placement),
        // apply auto-computed scale directly (cm→m) so it works regardless
        // of parent-child hierarchy propagation through ComputeWorldMatrix.
        transform.position = Math::Vector3(0.0f);
        transform.rotation = Math::Quaternion::Identity();
        transform.scale = Math::Vector3(skelCtx.unitScale, skelCtx.unitScale, skelCtx.unitScale);
    } else {
        transform.position = pos;
        transform.rotation = rot;
        transform.scale = node.scale;
    }

    // Add mesh component if node has meshes
    // Combine all meshes referenced by this node into one MeshComponent
    const auto& meshIndices = node.meshIndices.empty()
        ? (node.meshIndex >= 0 ? std::vector<i32>{node.meshIndex} : std::vector<i32>{})
        : node.meshIndices;

    bool nodeHasSkinning = false;
    if (!meshIndices.empty()) {
        ECS::MeshComponent meshComp;

        // For SKINNED meshes: compute the accumulated world transform of THIS mesh node
        // and bake it into vertex positions. Bone bind data + IBMs come from the FBX in
        // a single common space (typically cm), but mesh vertices are in mesh-local
        // space — for files where the mesh node has a non-identity transform (e.g. a
        // 100x scale because the mesh was authored in meters), the vertices end up in
        // a different unit than the bones, and the GPU skinning produces a stretched
        // mess. Walking happens to work because its mesh node is identity; "Female
        // Laying Pose" / "Laying Idle" has mesh scale baked into the node transform.
        Math::Matrix4 meshNodeWorld = Math::Matrix4::Identity();
        bool bakeMeshNodeIntoVertices = (skelCtx.skeleton != nullptr);
        if (bakeMeshNodeIntoVertices) {
            // Walk parent chain from THIS node up to the root, accumulating local
            // transforms. The result is the node's world transform in FBX space.
            std::vector<i32> chain;
            i32 walk = nodeIndex;
            while (walk >= 0) {
                chain.push_back(walk);
                walk = scene.nodes[walk].parentIndex;
            }
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                const auto& n = scene.nodes[*it];
                Math::Matrix4 local = Math::Matrix4::Translation(n.translation) *
                                      n.rotation.ToMatrix() *
                                      Math::Matrix4::Scale(n.scale);
                meshNodeWorld = meshNodeWorld * local;
            }
        }

        u32 vertexOffset = 0;
        i32 materialIndex = -1;  // First material (backward compat for single-material)

        // Track per-primitive sub-mesh data for multi-material support.
        // Each primitive gets its own SubMesh entry; we'll deduplicate material
        // slots and remap after the merge loop.
        struct PrimSubMesh {
            u32 indexOffset;
            u32 indexCount;
            i32 assimpMaterialIndex;
            std::string meshName;
        };
        std::vector<PrimSubMesh> primSubMeshes;
        std::unordered_map<i32, i32> uniqueMaterials;  // assimpMaterialIndex -> slot index

        for (i32 meshIdx : meshIndices) {
            if (meshIdx < 0 || meshIdx >= static_cast<i32>(scene.meshes.size())) continue;
            const AssimpMesh& assimpMesh = scene.meshes[meshIdx];

            for (const auto& primitive : assimpMesh.primitives) {
                // Track first material index (backward compat)
                if (materialIndex < 0 && primitive.materialIndex >= 0) {
                    materialIndex = primitive.materialIndex;
                }

                // Record sub-mesh boundary before adding indices
                u32 subMeshIndexOffset = static_cast<u32>(meshComp.indices.size());

                // Add vertices
                for (const auto& assimpVert : primitive.vertices) {
                    ECS::MeshComponent::Vertex vertex;
                    vertex.position = assimpVert.position;
                    vertex.normal = assimpVert.normal;
                    // For skinned meshes, bake the mesh node's accumulated world
                    // transform into the vertex position so it ends up in the SAME
                    // coordinate space as the bone bind data (see meshNodeWorld above).
                    if (bakeMeshNodeIntoVertices) {
                        Math::Vector4 p4 = meshNodeWorld * Math::Vector4(vertex.position.x, vertex.position.y, vertex.position.z, 1.0f);
                        vertex.position = Math::Vector3(p4.x, p4.y, p4.z);
                        Math::Vector4 n4 = meshNodeWorld * Math::Vector4(vertex.normal.x, vertex.normal.y, vertex.normal.z, 0.0f);
                        vertex.normal = Math::Vector3(n4.x, n4.y, n4.z).Normalized();
                    }
                    // Apply axis conversion to vertex positions, normals, tangents.
                    // Positions use ConvertPosition (includes flip overrides),
                    // normals/tangents use ConvertNormal (direction-only, no scale).
                    // Check flip flags independently — user may set them even when
                    // source app is Auto (no zToY/lToR auto-detection).
                    if (zToY || lToR || options.flipX || options.flipY || options.flipZ) {
                        vertex.position = ConvertPosition(vertex.position, zToY, lToR, options.flipX, options.flipY, options.flipZ);
                        vertex.normal = ConvertNormal(vertex.normal, zToY, lToR);
                        Math::Vector3 tang3(assimpVert.tangent.x, assimpVert.tangent.y, assimpVert.tangent.z);
                        tang3 = ConvertNormal(tang3, zToY, lToR);
                        vertex.tangent = Math::Vector4(tang3.x, tang3.y, tang3.z, assimpVert.tangent.w);
                    }
                    vertex.uv = assimpVert.texCoord;
                    if (assimpVert.hasColor) vertex.color = assimpVert.color;
                    // Copy bone weights from Assimp vertex data
                    vertex.boneWeights = assimpVert.boneWeights;
                    vertex.boneIndices[0] = assimpVert.boneIndices[0];
                    vertex.boneIndices[1] = assimpVert.boneIndices[1];
                    vertex.boneIndices[2] = assimpVert.boneIndices[2];
                    vertex.boneIndices[3] = assimpVert.boneIndices[3];
                    // Track if any vertex has bone weights
                    if (assimpVert.boneWeights.x > 0.0f) nodeHasSkinning = true;
                    meshComp.vertices.push_back(vertex);
                }

                // Add indices (offset by current vertex count).
                // When handedness conversion flips one axis, triangle winding
                // must be reversed to prevent inside-out rendering.
                bool reverseWinding = lToR;  // Left-to-right flips X → reverses winding
                if (options.flipX != options.flipY != options.flipZ) {
                    reverseWinding = !reverseWinding; // Odd number of flips also reverses
                }
                if (reverseWinding) {
                    // Swap second and third vertex of each triangle
                    for (usize idx = 0; idx + 2 < primitive.indices.size(); idx += 3) {
                        meshComp.indices.push_back(primitive.indices[idx + 0] + vertexOffset);
                        meshComp.indices.push_back(primitive.indices[idx + 2] + vertexOffset);
                        meshComp.indices.push_back(primitive.indices[idx + 1] + vertexOffset);
                    }
                } else {
                    for (u32 index : primitive.indices) {
                        meshComp.indices.push_back(index + vertexOffset);
                    }
                }

                u32 subMeshIndexCount = static_cast<u32>(meshComp.indices.size()) - subMeshIndexOffset;

                // Record this primitive's sub-mesh info
                i32 primMatIdx = primitive.materialIndex >= 0 ? primitive.materialIndex : 0;
                primSubMeshes.push_back({ subMeshIndexOffset, subMeshIndexCount, primMatIdx, assimpMesh.name });

                // Track unique material indices
                if (uniqueMaterials.find(primMatIdx) == uniqueMaterials.end()) {
                    uniqueMaterials[primMatIdx] = static_cast<i32>(uniqueMaterials.size());
                }

                vertexOffset += static_cast<u32>(primitive.vertices.size());
            }
        }

        if (!meshComp.vertices.empty()) {
            // Accumulate vertex/index stats
            stats.totalVertexCount += static_cast<u32>(meshComp.vertices.size());
            stats.totalIndexCount += static_cast<u32>(meshComp.indices.size());

            // Compute AABB from vertex positions for auto-generated box collider
            Math::Vector3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
            Math::Vector3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            for (const auto& v : meshComp.vertices) {
                if (v.position.x < minBounds.x) minBounds.x = v.position.x;
                if (v.position.y < minBounds.y) minBounds.y = v.position.y;
                if (v.position.z < minBounds.z) minBounds.z = v.position.z;
                if (v.position.x > maxBounds.x) maxBounds.x = v.position.x;
                if (v.position.y > maxBounds.y) maxBounds.y = v.position.y;
                if (v.position.z > maxBounds.z) maxBounds.z = v.position.z;
            }

            ENJIN_LOG_INFO(Asset, "Assimp mesh '%s': %zu vertices, %zu indices, bounds [%.1f,%.1f,%.1f]-[%.1f,%.1f,%.1f] (size %.1fx%.1fx%.1f)",
                name.c_str(), meshComp.vertices.size(), meshComp.indices.size(),
                minBounds.x, minBounds.y, minBounds.z,
                maxBounds.x, maxBounds.y, maxBounds.z,
                maxBounds.x - minBounds.x, maxBounds.y - minBounds.y, maxBounds.z - minBounds.z);

            // Build sub-meshes if multiple materials were found
            bool isMultiMaterial = uniqueMaterials.size() > 1;
            if (isMultiMaterial) {
                for (const auto& psm : primSubMeshes) {
                    ECS::MeshComponent::SubMesh sm;
                    sm.indexOffset = psm.indexOffset;
                    sm.indexCount = psm.indexCount;
                    sm.materialSlot = uniqueMaterials[psm.assimpMaterialIndex];
                    sm.name = psm.meshName;
                    meshComp.subMeshes.push_back(sm);
                }
                ENJIN_LOG_INFO(Asset, "  Multi-material mesh: %zu sub-meshes, %zu material slots",
                    meshComp.subMeshes.size(), uniqueMaterials.size());
            }

            u32 totalAssimpVerts = static_cast<u32>(meshComp.vertices.size());
            world->AddComponent<ECS::MeshComponent>(entity, std::move(meshComp));

            // Create MorphTargetComponent if any primitive has morph targets (FBX blend shapes)
            {
                bool hasMorphs = false;
                for (i32 mi : meshIndices) {
                    if (mi < 0 || mi >= static_cast<i32>(scene.meshes.size())) continue;
                    for (const auto& prim : scene.meshes[mi].primitives) {
                        if (!prim.morphTargets.empty()) { hasMorphs = true; break; }
                    }
                    if (hasMorphs) break;
                }
                if (hasMorphs) {
                    auto& morphComp = world->AddComponent<ECS::MorphTargetComponent>(entity);
                    u32 vOff = 0;
                    for (i32 mi : meshIndices) {
                        if (mi < 0 || mi >= static_cast<i32>(scene.meshes.size())) continue;
                        for (const auto& prim : scene.meshes[mi].primitives) {
                            for (const auto& src : prim.morphTargets) {
                                i32 existingIdx = morphComp.FindTarget(src.name);
                                if (existingIdx < 0) {
                                    ECS::MorphTarget tgt;
                                    tgt.name = src.name;
                                    tgt.deltas.resize(totalAssimpVerts, ECS::MorphTargetDelta{});
                                    morphComp.targets.push_back(std::move(tgt));
                                    existingIdx = static_cast<i32>(morphComp.targets.size()) - 1;
                                }
                                auto& dst = morphComp.targets[existingIdx];
                                for (usize v = 0; v < src.positionDeltas.size() && (vOff + v) < dst.deltas.size(); ++v) {
                                    dst.deltas[vOff + v].positionDelta = src.positionDeltas[v];
                                    if (v < src.normalDeltas.size()) dst.deltas[vOff + v].normalDelta = src.normalDeltas[v];
                                }
                            }
                            vOff += static_cast<u32>(prim.vertices.size());
                        }
                    }
                    morphComp.weights.resize(morphComp.targets.size(), 0.0f);
                    stats.warnings.push_back("Morph targets imported (FBX): " + std::to_string(morphComp.targets.size()) + " targets");
                }
            }

            // Add box collider from mesh AABB
            if (options.generateColliders) {
                auto& collider = world->AddComponent<ECS::BoxColliderComponent>(entity);
                collider.center = (minBounds + maxBounds) * 0.5f;
                collider.size = maxBounds - minBounds;
            }

            // Resolve texture paths relative to model directory with fallback directories
            auto resolveTexPath = [&](const std::string& texPath) -> std::string {
                if (texPath.empty()) return "";
                namespace fs = std::filesystem;
                fs::path p(texPath);
                if (p.is_absolute() && fs::exists(p)) {
                    stats.texturePathsResolved.push_back(p.string());
                    return p.string();
                }
                // Try relative to model's base directory
                fs::path resolved = fs::path(scene.basePath) / p;
                if (fs::exists(resolved)) {
                    stats.texturePathsResolved.push_back(resolved.string());
                    return resolved.string();
                }
                // Try just the filename in the base directory
                resolved = fs::path(scene.basePath) / p.filename();
                if (fs::exists(resolved)) {
                    stats.texturePathsResolved.push_back(resolved.string());
                    return resolved.string();
                }
                // Try textures/ subdirectory
                resolved = fs::path(scene.basePath) / "textures" / p.filename();
                if (fs::exists(resolved)) {
                    stats.texturePathsResolved.push_back(resolved.string());
                    return resolved.string();
                }
                // Try Textures/ subdirectory (capital T)
                resolved = fs::path(scene.basePath) / "Textures" / p.filename();
                if (fs::exists(resolved)) {
                    stats.texturePathsResolved.push_back(resolved.string());
                    return resolved.string();
                }
                // Try user-specified texture search paths
                if (!options.textureSearchPaths.empty()) {
                    std::string found = TryTextureSearchPaths(
                        p.filename().string(), options.textureSearchPaths);
                    if (!found.empty()) {
                        stats.texturePathsResolved.push_back(found);
                        return found;
                    }
                }
                stats.texturePathsMissing.push_back(texPath);
                return texPath; // Return as-is, RenderSystem will attempt to load
            };

            // Helper: create MaterialComponent from AssimpMaterial
            auto buildMatComp = [&](const AssimpMaterial& assimpMat) -> ECS::MaterialComponent {
                ECS::MaterialComponent matComp;
                matComp.baseColor = Math::Vector3(
                    assimpMat.baseColorFactor.x,
                    assimpMat.baseColorFactor.y,
                    assimpMat.baseColorFactor.z
                );
                matComp.opacity = assimpMat.opacity;
                matComp.metallic = assimpMat.metallicFactor;
                matComp.roughness = assimpMat.roughnessFactor;
                matComp.emissiveColor = assimpMat.emissiveFactor;
                matComp.doubleSided = assimpMat.doubleSided;
                matComp.baseColorTexturePath = resolveTexPath(assimpMat.baseColorTexture);
                if (!matComp.baseColorTexturePath.empty()) {
                    matComp.baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
                }
                matComp.normalTexturePath = resolveTexPath(assimpMat.normalTexture);
                matComp.metallicRoughnessTexturePath = resolveTexPath(assimpMat.metallicRoughnessTexture);
                matComp.emissiveTexturePath = resolveTexPath(assimpMat.emissiveTexture);
                return matComp;
            };

            if (options.importMaterials) {
                if (isMultiMaterial) {
                    // Multi-material: create MaterialSlotsComponent with one slot per unique material.
                    // Also add a MaterialComponent (first slot) for backward compatibility with
                    // systems that only check MaterialComponent (shadow pass, outline pass, etc.).
                    ECS::MaterialSlotsComponent slotsComp;
                    slotsComp.slots.resize(uniqueMaterials.size());

                    for (const auto& [assimpIdx, slotIdx] : uniqueMaterials) {
                        if (assimpIdx >= 0 && assimpIdx < static_cast<i32>(scene.materials.size())) {
                            slotsComp.slots[slotIdx] = buildMatComp(scene.materials[assimpIdx]);
                        }
                    }

                    world->AddComponent<ECS::MaterialSlotsComponent>(entity, std::move(slotsComp));

                    // Add primary MaterialComponent from first material for backward compat
                    if (materialIndex >= 0 && materialIndex < static_cast<i32>(scene.materials.size())) {
                        world->AddComponent<ECS::MaterialComponent>(entity,
                            buildMatComp(scene.materials[materialIndex]));
                    }
                } else {
                    // Single material (original path)
                    if (materialIndex >= 0 && materialIndex < static_cast<i32>(scene.materials.size())) {
                        world->AddComponent<ECS::MaterialComponent>(entity,
                            buildMatComp(scene.materials[materialIndex]));
                    }
                }
            }

            // Auto-generate LODs for imported meshes with enough geometry.
            // IMPORTANT: AddComponent can reallocate component storage, invalidating
            // any raw pointers from GetComponent. Re-fetch after AddComponent.
            if (options.generateLODs) {
                auto* meshCheck = world->GetComponent<ECS::MeshComponent>(entity);
                if (meshCheck && meshCheck->vertices.size() > 64) {
                    auto& lod = world->AddComponent<ECS::LODComponent>(entity);
                    auto* meshForLOD = world->GetComponent<ECS::MeshComponent>(entity);
                    if (meshForLOD) {
                        Renderer::MeshSimplifier::GenerateLODs(*meshForLOD, lod);
                    }
                }
            }
        }
    }

    // Attach skeleton and animator to EVERY node with skinned mesh data.
    // Previously only attached to the first skinned node, which left other
    // skinned mesh entities without an AnimatorComponent — they rendered
    // without bone transforms (distorted). All skinned meshes in the same
    // model share the same skeleton and bone matrices.
    if (options.importAnimations && skelCtx.skeleton && nodeHasSkinning) {
        if (!skelCtx.attached) skelCtx.attached = true;
        // Track first skinned entity (used for bone buffer fallback in render system)
        if (skelCtx.bodyEntity == 0) {
            skelCtx.bodyEntity = entity;
        }

        // Add skeleton component
        auto& skelComp = world->AddComponent<ECS::SkeletonComponent>(entity);
        skelComp.skeleton = skelCtx.skeleton;
        skelComp.sourceAssetPath = stats.sourceFilePath;

        // Add animator component
        auto& animComp = world->AddComponent<ECS::AnimatorComponent>(entity);
        animComp.Initialize(skelCtx.skeleton);

        // Convert Assimp animations to skeletal animations
        for (const auto& assimpAnim : scene.animations) {
            Animation::SkeletalAnimation skelAnim;
            skelAnim.name = assimpAnim.name;
            skelAnim.duration = assimpAnim.duration;

            for (const auto& channel : assimpAnim.channels) {
                // Find which bone this channel targets
                i32 boneIndex = skelCtx.skeleton->FindBoneIndex(channel.nodeName);
                if (boneIndex < 0) {
                    continue;
                }

                // Find or create a track for this bone
                Animation::BoneTrack* track = nullptr;
                for (auto& t : skelAnim.tracks) {
                    if (t.boneIndex == boneIndex) {
                        track = &t;
                        break;
                    }
                }
                if (!track) {
                    skelAnim.tracks.push_back({});
                    track = &skelAnim.tracks.back();
                    track->boneIndex = boneIndex;
                    track->boneName = skelCtx.skeleton->bones[boneIndex].name;
                }

                // Copy position keyframes with axis conversion
                if (!channel.positions.empty()) {
                    track->positionTimes = channel.positionTimes;
                    track->positions.resize(channel.positions.size());
                    for (usize k = 0; k < channel.positions.size(); ++k) {
                        Math::Vector3 p = channel.positions[k];
                        if (zToY || lToR || options.flipX || options.flipY || options.flipZ) {
                            p = ConvertPosition(p, zToY, lToR, options.flipX, options.flipY, options.flipZ);
                        }
                        track->positions[k] = p;
                    }
                }

                // Copy rotation keyframes with axis conversion
                if (!channel.rotations.empty()) {
                    track->rotationTimes = channel.rotationTimes;
                    track->rotations.resize(channel.rotations.size());
                    for (usize k = 0; k < channel.rotations.size(); ++k) {
                        Math::Quaternion q = channel.rotations[k];
                        if (zToY || lToR) {
                            q = ConvertRotation(q, zToY, lToR);
                        }
                        track->rotations[k] = q;
                    }
                }

                // Copy scale keyframes (no axis conversion needed for scale)
                if (!channel.scales.empty()) {
                    track->scaleTimes = channel.scaleTimes;
                    track->scales = channel.scales;
                }
            }

            if (!skelAnim.tracks.empty()) {
                animComp.animator.AddAnimation(skelAnim);
            }
        }

        // Auto-play first animation
        if (!scene.animations.empty()) {
            animComp.animator.Play(scene.animations[0].name);
            ENJIN_LOG_INFO(Asset, "Auto-playing animation '%s' on entity '%s'",
                scene.animations[0].name.c_str(), name.c_str());
        }

    }

    // Recursively create child entities with parent-child hierarchy
    for (i32 childIndex : node.children) {
        ECS::Entity childEntity = CreateEntityFromAssimpNode(scene, childIndex, world, options,
                                                              outEntities, stats, skelCtx);
        if (childEntity != ECS::INVALID_ENTITY) {
            ECS::SetParent(world, childEntity, entity);
        }
    }

    return entity;
}

} // namespace Assets
} // namespace Enjin
