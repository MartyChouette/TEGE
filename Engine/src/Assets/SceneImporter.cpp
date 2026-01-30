#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Logging/Log.h"
#include <cfloat>
#include <filesystem>
#include <algorithm>

namespace Enjin {
namespace Assets {

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

    // Create entities from root nodes
    for (i32 rootIndex : scene.rootNodes) {
        ECS::Entity entity = CreateEntityFromNode(scene, rootIndex, world, options, result.entities);
        if (result.rootEntity == ECS::INVALID_ENTITY) {
            result.rootEntity = entity;
        }
    }

    result.success = true;
    ENJIN_LOG_INFO(Asset, "Imported %zu entities from %s", result.entities.size(), filepath.c_str());
    return result;
}

ECS::Entity SceneImporter::CreateEntityFromNode(const GLTFScene& scene, i32 nodeIndex,
                                                  ECS::World* world, const ImportOptions& options,
                                                  std::vector<ECS::Entity>& outEntities) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(scene.nodes.size())) {
        return ECS::INVALID_ENTITY;
    }

    const GLTFNode& node = scene.nodes[nodeIndex];

    // Create entity
    ECS::Entity entity = world->CreateEntity();
    outEntities.push_back(entity);

    // Add name component
    std::string name = node.name.empty() ? "Node_" + std::to_string(nodeIndex) : node.name;
    world->AddComponent<ECS::NameComponent>(entity, name);

    // Add transform component
    auto& transform = world->AddComponent<ECS::TransformComponent>(entity);
    transform.position = node.translation * options.scale;
    transform.rotation = node.rotation;
    transform.scale = node.scale * options.scale;

    // Add mesh component if node has a mesh
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<i32>(scene.meshes.size())) {
        const GLTFMesh& gltfMesh = scene.meshes[node.meshIndex];

        // For simplicity, combine all primitives into one mesh component
        // A more advanced implementation would create sub-entities for each primitive
        ECS::MeshComponent meshComp;

        u32 vertexOffset = 0;
        for (const auto& primitive : gltfMesh.primitives) {
            // Add vertices
            for (const auto& gltfVert : primitive.vertices) {
                ECS::MeshComponent::Vertex vertex;
                vertex.position = gltfVert.position;
                vertex.normal = gltfVert.normal;
                vertex.uv = gltfVert.texCoord;
                vertex.tangent = gltfVert.tangent;
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

            vertexOffset += static_cast<u32>(primitive.vertices.size());
        }

        if (!meshComp.vertices.empty()) {
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

            world->AddComponent<ECS::MeshComponent>(entity, std::move(meshComp));

            // Add box collider from mesh AABB
            auto& collider = world->AddComponent<ECS::BoxColliderComponent>(entity);
            collider.center = (minBounds + maxBounds) * 0.5f;
            collider.size = maxBounds - minBounds;
        }
    }

    // Set up skeleton and animator if this node has a skin
    if (node.skinIndex >= 0 && node.skinIndex < static_cast<i32>(scene.skins.size())) {
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
                        track->positionTimes = channel.times;
                        track->positions.resize(channel.times.size());
                        for (usize k = 0; k < channel.times.size(); ++k) {
                            track->positions[k] = Math::Vector3(
                                channel.values[k * 3 + 0],
                                channel.values[k * 3 + 1],
                                channel.values[k * 3 + 2]);
                        }
                        break;
                    }
                    case GLTFAnimationChannel::Path::Rotation: {
                        track->rotationTimes = channel.times;
                        track->rotations.resize(channel.times.size());
                        for (usize k = 0; k < channel.times.size(); ++k) {
                            track->rotations[k] = Math::Quaternion(
                                channel.values[k * 4 + 0],
                                channel.values[k * 4 + 1],
                                channel.values[k * 4 + 2],
                                channel.values[k * 4 + 3]);
                        }
                        break;
                    }
                    case GLTFAnimationChannel::Path::Scale: {
                        track->scaleTimes = channel.times;
                        track->scales.resize(channel.times.size());
                        for (usize k = 0; k < channel.times.size(); ++k) {
                            track->scales[k] = Math::Vector3(
                                channel.values[k * 3 + 0],
                                channel.values[k * 3 + 1],
                                channel.values[k * 3 + 2]);
                        }
                        break;
                    }
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
                scene.animations[0].name.c_str(), node.name.c_str());
        }
    }

    // Recursively create child entities
    for (i32 childIndex : node.children) {
        CreateEntityFromNode(scene, childIndex, world, options, outEntities);
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

    // Create entities from root nodes
    for (i32 rootIndex : scene.rootNodes) {
        ECS::Entity entity = CreateEntityFromAssimpNode(scene, rootIndex, world, options, result.entities);
        if (result.rootEntity == ECS::INVALID_ENTITY) {
            result.rootEntity = entity;
        }
    }

    result.success = true;
    ENJIN_LOG_INFO(Asset, "Imported %zu entities from %s (via Assimp)", result.entities.size(), filepath.c_str());
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

    // Use Assimp for all other supported formats
    if (AssimpLoader::IsSupported(filepath)) {
        return ImportAssimp(filepath, world, options);
    }

    ImportResult result;
    result.errorMessage = "Unsupported file format: " + ext;
    return result;
}

