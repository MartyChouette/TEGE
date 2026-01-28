#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Assets/AssimpLoader.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"
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
                meshComp.vertices.push_back(vertex);
            }

            // Add indices (offset by current vertex count)
            for (u32 index : primitive.indices) {
                meshComp.indices.push_back(index + vertexOffset);
            }

            vertexOffset += static_cast<u32>(primitive.vertices.size());
        }

        if (!meshComp.vertices.empty()) {
            world->AddComponent<ECS::MeshComponent>(entity, std::move(meshComp));
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
            world->AddComponent<ECS::MeshComponent>(entity, std::move(meshComp));

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
