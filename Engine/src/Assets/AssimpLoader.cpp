#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Logging/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <algorithm>
#include <functional>

namespace Enjin {
namespace Assets {

std::string AssimpLoader::s_LastError;

bool AssimpLoader::IsSupported(const std::string& filepath) {
    std::filesystem::path path(filepath);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Common supported formats
    static const std::vector<std::string> supportedExtensions = {
        ".fbx", ".obj", ".dae", ".3ds", ".blend", ".ase",
        ".ifc", ".xgl", ".zgl", ".ply", ".lwo", ".lws",
        ".lxo", ".stl", ".x", ".ac", ".ms3d", ".cob",
        ".scn", ".bvh", ".csm", ".irrmesh", ".irr",
        ".mdl", ".md2", ".md3", ".md5mesh", ".md5anim",
        ".q3o", ".q3s", ".raw", ".nff", ".off", ".ter",
        ".hmp", ".mesh.xml", ".skeleton.xml", ".material"
    };

    for (const auto& supported : supportedExtensions) {
        if (ext == supported) return true;
    }
    return false;
}

std::vector<std::string> AssimpLoader::GetSupportedExtensions() {
    return {
        ".fbx", ".obj", ".dae", ".3ds", ".blend", ".ase",
        ".stl", ".ply", ".x", ".ms3d"
    };
}

bool AssimpLoader::Load(const std::string& filepath, AssimpScene& outScene) {
    Assimp::Importer importer;

    // Configure post-processing flags
    unsigned int flags =
        aiProcess_Triangulate |           // Triangulate all faces
        aiProcess_GenSmoothNormals |      // Generate smooth normals if missing
        aiProcess_CalcTangentSpace |      // Calculate tangents
        aiProcess_JoinIdenticalVertices | // Optimize mesh
        aiProcess_FlipUVs |               // Flip UVs for OpenGL/Vulkan
        aiProcess_LimitBoneWeights |      // Limit bone weights
        aiProcess_ValidateDataStructure;  // Validate the data

    const aiScene* scene = importer.ReadFile(filepath, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        s_LastError = "Assimp error: " + std::string(importer.GetErrorString());
        ENJIN_LOG_ERROR(Asset, "Failed to load model: %s - %s", filepath.c_str(), s_LastError.c_str());
        return false;
    }

    // Get base path for textures
    std::filesystem::path path(filepath);
    outScene.basePath = path.parent_path().string();

    // Load materials
    outScene.materials.resize(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        const aiMaterial* aiMat = scene->mMaterials[i];
        AssimpMaterial& mat = outScene.materials[i];

        // Name
        aiString name;
        if (aiMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            mat.name = name.C_Str();
        }

        // Base color / diffuse
        aiColor4D color;
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            mat.baseColorFactor = Math::Vector4(color.r, color.g, color.b, color.a);
        }
        if (aiMat->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS) {
            mat.baseColorFactor = Math::Vector4(color.r, color.g, color.b, color.a);
        }

        // Opacity
        float opacity = 1.0f;
        if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
            mat.opacity = opacity;
        }

        // Metallic/roughness
        float metallic = 0.0f, roughness = 0.5f;
        aiMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
        aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
        mat.metallicFactor = metallic;
        mat.roughnessFactor = roughness;

        // Shininess to roughness conversion (for non-PBR materials)
        float shininess = 0.0f;
        if (aiMat->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS && shininess > 0.0f) {
            // Convert shininess to roughness (approximate)
            mat.roughnessFactor = 1.0f - std::min(1.0f, shininess / 100.0f);
        }

        // Emissive
        aiColor3D emissive;
        if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
            mat.emissiveFactor = Math::Vector3(emissive.r, emissive.g, emissive.b);
        }

        // Two-sided
        int twoSided = 0;
        if (aiMat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS) {
            mat.doubleSided = (twoSided != 0);
        }

