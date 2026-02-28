#include "Enjin/Assets/Prefab.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Logging/Log.h"
#include <nlohmann/json.hpp>
#include <fstream>

namespace Enjin {
namespace Assets {

using json = nlohmann::json;

// ============================================================================
// Prefab
// ============================================================================

u64 Prefab::s_NextId = 1;

Prefab::Prefab(const std::string& name)
    : m_Name(name)
    , m_Id(s_NextId++) {
}

void Prefab::AddEntity(const PrefabEntityData& entity) {
    m_Entities.push_back(entity);
}

void Prefab::Clear() {
    m_Entities.clear();
}

u32 Prefab::GetRootCount() const {
    u32 count = 0;
    for (const auto& entity : m_Entities) {
        if (entity.parentIndex < 0) {
            count++;
        }
    }
    return count;
}

// ============================================================================
// PrefabManager
// ============================================================================

PrefabManager::PrefabManager() {
    RegisterBuiltInComponents();
}

PrefabManager::~PrefabManager() {
    ClearCache();
}

PrefabManager& PrefabManager::Get() {
    static PrefabManager instance;
    return instance;
}

void PrefabManager::RegisterBuiltInComponents() {
    // Transform
    RegisterComponentType("Transform",
        [](ECS::World* world, ECS::Entity entity) -> PrefabComponentData {
            PrefabComponentData data;
            data.typeName = "Transform";
            if (world->HasComponent<ECS::TransformComponent>(entity)) {
                auto* t = world->GetComponent<ECS::TransformComponent>(entity);
                data.vec3Properties["position"] = t->position;
                // Store quaternion as vec4 (x, y, z, w)
                data.vec4Properties["rotation"] = Math::Vector4(t->rotation.x, t->rotation.y, t->rotation.z, t->rotation.w);
                data.vec3Properties["scale"] = t->scale;
            }
            return data;
        },
        [](ECS::World* world, ECS::Entity entity, const PrefabComponentData& data) {
            ECS::TransformComponent* t = nullptr;
            if (world->HasComponent<ECS::TransformComponent>(entity)) {
                t = world->GetComponent<ECS::TransformComponent>(entity);
            } else {
                t = &world->AddComponent<ECS::TransformComponent>(entity);
            }
            auto posIt = data.vec3Properties.find("position");
            if (posIt != data.vec3Properties.end()) t->position = posIt->second;
            auto rotIt = data.vec4Properties.find("rotation");
            if (rotIt != data.vec4Properties.end()) {
                const Math::Vector4& r = rotIt->second;
                t->rotation = Math::Quaternion(r.x, r.y, r.z, r.w);
            }
            auto scaleIt = data.vec3Properties.find("scale");
            if (scaleIt != data.vec3Properties.end()) t->scale = scaleIt->second;
        }
    );

    // Name
    RegisterComponentType("Name",
        [](ECS::World* world, ECS::Entity entity) -> PrefabComponentData {
            PrefabComponentData data;
            data.typeName = "Name";
            if (world->HasComponent<ECS::NameComponent>(entity)) {
                data.stringProperties["name"] = world->GetComponent<ECS::NameComponent>(entity)->name;
            }
            return data;
        },
        [](ECS::World* world, ECS::Entity entity, const PrefabComponentData& data) {
            ECS::NameComponent* n = nullptr;
            if (world->HasComponent<ECS::NameComponent>(entity)) {
                n = world->GetComponent<ECS::NameComponent>(entity);
            } else {
                n = &world->AddComponent<ECS::NameComponent>(entity);
            }
            auto it = data.stringProperties.find("name");
            if (it != data.stringProperties.end()) n->name = it->second;
        }
    );

    // Material
    RegisterComponentType("Material",
        [](ECS::World* world, ECS::Entity entity) -> PrefabComponentData {
            PrefabComponentData data;
            data.typeName = "Material";
            if (world->HasComponent<ECS::MaterialComponent>(entity)) {
                auto* m = world->GetComponent<ECS::MaterialComponent>(entity);
                data.vec3Properties["baseColor"] = m->baseColor;
                data.floatProperties["metallic"] = m->metallic;
                data.floatProperties["roughness"] = m->roughness;
                data.vec3Properties["emissiveColor"] = m->emissiveColor;
                data.floatProperties["emissiveStrength"] = m->emissiveStrength;
                data.floatProperties["opacity"] = m->opacity;
                data.floatProperties["alphaCutoff"] = m->alphaCutoff;
                if (!m->baseColorTexturePath.empty())
                    data.stringProperties["baseColorTexturePath"] = m->baseColorTexturePath;
                if (!m->normalTexturePath.empty())
                    data.stringProperties["normalTexturePath"] = m->normalTexturePath;
                if (!m->metallicRoughnessTexturePath.empty())
                    data.stringProperties["metallicRoughnessTexturePath"] = m->metallicRoughnessTexturePath;
                if (!m->emissiveTexturePath.empty())
                    data.stringProperties["emissiveTexturePath"] = m->emissiveTexturePath;
                if (!m->heightTexturePath.empty())
                    data.stringProperties["heightTexturePath"] = m->heightTexturePath;
            }
            return data;
        },
        [](ECS::World* world, ECS::Entity entity, const PrefabComponentData& data) {
            ECS::MaterialComponent* m = nullptr;
            if (world->HasComponent<ECS::MaterialComponent>(entity)) {
                m = world->GetComponent<ECS::MaterialComponent>(entity);
            } else {
                m = &world->AddComponent<ECS::MaterialComponent>(entity);
            }

            auto it = data.vec3Properties.find("baseColor");
            if (it != data.vec3Properties.end()) m->baseColor = it->second;

            auto fIt = data.floatProperties.find("metallic");
            if (fIt != data.floatProperties.end()) m->metallic = fIt->second;

            fIt = data.floatProperties.find("roughness");
            if (fIt != data.floatProperties.end()) m->roughness = fIt->second;

            it = data.vec3Properties.find("emissiveColor");
            if (it != data.vec3Properties.end()) m->emissiveColor = it->second;

            fIt = data.floatProperties.find("emissiveStrength");
            if (fIt != data.floatProperties.end()) m->emissiveStrength = fIt->second;

            fIt = data.floatProperties.find("opacity");
            if (fIt != data.floatProperties.end()) m->opacity = fIt->second;

            fIt = data.floatProperties.find("alphaCutoff");
            if (fIt != data.floatProperties.end()) m->alphaCutoff = fIt->second;

            auto sIt = data.stringProperties.find("baseColorTexturePath");
            if (sIt != data.stringProperties.end()) m->baseColorTexturePath = sIt->second;
            sIt = data.stringProperties.find("normalTexturePath");
            if (sIt != data.stringProperties.end()) m->normalTexturePath = sIt->second;
            sIt = data.stringProperties.find("metallicRoughnessTexturePath");
            if (sIt != data.stringProperties.end()) m->metallicRoughnessTexturePath = sIt->second;
            sIt = data.stringProperties.find("emissiveTexturePath");
            if (sIt != data.stringProperties.end()) m->emissiveTexturePath = sIt->second;
            sIt = data.stringProperties.find("heightTexturePath");
            if (sIt != data.stringProperties.end()) m->heightTexturePath = sIt->second;
            m->textureCacheDirty = true;
        }
    );

    // Light
    RegisterComponentType("Light",
        [](ECS::World* world, ECS::Entity entity) -> PrefabComponentData {
            PrefabComponentData data;
            data.typeName = "Light";
            if (world->HasComponent<ECS::LightComponent>(entity)) {
                auto* l = world->GetComponent<ECS::LightComponent>(entity);
                data.intProperties["type"] = static_cast<i32>(l->type);
                data.vec3Properties["color"] = l->color;
                data.floatProperties["intensity"] = l->intensity;
                data.floatProperties["range"] = l->range;
                data.floatProperties["innerConeAngle"] = l->innerConeAngle;
                data.floatProperties["outerConeAngle"] = l->outerConeAngle;
                data.boolProperties["castShadows"] = l->castShadows;
            }
            return data;
        },
        [](ECS::World* world, ECS::Entity entity, const PrefabComponentData& data) {
            ECS::LightComponent* l = nullptr;
            if (world->HasComponent<ECS::LightComponent>(entity)) {
                l = world->GetComponent<ECS::LightComponent>(entity);
            } else {
                l = &world->AddComponent<ECS::LightComponent>(entity);
            }

            auto iIt = data.intProperties.find("type");
            if (iIt != data.intProperties.end()) l->type = static_cast<ECS::LightType>(iIt->second);

            auto vIt = data.vec3Properties.find("color");
            if (vIt != data.vec3Properties.end()) l->color = vIt->second;

            auto fIt = data.floatProperties.find("intensity");
            if (fIt != data.floatProperties.end()) l->intensity = fIt->second;

            fIt = data.floatProperties.find("range");
            if (fIt != data.floatProperties.end()) l->range = fIt->second;

            fIt = data.floatProperties.find("innerConeAngle");
            if (fIt != data.floatProperties.end()) l->innerConeAngle = fIt->second;

            fIt = data.floatProperties.find("outerConeAngle");
            if (fIt != data.floatProperties.end()) l->outerConeAngle = fIt->second;

            auto bIt = data.boolProperties.find("castShadows");
            if (bIt != data.boolProperties.end()) l->castShadows = bIt->second;
        }
    );

    // Mesh — encode vertex/index data as JSON string in stringProperties
    RegisterComponentType("Mesh",
        [](ECS::World* world, ECS::Entity entity) -> PrefabComponentData {
            PrefabComponentData data;
            data.typeName = "Mesh";
            if (world->HasComponent<ECS::MeshComponent>(entity)) {
                auto* mesh = world->GetComponent<ECS::MeshComponent>(entity);
                json j;
                json verts = json::array();
                for (const auto& v : mesh->vertices) {
                    json vert;
                    vert["p"] = {v.position.x, v.position.y, v.position.z};
                    vert["n"] = {v.normal.x, v.normal.y, v.normal.z};
                    vert["u"] = {v.uv.x, v.uv.y};
                    if (v.color.x != 1.0f || v.color.y != 1.0f ||
                        v.color.z != 1.0f || v.color.w != 1.0f) {
                        vert["c"] = {v.color.x, v.color.y, v.color.z, v.color.w};
                    }
                    if (v.tangent.x != 0.0f || v.tangent.y != 0.0f ||
                        v.tangent.z != 0.0f || v.tangent.w != 1.0f) {
                        vert["t"] = {v.tangent.x, v.tangent.y, v.tangent.z, v.tangent.w};
                    }
                    verts.push_back(vert);
                }
                j["v"] = verts;
                j["i"] = mesh->indices;
                data.stringProperties["meshJson"] = j.dump();
            }
            return data;
        },
        [](ECS::World* world, ECS::Entity entity, const PrefabComponentData& data) {
            auto sIt = data.stringProperties.find("meshJson");
            if (sIt == data.stringProperties.end()) return;

            ECS::MeshComponent* mesh = nullptr;
            if (world->HasComponent<ECS::MeshComponent>(entity)) {
                mesh = world->GetComponent<ECS::MeshComponent>(entity);
            } else {
                mesh = &world->AddComponent<ECS::MeshComponent>(entity);
            }

            try {
                json j = json::parse(sIt->second);
                mesh->vertices.clear();
                mesh->indices.clear();

                if (j.contains("v")) {
                    for (const auto& vert : j["v"]) {
                        ECS::MeshComponent::Vertex v;
                        auto& p = vert["p"];
                        v.position = Math::Vector3(p[0], p[1], p[2]);
                        auto& n = vert["n"];
                        v.normal = Math::Vector3(n[0], n[1], n[2]);
                        auto& u = vert["u"];
                        v.uv = Math::Vector2(u[0], u[1]);
                        if (vert.contains("c")) {
                            auto& c = vert["c"];
                            v.color = Math::Vector4(c[0], c[1], c[2], c[3]);
                        }
                        if (vert.contains("t")) {
                            auto& t = vert["t"];
                            v.tangent = Math::Vector4(t[0], t[1], t[2], t[3]);
                        }
                        mesh->vertices.push_back(v);
                    }
                }
                if (j.contains("i")) {
                    mesh->indices = j["i"].get<std::vector<u32>>();
                }
            } catch (const json::exception& e) {
                ENJIN_LOG_ERROR(Assets, "Failed to deserialize mesh data: %s", e.what());
            }
        }
    );

    // Camera
    RegisterComponentType("Camera",
        [](ECS::World* world, ECS::Entity entity) -> PrefabComponentData {
            PrefabComponentData data;
            data.typeName = "Camera";
            if (world->HasComponent<ECS::CameraComponent>(entity)) {
                auto* cam = world->GetComponent<ECS::CameraComponent>(entity);
                data.floatProperties["fieldOfView"] = cam->fieldOfView;
                data.floatProperties["nearPlane"] = cam->nearPlane;
                data.floatProperties["farPlane"] = cam->farPlane;
                data.boolProperties["isActive"] = cam->isActive;
                data.intProperties["projectionType"] = static_cast<i32>(cam->projectionType);
                data.floatProperties["orthoSize"] = cam->orthoSize;
            }
            return data;
        },
        [](ECS::World* world, ECS::Entity entity, const PrefabComponentData& data) {
            ECS::CameraComponent* cam = nullptr;
            if (world->HasComponent<ECS::CameraComponent>(entity)) {
                cam = world->GetComponent<ECS::CameraComponent>(entity);
            } else {
                cam = &world->AddComponent<ECS::CameraComponent>(entity);
            }

            auto fIt = data.floatProperties.find("fieldOfView");
            if (fIt != data.floatProperties.end()) cam->fieldOfView = fIt->second;
            fIt = data.floatProperties.find("nearPlane");
            if (fIt != data.floatProperties.end()) cam->nearPlane = fIt->second;
            fIt = data.floatProperties.find("farPlane");
            if (fIt != data.floatProperties.end()) cam->farPlane = fIt->second;
            fIt = data.floatProperties.find("orthoSize");
            if (fIt != data.floatProperties.end()) cam->orthoSize = fIt->second;

            auto bIt = data.boolProperties.find("isActive");
            if (bIt != data.boolProperties.end()) cam->isActive = bIt->second;

            auto iIt = data.intProperties.find("projectionType");
            if (iIt != data.intProperties.end()) cam->projectionType = static_cast<ECS::ProjectionType>(iIt->second);
        }
    );

    // Notes
    RegisterComponentType("Notes",
        [](ECS::World* world, ECS::Entity entity) -> PrefabComponentData {
            PrefabComponentData data;
            data.typeName = "Notes";
            if (world->HasComponent<ECS::NotesComponent>(entity)) {
                data.stringProperties["notes"] = world->GetComponent<ECS::NotesComponent>(entity)->notes;
            }
            return data;
        },
        [](ECS::World* world, ECS::Entity entity, const PrefabComponentData& data) {
            ECS::NotesComponent* n = nullptr;
            if (world->HasComponent<ECS::NotesComponent>(entity)) {
                n = world->GetComponent<ECS::NotesComponent>(entity);
            } else {
                n = &world->AddComponent<ECS::NotesComponent>(entity);
            }
            auto it = data.stringProperties.find("notes");
            if (it != data.stringProperties.end()) n->notes = it->second;
        }
    );
}

void PrefabManager::RegisterComponentType(const std::string& typeName,
                                         ComponentSerializer serializer,
                                         ComponentDeserializer deserializer) {
    m_ComponentCallbacks[typeName] = {serializer, deserializer};
}

std::shared_ptr<Prefab> PrefabManager::CreateFromEntity(ECS::World* world, ECS::Entity rootEntity,
                                                        const std::string& name) {
    auto prefab = std::make_shared<Prefab>(name);
    SerializeEntityRecursive(world, rootEntity, *prefab, -1);
    RegisterPrefab(prefab);
    return prefab;
}

std::shared_ptr<Prefab> PrefabManager::CreateFromEntities(ECS::World* world,
                                                          const std::vector<ECS::Entity>& entities,
                                                          const std::string& name) {
    auto prefab = std::make_shared<Prefab>(name);
    for (auto entity : entities) {
        SerializeEntityRecursive(world, entity, *prefab, -1);
    }
    RegisterPrefab(prefab);
    return prefab;
}

void PrefabManager::SerializeEntityRecursive(ECS::World* world, ECS::Entity entity,
                                             Prefab& prefab, i32 parentIndex) {
    // Guard against excessive recursion depth (e.g. circular parent-child references)
    static constexpr i32 MAX_SERIALIZE_DEPTH = 64;
    static thread_local i32 s_SerializeDepth = 0;
    s_SerializeDepth++;
    struct DepthGuard { ~DepthGuard() { s_SerializeDepth--; } } depthGuard;

    if (s_SerializeDepth > MAX_SERIALIZE_DEPTH) {
        ENJIN_LOG_ERROR(Assets, "Prefab serialization depth exceeded (%d), aborting recursive serialize", MAX_SERIALIZE_DEPTH);
        return;
    }

    PrefabEntityData entityData;
    entityData.parentIndex = parentIndex;

    // Get name
    if (world->HasComponent<ECS::NameComponent>(entity)) {
        entityData.name = world->GetComponent<ECS::NameComponent>(entity)->name;
    } else {
        entityData.name = "Entity_" + std::to_string(entity);
    }

    // Serialize all registered component types
    for (const auto& [typeName, callbacks] : m_ComponentCallbacks) {
        PrefabComponentData compData = callbacks.serializer(world, entity);
        if (!compData.typeName.empty()) {
            entityData.components.push_back(compData);
        }
    }

    i32 currentIndex = static_cast<i32>(prefab.GetEntities().size());
    prefab.AddEntity(entityData);

    // Recursively serialize children via ChildrenComponent
    if (world->HasComponent<ECS::ChildrenComponent>(entity)) {
        auto* children = world->GetComponent<ECS::ChildrenComponent>(entity);
        if (children) {
            for (ECS::Entity child : children->children) {
                if (world->IsValid(child)) {
                    SerializeEntityRecursive(world, child, prefab, currentIndex);
                }
            }
        }
    }
}

ECS::Entity PrefabManager::Instantiate(ECS::World* world, const Prefab& prefab,
                                       const Math::Vector3& position,
                                       const Math::Vector3& rotation,
                                       const Math::Vector3& scale) {
    if (prefab.IsEmpty()) {
        ENJIN_LOG_WARN(Assets, "Cannot instantiate empty prefab");
        return ECS::INVALID_ENTITY;
    }

    // Guard against circular prefab references and excessive nesting
    static thread_local u32 s_InstantiateDepth = 0;
    static thread_local std::vector<u64> s_InstantiateStack;
    constexpr u32 MAX_PREFAB_DEPTH = 10;

    if (s_InstantiateDepth >= MAX_PREFAB_DEPTH) {
        ENJIN_LOG_ERROR(Assets, "Prefab nesting depth exceeded (%u): '%s'", MAX_PREFAB_DEPTH, prefab.GetName().c_str());
        return ECS::INVALID_ENTITY;
    }
    for (u64 id : s_InstantiateStack) {
        if (id == prefab.GetId()) {
            ENJIN_LOG_ERROR(Assets, "Circular prefab reference detected: '%s' (ID: %llu)", prefab.GetName().c_str(), prefab.GetId());
            return ECS::INVALID_ENTITY;
        }
    }

    s_InstantiateDepth++;
    s_InstantiateStack.push_back(prefab.GetId());
    struct DepthGuard { ~DepthGuard() { s_InstantiateDepth--; s_InstantiateStack.pop_back(); } } guard;

    std::vector<ECS::Entity> createdEntities;
    createdEntities.reserve(prefab.GetEntities().size());

    // Create all entities first
    for (const auto& entityData : prefab.GetEntities()) {
        ECS::Entity entity = world->CreateEntity();
        createdEntities.push_back(entity);

        // Deserialize components
        for (const auto& compData : entityData.components) {
            auto it = m_ComponentCallbacks.find(compData.typeName);
            if (it != m_ComponentCallbacks.end()) {
                it->second.deserializer(world, entity, compData);
            }
        }
    }

    // Establish parent-child relationships using Hierarchy components
    for (usize i = 0; i < createdEntities.size(); ++i) {
        const auto& entityData = prefab.GetEntities()[i];
        if (entityData.parentIndex >= 0 &&
            entityData.parentIndex < static_cast<i32>(createdEntities.size())) {
            ECS::Entity childEntity = createdEntities[i];
            ECS::Entity parentEntity = createdEntities[entityData.parentIndex];
            ECS::SetParent(world, childEntity, parentEntity);
        }
    }

    // Apply root transform offset
    if (!createdEntities.empty()) {
        ECS::Entity rootEntity = createdEntities[0];
        if (world->HasComponent<ECS::TransformComponent>(rootEntity)) {
            auto* transform = world->GetComponent<ECS::TransformComponent>(rootEntity);
            transform->position = transform->position + position;
            // Apply rotation as a quaternion multiplication
            Math::Quaternion rotQuat = Math::Quaternion::FromEuler(Math::Vector3(
                Math::Radians(rotation.x),
                Math::Radians(rotation.y),
                Math::Radians(rotation.z)
            ));
            transform->rotation = rotQuat * transform->rotation;
            transform->scale = Math::Vector3(
                transform->scale.x * scale.x,
                transform->scale.y * scale.y,
                transform->scale.z * scale.z
            );
        }

        // Add prefab instance component to root
        auto& prefabComp = world->AddComponent<PrefabInstanceComponent>(rootEntity);
        prefabComp.prefabId = prefab.GetId();
        prefabComp.prefabPath = prefab.GetPath();
    }

    ENJIN_LOG_INFO(Assets, "Instantiated prefab '%s' with %zu entities",
                   prefab.GetName().c_str(), createdEntities.size());

    return createdEntities.empty() ? ECS::INVALID_ENTITY : createdEntities[0];
}

ECS::Entity PrefabManager::Instantiate(ECS::World* world, u64 prefabId,
                                       const Math::Vector3& position) {
    auto prefab = GetPrefabById(prefabId);
    if (!prefab) {
        ENJIN_LOG_ERROR(Assets, "Prefab with ID %llu not found", prefabId);
        return ECS::INVALID_ENTITY;
    }
    return Instantiate(world, *prefab, position);
}

bool PrefabManager::SavePrefab(const Prefab& prefab, const std::string& filepath) {
    json j;
    j["name"] = prefab.GetName();
    j["entities"] = json::array();

    for (const auto& entity : prefab.GetEntities()) {
        json entityJson;
        entityJson["name"] = entity.name;
        entityJson["parentIndex"] = entity.parentIndex;
        entityJson["components"] = json::array();

        for (const auto& comp : entity.components) {
            json compJson;
            compJson["type"] = comp.typeName;

            if (!comp.stringProperties.empty()) {
                compJson["strings"] = comp.stringProperties;
            }
            if (!comp.floatProperties.empty()) {
                compJson["floats"] = comp.floatProperties;
            }
            if (!comp.intProperties.empty()) {
                compJson["ints"] = comp.intProperties;
            }
            if (!comp.boolProperties.empty()) {
                compJson["bools"] = comp.boolProperties;
            }
            if (!comp.vec3Properties.empty()) {
                json vec3s;
                for (const auto& [key, v] : comp.vec3Properties) {
                    vec3s[key] = {v.x, v.y, v.z};
                }
                compJson["vec3s"] = vec3s;
            }
            if (!comp.vec4Properties.empty()) {
                json vec4s;
                for (const auto& [key, v] : comp.vec4Properties) {
                    vec4s[key] = {v.x, v.y, v.z, v.w};
                }
                compJson["vec4s"] = vec4s;
            }

            entityJson["components"].push_back(compJson);
        }

        j["entities"].push_back(entityJson);
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Assets, "Failed to save prefab to '%s'", filepath.c_str());
        return false;
    }