ECS::Entity SceneImporter::CreateEntityFromAssimpNode(const AssimpScene& scene, i32 nodeIndex,
                                                       ECS::World* world, const ImportOptions& options,
                                                       std::vector<ECS::Entity>& outEntities) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<i32>(scene.nodes.size())) {
        return ECS::INVALID_ENTITY;
    }

    const AssimpNode& node = scene.nodes[nodeIndex];

    // Create entity
    ECS::Entity entity = world->CreateEntity();
    outEntities.push_back(entity);

    // Add name component
    std::string name = node.name.empty() ? "Node_" + std::to_string(nodeIndex) : node.name;
    world->AddComponent<ECS::NameComponent>(entity, name);

    // Add transform component
    auto& transform = world->AddComponent<ECS::TransformComponent>(entity);
    transform.position = node.translation * options.scale;
    transform.rotation = node.rotation;
    transform.scale = node.scale * options.scale;

    // Add mesh component if node has a mesh
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<i32>(scene.meshes.size())) {
        const AssimpMesh& assimpMesh = scene.meshes[node.meshIndex];

        // For simplicity, combine all primitives into one mesh component
        ECS::MeshComponent meshComp;

        u32 vertexOffset = 0;
        i32 materialIndex = -1;

        for (const auto& primitive : assimpMesh.primitives) {
            // Track material index
            if (materialIndex < 0 && primitive.materialIndex >= 0) {
                materialIndex = primitive.materialIndex;
            }

            // Add vertices
            for (const auto& assimpVert : primitive.vertices) {
                ECS::MeshComponent::Vertex vertex;
                vertex.position = assimpVert.position;
                vertex.normal = assimpVert.normal;
                vertex.uv = assimpVert.texCoord;
                meshComp.vertices.push_back(vertex);
            }

            // Add indices (offset by current vertex count)
            for (u32 index : primitive.indices) {
                meshComp.indices.push_back(index + vertexOffset);
            }

            vertexOffset += static_cast<u32>(primitive.vertices.size());
        }

        if (!meshComp.vertices.empty()) {
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

            world->AddComponent<ECS::MeshComponent>(entity, std::move(meshComp));

            // Add box collider from mesh AABB
            auto& collider = world->AddComponent<ECS::BoxColliderComponent>(entity);
            collider.center = (minBounds + maxBounds) * 0.5f;
            collider.size = maxBounds - minBounds;

            // Add material component if available and options allow
            if (options.importMaterials && materialIndex >= 0 &&
                materialIndex < static_cast<i32>(scene.materials.size())) {
                const AssimpMaterial& assimpMat = scene.materials[materialIndex];

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

                world->AddComponent<ECS::MaterialComponent>(entity, matComp);
            }
        }
    }

    // Recursively create child entities
    for (i32 childIndex : node.children) {
        CreateEntityFromAssimpNode(scene, childIndex, world, options, outEntities);
    }

    return entity;
}

} // namespace Assets
} // namespace Enjin
