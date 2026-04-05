#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/Math/Matrix.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

// Vertex data from glTF mesh
struct GLTFVertex {
    Math::Vector3 position;
    Math::Vector3 normal;
    Math::Vector2 texCoord;
    Math::Vector4 tangent;
    Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);  // Vertex color (RGBA)
    bool hasColor = false;
    // Skeletal animation bone data
    Math::Vector4 boneWeights;
    u32 boneIndices[4] = {0, 0, 0, 0};
};

// Morph target delta data (per-vertex)
struct GLTFMorphTarget {
    std::string name;
    std::vector<Math::Vector3> positionDeltas;
    std::vector<Math::Vector3> normalDeltas;
};

// Primitive/submesh data
struct GLTFPrimitive {
    std::vector<GLTFVertex> vertices;
    std::vector<u32> indices;
    i32 materialIndex = -1;
    std::vector<GLTFMorphTarget> morphTargets;
};

// Mesh containing one or more primitives
struct GLTFMesh {
    std::string name;
    std::vector<GLTFPrimitive> primitives;
    std::vector<f32> defaultMorphWeights;  // Default weights for morph targets
};

// Material data
struct GLTFMaterial {
    std::string name;
    Math::Vector4 baseColorFactor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    f32 metallicFactor = 1.0f;
    f32 roughnessFactor = 1.0f;
    Math::Vector3 emissiveFactor = Math::Vector3(0.0f);

    // Texture indices (-1 means no texture)
    i32 baseColorTextureIndex = -1;
    i32 metallicRoughnessTextureIndex = -1;
    i32 normalTextureIndex = -1;
    i32 occlusionTextureIndex = -1;
    i32 emissiveTextureIndex = -1;

    bool doubleSided = false;
    f32 alphaCutoff = 0.5f;
    enum class AlphaMode { Opaque, Mask, Blend } alphaMode = AlphaMode::Opaque;
};

// Image/texture reference
struct GLTFImage {
    std::string uri;           // File path or embedded data URI
    std::string mimeType;
    std::vector<u8> data;      // Embedded image data (if any)
};

// Node in the scene hierarchy
struct GLTFNode {
    std::string name;
    i32 meshIndex = -1;
    i32 skinIndex = -1;
    Math::Vector3 translation = Math::Vector3(0.0f);
    Math::Quaternion rotation = Math::Quaternion::Identity();
    Math::Vector3 scale = Math::Vector3(1.0f);
    std::vector<i32> children;
};

// Skin data (skeleton binding)
struct GLTFSkin {
    std::string name;
    std::vector<i32> jointNodeIndices;
    std::vector<Math::Matrix4> inverseBindMatrices;
    i32 skeletonRootNode = -1;
};

// Animation channel (single property track for a target node)
struct GLTFAnimationChannel {
    i32 targetNode = -1;
    enum class Path : u8 { Translation, Rotation, Scale, Weights } path = Path::Translation;
    std::vector<f32> times;
    std::vector<f32> values;  // 3 floats for T/S, 4 floats for R (xyzw)
};

// Complete animation clip
struct GLTFAnimation {
    std::string name;
    f32 duration = 0.0f;
    std::vector<GLTFAnimationChannel> channels;
};

// Complete glTF scene data
struct GLTFScene {
    std::vector<GLTFMesh> meshes;
    std::vector<GLTFMaterial> materials;
    std::vector<GLTFImage> images;
    std::vector<GLTFNode> nodes;
    std::vector<i32> rootNodes;
    std::vector<GLTFSkin> skins;
    std::vector<GLTFAnimation> animations;
    std::string basePath;    // Directory containing the glTF file
    std::string generator;   // DCC tool that created this file (from asset.generator)
};

// glTF loader class
class ENJIN_API GLTFLoader {
public:
    // Load a glTF file (.gltf or .glb)
    static bool Load(const std::string& filepath, GLTFScene& outScene);

    // Get the last error message
    static const std::string& GetLastError();

private:
    static std::string s_LastError;
};

} // namespace Assets
} // namespace Enjin