    file << j.dump(2);
    ENJIN_LOG_INFO(Assets, "Saved prefab '%s' to '%s'", prefab.GetName().c_str(), filepath.c_str());
    return true;
}

std::shared_ptr<Prefab> PrefabManager::LoadPrefab(const std::string& filepath) {
    // Check cache first
    auto cacheIt = m_PathCache.find(filepath);
    if (cacheIt != m_PathCache.end()) {
        return cacheIt->second;
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Assets, "Failed to load prefab from '%s'", filepath.c_str());
        return nullptr;
    }

    json j;
    try {
        file >> j;
    } catch (const json::exception& e) {
        ENJIN_LOG_ERROR(Assets, "Failed to parse prefab JSON: %s", e.what());
        return nullptr;
    }

    auto prefab = std::make_shared<Prefab>(j.value("name", "Prefab"));
    prefab->SetPath(filepath);

    if (!j.contains("entities") || !j["entities"].is_array()) {
        ENJIN_LOG_WARN(Assets, "Prefab missing 'entities' array: %s", filepath.c_str());
        return prefab;
    }

    for (const auto& entityJson : j["entities"]) {
        PrefabEntityData entityData;
        entityData.name = entityJson.value("name", "Entity");
        entityData.parentIndex = entityJson.value("parentIndex", -1);

        if (!entityJson.contains("components") || !entityJson["components"].is_array()) continue;

        for (const auto& compJson : entityJson["components"]) {
            PrefabComponentData compData;
            compData.typeName = compJson.value("type", "");

            if (compJson.contains("strings")) {
                for (auto& [key, val] : compJson["strings"].items()) {
                    compData.stringProperties[key] = val.get<std::string>();
                }
            }
            if (compJson.contains("floats")) {
                for (auto& [key, val] : compJson["floats"].items()) {
                    compData.floatProperties[key] = val.get<f32>();
                }
            }
            if (compJson.contains("ints")) {
                for (auto& [key, val] : compJson["ints"].items()) {
                    compData.intProperties[key] = val.get<i32>();
                }
            }
            if (compJson.contains("bools")) {
                for (auto& [key, val] : compJson["bools"].items()) {
                    compData.boolProperties[key] = val.get<bool>();
                }
            }
            if (compJson.contains("vec3s")) {
                for (auto& [key, val] : compJson["vec3s"].items()) {
                    if (val.is_array() && val.size() >= 3) {
                        compData.vec3Properties[key] = Math::Vector3(val[0], val[1], val[2]);
                    } else {
                        ENJIN_LOG_WARN(Assets, "Invalid vec3 array for key '%s' in prefab", key.c_str());
                    }
                }
            }
            if (compJson.contains("vec4s")) {
                for (auto& [key, val] : compJson["vec4s"].items()) {
                    if (val.is_array() && val.size() >= 4) {
                        compData.vec4Properties[key] = Math::Vector4(val[0], val[1], val[2], val[3]);
                    } else {
                        ENJIN_LOG_WARN(Assets, "Invalid vec4 array for key '%s' in prefab", key.c_str());
                    }
                }
            }

            entityData.components.push_back(compData);
        }

        prefab->AddEntity(entityData);
    }

