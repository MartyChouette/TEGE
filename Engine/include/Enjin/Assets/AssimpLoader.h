#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Matrix.h"
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
    Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);  // Vertex color (RGBA)
    bool hasColor = false;
    // Skeletal animation bone data
    Math::Vector4 boneWeights = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    u32 boneIndices[4] = {0, 0, 0, 0};
};

// Morph target delta data (per-vertex)
struct AssimpMorphTarget {
    std::string name;
    std::vector<Math::Vector3> positionDeltas;
    std::vector<Math::Vector3> normalDeltas;
};

// Primitive/submesh data
struct AssimpPrimitive {
    std::vector<AssimpVertex> vertices;
    std::vector<u32> indices;
    i32 materialIndex = -1;
    std::vector<AssimpMorphTarget> morphTargets;
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
    i32 parentIndex = -1;                  // Index into AssimpScene::nodes (-1 = root)
    Math::Vector3 translation = Math::Vector3(0.0f);
    Math::Quaternion rotation = Math::Quaternion::Identity();
    Math::Vector3 scale = Math::Vector3(1.0f);
    std::vector<i32> children;
};

// Bone data extracted from Assimp meshes
struct AssimpBone {
    std::string name;
    Math::Matrix4 offsetMatrix;  // Inverse bind matrix from Assimp
};

// Animation channel for a single node
struct AssimpAnimChannel {
    std::string nodeName;
    std::vector<f32> positionTimes;
    std::vector<Math::Vector3> positions;
    std::vector<f32> rotationTimes;
    std::vector<Math::Quaternion> rotations;
    std::vector<f32> scaleTimes;
    std::vector<Math::Vector3> scales;
};

// Complete animation clip
struct AssimpAnimation {
    std::string name;
    f32 duration = 0.0f;
    f32 ticksPerSecond = 25.0f;
    std::vector<AssimpAnimChannel> channels;
};

// Complete scene data loaded via Assimp
struct AssimpScene {
    std::vector<AssimpMesh> meshes;
    std::vector<AssimpMaterial> materials;
    std::vector<AssimpNode> nodes;
    std::vector<i32> rootNodes;
    std::string basePath;   // Directory containing the file
    std::string creator;    // DCC tool that created this file (from FBX metadata)
    f32 unitScaleFactor = 1.0f; // FBX UnitScaleFactor (cm=1, m=100). Convert: scale = 1/unitScale * 0.01

    // Skeletal animation data
    std::vector<AssimpBone> bones;           // Deduplicated across all meshes
    std::vector<AssimpAnimation> animations;
    bool hasSkinning = false;
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
