#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Assets/TextureCompressor.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>

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

// Import options for scene loading
struct ImportOptions {
    f32 scale = 1.0f;
    bool importMaterials = true;
    bool importLights = true;
    bool importAnimations = true;
    bool generateColliders = true;
    bool generateLODs = false;  // Off by default — LOD generation is expensive for large meshes

    // Source application preset
    SourceApp sourceApp = SourceApp::Auto;
    bool convertAxes = true;     // Apply axis conversion from source app preset
    bool flipX = false;          // Per-axis sign overrides
    bool flipY = false;
    bool flipZ = false;
    std::vector<std::string> textureSearchPaths;  // Additional dirs to search for textures

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
};

} // namespace Assets
} // namespace Enjin
