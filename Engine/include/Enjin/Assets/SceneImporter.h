#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

// Import options for scene loading
struct ImportOptions {
    f32 scale = 1.0f;
    bool importMaterials = true;
    bool importLights = true;
    bool generateColliders = false;
};

// Result of a scene import operation
struct ImportResult {
    bool success = false;
    std::vector<ECS::Entity> entities;
    ECS::Entity rootEntity = ECS::INVALID_ENTITY;
    std::string errorMessage;
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

private:
    // Create entities from glTF nodes recursively
    static ECS::Entity CreateEntityFromNode(const GLTFScene& scene, i32 nodeIndex,
                                             ECS::World* world, const ImportOptions& options,
                                             std::vector<ECS::Entity>& outEntities);

    // Create entities from Assimp nodes recursively
    static ECS::Entity CreateEntityFromAssimpNode(const AssimpScene& scene, i32 nodeIndex,
                                                   ECS::World* world, const ImportOptions& options,
                                                   std::vector<ECS::Entity>& outEntities);
};

} // namespace Assets
} // namespace Enjin