    RegisterPrefab(prefab);
    m_PathCache[filepath] = prefab;

    ENJIN_LOG_INFO(Assets, "Loaded prefab '%s' from '%s'", prefab->GetName().c_str(), filepath.c_str());
    return prefab;
}

std::shared_ptr<Prefab> PrefabManager::GetPrefab(const std::string& filepath) {
    auto it = m_PathCache.find(filepath);
    if (it != m_PathCache.end()) {
        return it->second;
    }
    return LoadPrefab(filepath);
}

std::shared_ptr<Prefab> PrefabManager::GetPrefabById(u64 id) {
    auto it = m_Prefabs.find(id);
    return (it != m_Prefabs.end()) ? it->second : nullptr;
}

void PrefabManager::RegisterPrefab(std::shared_ptr<Prefab> prefab) {
    m_Prefabs[prefab->GetId()] = prefab;
}

void PrefabManager::UnregisterPrefab(u64 id) {
    m_Prefabs.erase(id);
}

void PrefabManager::ClearCache() {
    m_Prefabs.clear();
    m_PathCache.clear();
}

void PrefabManager::ApplyPrefabToInstances(ECS::World* world, u64 prefabId) {
    auto prefab = GetPrefabById(prefabId);
    if (!prefab || !world) return;

    u32 updateCount = 0;
    for (ECS::Entity entity : world->GetEntitiesWithComponent<PrefabInstanceComponent>()) {
        auto* pi = world->GetComponent<PrefabInstanceComponent>(entity);
        if (!pi || pi->prefabId != prefabId) continue;

        // Re-apply all registered component types from prefab data
        const auto& prefabEntities = prefab->GetEntities();
        if (prefabEntities.empty()) continue;

        // Apply root entity's components (index 0) to this instance
        const auto& rootData = prefabEntities[0];
        for (const auto& compData : rootData.components) {
            auto cbIt = m_ComponentCallbacks.find(compData.typeName);
            if (cbIt != m_ComponentCallbacks.end()) {
                cbIt->second.deserializer(world, entity, compData);
            }
        }
        pi->overridesApplied = true;
        updateCount++;
    }
    ENJIN_LOG_INFO(Assets, "Applied prefab %llu to %u instances", prefabId, updateCount);
}

