#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace Enjin {
namespace Scene {

// JSON serialization helpers for math types
namespace {

json SerializeVector2(const Math::Vector2& v) {
    return json::array({v.x, v.y});
}

json SerializeVector3(const Math::Vector3& v) {
    return json::array({v.x, v.y, v.z});
}

json SerializeQuaternion(const Math::Quaternion& q) {
    return json::array({q.x, q.y, q.z, q.w});
}

Math::Vector2 DeserializeVector2(const json& j) {
    return Math::Vector2(j[0].get<f32>(), j[1].get<f32>());
}

Math::Vector3 DeserializeVector3(const json& j) {
    return Math::Vector3(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>());
}

Math::Quaternion DeserializeQuaternion(const json& j) {
    return Math::Quaternion(j[0].get<f32>(), j[1].get<f32>(), j[2].get<f32>(), j[3].get<f32>());
}

// Serialize components
json SerializeNameComponent(const ECS::NameComponent& name) {
    json j;
    j["name"] = name.name;
    return j;
}

json SerializeTransformComponent(const ECS::TransformComponent& transform) {
    json j;
    j["position"] = SerializeVector3(transform.position);
    j["rotation"] = SerializeQuaternion(transform.rotation);
    j["scale"] = SerializeVector3(transform.scale);
    return j;
}

json SerializeMaterialComponent(const ECS::MaterialComponent& material) {
    json j;
    j["baseColor"] = SerializeVector3(material.baseColor);
    j["opacity"] = material.opacity;
    j["metallic"] = material.metallic;
    j["roughness"] = material.roughness;
    j["emissiveColor"] = SerializeVector3(material.emissiveColor);
    j["emissiveStrength"] = material.emissiveStrength;
    j["baseColorTexture"] = material.baseColorTexture;
    j["normalTexture"] = material.normalTexture;
    j["metallicRoughnessTexture"] = material.metallicRoughnessTexture;
    j["emissiveTexture"] = material.emissiveTexture;
    // Texture paths
    j["baseColorTexturePath"] = material.baseColorTexturePath;
    j["normalTexturePath"] = material.normalTexturePath;
    j["metallicRoughnessTexturePath"] = material.metallicRoughnessTexturePath;
    j["emissiveTexturePath"] = material.emissiveTexturePath;
    j["doubleSided"] = material.doubleSided;
    j["castShadows"] = material.castShadows;
    j["receiveShadows"] = material.receiveShadows;
    j["alphaMode"] = static_cast<i32>(material.alphaMode);
    j["alphaCutoff"] = material.alphaCutoff;
    return j;
}

json SerializeMeshComponent(const ECS::MeshComponent& mesh, bool includeVertexData) {
    json j;
    j["vertexCount"] = static_cast<u32>(mesh.vertices.size());
    j["indexCount"] = static_cast<u32>(mesh.indices.size());

    if (includeVertexData) {
        json vertices = json::array();
        for (const auto& v : mesh.vertices) {
            json vertex;
            vertex["position"] = SerializeVector3(v.position);
            vertex["normal"] = SerializeVector3(v.normal);
            vertex["uv"] = SerializeVector2(v.uv);
            vertices.push_back(vertex);
        }
        j["vertices"] = vertices;
        j["indices"] = mesh.indices;
    }

    return j;
}

json SerializeLightComponent(const ECS::LightComponent& light) {
    json j;
    j["type"] = static_cast<i32>(light.type);
    j["color"] = SerializeVector3(light.color);
    j["intensity"] = light.intensity;
    j["range"] = light.range;
    j["constantAttenuation"] = light.constantAttenuation;
    j["linearAttenuation"] = light.linearAttenuation;
    j["quadraticAttenuation"] = light.quadraticAttenuation;
    j["innerConeAngle"] = light.innerConeAngle;
    j["outerConeAngle"] = light.outerConeAngle;
    j["castShadows"] = light.castShadows;
    j["shadowMapResolution"] = light.shadowMapResolution;
    return j;
}

json SerializeNotesComponent(const ECS::NotesComponent& notes) {
    json j;
    j["notes"] = notes.notes;
    return j;
}

json SerializeCameraComponent(const ECS::CameraComponent& camera) {
    json j;
    j["projectionType"] = static_cast<i32>(camera.projectionType);
    j["fieldOfView"] = camera.fieldOfView;
    j["nearPlane"] = camera.nearPlane;
    j["farPlane"] = camera.farPlane;
    j["orthoSize"] = camera.orthoSize;
    j["priority"] = camera.priority;
    j["isActive"] = camera.isActive;
    j["clearDepth"] = camera.clearDepth;
    j["clearColor"] = camera.clearColor;
    j["backgroundColor"] = SerializeVector3(camera.backgroundColor);
    j["viewportX"] = camera.viewportX;
    j["viewportY"] = camera.viewportY;
    j["viewportWidth"] = camera.viewportWidth;
    j["viewportHeight"] = camera.viewportHeight;
    j["cullingMask"] = camera.cullingMask;
    return j;
}

// Deserialize components
ECS::NameComponent DeserializeNameComponent(const json& j) {
    ECS::NameComponent name;
    name.name = j["name"].get<std::string>();
    return name;
}

ECS::TransformComponent DeserializeTransformComponent(const json& j) {
    ECS::TransformComponent transform;
    transform.position = DeserializeVector3(j["position"]);
    transform.rotation = DeserializeQuaternion(j["rotation"]);
    transform.scale = DeserializeVector3(j["scale"]);
    return transform;
}

ECS::MaterialComponent DeserializeMaterialComponent(const json& j) {
    ECS::MaterialComponent material;
    material.baseColor = DeserializeVector3(j["baseColor"]);
    material.opacity = j["opacity"].get<f32>();
    material.metallic = j["metallic"].get<f32>();
    material.roughness = j["roughness"].get<f32>();
    material.emissiveColor = DeserializeVector3(j["emissiveColor"]);
    material.emissiveStrength = j["emissiveStrength"].get<f32>();
    material.baseColorTexture = j["baseColorTexture"].get<i32>();
    material.normalTexture = j["normalTexture"].get<i32>();
    material.metallicRoughnessTexture = j["metallicRoughnessTexture"].get<i32>();
    material.emissiveTexture = j["emissiveTexture"].get<i32>();
    // Texture paths (optional, added in later versions)
    if (j.contains("baseColorTexturePath")) {
        material.baseColorTexturePath = j["baseColorTexturePath"].get<std::string>();
    }
    if (j.contains("normalTexturePath")) {
        material.normalTexturePath = j["normalTexturePath"].get<std::string>();
    }
    if (j.contains("metallicRoughnessTexturePath")) {
        material.metallicRoughnessTexturePath = j["metallicRoughnessTexturePath"].get<std::string>();
    }
    if (j.contains("emissiveTexturePath")) {
        material.emissiveTexturePath = j["emissiveTexturePath"].get<std::string>();
    }
    material.doubleSided = j["doubleSided"].get<bool>();
    material.castShadows = j["castShadows"].get<bool>();
    material.receiveShadows = j["receiveShadows"].get<bool>();
    material.alphaMode = static_cast<ECS::MaterialComponent::AlphaMode>(j["alphaMode"].get<i32>());
    material.alphaCutoff = j["alphaCutoff"].get<f32>();
    return material;
}

ECS::MeshComponent DeserializeMeshComponent(const json& j) {
    ECS::MeshComponent mesh;

    if (j.contains("vertices") && j["vertices"].is_array()) {
        for (const auto& v : j["vertices"]) {
            ECS::MeshComponent::Vertex vertex;
            vertex.position = DeserializeVector3(v["position"]);
            vertex.normal = DeserializeVector3(v["normal"]);
            vertex.uv = DeserializeVector2(v["uv"]);
            mesh.vertices.push_back(vertex);
        }
    }

    if (j.contains("indices") && j["indices"].is_array()) {
        mesh.indices = j["indices"].get<std::vector<u32>>();
    }

    return mesh;
}

ECS::LightComponent DeserializeLightComponent(const json& j) {
    ECS::LightComponent light;
    light.type = static_cast<ECS::LightType>(j["type"].get<i32>());
    light.color = DeserializeVector3(j["color"]);
    light.intensity = j["intensity"].get<f32>();
    light.range = j["range"].get<f32>();
    light.constantAttenuation = j["constantAttenuation"].get<f32>();
    light.linearAttenuation = j["linearAttenuation"].get<f32>();
    light.quadraticAttenuation = j["quadraticAttenuation"].get<f32>();
    light.innerConeAngle = j["innerConeAngle"].get<f32>();
    light.outerConeAngle = j["outerConeAngle"].get<f32>();
    light.castShadows = j["castShadows"].get<bool>();
    light.shadowMapResolution = j["shadowMapResolution"].get<u32>();
    return light;
}

ECS::NotesComponent DeserializeNotesComponent(const json& j) {
    ECS::NotesComponent notes;
    notes.notes = j["notes"].get<std::string>();
    return notes;
}

ECS::CameraComponent DeserializeCameraComponent(const json& j) {
    ECS::CameraComponent camera;
    camera.projectionType = static_cast<ECS::ProjectionType>(j["projectionType"].get<i32>());
    camera.fieldOfView = j["fieldOfView"].get<f32>();
    camera.nearPlane = j["nearPlane"].get<f32>();
    camera.farPlane = j["farPlane"].get<f32>();
    camera.orthoSize = j["orthoSize"].get<f32>();
    camera.priority = j["priority"].get<i32>();
    camera.isActive = j["isActive"].get<bool>();
    camera.clearDepth = j["clearDepth"].get<bool>();
    camera.clearColor = j["clearColor"].get<bool>();
    camera.backgroundColor = DeserializeVector3(j["backgroundColor"]);
    camera.viewportX = j["viewportX"].get<f32>();
    camera.viewportY = j["viewportY"].get<f32>();
    camera.viewportWidth = j["viewportWidth"].get<f32>();
    camera.viewportHeight = j["viewportHeight"].get<f32>();
    camera.cullingMask = j["cullingMask"].get<u32>();
    return camera;
}

} // anonymous namespace

SceneSerializer::SceneSerializer(ECS::World* world)
    : m_World(world) {
}

SerializationResult SceneSerializer::Save(const std::string& filepath, const SerializationOptions& options) {
    if (!m_World) {
        SerializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    return SaveEntities(filepath, m_World->GetAllEntities(), options);
}

SerializationResult SceneSerializer::SaveEntities(const std::string& filepath, const std::vector<ECS::Entity>& entities, const SerializationOptions& options) {
    if (!m_World) {
        SerializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    try {
        json sceneJson;
        sceneJson["version"] = "1.0";
        sceneJson["entityCount"] = static_cast<u32>(entities.size());

        json entitiesArray = json::array();

        for (ECS::Entity entity : entities) {
            if (!m_World->IsValid(entity)) {
                continue;
            }

            json entityJson;
            entityJson["id"] = static_cast<u64>(entity);

            // Serialize components
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                const auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                entityJson["name"] = SerializeNameComponent(*name);
            }

            if (m_World->HasComponent<ECS::TransformComponent>(entity)) {
                const auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                entityJson["transform"] = SerializeTransformComponent(*transform);
            }

            if (m_World->HasComponent<ECS::MaterialComponent>(entity)) {
                const auto* material = m_World->GetComponent<ECS::MaterialComponent>(entity);
                entityJson["material"] = SerializeMaterialComponent(*material);
            }

            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
                entityJson["mesh"] = SerializeMeshComponent(*mesh, options.includeVertexData);
            }

            if (m_World->HasComponent<ECS::LightComponent>(entity)) {
                const auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
                entityJson["light"] = SerializeLightComponent(*light);
            }

            if (m_World->HasComponent<ECS::NotesComponent>(entity)) {
                const auto* notes = m_World->GetComponent<ECS::NotesComponent>(entity);
                entityJson["notes"] = SerializeNotesComponent(*notes);
            }

            if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
                const auto* camera = m_World->GetComponent<ECS::CameraComponent>(entity);
                entityJson["camera"] = SerializeCameraComponent(*camera);
            }

            entitiesArray.push_back(entityJson);
        }

        sceneJson["entities"] = entitiesArray;

        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) {
            SerializationResult result;
            result.success = false;
            result.error = "Failed to open file for writing: " + filepath;
            result.filepath = filepath;
            return result;
        }

