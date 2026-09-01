#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Assets/TextureCompressor.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Animation/Animation.h"
#include <string>
#include <vector>
#include <memory>

namespace Enjin {
namespace Assets {

// Source application presets for axis/scale conversion
enum class SourceApp : u8 {
    Auto,               // Auto-detect from file metadata
    Blender,            // Z-up, 1m scale, -X forward
    Maya,               // Y-up, cm (scale 0.01)
    Max3ds,             // Z-up, system-unit auto
    Houdini,            // Y-up, 1m scale
    Cinema4D,           // Y-up, cm (scale 0.01)
    ZBrush,             // Z-up, large scale
    SubstancePainter,   // Texture-centric, Y-up
    Unreal,             // Left-hand, cm (scale 0.01)
    Unity,              // Left-hand, 1m scale
    SketchUp,           // Z-up, inches (scale 0.0254)
    Custom              // User-defined
};

// Preset values for a source application
struct SourceAppPreset {
    f32 scale;
    bool zUpToYUp;      // Needs Z->Y axis swap
    bool leftToRight;   // Needs left->right handedness flip
    const char* name;
};

// Get default preset values for a source app
ENJIN_API SourceAppPreset GetSourceAppPreset(SourceApp app);

// Get the display name for a source app enum value
ENJIN_API const char* GetSourceAppName(SourceApp app);

// Shape of the auto-generated collider for imported meshes. All shapes are
// sized in WORLD space (the entity's import scale is baked in — physics
// backends ignore transform scale).
enum class ImportColliderShape : u8 {
    Box = 0,      // AABB of the mesh (default)
    Sphere,       // bounding sphere (radius = half the largest extent)
    Capsule,      // Y-axis capsule fit to the AABB
    ConvexMesh,   // MeshColliderComponent convex hull from the mesh vertices
};

// Import options for scene loading
struct ImportOptions {
    f32 scale = 1.0f;
    // Normalize the model to a sane on-screen size (~1.8 m largest dimension) when the
    // file's unit is unreliable or the result is absurd. OFF = import at the file's
    // real unit-converted size, no size magic (predictable, you scale it yourself).
    // DEFAULT OFF: the ~1.8 m normalization forced every model to human height, which
    // is wrong for props/coins/buildings/anything non-human-sized (Marty: "auto-scale
    // is usually incorrect"). Trust the file's unit conversion by default; the user
    // opts in to size-magic only when a file's units are genuinely unreliable.
    bool normalizeScale = false;
    bool importMaterials = true;
    bool importLights = true;
    bool importAnimations = true;
    // Start playing the first animation on import. OFF = the model comes in at its
    // bind/rest pose. Default ON — imported rigs animate immediately.
    bool autoPlayAnimation = true;
    bool generateColliders = true;
    ImportColliderShape colliderShape = ImportColliderShape::Box;
    bool generateLODs = false;  // Off by default — LOD generation is expensive for large meshes

    // Source application preset
    SourceApp sourceApp = SourceApp::Auto;
    bool convertAxes = true;     // Apply axis conversion from source app preset
    bool flipX = false;          // Per-axis sign overrides
    bool flipY = false;
    bool flipZ = false;
    std::vector<std::string> textureSearchPaths;  // Additional dirs to search for textures

    // Node filtering — skip specific nodes during import (by index)
    std::vector<i32> excludedNodeIndices;  // Node indices to skip (from import preview)

    // Texture compression settings (applied to imported textures)
    TextureCompressionSettings textureCompression;
};

// Result of a scene import operation
struct ImportResult {
    bool success = false;
    std::vector<ECS::Entity> entities;
    ECS::Entity rootEntity = ECS::INVALID_ENTITY;
    std::string errorMessage;

    // Import statistics
    u32 meshCount = 0;
    u32 materialCount = 0;
    u32 animationCount = 0;
    u32 totalVertexCount = 0;
    u32 totalIndexCount = 0;
    std::vector<std::string> entityNames;
    std::vector<std::string> texturePathsResolved;
    std::vector<std::string> texturePathsMissing;
    std::vector<std::string> warnings;
};

// Imports 3D scenes (glTF, FBX, OBJ, etc.) into the ECS world
class ENJIN_API SceneImporter {
public:
    // Import a glTF file and create entities in the world
    static ImportResult ImportGLTF(const std::string& filepath, ECS::World* world,
                                    const ImportOptions& options = {});

    // Import FBX, OBJ, DAE, or other Assimp-supported formats
    static ImportResult ImportAssimp(const std::string& filepath, ECS::World* world,
                                      const ImportOptions& options = {});

    // Auto-detect format and import (uses file extension)
    static ImportResult Import(const std::string& filepath, ECS::World* world,
                               const ImportOptions& options = {});

    // Internal statistics accumulated during import
    struct ImportStats {
        u32 meshCount = 0;
        u32 materialCount = 0;
        u32 totalVertexCount = 0;
        u32 totalIndexCount = 0;
        std::vector<std::string> entityNames;
        std::vector<std::string> texturePathsResolved;
        std::vector<std::string> texturePathsMissing;
        std::vector<std::string> warnings;
        std::string sourceFilePath;  // Original import file path for skeleton re-import
    };

private:
    // Create entities from glTF nodes recursively
    static ECS::Entity CreateEntityFromNode(const GLTFScene& scene, i32 nodeIndex,
                                             ECS::World* world, const ImportOptions& options,
                                             std::vector<ECS::Entity>& outEntities,
                                             ImportStats& stats);

    // Create entities from Assimp nodes recursively
    static ECS::Entity CreateEntityFromAssimpNode(const AssimpScene& scene, i32 nodeIndex,
                                                   ECS::World* world, const ImportOptions& options,
                                                   std::vector<ECS::Entity>& outEntities,
                                                   ImportStats& stats);

    // Skeleton context passed through Assimp node recursion
    struct AssimpSkeletonContext {
        std::shared_ptr<Animation::Skeleton> skeleton;
        bool attached = false;       // True after skeleton is attached to first skinned node
        ECS::Entity bodyEntity = 0;  // First skinned mesh entity — subsequent meshes merge into this
        f32 unitScale = 1.0f;        // Auto-computed scale (cm→m) for skinned mesh entities
        u64 groupId = 0;             // Stamped onto every co-skeleton mesh so the shared Skeleton
                                     // survives save/load (SkeletonComponent::skeletonGroupId)
        // World transform of the root bone's PARENT node (the "armature" frame the bone
        // hierarchy is relative to). Skinned vertices are baked in full scene space,
        // which differs from this frame — the delta is what breaks animation.
        Math::Matrix4 armatureWorld = Math::Matrix4::Identity();
        Math::Matrix4 armatureWorldInverse = Math::Matrix4::Identity();
    };

    // Overload with skeleton context for skinned mesh import. pendingParent is the
    // entity the created node should attach to — threaded through the recursion so
    // meshes under SKIPPED nodes ($AssimpFbx$ helpers, bone nodes, empty transforms)
    // still land under the import root instead of orphaned at scene root.
    static ECS::Entity CreateEntityFromAssimpNode(const AssimpScene& scene, i32 nodeIndex,
                                                   ECS::World* world, const ImportOptions& options,
                                                   std::vector<ECS::Entity>& outEntities,
                                                   ImportStats& stats,
                                                   AssimpSkeletonContext& skelCtx,
                                                   ECS::Entity pendingParent = ECS::INVALID_ENTITY);
};

} // namespace Assets
} // namespace Enjin