void PrefabManager::UnpackInstance(ECS::World* world, ECS::Entity entity) {
    if (world->HasComponent<PrefabInstanceComponent>(entity)) {
        world->RemoveComponent<PrefabInstanceComponent>(entity);
        ENJIN_LOG_INFO(Assets, "Unpacked prefab instance");
    }
}

// ============================================================================
// PrefabUtils
// ============================================================================

namespace PrefabUtils {

bool IsPrefabInstance(ECS::World* world, ECS::Entity entity) {
    return world->HasComponent<PrefabInstanceComponent>(entity);
}

bool IsPartOfPrefabInstance(ECS::World* world, ECS::Entity entity) {
    // Check self and ancestors up the hierarchy
    ECS::Entity current = entity;
    while (current != ECS::INVALID_ENTITY && world->IsValid(current)) {
        if (IsPrefabInstance(world, current)) return true;
        if (world->HasComponent<ECS::ParentComponent>(current)) {
            current = world->GetComponent<ECS::ParentComponent>(current)->parent;
        } else {
            break;
        }
    }
    return false;
}

ECS::Entity GetPrefabInstanceRoot(ECS::World* world, ECS::Entity entity) {
    if (IsPrefabInstance(world, entity)) {
        return entity;
    }
    return ECS::INVALID_ENTITY;
}

u64 GetPrefabId(ECS::World* world, ECS::Entity entity) {
    if (world->HasComponent<PrefabInstanceComponent>(entity)) {
        return world->GetComponent<PrefabInstanceComponent>(entity)->prefabId;
    }
    return 0;
}

} // namespace PrefabUtils

} // namespace Assets
} // namespace Enjin