        if (options.prettyPrint) {
            file << sceneJson.dump(static_cast<int>(options.indentSize));
        } else {
            file << sceneJson.dump();
        }

        file.close();

        ENJIN_LOG_INFO(Asset, "Saved scene to %s (%zu entities)", filepath.c_str(), entities.size());

        SerializationResult result;
        result.success = true;
        result.filepath = filepath;
        return result;

    } catch (const std::exception& e) {
        SerializationResult result;
        result.success = false;
        result.error = std::string("JSON serialization error: ") + e.what();
        result.filepath = filepath;
        return result;
    }
}

DeserializationResult SceneSerializer::Load(const std::string& filepath, bool clearExisting) {
    if (!m_World) {
        DeserializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    if (clearExisting) {
        m_World->Clear();
    }

    return LoadAdditive(filepath);
}

DeserializationResult SceneSerializer::LoadAdditive(const std::string& filepath) {
    if (!m_World) {
        DeserializationResult result;
        result.success = false;
        result.error = "No world set";
        result.filepath = filepath;
        return result;
    }

    DeserializationResult result;
    result.filepath = filepath;

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            result.error = "Failed to open file: " + filepath;
            return result;
        }

        json sceneJson;
        file >> sceneJson;
        file.close();

        // Check version
        std::string version = sceneJson.value("version", "1.0");
        if (version != "1.0") {
            ENJIN_LOG_WARN(Asset, "Scene file version %s may not be fully compatible", version.c_str());
        }

        // Deserialize entities
        if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
            result.error = "Invalid scene format: missing entities array";
            return result;
        }

        for (const auto& entityJson : sceneJson["entities"]) {
            ECS::Entity entity = m_World->CreateEntity();
            result.entities.push_back(entity);

            // Set root entity to first entity
            if (result.rootEntity == ECS::INVALID_ENTITY) {
                result.rootEntity = entity;
            }

            // Deserialize components
            if (entityJson.contains("name")) {
                auto name = DeserializeNameComponent(entityJson["name"]);
                m_World->AddComponent<ECS::NameComponent>(entity, name);
            }

            if (entityJson.contains("transform")) {
                auto transform = DeserializeTransformComponent(entityJson["transform"]);
                m_World->AddComponent<ECS::TransformComponent>(entity, transform);
            }

            if (entityJson.contains("material")) {
                auto material = DeserializeMaterialComponent(entityJson["material"]);
                m_World->AddComponent<ECS::MaterialComponent>(entity, material);
            }

            if (entityJson.contains("mesh")) {
                auto mesh = DeserializeMeshComponent(entityJson["mesh"]);
                if (mesh.IsValid()) {
                    m_World->AddComponent<ECS::MeshComponent>(entity, mesh);
                }
            }

            if (entityJson.contains("light")) {
                auto light = DeserializeLightComponent(entityJson["light"]);
                m_World->AddComponent<ECS::LightComponent>(entity, light);
            }

            if (entityJson.contains("notes")) {
                auto notes = DeserializeNotesComponent(entityJson["notes"]);
                m_World->AddComponent<ECS::NotesComponent>(entity, notes);
            }

            if (entityJson.contains("camera")) {
                auto camera = DeserializeCameraComponent(entityJson["camera"]);
                m_World->AddComponent<ECS::CameraComponent>(entity, camera);
            }
        }

        result.success = true;
        ENJIN_LOG_INFO(Asset, "Loaded scene from %s (%zu entities)", filepath.c_str(), result.entities.size());

    } catch (const std::exception& e) {
        result.error = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}

