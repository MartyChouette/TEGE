#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/Logging/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <functional>
#include <unordered_map>

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

    // Configure post-processing flags.
    // IMPORTANT: Use GenNormals (not GenSmoothNormals) — GenSmoothNormals OVERWRITES
    // hand-crafted normals from DCC tools (Blender, Maya, Mixamo). GenNormals only
    // generates normals when the mesh has none, preserving artist-authored normals.
    unsigned int flags =
        aiProcess_Triangulate |           // Triangulate all faces
        aiProcess_GenNormals |            // Generate normals ONLY if missing (preserves existing)
        aiProcess_CalcTangentSpace |      // Calculate tangents if missing
        aiProcess_JoinIdenticalVertices | // Optimize mesh
        aiProcess_FlipUVs |               // Flip V for Vulkan/OpenGL (origin bottom-left)
        aiProcess_LimitBoneWeights |      // Max 4 bones per vertex for GPU skinning
        aiProcess_ValidateDataStructure | // Validate the imported data
        aiProcess_PopulateArmatureData;   // Associate meshes with armatures for skinned models

    const aiScene* scene = importer.ReadFile(filepath, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        s_LastError = "Assimp error: " + std::string(importer.GetErrorString());
        ENJIN_LOG_ERROR(Asset, "Failed to load model: %s - %s", filepath.c_str(), s_LastError.c_str());
        return false;
    }

    // Get base path for textures
    std::filesystem::path path(filepath);
    outScene.basePath = path.parent_path().string();

    // Extract creator/DCC tool and unit scale from scene metadata (FBX stores this)
    if (scene->mMetaData) {
        aiString creatorStr;
        if (scene->mMetaData->Get("Creator", creatorStr)) {
            outScene.creator = creatorStr.C_Str();
        }
        if (outScene.creator.empty()) {
            aiString appStr;
            if (scene->mMetaData->Get("SourceAsset_Generator", appStr)) {
                outScene.creator = appStr.C_Str();
            }
        }
        // FBX UnitScaleFactor: 1.0 = cm (Mixamo, Maya default), 100.0 = meters
        double unitScale = 1.0;
        if (scene->mMetaData->Get("UnitScaleFactor", unitScale)) {
            outScene.unitScaleFactor = static_cast<f32>(unitScale);
        }
    }

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

        // Textures — extract embedded textures (paths starting with '*') to disk
        // so the render system can load them as normal image files.
        auto resolveEmbedded = [&](const std::string& rawPath) -> std::string {
            if (rawPath.empty() || rawPath[0] != '*') return rawPath;
            const aiTexture* embTex = scene->GetEmbeddedTexture(rawPath.c_str());
            if (!embTex) return rawPath;

            // Determine extension from the embedded texture hint
            std::string ext = embTex->achFormatHint[0] ? std::string(".") + embTex->achFormatHint : ".png";
            // Save next to the model file: modelname_texN.ext
            namespace fs = std::filesystem;
            std::string baseName = fs::path(filepath).stem().string();
            std::string outName = baseName + "_tex" + rawPath.substr(1) + ext;
            fs::path outPath = fs::path(outScene.basePath) / outName;

            if (!fs::exists(outPath)) {
                std::ofstream ofs(outPath, std::ios::binary);
                if (ofs.is_open()) {
                    if (embTex->mHeight == 0) {
                        // Compressed (PNG/JPG): mWidth = byte count
                        ofs.write(reinterpret_cast<const char*>(embTex->pcData), embTex->mWidth);
                    } else {
                        // Raw RGBA: mWidth * mHeight * 4 bytes
                        ofs.write(reinterpret_cast<const char*>(embTex->pcData),
                                  embTex->mWidth * embTex->mHeight * 4);
                    }
                    ENJIN_LOG_INFO(Asset, "Extracted embedded texture '%s' -> %s",
                        rawPath.c_str(), outPath.string().c_str());
                }
            }
            return outPath.string();
        };

        aiString texPath;
        if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS ||
            aiMat->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS) {
            mat.baseColorTexture = resolveEmbedded(texPath.C_Str());
        }
        if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS ||
            aiMat->GetTexture(aiTextureType_HEIGHT, 0, &texPath) == AI_SUCCESS) {
            mat.normalTexture = resolveEmbedded(texPath.C_Str());
        }
        if (aiMat->GetTexture(aiTextureType_METALNESS, 0, &texPath) == AI_SUCCESS) {
            mat.metallicRoughnessTexture = resolveEmbedded(texPath.C_Str());
        }
        if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &texPath) == AI_SUCCESS) {
            mat.emissiveTexture = resolveEmbedded(texPath.C_Str());
        }
    }

    // Load meshes
    std::unordered_map<std::string, u32> boneNameToGlobal; // Dedup bones across all meshes
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

            // Vertex colors (first color set)
            if (aiMeshData->HasVertexColors(0)) {
                const auto& c = aiMeshData->mColors[0][v];
                vertex.color = Math::Vector4(c.r, c.g, c.b, c.a);
                vertex.hasColor = true;
            }

            primitive.vertices.push_back(vertex);
        }

        // Extract bone weights from aiMesh::mBones
        if (aiMeshData->HasBones()) {
            for (unsigned int b = 0; b < aiMeshData->mNumBones; ++b) {
                const aiBone* aiBoneData = aiMeshData->mBones[b];
                std::string boneName = aiBoneData->mName.C_Str();

                // Look up or insert into global bone table (dedup across meshes)
                u32 globalBoneIdx;
                auto it = boneNameToGlobal.find(boneName);
                if (it != boneNameToGlobal.end()) {
                    globalBoneIdx = it->second;
                } else {
                    globalBoneIdx = static_cast<u32>(outScene.bones.size());
                    boneNameToGlobal[boneName] = globalBoneIdx;

                    AssimpBone bone;
                    bone.name = boneName;
                    // Assimp aiMatrix4x4 is row-major: a1-a4 = row 0, b1-b4 = row 1.
                    // Our Matrix4 constructor takes row-major conceptual order and
                    // stores column-major for GPU. Pass Assimp rows directly.
                    // NO transpose needed — both Assimp and our shader use the same
                    // column-vector convention (M * v) for the final GPU operation.
                    const aiMatrix4x4& m = aiBoneData->mOffsetMatrix;
                    bone.offsetMatrix = Math::Matrix4(
                        m.a1, m.a2, m.a3, m.a4,   // Row 0
                        m.b1, m.b2, m.b3, m.b4,   // Row 1
                        m.c1, m.c2, m.c3, m.c4,   // Row 2
                        m.d1, m.d2, m.d3, m.d4    // Row 3
                    );
                    outScene.bones.push_back(bone);
                }

                // Assign bone weights to vertices
                for (unsigned int w = 0; w < aiBoneData->mNumWeights; ++w) {
                    unsigned int vertId = aiBoneData->mWeights[w].mVertexId;
                    f32 weight = aiBoneData->mWeights[w].mWeight;
                    if (vertId >= primitive.vertices.size()) continue;

                    AssimpVertex& vert = primitive.vertices[vertId];
                    // Find next free slot (0-3) via weight array access
                    f32* weights = &vert.boneWeights.x;
                    for (int slot = 0; slot < 4; ++slot) {
                        if (weights[slot] == 0.0f) {
                            weights[slot] = weight;
                            vert.boneIndices[slot] = globalBoneIdx;
                            break;
                        }
                    }
                }
            }
            outScene.hasSkinning = true;

            // Normalize bone weights so they sum to 1.0 per vertex.
            // LimitBoneWeights caps at 4 but doesn't renormalize the remaining weights.
            // Without normalization, skinned vertices can appear too dark or too bright.
            for (auto& vert : primitive.vertices) {
                f32 sum = vert.boneWeights.x + vert.boneWeights.y +
                          vert.boneWeights.z + vert.boneWeights.w;
                if (sum > 0.0001f && std::abs(sum - 1.0f) > 0.001f) {
                    f32 invSum = 1.0f / sum;
                    vert.boneWeights.x *= invSum;
                    vert.boneWeights.y *= invSum;
                    vert.boneWeights.z *= invSum;
                    vert.boneWeights.w *= invSum;
                }
            }
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
        outScene.nodes[nodeIdx].parentIndex = parentIdx;

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

    // Extract animations from aiScene::mAnimations
    for (unsigned int a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* aiAnim = scene->mAnimations[a];

        AssimpAnimation anim;
        anim.name = aiAnim->mName.C_Str();
        if (anim.name.empty()) {
            anim.name = "Animation_" + std::to_string(a);
        }

        anim.ticksPerSecond = (aiAnim->mTicksPerSecond > 0.0) ?
            static_cast<f32>(aiAnim->mTicksPerSecond) : 25.0f;
        anim.duration = static_cast<f32>(aiAnim->mDuration) / anim.ticksPerSecond;

        for (unsigned int c = 0; c < aiAnim->mNumChannels; ++c) {
            const aiNodeAnim* aiChannel = aiAnim->mChannels[c];

            AssimpAnimChannel channel;
            channel.nodeName = aiChannel->mNodeName.C_Str();

            // Position keys
            channel.positionTimes.reserve(aiChannel->mNumPositionKeys);
            channel.positions.reserve(aiChannel->mNumPositionKeys);
            for (unsigned int k = 0; k < aiChannel->mNumPositionKeys; ++k) {
                channel.positionTimes.push_back(
                    static_cast<f32>(aiChannel->mPositionKeys[k].mTime) / anim.ticksPerSecond);
                const aiVector3D& v = aiChannel->mPositionKeys[k].mValue;
                channel.positions.push_back(Math::Vector3(v.x, v.y, v.z));
            }

            // Rotation keys
            channel.rotationTimes.reserve(aiChannel->mNumRotationKeys);
            channel.rotations.reserve(aiChannel->mNumRotationKeys);
            for (unsigned int k = 0; k < aiChannel->mNumRotationKeys; ++k) {
                channel.rotationTimes.push_back(
                    static_cast<f32>(aiChannel->mRotationKeys[k].mTime) / anim.ticksPerSecond);
                const aiQuaternion& q = aiChannel->mRotationKeys[k].mValue;
                channel.rotations.push_back(Math::Quaternion(q.x, q.y, q.z, q.w));
            }

            // Scale keys
            channel.scaleTimes.reserve(aiChannel->mNumScalingKeys);
            channel.scales.reserve(aiChannel->mNumScalingKeys);
            for (unsigned int k = 0; k < aiChannel->mNumScalingKeys; ++k) {
                channel.scaleTimes.push_back(
                    static_cast<f32>(aiChannel->mScalingKeys[k].mTime) / anim.ticksPerSecond);
                const aiVector3D& v = aiChannel->mScalingKeys[k].mValue;
                channel.scales.push_back(Math::Vector3(v.x, v.y, v.z));
            }

            anim.channels.push_back(std::move(channel));
        }

        outScene.animations.push_back(std::move(anim));
    }

    ENJIN_LOG_INFO(Asset, "Loaded model: %s (%zu meshes, %zu materials, %zu nodes, %zu bones, %zu animations)",
        filepath.c_str(), outScene.meshes.size(), outScene.materials.size(),
        outScene.nodes.size(), outScene.bones.size(), outScene.animations.size());

    return true;
}

const std::string& AssimpLoader::GetLastError() {
    return s_LastError;
}

} // namespace Assets
} // namespace Enjin
