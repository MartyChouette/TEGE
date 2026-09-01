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
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <unordered_map>

namespace Enjin {
namespace Assets {

// Auto-collider generation for imported meshes. Collider sizes are WORLD SPACE
// (physics backends ignore transform scale), so the entity's scale is baked in —
// otherwise a cm-unit FBX gets a collider 100x bigger than the displayed model.
static void AddImportCollider(ECS::World* world, ECS::Entity entity,
                              const Math::Vector3& minBounds, const Math::Vector3& maxBounds,
                              const ImportOptions& options) {
    // WORLD scale: the entity is already parented (import root carries the
    // cm→m auto-scale), so read the accumulated hierarchy scale, not the local one.
    const Math::Matrix4 wm = ECS::ComputeWorldMatrix(world, entity);
    const Math::Vector3 s(
        Math::Vector3(wm.m[0], wm.m[1], wm.m[2]).Length(),
        Math::Vector3(wm.m[4], wm.m[5], wm.m[6]).Length(),
        Math::Vector3(wm.m[8], wm.m[9], wm.m[10]).Length());
    const Math::Vector3 center((minBounds.x + maxBounds.x) * 0.5f * s.x,
                               (minBounds.y + maxBounds.y) * 0.5f * s.y,
                               (minBounds.z + maxBounds.z) * 0.5f * s.z);
    const Math::Vector3 size((maxBounds.x - minBounds.x) * s.x,
                             (maxBounds.y - minBounds.y) * s.y,
                             (maxBounds.z - minBounds.z) * s.z);
    switch (options.colliderShape) {
        case ImportColliderShape::Sphere: {
            auto& c = world->AddComponent<ECS::SphereColliderComponent>(entity);
            c.center = center;
            c.radius = 0.5f * std::max(size.x, std::max(size.y, size.z));
            break;
        }
        case ImportColliderShape::Capsule: {
            auto& c = world->AddComponent<ECS::CapsuleColliderComponent>(entity);
            c.center = center;
            c.direction = ECS::CapsuleColliderComponent::Direction::Y;
            c.radius = 0.5f * std::max(size.x, size.z);
            // height = cylinder section only (total = height + 2*radius)
            c.height = std::max(0.0f, size.y - 2.0f * c.radius);
            break;
        }
        case ImportColliderShape::ConvexMesh: {
            auto& c = world->AddComponent<ECS::MeshColliderComponent>(entity);
            c.convex = true;
            c.autoGenerate = true;  // populated (world-scaled) at first physics use
            break;
        }
        case ImportColliderShape::Box:
        default: {
            auto& c = world->AddComponent<ECS::BoxColliderComponent>(entity);
            c.center = center;
            c.size = size;
            break;
        }
    }
}

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

            // Bone weight validation — sum across all 8 influences and normalize if needed
            f32 wSum = vert.boneWeights.x + vert.boneWeights.y + vert.boneWeights.z + vert.boneWeights.w
                     + vert.boneWeights2.x + vert.boneWeights2.y + vert.boneWeights2.z + vert.boneWeights2.w;
            if (wSum > 0.001f && std::fabs(wSum - 1.0f) > 0.01f) {
                badBoneWeights++;
                // Auto-fix: normalize all 8 weights so they sum to 1
                f32 inv = 1.0f / wSum;
                vert.boneWeights.x *= inv;
                vert.boneWeights.y *= inv;
                vert.boneWeights.z *= inv;
                vert.boneWeights.w *= inv;
                vert.boneWeights2.x *= inv;
                vert.boneWeights2.y *= inv;
                vert.boneWeights2.z *= inv;
                vert.boneWeights2.w *= inv;
                fixedBoneWeights++;
            }
        }