std::string SceneSerializer::SaveToString(const SerializationOptions& options) {
    if (!m_World) {
        return "";
    }

    try {
        json sceneJson;
        sceneJson["version"] = "1.0";
        const auto& entities = m_World->GetAllEntities();
        sceneJson["entityCount"] = static_cast<u32>(entities.size());

        json entitiesArray = json::array();

        for (ECS::Entity entity : entities) {
            if (!m_World->IsValid(entity)) {
                continue;
            }

            json entityJson;
            entityJson["id"] = static_cast<u64>(entity);

            // Serialize components
            if (m_World->HasComponent<ECS::NameComponent>(entity)) {
                const auto* name = m_World->GetComponent<ECS::NameComponent>(entity);
                entityJson["name"] = SerializeNameComponent(*name);
            }

            if (m_World->HasComponent<ECS::TransformComponent>(entity)) {
                const auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                entityJson["transform"] = SerializeTransformComponent(*transform);
            }

            if (m_World->HasComponent<ECS::MaterialComponent>(entity)) {
                const auto* material = m_World->GetComponent<ECS::MaterialComponent>(entity);
                entityJson["material"] = SerializeMaterialComponent(*material);
            }

            if (m_World->HasComponent<ECS::MeshComponent>(entity)) {
                const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(entity);
                entityJson["mesh"] = SerializeMeshComponent(*mesh, options.includeVertexData);
            }

            if (m_World->HasComponent<ECS::LightComponent>(entity)) {
                const auto* light = m_World->GetComponent<ECS::LightComponent>(entity);
                entityJson["light"] = SerializeLightComponent(*light);
            }

            if (m_World->HasComponent<ECS::NotesComponent>(entity)) {
                const auto* notes = m_World->GetComponent<ECS::NotesComponent>(entity);
                entityJson["notes"] = SerializeNotesComponent(*notes);
            }

            if (m_World->HasComponent<ECS::CameraComponent>(entity)) {
                const auto* camera = m_World->GetComponent<ECS::CameraComponent>(entity);
                entityJson["camera"] = SerializeCameraComponent(*camera);
            }

            entitiesArray.push_back(entityJson);
        }

        sceneJson["entities"] = entitiesArray;

        if (options.prettyPrint) {
            return sceneJson.dump(static_cast<int>(options.indentSize));
        } else {
            return sceneJson.dump();
        }

    } catch (const std::exception& e) {
        ENJIN_LOG_ERROR(Asset, "Failed to serialize scene to string: %s", e.what());
        return "";
    }
}

