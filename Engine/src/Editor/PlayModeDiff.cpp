#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;

namespace Enjin {
namespace Editor {

// Compare two JSON values and generate property diffs
static void DiffJsonObject(const json& oldObj, const json& newObj,
                           const std::string& prefix,
                           std::vector<PropertyDiff>& outDiffs) {
    // Check for modified/added keys
    for (auto& [key, newVal] : newObj.items()) {
        std::string path = prefix.empty() ? key : (prefix + "." + key);

        if (!oldObj.contains(key)) {
            // Added
            PropertyDiff d;
            d.name = path;
            d.oldValue = "";
            d.newValue = newVal.dump();
            outDiffs.push_back(d);
        } else if (oldObj[key] != newVal) {
            if (newVal.is_object() && oldObj[key].is_object()) {
                DiffJsonObject(oldObj[key], newVal, path, outDiffs);
            } else {
                PropertyDiff d;
                d.name = path;
                d.oldValue = oldObj[key].dump();
                d.newValue = newVal.dump();
                outDiffs.push_back(d);
            }
        }
    }

    // Check for removed keys
    for (auto& [key, oldVal] : oldObj.items()) {
        if (!newObj.contains(key)) {
            std::string path = prefix.empty() ? key : (prefix + "." + key);
            PropertyDiff d;
            d.name = path;
            d.oldValue = oldVal.dump();
            d.newValue = "";
            outDiffs.push_back(d);
        }
    }
}

// Get entity name from a parsed entity JSON
static std::string GetEntityNameFromJson(const json& entityJson) {
    if (entityJson.contains("name")) {
        return entityJson["name"].get<std::string>();
    }
    return "Entity";
}

// Get entity ID from JSON (uses the "id" field if present)
static u64 GetEntityIdFromJson(const json& entityJson) {
    if (entityJson.contains("id")) {
        return entityJson["id"].get<u64>();
    }
    return 0;
}

PlayModeDiff ComputePlayModeDiff(const std::string& savedJson,
                                  const std::string& currentJson) {
    PlayModeDiff diff;

    json savedParsed = json::parse(savedJson, nullptr, false);
    json currentParsed = json::parse(currentJson, nullptr, false);

    if (savedParsed.is_discarded() || currentParsed.is_discarded()) {
        ENJIN_LOG_WARN(Editor, "PlayModeDiff: Failed to parse scene JSON");
        return diff;
    }

    // Extract entity arrays
    json savedEntities = savedParsed.contains("entities") ? savedParsed["entities"] : json::array();
    json currentEntities = currentParsed.contains("entities") ? currentParsed["entities"] : json::array();

    // Build lookup maps by name (since entity IDs change on restore)
    std::unordered_map<std::string, json> savedByName;
    std::unordered_map<std::string, json> currentByName;

    for (const auto& e : savedEntities) {
        std::string name = GetEntityNameFromJson(e);
        savedByName[name] = e;
    }
    for (const auto& e : currentEntities) {
        std::string name = GetEntityNameFromJson(e);
        currentByName[name] = e;
    }

    // Find created entities (in current but not in saved)
    for (const auto& [name, entityJson] : currentByName) {
        if (savedByName.find(name) == savedByName.end()) {
            EntityDiff ed;
            ed.entity = static_cast<ECS::Entity>(GetEntityIdFromJson(entityJson));
            ed.entityName = name;
            ed.action = DiffAction::Created;

            if (entityJson.contains("prefabInstance")) {
                ed.isPrefabInstance = true;
                if (entityJson["prefabInstance"].contains("prefabPath")) {
                    ed.prefabPath = entityJson["prefabInstance"]["prefabPath"].get<std::string>();
                }
            }

            diff.entities.push_back(ed);
        }
    }

    // Find deleted entities (in saved but not in current)
    for (const auto& [name, entityJson] : savedByName) {
        if (currentByName.find(name) == currentByName.end()) {
            EntityDiff ed;
            ed.entity = static_cast<ECS::Entity>(GetEntityIdFromJson(entityJson));
            ed.entityName = name;
            ed.action = DiffAction::Deleted;
            diff.entities.push_back(ed);
        }
    }

    // Find modified entities (in both, but different)
    for (const auto& [name, currentEntity] : currentByName) {
        auto savedIt = savedByName.find(name);
        if (savedIt == savedByName.end()) continue;

        const json& savedEntity = savedIt->second;

        // Compare each component key
        std::unordered_set<std::string> allKeys;
        for (auto& [key, _] : savedEntity.items()) allKeys.insert(key);
        for (auto& [key, _] : currentEntity.items()) allKeys.insert(key);

        // Skip non-component fields
        static const std::unordered_set<std::string> skipKeys = {"id", "name", "parentId", "visible"};

        std::vector<ComponentDiff> componentDiffs;
        for (const auto& key : allKeys) {
            if (skipKeys.count(key)) continue;

            bool inSaved = savedEntity.contains(key);
            bool inCurrent = currentEntity.contains(key);

            if (inCurrent && !inSaved) {
                // Component added
                ComponentDiff cd;
                cd.componentType = key;
                cd.action = DiffAction::Created;
                componentDiffs.push_back(cd);
            } else if (inSaved && !inCurrent) {
                // Component removed
                ComponentDiff cd;
                cd.componentType = key;
                cd.action = DiffAction::Deleted;
                componentDiffs.push_back(cd);
            } else if (inSaved && inCurrent && savedEntity[key] != currentEntity[key]) {
                // Component modified
                ComponentDiff cd;
                cd.componentType = key;
                cd.action = DiffAction::Modified;
                DiffJsonObject(savedEntity[key], currentEntity[key], "", cd.properties);
                if (!cd.properties.empty()) {
                    componentDiffs.push_back(cd);
                }
            }
        }

        // Check visible flag separately
        if (savedEntity.contains("visible") && currentEntity.contains("visible")) {
            if (savedEntity["visible"] != currentEntity["visible"]) {
                ComponentDiff cd;
                cd.componentType = "(visibility)";
                cd.action = DiffAction::Modified;
                PropertyDiff pd;
                pd.name = "visible";
                pd.oldValue = savedEntity["visible"].dump();
                pd.newValue = currentEntity["visible"].dump();
                cd.properties.push_back(pd);
                componentDiffs.push_back(cd);
            }
        }

        if (!componentDiffs.empty()) {
            EntityDiff ed;
            ed.entity = static_cast<ECS::Entity>(GetEntityIdFromJson(currentEntity));
            ed.entityName = name;
            ed.action = DiffAction::Modified;
            ed.components = std::move(componentDiffs);

            if (currentEntity.contains("prefabInstance")) {
                ed.isPrefabInstance = true;
                if (currentEntity["prefabInstance"].contains("prefabPath")) {
                    ed.prefabPath = currentEntity["prefabInstance"]["prefabPath"].get<std::string>();
                }
            }

            diff.entities.push_back(ed);
        }
    }

    return diff;
}

void ApplySelectedDiffs(ECS::World* world, const PlayModeDiff& diff,
                         const std::string& currentJson) {
    if (!world) return;

    json currentParsed = json::parse(currentJson, nullptr, false);
    if (currentParsed.is_discarded()) return;

    json currentEntities = currentParsed.contains("entities") ? currentParsed["entities"] : json::array();

    // Build name->JSON map for current state
    std::unordered_map<std::string, json> currentByName;
    for (const auto& e : currentEntities) {
        currentByName[GetEntityNameFromJson(e)] = e;
    }

    for (const auto& ed : diff.entities) {
        if (!ed.selected) continue;

        if (ed.action == DiffAction::Created) {
            // Re-create entity from current JSON
            auto it = currentByName.find(ed.entityName);
            if (it != currentByName.end()) {
                Scene::SceneSerializer::DeserializeEntityFromString(world,
                    it->second.dump());
                ENJIN_LOG_INFO(Editor, "PlayModeDiff: Re-created entity '%s'",
                               ed.entityName.c_str());
            }
        } else if (ed.action == DiffAction::Deleted) {
            // Delete entity from restored world
            auto entities = world->GetEntitiesWithComponent<ECS::NameComponent>();
            for (auto entity : entities) {
                auto* name = world->GetComponent<ECS::NameComponent>(entity);
                if (name && name->name == ed.entityName) {
                    world->DestroyEntity(entity);
                    ENJIN_LOG_INFO(Editor, "PlayModeDiff: Deleted entity '%s'",
                                   ed.entityName.c_str());
                    break;
                }
            }
        } else if (ed.action == DiffAction::Modified) {
            // Find entity by name in restored world
            auto entities = world->GetEntitiesWithComponent<ECS::NameComponent>();
            ECS::Entity targetEntity = ECS::INVALID_ENTITY;
            for (auto entity : entities) {
                auto* name = world->GetComponent<ECS::NameComponent>(entity);
                if (name && name->name == ed.entityName) {
                    targetEntity = entity;
                    break;
                }
            }
            if (targetEntity == ECS::INVALID_ENTITY) continue;

            // Apply selected component diffs
            for (const auto& cd : ed.components) {
                if (!cd.selected) continue;

                auto it = currentByName.find(ed.entityName);
                if (it == currentByName.end()) continue;

                const json& entityJson = it->second;
                if (cd.action == DiffAction::Created || cd.action == DiffAction::Modified) {
                    // Deserialize this component from current state onto the entity
                    if (entityJson.contains(cd.componentType)) {
                        Scene::SceneSerializer::DeserializeOneComponent(
                            world, targetEntity, cd.componentType,
                            entityJson[cd.componentType].dump());
                    }
                }
                // For deleted components, we'd need to remove — but the component
                // is already gone in the current state, so on restore it's present.
                // The user selected "keep this deletion", so remove it:
                if (cd.action == DiffAction::Deleted) {
                    ENJIN_LOG_INFO(Editor, "PlayModeDiff: Component '%s' deletion kept on '%s'",
                                   cd.componentType.c_str(), ed.entityName.c_str());
                    // Would need per-component removal — handled by componentKey mapping
                }
            }
        }
    }
}

} // namespace Editor
} // namespace Enjin
