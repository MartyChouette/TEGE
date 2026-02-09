#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Quaternion.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

// Vertex data from Assimp mesh
struct AssimpVertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector2 texCoord;
    Math::Vector4 tangent;
};

// Primitive/submesh data
struct AssimpPrimitive {
    std::vector<AssimpVertex> vertices;
    std::vector<u32> indices;
    i32 materialIndex = -1;
};

// Mesh containing one or more primitives
struct AssimpMesh {
    std::string name;
    std::vector<AssimpPrimitive> primitives;
};

// Material data
struct AssimpMaterial {
    std::string name;
    Math::Vector4 baseColorFactor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    f32 metallicFactor = 0.0f;
    f32 roughnessFactor = 0.5f;
    Math::Vector3 emissiveFactor = Math::Vector3(0.0f);

    // Texture paths (empty means no texture)
    std::string baseColorTexture;
    std::string normalTexture;
    std::string metallicRoughnessTexture;
    std::string emissiveTexture;

    bool doubleSided = false;
    f32 alphaCutoff = 0.5f;
    f32 opacity = 1.0f;
};

// Node in the scene hierarchy
struct AssimpNode {
    std::string name;
    i32 meshIndex = -1;                    // Primary mesh (first mesh, for backward compat)
    std::vector<i32> meshIndices;          // All meshes referenced by this node
    Math::Vector3 translation = Math::Vector3(0.0f);
    Math::Quaternion rotation = Math::Quaternion::Identity();
    Math::Vector3 scale = Math::Vector3(1.0f);
    std::vector<i32> children;
};

// Complete scene data loaded via Assimp
struct AssimpScene {
    std::vector<AssimpMesh> meshes;
    std::vector<AssimpMaterial> materials;
    std::vector<AssimpNode> nodes;
    std::vector<i32> rootNodes;
    std::string basePath;   // Directory containing the file
    std::string creator;    // DCC tool that created this file (from FBX metadata)
};

// Assimp loader class - supports FBX, OBJ, DAE, 3DS, and more
class ENJIN_API AssimpLoader {
public:
    // Supported file formats
    static bool IsSupported(const std::string& filepath);

    // Load a model file (FBX, OBJ, DAE, etc.)
    static bool Load(const std::string& filepath, AssimpScene& outScene);

    // Get the last error message
    static const std::string& GetLastError();

    // Get list of supported extensions
    static std::vector<std::string> GetSupportedExtensions();

private:
    static std::string s_LastError;
};

} // namespace Assets
} // namespace Enjin