        // Strip ONLY truly-degenerate triangles: a repeated index, an out-of-range
        // index, or two coincident vertices. Do NOT use a small-area threshold —
        // triangle area scales with mesh density, so on a dense high-poly mesh most
        // real triangles are tiny (median ~1e-7 on a 2-unit model). Culling by area
        // deletes real fine geometry and punches holes. A triangle with three
        // distinct, non-coincident vertices is real, however small, so it stays.
        {
            std::vector<u32> cleanIndices;
            cleanIndices.reserve(mesh->indices.size());
            for (usize i = 0; i + 2 < mesh->indices.size(); i += 3) {
                u32 i0 = mesh->indices[i], i1 = mesh->indices[i + 1], i2 = mesh->indices[i + 2];
                if (i0 >= mesh->vertices.size() || i1 >= mesh->vertices.size() || i2 >= mesh->vertices.size()) {
                    degenerateTriangles++; continue; // out-of-range index
                }
                if (i0 == i1 || i1 == i2 || i0 == i2) {
                    degenerateTriangles++; continue; // repeated index — zero area by definition
                }
                const auto& v0 = mesh->vertices[i0].position;
                const auto& v1 = mesh->vertices[i1].position;
                const auto& v2 = mesh->vertices[i2].position;
                if ((v0.x == v1.x && v0.y == v1.y && v0.z == v1.z) ||
                    (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z) ||
                    (v0.x == v2.x && v0.y == v2.y && v0.z == v2.z)) {
                    degenerateTriangles++; continue; // coincident vertices — zero area
                }
                cleanIndices.push_back(i0); cleanIndices.push_back(i1); cleanIndices.push_back(i2);
            }
            // Only swap in the cleaned list if real geometry survives.
            if (degenerateTriangles > 0 && cleanIndices.size() >= 3) {
                mesh->indices = std::move(cleanIndices);
            }
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
// Stamp the source file path onto every imported mesh. The per-node builders set
// meshIndex/axis/hash (they know the geometry) but have no filepath in scope, so the
// path is filled here once all entities exist. Authored/procedural meshes never set a
// meshIndex, so they stay reference-less and keep serializing inline.
static void FillMeshSourcePaths(ECS::World* world, const std::vector<ECS::Entity>& entities,
                                const std::string& filepath) {
    if (!world) return;
    for (ECS::Entity e : entities) {
        if (!world->IsValid(e)) continue;
        auto* mc = world->GetComponent<ECS::MeshComponent>(e);
        if (mc && mc->source.meshIndex >= 0 && mc->source.sourcePath.empty()) {
            mc->source.sourcePath = filepath;
        }
    }
}

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

    // Skinned glTF: ONE capsule on the root (per-part colliders are skipped
    // in CreateEntityFromNode - same rule as the Assimp path). Bounds come
    // straight from the vertex data; glTF is meters by spec, no unit drama.
    if (effectiveOptions.generateColliders && !scene.skins.empty() &&
        result.rootEntity != ECS::INVALID_ENTITY) {
        Math::Vector3 mn(1e9f, 1e9f, 1e9f), mx(-1e9f, -1e9f, -1e9f);
        bool any = false;
        for (const auto& mesh : scene.meshes)
            for (const auto& prim : mesh.primitives)
                for (const auto& v : prim.vertices) {
                    mn.x = std::min(mn.x, v.position.x); mx.x = std::max(mx.x, v.position.x);
                    mn.y = std::min(mn.y, v.position.y); mx.y = std::max(mx.y, v.position.y);
                    mn.z = std::min(mn.z, v.position.z); mx.z = std::max(mx.z, v.position.z);
                    any = true;
                }
        if (any) {
            Math::Vector3 size = mx - mn;
            auto& cap = world->AddComponent<ECS::CapsuleColliderComponent>(result.rootEntity);
            cap.center = (mn + mx) * 0.5f;
            cap.direction = ECS::CapsuleColliderComponent::Direction::Y;
            cap.radius = std::min(0.3f * std::min(size.x, size.z), 0.25f * size.y);
            cap.height = std::max(0.0f, size.y - 2.0f * cap.radius);
            stats.warnings.push_back("Skinned model: single root capsule collider (per-part colliders skipped)");
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

    FillMeshSourcePaths(world, result.entities, filepath);
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
                vertex.uv1 = gltfVert.texCoord1;
                if (gltfVert.hasColor) vertex.color = gltfVert.color;
                vertex.boneWeights = gltfVert.boneWeights;
                vertex.boneIndices[0] = gltfVert.boneIndices[0];
                vertex.boneIndices[1] = gltfVert.boneIndices[1];
                vertex.boneIndices[2] = gltfVert.boneIndices[2];
                vertex.boneIndices[3] = gltfVert.boneIndices[3];
                // Influences 5-8 (JOINTS_1/WEIGHTS_1); all-zero when the rig has <=4 per vertex
                vertex.boneWeights2 = gltfVert.boneWeights2;
                vertex.boneIndices2[0] = gltfVert.boneIndices2[0];
                vertex.boneIndices2[1] = gltfVert.boneIndices2[1];
                vertex.boneIndices2[2] = gltfVert.boneIndices2[2];
                vertex.boneIndices2[3] = gltfVert.boneIndices2[3];
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

            // Record the source reference so the scene can serialize a compact
            // reference and the engine can share/free this geometry later. sourcePath
            // is filled by ImportGLTF's post-pass (this fn has no filepath in scope).
            meshComp.source.meshIndex = node.meshIndex;
            meshComp.source.axisZToY = zToY;
            meshComp.source.axisLToR = lToR;
            meshComp.source.contentHash =
                ECS::MeshComponent::ComputeContentHash(meshComp.vertices, meshComp.indices);

            world->AddComponent<ECS::MeshComponent>(entity, std::move(meshComp));

            // Create MorphTargetComponent if any primitive has morph targets.
            // The GPU side (UploadMorphTargetSSBO + binding 20 + triangle.vert) is
            // already wired; this populates the per-vertex deltas it consumes.
            {
                bool hasMorphTargets = false;
                for (const auto& prim : gltfMesh.primitives) {
                    if (!prim.morphTargets.empty()) { hasMorphTargets = true; break; }
                }
                if (hasMorphTargets) {
                    u32 totalVerts = static_cast<u32>(world->GetComponent<ECS::MeshComponent>(entity)->vertices.size());
                    auto& morphComp = world->AddComponent<ECS::MorphTargetComponent>(entity);
                    u32 vOff = 0;
                    for (const auto& prim : gltfMesh.primitives) {
                        for (usize t = 0; t < prim.morphTargets.size(); ++t) {
                            const auto& src = prim.morphTargets[t];
                            i32 existingIdx = morphComp.FindTarget(src.name);
                            if (existingIdx < 0) {
                                ECS::MorphTarget tgt;
                                tgt.name = src.name;
                                tgt.deltas.resize(totalVerts, ECS::MorphTargetDelta{});
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
                    morphComp.weights.resize(morphComp.targets.size(), 0.0f);
                    if (!gltfMesh.defaultMorphWeights.empty()) {
                        for (usize w = 0; w < morphComp.weights.size() && w < gltfMesh.defaultMorphWeights.size(); ++w)
                            morphComp.weights[w] = gltfMesh.defaultMorphWeights[w];
                    }
                    stats.warnings.push_back("Morph targets imported: " + std::to_string(morphComp.targets.size()) + " targets");
                }
            }

            // Add collider from mesh AABB (shape from import options, world-scaled).
            // Skinned scenes skip per-part colliders (same rule as the Assimp
            // path) - ImportGLTF adds a single root capsule instead.
            if (options.generateColliders && scene.skins.empty()) {
                AddImportCollider(world, entity, minBounds, maxBounds, options);
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

        // Topological sort: parent bones MUST have lower indices than their children,
        // or CalculateWorldTransforms reads an uninitialized parent transform and the
        // bind pose collapses — the classic "skinned glTF imports invisible" bug. The
        // FBX path already did this; the glTF path did not. Build an old->new remap and
        // fix EVERY reference into the bone array: bone.parentIndex, this mesh's vertex
        // joint indices (JOINTS_0/JOINTS_1), and the animation tracks below.
        std::vector<i32> boneRemap(skeleton->bones.size());
        for (usize i = 0; i < boneRemap.size(); ++i) boneRemap[i] = static_cast<i32>(i); // identity default
        {
            std::vector<i32> sortedOrder;
            sortedOrder.reserve(skeleton->bones.size());
            std::vector<bool> placed(skeleton->bones.size(), false);
            bool progress = true;
            while (progress && sortedOrder.size() < skeleton->bones.size()) {
                progress = false;
                for (usize i = 0; i < skeleton->bones.size(); ++i) {
                    if (placed[i]) continue;
                    i32 parent = skeleton->bones[i].parentIndex;
                    if (parent < 0 || placed[parent]) {
                        sortedOrder.push_back(static_cast<i32>(i));
                        placed[i] = true; progress = true;
                    }
                }
            }
            // Cycles (shouldn't happen in a valid skin): append leftovers as-is.
            for (usize i = 0; i < skeleton->bones.size(); ++i)
                if (!placed[i]) sortedOrder.push_back(static_cast<i32>(i));

            bool needsReorder = false;
            for (usize i = 0; i < sortedOrder.size(); ++i)
                if (sortedOrder[i] != static_cast<i32>(i)) { needsReorder = true; break; }

            if (needsReorder) {
                for (usize i = 0; i < sortedOrder.size(); ++i) boneRemap[sortedOrder[i]] = static_cast<i32>(i);

                std::vector<Animation::Bone> reordered(skeleton->bones.size());
                for (usize i = 0; i < sortedOrder.size(); ++i) {
                    reordered[i] = skeleton->bones[sortedOrder[i]];
                    if (reordered[i].parentIndex >= 0)
                        reordered[i].parentIndex = boneRemap[reordered[i].parentIndex];
                }
                skeleton->bones = std::move(reordered);

                // Remap the skinned mesh's vertex joint indices (already on this entity).
                if (auto* skinnedMesh = world->GetComponent<ECS::MeshComponent>(entity)) {
                    for (auto& v : skinnedMesh->vertices) {
                        f32* w0 = &v.boneWeights.x;
                        for (int s = 0; s < 4; ++s)
                            if (w0[s] > 0.0f && v.boneIndices[s] < boneRemap.size())
                                v.boneIndices[s] = static_cast<u32>(boneRemap[v.boneIndices[s]]);
                        f32* w1 = &v.boneWeights2.x;
                        for (int s = 0; s < 4; ++s)
                            if (w1[s] > 0.0f && v.boneIndices2[s] < boneRemap.size())
                                v.boneIndices2[s] = static_cast<u32>(boneRemap[v.boneIndices2[s]]);
                    }
                }
                ENJIN_LOG_INFO(Asset, "glTF: reordered %zu bones for parent-before-child traversal",
                    skeleton->bones.size());
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

                // Find which bone this channel targets, then map through the bone
                // reorder so tracks point at the sorted bone indices (identity if the
                // skeleton didn't need reordering).
                auto jointIt = nodeToJoint.find(channel.targetNode);
                if (jointIt == nodeToJoint.end()) continue;
                i32 boneIndex = jointIt->second;
                if (boneIndex >= 0 && boneIndex < static_cast<i32>(boneRemap.size()))
                    boneIndex = boneRemap[boneIndex];

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

        // Auto-play first animation (opt-in — off by default keeps imports at rest)
        if (options.autoPlayAnimation && !scene.animations.empty()) {
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

    // Axis conversion comes from the source-app preset. SCALE does NOT: the preset
    // scale was only ever a guess at the file's unit, and we have the real thing —
    // the FBX UnitScaleFactor metadata — which is applied below. Keeping the preset
    // scale here on top of the metadata would double-convert.
    if (effectiveOptions.convertAxes && resolvedApp != SourceApp::Auto) {
        SourceAppPreset preset = GetSourceAppPreset(resolvedApp);
        ENJIN_LOG_INFO(Asset, "Applying %s preset axes (zToY=%d, lToR=%d) for %s",
                       preset.name, preset.zUpToYUp, preset.leftToRight, filepath.c_str());
    }

    // Auto up-axis fix: the FBX metadata says whether the file is Z-up (Blender/Max)
    // and the engine is Y-up. The preset-driven zToY above only converts SKINNED bone
    // poses; static (non-skinned) meshes bake node transforms and were never axis-
    // converted, so a Z-up static model imported lying on its side. When the file is
    // Z-up and the user hasn't picked an explicit source-app preset, rotate the root
    // nodes -90° about X (Z-up -> Y-up) so the whole hierarchy stands up correctly.
    // Guarded on sourceApp==Auto so it never doubles with a chosen preset's zToY.
    if (effectiveOptions.convertAxes && options.sourceApp == SourceApp::Auto &&
        scene.sourceUpAxis == 2) {
        const Math::Quaternion zToYQ = Math::Quaternion::FromEuler(
            Math::Vector3(-1.57079633f, 0.0f, 0.0f));  // -90 deg about X
        for (i32 rootIdx : scene.rootNodes) {
            if (rootIdx < 0 || rootIdx >= static_cast<i32>(scene.nodes.size())) continue;
            auto& rn = scene.nodes[rootIdx];
            // Pre-multiply the root's local matrix by Rx(-90): rotate both its
            // translation vector and its rotation. (x,y,z) -> (x, z, -y).
            rn.translation = Math::Vector3(rn.translation.x, rn.translation.z, -rn.translation.y);
            rn.rotation = zToYQ * rn.rotation;
        }
        ENJIN_LOG_INFO(Asset, "Auto axis fix: Z-up source -> Y-up (rotated root nodes) for %s",
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

                // Remap vertex bone indices in ALL meshes — BOTH influence sets. The
                // loader fills up to 8 influences (boneIndices + boneIndices2); missing
                // the second set left dense-rig verts pointing at pre-sort (wrong) bones
                // whenever a reorder happened, corrupting exactly the heavily-weighted
                // vertices.
                for (auto& mesh : scene.meshes) {
                    for (auto& prim : mesh.primitives) {
                        for (auto& vert : prim.vertices) {
                            f32* weights = &vert.boneWeights.x;
                            for (int s = 0; s < 4; ++s) {
                                if (weights[s] > 0.0f && vert.boneIndices[s] < oldToNew.size()) {
                                    vert.boneIndices[s] = static_cast<u32>(oldToNew[vert.boneIndices[s]]);
                                }
                            }
                            f32* weights2 = &vert.boneWeights2.x;
                            for (int s = 0; s < 4; ++s) {
                                if (weights2[s] > 0.0f && vert.boneIndices2[s] < oldToNew.size()) {
                                    vert.boneIndices2[s] = static_cast<u32>(oldToNew[vert.boneIndices2[s]]);
                                }
                            }
                        }
                    }
                }

                ENJIN_LOG_INFO(Asset, "Reordered %zu bones for parent-before-child traversal",
                    skeleton->bones.size());
            }
        }

        // Derive inverse-bind matrices from the bind pose. Assimp's offsetMatrix does
        // not match these files' node bind pose (it collapses the mesh), so instead
        // IBM = inverse(globalBindTransform) from the bone bind TRS, which guarantees
        // worldBind*IBM == identity at bind. Bones are topologically sorted above, so
        // each parent's global bind is computed before its children.
        {
            std::vector<Math::Matrix4> globalBind(skeleton->bones.size());
            for (usize b = 0; b < skeleton->bones.size(); ++b) {
                Math::Matrix4 local = Math::Matrix4::Translation(skeleton->bones[b].bindPosition) *
                                      skeleton->bones[b].bindRotation.ToMatrix() *
                                      Math::Matrix4::Scale(skeleton->bones[b].bindScale);
                i32 p = skeleton->bones[b].parentIndex;
                globalBind[b] = (p >= 0 && p < static_cast<i32>(b)) ? globalBind[p] * local : local;
                skeleton->bones[b].inverseBindMatrix = globalBind[b].Inverse();
            }
        }

        // Compute the "armature" frame: the world transform of the root bone's PARENT
        // node. The bone hierarchy's bind pose is relative to this frame (Mixamo/Blender
        // put a 100x scale on the armature node), but skinned vertices are baked in full
        // scene space. Skinned meshes get their vertices dropped into THIS frame and the
        // entity carries armatureWorld back out, so bone motions animate at the matching
        // scale instead of 100x off. See the bake + the weighted-mesh transform below.
        {
            Math::Matrix4 armWorld = Math::Matrix4::Identity();
            if (!skeleton->bones.empty()) {
                auto it = boneNameToNodeIdx.find(skeleton->bones[0].name);
                if (it != boneNameToNodeIdx.end()) {
                    i32 rootParentNode = scene.nodes[it->second].parentIndex;
                    if (rootParentNode >= 0) {
                        std::vector<i32> chain;
                        i32 walk = rootParentNode;
                        while (walk >= 0) { chain.push_back(walk); walk = scene.nodes[walk].parentIndex; }
                        for (auto cit = chain.rbegin(); cit != chain.rend(); ++cit) {
                            const auto& n = scene.nodes[*cit];
                            armWorld = armWorld * (Math::Matrix4::Translation(n.translation) *
                                                   n.rotation.ToMatrix() * Math::Matrix4::Scale(n.scale));
                        }
                    }
                }
            }
            skelCtx.armatureWorld = armWorld;
            skelCtx.armatureWorldInverse = armWorld.Inverse();
        }

        skelCtx.skeleton = skeleton;

        // Assign a scene-unique group id so every co-skeleton mesh of THIS model re-shares one
        // Skeleton after save/load (SkeletonComponent::skeletonGroupId). Take max+1 over skeletons
        // already in the scene so two distinct characters never collapse onto one skeleton.
        {
            u64 maxId = 0;
            for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::SkeletonComponent>()) {
                const auto* sc = world->GetComponent<ECS::SkeletonComponent>(e);
                if (sc && sc->skeletonGroupId > maxId) maxId = sc->skeletonGroupId;
            }
            skelCtx.groupId = maxId + 1;
        }

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

    // For SKINNED meshes, the entity build path bakes each mesh node's accumulated
    // world transform into vertices — so measure the POST-BAKE mesh bounds by
    // applying the same node world transforms here. The previous approach summed
    // BONE bind positions, which wildly underestimates rigs whose armature node
    // carries the unit scale (bones read ~1.8 while the baked mesh is 395 units):
    // auto-scale never fired and cm-unit animals imported giant, with the import
    // focus flying the camera INSIDE the model — "only see a shadow"
    // (ShibaInu/Stag/Wolf, 2026-08-08).
    if (skelCtx.skeleton) {
        for (usize ni = 0; ni < scene.nodes.size(); ++ni) {
            const auto& n = scene.nodes[ni];
            const bool nodeHasMesh = n.meshIndex >= 0 || !n.meshIndices.empty();
            if (!nodeHasMesh) continue;

            // Accumulated FBX world transform of this mesh node (same math the
            // entity build path uses for the vertex bake)
            Math::Matrix4 nodeWorld = Math::Matrix4::Identity();
            std::vector<i32> chain;
            i32 walk = static_cast<i32>(ni);
            while (walk >= 0) {
                chain.push_back(walk);
                walk = scene.nodes[walk].parentIndex;
            }
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                const auto& cn = scene.nodes[*it];
                nodeWorld = nodeWorld * (Math::Matrix4::Translation(cn.translation) *
                                         cn.rotation.ToMatrix() *
                                         Math::Matrix4::Scale(cn.scale));
            }

            const std::vector<i32> nodeMeshes = n.meshIndices.empty()
                ? std::vector<i32>{n.meshIndex} : n.meshIndices;
            for (i32 mi : nodeMeshes) {
                if (mi < 0 || mi >= static_cast<i32>(scene.meshes.size())) continue;
                for (const auto& prim : scene.meshes[mi].primitives) {
                    for (const auto& v : prim.vertices) {
                        Math::Vector4 p = nodeWorld * Math::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
                        boundsMin.x = Math::Min(boundsMin.x, p.x);
                        boundsMin.y = Math::Min(boundsMin.y, p.y);
                        boundsMin.z = Math::Min(boundsMin.z, p.z);
                        boundsMax.x = Math::Max(boundsMax.x, p.x);
                        boundsMax.y = Math::Max(boundsMax.y, p.y);
                        boundsMax.z = Math::Max(boundsMax.z, p.z);
                    }
                }
            }
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
    // ---- Import scale: deterministic from unit metadata, self-correcting fallback ----
    // Measure the LARGEST extent (in raw file units), not just Y height: a wide/flat
    // model short in Y (vehicle, dragon) would otherwise slip a height-only check and
    // import as a giant the camera sits inside.
    f32 extentX = boundsMax.x - boundsMin.x;
    f32 extentY = boundsMax.y - boundsMin.y;
    f32 extentZ = boundsMax.z - boundsMin.z;
    f32 largest = Math::Max(extentX, Math::Max(extentY, extentZ));
    // Measurable only if at least one vertex was seen (otherwise the ±FLT_MAX seed
    // makes extents -inf) and finite. Guarding this stops the classic silent failure:
    // unmeasured bounds -> no sane scale -> model imports at raw units and vanishes.
    bool boundsMeasurable = (boundsMax.y >= boundsMin.y) && std::isfinite(largest) && largest > 1e-6f;

    // Primary: convert file units to meters using the file's real UnitScaleFactor
    // (FBX metadata: 1 = cm, 100 = m). This is deterministic per file and works even
    // when the source app can't be detected — unlike guessing from height.
    bool haveUnitMeta = std::isfinite(scene.unitScaleFactor) && scene.unitScaleFactor > 0.0f;
    f32 unitConv = haveUnitMeta ? (scene.unitScaleFactor / 100.0f) : 1.0f;   // cm->0.01, m->1.0
    unitScale = effectiveOptions.scale * unitConv;   // effectiveOptions.scale = user multiplier

    if (boundsMeasurable) {
        f32 sizeMeters = largest * unitScale;   // model's largest dimension, in meters
        // Sanity net. Without metadata, normalize any oversized model to human scale.
        // With metadata, trust it UNLESS the result is outside a sane viewable range —
        // FBX UnitScaleFactor often LIES (these Wolf/Cow files claim cm but are ~5-unit
        // meshes, so the cm conversion made a 5 cm wolf). A real 0.5-20 m model is kept;
        // anything that lands under ~0.4 m or over ~25 m is re-normalized to ~1.8 m.
        bool absurd = haveUnitMeta ? (sizeMeters > 25.0f || sizeMeters < 0.4f)
                                   : (sizeMeters > 10.0f || sizeMeters < 0.4f);
        if (!effectiveOptions.normalizeScale) absurd = false;   // user opted out of size magic
        if (absurd && sizeMeters > 1e-6f) {
            f32 corrected = unitScale * (1.8f / sizeMeters);   // ~1.8 m target
            ENJIN_LOG_WARN(Asset, "FBX auto-scale: %s produced %.3f m (largest=%.1f units); "
                                  "rescaling to ~1.8 m (scale %.5f -> %.5f) for %s",
                           haveUnitMeta ? "unit metadata" : "no unit metadata", sizeMeters,
                           largest, unitScale, corrected, filepath.c_str());
            unitScale = corrected;
        } else {
            ENJIN_LOG_INFO(Asset, "FBX scale: UnitScaleFactor=%.3f, largest=%.1f units -> %.3f m (scale=%.5f) for %s",
                           scene.unitScaleFactor, largest, sizeMeters, unitScale, filepath.c_str());
        }
    } else {
        ENJIN_LOG_WARN(Asset, "FBX import: could not measure model bounds (degenerate/empty geometry) "
                              "— importing at scale %.5f; model may be off-screen or collapsed", unitScale);
    }

    // Final safety: a zero / NaN / inf root scale collapses the ENTIRE imported
    // hierarchy to a point (every child is parentWorld * local, and skinned children
    // keep scale=1/pos=0 so they cannot survive a bad root). That is the difference
    // between "wrong size" and "completely invisible" — never let it through.
    if (!std::isfinite(unitScale) || unitScale <= 1e-6f) {
        ENJIN_LOG_WARN(Asset, "FBX import: computed unit scale %.6f is invalid — forcing 1.0", unitScale);
        unitScale = 1.0f;
    }
    rootTransform.scale = Math::Vector3(unitScale, unitScale, unitScale);
    skelCtx.unitScale = unitScale; // Pass to skinned mesh entities

    // Clean-import scale bake — RIGID (non-skinned) files only. The FBX path
    // stores rigid node positions in native file units and puts the cm->m unit
    // scale on the import ROOT, so each piece's local origin sits hundreds-to-
    // thousands of units from where it renders (ugly pivots / inspector values).
    // In a purely-rigid file every node takes the plain-local branch, so folding
    // the unit scale into node translations + vertices and setting the root scale
    // to 1 preserves every world transform exactly (a uniform parent scale
    // distributes cleanly into child translations + geometry) while giving
    // in-meters pivots. Skinned/mixed files keep the root-scale path untouched:
    // baking their bind poses + animation is risky, and their gizmos are already
    // world-correct after the gizmo fix.
    if (!scene.hasSkinning && std::isfinite(unitScale) && unitScale > 1e-6f &&
        std::fabs(unitScale - 1.0f) > 1e-4f) {
        for (auto& n : scene.nodes) n.translation = n.translation * unitScale;
        for (auto& m : scene.meshes)
            for (auto& prim : m.primitives)
                for (auto& v : prim.vertices) v.position = v.position * unitScale;
        rootTransform.scale = Math::Vector3(1.0f, 1.0f, 1.0f);
        skelCtx.unitScale = 1.0f;
        ENJIN_LOG_INFO(Asset, "Clean-import: baked unit scale %.5f into rigid geometry (root scale=1) for %s",
                       unitScale, filepath.c_str());
        unitScale = 1.0f;
    }

    result.entities.push_back(importRoot);
    result.rootEntity = importRoot;

    // Create entities from root nodes, parented under the import root.
    // pendingParent threads through skipped nodes, so meshes buried under
    // $AssimpFbx$/bone/empty nodes still land under the import root.
    for (i32 rootIndex : scene.rootNodes) {
        CreateEntityFromAssimpNode(scene, rootIndex, world, effectiveOptions,
                                   result.entities, stats, skelCtx, importRoot);
    }

    // Skinned characters: ONE capsule on the import root (per-part colliders
    // are skipped above). Sized from the whole model's bounds in world units;
    // the radius leans slim because the raw bounds usually include a T-pose
    // arm span. Collider sizes are WORLD SPACE (backends ignore scale).
    if (effectiveOptions.generateColliders && scene.hasSkinning && boundsMeasurable) {
        Math::Vector3 c = (boundsMin + boundsMax) * (0.5f * unitScale);
        Math::Vector3 size = (boundsMax - boundsMin) * unitScale;
        auto& cap = world->AddComponent<ECS::CapsuleColliderComponent>(importRoot);
        cap.center = c;
        cap.direction = ECS::CapsuleColliderComponent::Direction::Y;
        cap.radius = std::min(0.3f * std::min(size.x, size.z), 0.25f * size.y);
        cap.height = std::max(0.0f, size.y - 2.0f * cap.radius);   // cylinder section only
        stats.warnings.push_back("Skinned model: single root capsule collider (per-part colliders skipped)");
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

    FillMeshSourcePaths(world, result.entities, filepath);
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
                                                       AssimpSkeletonContext& skelCtx,
                                                       ECS::Entity pendingParent) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(scene.nodes.size())) {
        return ECS::INVALID_ENTITY;
    }

    // Skip excluded nodes (from import preview deselection)
    for (i32 excluded : options.excludedNodeIndices) {
        if (excluded == nodeIndex) {
            const AssimpNode& skippedNode = scene.nodes[nodeIndex];
            for (i32 childIdx : skippedNode.children) {
                CreateEntityFromAssimpNode(scene, childIdx, world, options, outEntities, stats, skelCtx,
                                           pendingParent);
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
        // Don't create an entity — recurse into children, passing the pending
        // parent through so meshes under skipped nodes still get parented.
        // (Returning INVALID_ENTITY used to drop the whole subtree out of the
        // hierarchy: callers only SetParent on a valid return, so mesh entities
        // under $AssimpFbx$/bone/empty nodes were orphaned at scene root —
        // "pieces not placed into the auto parent", Marty 2026-08-08.)
        for (i32 childIdx : node.children) {
            CreateEntityFromAssimpNode(scene, childIdx, world, options, outEntities, stats, skelCtx,
                                       pendingParent);
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
    if (pendingParent != ECS::INVALID_ENTITY) {
        ECS::SetParent(world, entity, pendingParent);
    }

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

    // Combine all meshes referenced by this node into one MeshComponent (list
    // needed before the transform decision below)
    const std::vector<i32> meshIndices = node.meshIndices.empty()
        ? (node.meshIndex >= 0 ? std::vector<i32>{node.meshIndex} : std::vector<i32>{})
        : node.meshIndices;

    // Accumulated FBX-space world transform of this node (includes SKIPPED
    // ancestor nodes — $AssimpFbx$ helpers, bones). Used to bake weighted
    // meshes into skeleton space, or to preserve placement on the entity for
    // rigid meshes in skinned files.
    Math::Matrix4 meshNodeWorld = Math::Matrix4::Identity();
    if (skelCtx.skeleton && hasMeshes) {
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

    // Rigid-vs-weighted: a mesh node in a skinned file whose vertices carry NO
    // bone weights (antlers/props socketed under a bone node) is placed by its
    // node transform, not by the skeleton — flattening it to origin loses the
    // authored placement (Marty 2026-08-08).
    bool nodeMeshesWeighted = false;
    if (skelCtx.skeleton && hasMeshes) {
        for (i32 mi : meshIndices) {
            if (mi < 0 || mi >= static_cast<i32>(scene.meshes.size())) continue;
            for (const auto& prim : scene.meshes[mi].primitives) {
                for (const auto& v : prim.vertices) {
                    if (v.boneWeights.x > 0.0f || v.boneWeights.y > 0.0f ||
                        v.boneWeights.z > 0.0f || v.boneWeights.w > 0.0f) {
                        nodeMeshesWeighted = true;
                        break;
                    }
                }
                if (nodeMeshesWeighted) break;
            }
            if (nodeMeshesWeighted) break;
        }
    }

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
    if (skelCtx.skeleton && hasMeshes && nodeMeshesWeighted) {
        // Weighted (skinned) mesh: zero position/rotation — the SKELETON places
        // the vertices; the entity transform is an offset on top of that.
        // The import root carries the auto-computed cm→m unitScale and this
        // entity is parented under it (pendingParent), so local scale stays 1 —
        // re-applying unitScale here would double-scale through the hierarchy.
        // Unparented calls (legacy overload) still need the scale themselves.
        //
        // The skinned vertices are baked into the ARMATURE frame (÷ armatureWorld, see
        // the bake below), so the entity carries the armature transform to bring the
        // skinned result back to scene space. This is what makes ANIMATION correct: the
        // bone motions live in the armature frame and now match the vertex scale. At
        // rest it is identical to before (armatureWorld · armatureWorld⁻¹ · v = v).
        const Math::Matrix4& aw = skelCtx.armatureWorld;
        Math::Vector3 awPos(aw.m[12], aw.m[13], aw.m[14]);
        Math::Vector3 ac0(aw.m[0], aw.m[1], aw.m[2]);
        Math::Vector3 ac1(aw.m[4], aw.m[5], aw.m[6]);
        Math::Vector3 ac2(aw.m[8], aw.m[9], aw.m[10]);
        Math::Vector3 awScale(ac0.Length(), ac1.Length(), ac2.Length());
        if (awScale.x > 1e-6f) ac0 = ac0 / awScale.x;
        if (awScale.y > 1e-6f) ac1 = ac1 / awScale.y;
        if (awScale.z > 1e-6f) ac2 = ac2 / awScale.z;
        Math::Matrix4 awRotM = Math::Matrix4::Identity();
        awRotM.m[0] = ac0.x; awRotM.m[1] = ac0.y; awRotM.m[2]  = ac0.z;
        awRotM.m[4] = ac1.x; awRotM.m[5] = ac1.y; awRotM.m[6]  = ac1.z;
        awRotM.m[8] = ac2.x; awRotM.m[9] = ac2.y; awRotM.m[10] = ac2.z;
        Math::Quaternion awRot = Math::Quaternion::FromMatrix(awRotM);
        // Axis-convert the armature transform to match the (already-converted) vertices and inverse
        // bind matrices — the rigid path below does this, but the skinned path didn't, so skinned
        // meshes imported with convertAxes came in tilted (the entity frame and the skinning frame
        // disagreed). Mirrors the rigid-mesh conversion at the `else if` branch.
        if (zToY || lToR || options.flipX || options.flipY || options.flipZ) {
            awPos = ConvertPosition(awPos, zToY, lToR, options.flipX, options.flipY, options.flipZ);
            awRot = ConvertRotation(awRot, zToY, lToR);
        }
        const f32 extraScale = (pendingParent != ECS::INVALID_ENTITY) ? 1.0f : skelCtx.unitScale;
        transform.position = awPos * extraScale;
        transform.rotation = awRot;
        transform.scale = awScale * extraScale;
    } else if (skelCtx.skeleton && hasMeshes) {
        // Rigid mesh in a skinned file: keep the authored FBX placement on the
        // ENTITY (accumulated through skipped ancestors) and leave the vertices
        // mesh-local — position/rotation stay editable in the inspector.
        Math::Vector3 wpos(meshNodeWorld.m[12], meshNodeWorld.m[13], meshNodeWorld.m[14]);
        Math::Vector3 c0(meshNodeWorld.m[0], meshNodeWorld.m[1], meshNodeWorld.m[2]);
        Math::Vector3 c1(meshNodeWorld.m[4], meshNodeWorld.m[5], meshNodeWorld.m[6]);
        Math::Vector3 c2(meshNodeWorld.m[8], meshNodeWorld.m[9], meshNodeWorld.m[10]);
        Math::Vector3 wscale(c0.Length(), c1.Length(), c2.Length());
        if (wscale.x > 1e-6f) c0 = c0 / wscale.x;
        if (wscale.y > 1e-6f) c1 = c1 / wscale.y;
        if (wscale.z > 1e-6f) c2 = c2 / wscale.z;
        Math::Matrix4 rotM = Math::Matrix4::Identity();
        rotM.m[0] = c0.x; rotM.m[1] = c0.y; rotM.m[2]  = c0.z;
        rotM.m[4] = c1.x; rotM.m[5] = c1.y; rotM.m[6]  = c1.z;
        rotM.m[8] = c2.x; rotM.m[9] = c2.y; rotM.m[10] = c2.z;
        Math::Quaternion wrot = Math::Quaternion::FromMatrix(rotM);
        if (zToY || lToR || options.flipX || options.flipY || options.flipZ) {
            wpos = ConvertPosition(wpos, zToY, lToR, options.flipX, options.flipY, options.flipZ);
            wrot = ConvertRotation(wrot, zToY, lToR);
        }
        transform.position = wpos;
        transform.rotation = wrot;
        transform.scale = wscale;
    } else {
        transform.position = pos;
        transform.rotation = rot;
        transform.scale = node.scale;
    }

    // Add mesh component if node has meshes
    bool nodeHasSkinning = false;
    if (!meshIndices.empty()) {
        ECS::MeshComponent meshComp;

        // Bake the mesh node's world transform into skinned vertices so they share the
        // native space with the rigid parts and size correctly. This is the known-
        // VISIBLE state (bind-pose inverse-bind above keeps it from collapsing). The
        // animation-space mismatch (vertices in scene space vs bones in armature space)
        // is the remaining issue tackled separately.
        bool bakeMeshNodeIntoVertices = (skelCtx.skeleton != nullptr) && nodeMeshesWeighted;

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
                    // For skinned meshes, put the vertex in the ARMATURE frame:
                    // armatureWorld⁻¹ · meshNodeWorld · v. meshNodeWorld brings it to
                    // scene space; armatureWorld⁻¹ then drops it into the frame the bone
                    // hierarchy lives in (the armature node carries a 100× scale). The
                    // entity transform (= armatureWorld) puts the skinned result back.
                    // This is the fix that makes animation deform at the right scale.
                    if (bakeMeshNodeIntoVertices) {
                        Math::Vector4 p4 = skelCtx.armatureWorldInverse *
                            (meshNodeWorld * Math::Vector4(vertex.position.x, vertex.position.y, vertex.position.z, 1.0f));
                        vertex.position = Math::Vector3(p4.x, p4.y, p4.z);
                        Math::Vector4 n4 = skelCtx.armatureWorldInverse *
                            (meshNodeWorld * Math::Vector4(vertex.normal.x, vertex.normal.y, vertex.normal.z, 0.0f));
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
                    vertex.uv1 = assimpVert.texCoord1;
                    if (assimpVert.hasColor) vertex.color = assimpVert.color;
                    // Copy bone weights from Assimp vertex data (influences 1-4)
                    vertex.boneWeights = assimpVert.boneWeights;
                    vertex.boneIndices[0] = assimpVert.boneIndices[0];
                    vertex.boneIndices[1] = assimpVert.boneIndices[1];
                    vertex.boneIndices[2] = assimpVert.boneIndices[2];
                    vertex.boneIndices[3] = assimpVert.boneIndices[3];
                    // Influences 5-8 (dense rigs); all-zero when the vertex has <=4
                    vertex.boneWeights2 = assimpVert.boneWeights2;
                    vertex.boneIndices2[0] = assimpVert.boneIndices2[0];
                    vertex.boneIndices2[1] = assimpVert.boneIndices2[1];
                    vertex.boneIndices2[2] = assimpVert.boneIndices2[2];
                    vertex.boneIndices2[3] = assimpVert.boneIndices2[3];
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

            // Record the source reference. Key on nodeIndex (not mesh index): the
            // Assimp path merges a node's meshes AND bakes the node world transform
            // into skinned vertices, so the node is what reproduces this geometry.
            // sourcePath is filled by ImportAssimp's post-pass (no filepath here).
            meshComp.source.meshIndex = nodeIndex;
            meshComp.source.axisZToY = zToY;
            meshComp.source.axisLToR = lToR;
            meshComp.source.contentHash =
                ECS::MeshComponent::ComputeContentHash(meshComp.vertices, meshComp.indices);

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

            // Add collider from mesh AABB (shape from import options, world-scaled)
            if (options.generateColliders) {
                AddImportCollider(world, entity, minBounds, maxBounds, options);
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

                    // Import diagnostic: material slot values decide visibility
                    // (opacity 0 = invisible mesh with a working shadow)
                    for (usize si = 0; si < slotsComp.slots.size(); ++si) {
                        const auto& sm = slotsComp.slots[si];
                        ENJIN_LOG_INFO(Asset, "  slot %zu: opacity=%.3f baseColor=(%.2f,%.2f,%.2f) alphaMode=%d",
                            si, sm.opacity, sm.baseColor.x, sm.baseColor.y, sm.baseColor.z,
                            static_cast<i32>(sm.alphaMode));
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

    // Every skinned mesh in one model shares ONE skeleton and is driven by ONE animator
    // (the "leader" = first skinned mesh). Followers get only a SkeletonComponent that
    // references the same shared skeleton; the render system resolves the leader's animator
    // for them by shared-skeleton identity (RenderSystem::ResolveAnimator). This is why a
    // body + joints/clothing stay perfectly in sync: one clock, one set of bone matrices.
    // Giving each mesh its OWN animator (the previous approach) ran independent clocks, so
    // pausing one left the others walking and they slowly drifted apart.
    if (options.importAnimations && skelCtx.skeleton && nodeHasSkinning) {
        if (!skelCtx.attached) skelCtx.attached = true;
        // The first skinned node becomes the leader that owns the AnimatorComponent.
        const bool isLeader = (skelCtx.bodyEntity == 0);
        if (isLeader) {
            skelCtx.bodyEntity = entity;
        }

        // Add skeleton component (shared skeleton pointer — every mesh, leader and followers)
        auto& skelComp = world->AddComponent<ECS::SkeletonComponent>(entity);
        skelComp.skeleton = skelCtx.skeleton;
        skelComp.sourceAssetPath = stats.sourceFilePath;
        skelComp.skeletonGroupId = skelCtx.groupId;  // shared by leader + followers, persists across save/load

        // Only the leader carries the animator (playback clock + bone matrices).
        if (isLeader) {
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

        // Movement-driven animation: match clip names against the standard
        // locomotion vocabulary (idle/walk/run/jump, case-insensitive substring
        // — covers "AnimalArmature|Walk", "mixamo.com|Running", etc.) and give
        // the leader a MovementAnimatorComponent so the model idles when still
        // and walks/runs/jumps as it moves, with no scripting.
        std::string clipIdle, clipWalk, clipRun, clipJump, clipFirst;
        for (const auto& anim : scene.animations) {
            if (clipFirst.empty()) clipFirst = anim.name;
            std::string lower = anim.name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (clipIdle.empty() && lower.find("idle") != std::string::npos) clipIdle = anim.name;
            if (clipWalk.empty() && lower.find("walk") != std::string::npos) clipWalk = anim.name;
            if (clipRun.empty()  && (lower.find("run") != std::string::npos ||
                                     lower.find("gallop") != std::string::npos)) clipRun = anim.name;
            if (clipJump.empty() && (lower.find("jump") != std::string::npos ||
                                     lower.find("fall") != std::string::npos)) clipJump = anim.name;
        }
        if (!clipIdle.empty() || !clipWalk.empty()) {
            animComp.movement.idleClip = clipIdle;
            animComp.movement.walkClip = clipWalk;
            animComp.movement.runClip = clipRun;
            animComp.movement.jumpClip = clipJump;
            ENJIN_LOG_INFO(Asset, "Animator movement drive: idle='%s' walk='%s' run='%s' jump='%s'",
                clipIdle.c_str(), clipWalk.c_str(), clipRun.c_str(), clipJump.c_str());
        }

        // Auto-play: prefer the idle clip — playing whatever is alphabetically
        // first meant every dropped-in animal led with its attack animation.
        // Opt-in (off by default) so imports come in at their correct rest pose.
        if (options.autoPlayAnimation && !scene.animations.empty()) {
            const std::string& startClip = !clipIdle.empty() ? clipIdle : clipFirst;
            animComp.animator.Play(startClip);
            ENJIN_LOG_INFO(Asset, "Auto-playing animation '%s' on entity '%s' (leader; drives all co-skeleton meshes)",
                startClip.c_str(), name.c_str());
        }
        } // end if (isLeader)

    }

    // Import diagnostic: transform + skinning state per entity (invisible-model
    // bugs have hidden in exactly these values — scale/parent/weighted)
    if (hasMeshes) {
        auto* xfDiag = world->GetComponent<ECS::TransformComponent>(entity);
        if (xfDiag) {
            ENJIN_LOG_INFO(Asset, "  entity '%s': parent=%s pos=(%.2f,%.2f,%.2f) scale=(%.4f,%.4f,%.4f) weighted=%d",
                name.c_str(), pendingParent != ECS::INVALID_ENTITY ? "yes" : "NO",
                xfDiag->position.x, xfDiag->position.y, xfDiag->position.z,
                xfDiag->scale.x, xfDiag->scale.y, xfDiag->scale.z,
                nodeMeshesWeighted ? 1 : 0);
        }
    }

    // Recursively create child entities — parenting happens at creation time via
    // pendingParent (survives skipped intermediate nodes).
    for (i32 childIndex : node.children) {
        CreateEntityFromAssimpNode(scene, childIndex, world, options,
                                   outEntities, stats, skelCtx, entity);
    }

    return entity;
}

} // namespace Assets
} // namespace Enjin