        // Textures
        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS ||
            aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
            mat.baseColorTexture = texPath.C_Str();
        }
        if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
            aiMat->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS) {
            mat.normalTexture = texPath.C_Str();
        }
        if (aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS) {
            mat.metallicRoughnessTexture = texPath.C_Str();
        }
        if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS) {
            mat.emissiveTexture = texPath.C_Str();
        }
    }

    // Load meshes
    outScene.meshes.resize(scene->mNumMeshes);
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* aiMeshData = scene->mMeshes[i];
        AssimpMesh& mesh = outScene.meshes[i];

        mesh.name = aiMeshData->mName.C_Str();

        // Create single primitive for this mesh
        AssimpPrimitive primitive;
        primitive.materialIndex = static_cast<i32>(aiMeshData->mMaterialIndex);

        // Vertices
        primitive.vertices.reserve(aiMeshData->mNumVertices);
        for (unsigned int v = 0; v < aiMeshData->mNumVertices; ++v) {
            AssimpVertex vertex;

            // Position
            vertex.position = Math::Vector3(
                aiMeshData->mVertices[v].x,
                aiMeshData->mVertices[v].y,
                aiMeshData->mVertices[v].z
            );

            // Normal
            if (aiMeshData->HasNormals()) {
                vertex.normal = Math::Vector3(
                    aiMeshData->mNormals[v].x,
                    aiMeshData->mNormals[v].y,
                    aiMeshData->mNormals[v].z
                );
            }

            // Texture coordinates (first set)
            if (aiMeshData->HasTextureCoords(0)) {
                vertex.texCoord = Math::Vector2(
                    aiMeshData->mTextureCoords[0][v].x,
                    aiMeshData->mTextureCoords[0][v].y
                );
            }

            // Tangent
            if (aiMeshData->HasTangentsAndBitangents()) {
                vertex.tangent = Math::Vector4(
                    aiMeshData->mTangents[v].x,
                    aiMeshData->mTangents[v].y,
                    aiMeshData->mTangents[v].z,
                    1.0f
                );
            }

            primitive.vertices.push_back(vertex);
        }

        // Indices
        primitive.indices.reserve(aiMeshData->mNumFaces * 3);
        for (unsigned int f = 0; f < aiMeshData->mNumFaces; ++f) {
            const aiFace& face = aiMeshData->mFaces[f];
            for (unsigned int idx = 0; idx < face.mNumIndices; ++idx) {
                primitive.indices.push_back(face.mIndices[idx]);
            }
        }

        mesh.primitives.push_back(std::move(primitive));
    }

    // Process node hierarchy
    // NOTE: We must use indices (not references) to access outScene.nodes after
    // recursive calls, because emplace_back() can reallocate the vector and
    // invalidate all references/pointers to its elements.
    std::function<i32(aiNode*, i32)> processNode = [&](aiNode* node, i32 parentIdx) -> i32 {
        i32 nodeIdx = static_cast<i32>(outScene.nodes.size());
        outScene.nodes.emplace_back();

        // Populate node data via index (safe across reallocation)
        outScene.nodes[nodeIdx].name = node->mName.C_Str();

        // Decompose transformation matrix
        aiVector3D scale, position;
        aiQuaternion rotation;
        node->mTransformation.Decompose(scale, rotation, position);

        outScene.nodes[nodeIdx].translation = Math::Vector3(position.x, position.y, position.z);
        outScene.nodes[nodeIdx].rotation = Math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
        outScene.nodes[nodeIdx].scale = Math::Vector3(scale.x, scale.y, scale.z);

        // Mesh references (store all meshes for this node)
        for (unsigned int m = 0; m < node->mNumMeshes; ++m) {
            outScene.nodes[nodeIdx].meshIndices.push_back(static_cast<i32>(node->mMeshes[m]));
        }
        if (!outScene.nodes[nodeIdx].meshIndices.empty()) {
            outScene.nodes[nodeIdx].meshIndex = outScene.nodes[nodeIdx].meshIndices[0];
        }

        // Process children (vector may reallocate, so always use index)
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            i32 childIdx = processNode(node->mChildren[i], nodeIdx);
            outScene.nodes[nodeIdx].children.push_back(childIdx);
        }

        return nodeIdx;
    };

    // Process root node
    if (scene->mRootNode) {
        i32 rootIdx = processNode(scene->mRootNode, -1);
        outScene.rootNodes.push_back(rootIdx);
    }

    ENJIN_LOG_INFO(Asset, "Loaded model: %s (%zu meshes, %zu materials, %zu nodes)",
        filepath.c_str(), outScene.meshes.size(), outScene.materials.size(), outScene.nodes.size());

    return true;
}

const std::string& AssimpLoader::GetLastError() {
    return s_LastError;
}

} // namespace Assets
} // namespace Enjin