DeserializationResult SceneSerializer::LoadFromString(const std::string& jsonString, bool clearExisting) {
    DeserializationResult result;

    if (!m_World) {
        result.error = "No world set";
        return result;
    }

    if (jsonString.empty()) {
        result.error = "Empty JSON string";
        return result;
    }

    if (clearExisting) {
        m_World->Clear();
    }

    try {
        json sceneJson = json::parse(jsonString);

        // Check version
        std::string version = sceneJson.value("version", "1.0");

        // Deserialize entities
        if (!sceneJson.contains("entities") || !sceneJson["entities"].is_array()) {
            result.error = "Invalid scene format: missing entities array";
            return result;
        }

        for (const auto& entityJson : sceneJson["entities"]) {
            ECS::Entity entity = m_World->CreateEntity();
            result.entities.push_back(entity);

            if (result.rootEntity == ECS::INVALID_ENTITY) {
                result.rootEntity = entity;
            }

            // Deserialize components
            if (entityJson.contains("name")) {
                auto name = DeserializeNameComponent(entityJson["name"]);
                m_World->AddComponent<ECS::NameComponent>(entity, name);
            }

            if (entityJson.contains("transform")) {
                auto transform = DeserializeTransformComponent(entityJson["transform"]);
                m_World->AddComponent<ECS::TransformComponent>(entity, transform);
            }

            if (entityJson.contains("material")) {
                auto material = DeserializeMaterialComponent(entityJson["material"]);
                m_World->AddComponent<ECS::MaterialComponent>(entity, material);
            }

            if (entityJson.contains("mesh")) {
                auto mesh = DeserializeMeshComponent(entityJson["mesh"]);
                if (mesh.IsValid()) {
                    m_World->AddComponent<ECS::MeshComponent>(entity, mesh);
                }
            }

            if (entityJson.contains("light")) {
                auto light = DeserializeLightComponent(entityJson["light"]);
                m_World->AddComponent<ECS::LightComponent>(entity, light);
            }

            if (entityJson.contains("notes")) {
                auto notes = DeserializeNotesComponent(entityJson["notes"]);
                m_World->AddComponent<ECS::NotesComponent>(entity, notes);
            }

            if (entityJson.contains("camera")) {
                auto camera = DeserializeCameraComponent(entityJson["camera"]);
                m_World->AddComponent<ECS::CameraComponent>(entity, camera);
            }
        }

        result.success = true;
        ENJIN_LOG_DEBUG(Asset, "Loaded scene from string (%zu entities)", result.entities.size());

    } catch (const std::exception& e) {
        result.error = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}

} // namespace Scene
} // namespace Enjin
