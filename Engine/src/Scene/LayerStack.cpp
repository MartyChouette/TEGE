#include "Enjin/Scene/LayerStack.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>

namespace Enjin {
namespace Scene {

using json = nlohmann::json;

static constexpr u32 LAYER_FORMAT_VERSION = 1;

// ---------------------------------------------------------------------------
// Layer (de)serialization
// ---------------------------------------------------------------------------

std::string Layer::ToJson(bool prettyPrint) const {
    json root;
    root["layerVersion"] = LAYER_FORMAT_VERSION;
    root["name"] = name;
    root["enabled"] = enabled;
    root["locked"] = locked;

    json ents = json::array();
    for (const EntityDelta& d : entities) {
        json e;
        e["stableId"] = d.stableId;
        if (d.created)   e["created"] = true;
        if (d.destroyed) e["destroyed"] = true;

        // Components as a key -> (object | null) map. null == "remove this
        // component". A destroyed entity carries no component edits.
        if (!d.destroyed && !d.components.empty()) {
            json comps = json::object();
            for (const ComponentDelta& c : d.components) {
                if (c.json.empty()) {
                    comps[c.key] = nullptr;                 // removal marker
                } else {
                    comps[c.key] = json::parse(c.json);
                }
            }
            e["components"] = comps;
        }
        ents.push_back(std::move(e));
    }
    root["entities"] = std::move(ents);

    return prettyPrint ? root.dump(2) : root.dump();
}

Layer Layer::FromJson(const std::string& jsonStr) {
    Layer layer;
    if (jsonStr.empty()) return layer;

    try {
        json root = json::parse(jsonStr);
        layer.name    = root.value("name", std::string{});
        layer.enabled = root.value("enabled", true);
        layer.locked  = root.value("locked", false);

        if (root.contains("entities") && root["entities"].is_array()) {
            for (const auto& e : root["entities"]) {
                EntityDelta d;
                d.stableId  = e.value("stableId", u64{0});
                d.created   = e.value("created", false);
                d.destroyed = e.value("destroyed", false);
                if (e.contains("components") && e["components"].is_object()) {
                    for (auto it = e["components"].begin(); it != e["components"].end(); ++it) {
                        ComponentDelta c;
                        c.key  = it.key();
                        c.json = it.value().is_null() ? std::string{} : it.value().dump();
                        d.components.push_back(std::move(c));
                    }
                }
                layer.entities.push_back(std::move(d));
            }
        }
    } catch (const std::exception& ex) {
        ENJIN_LOG_ERROR(Build, "Layer::FromJson parse error: %s", ex.what());
    }
    return layer;
}

EntityDelta& Layer::EntityFor(u64 stableId) {
    for (EntityDelta& d : entities) {
        if (d.stableId == stableId) return d;
    }
    entities.push_back(EntityDelta{});
    entities.back().stableId = stableId;
    return entities.back();
}

const EntityDelta* Layer::FindEntity(u64 stableId) const {
    for (const EntityDelta& d : entities) {
        if (d.stableId == stableId) return &d;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Resolve: merge enabled layers over the base scene JSON
// ---------------------------------------------------------------------------

std::string LayerStack::Resolve(const std::string& baseSceneJson) const {
    json root;
    try {
        root = json::parse(baseSceneJson);
    } catch (const std::exception& ex) {
        ENJIN_LOG_ERROR(Build, "LayerStack::Resolve: base scene JSON parse error: %s", ex.what());
        return baseSceneJson;  // hand back the base unchanged on failure
    }

    if (!root.contains("entities") || !root["entities"].is_array()) {
        return baseSceneJson;  // nothing to layer over
    }

    // Pull the entities out into a working vector so we can append/erase without
    // fighting json-array index invalidation, and index them by stable id.
    std::vector<json> ents;
    ents.reserve(root["entities"].size());
    std::unordered_map<u64, usize> indexByStableId;   // stableId -> position in ents
    std::unordered_set<u64> removed;                   // tombstoned stable ids

    for (auto& e : root["entities"]) {
        if (e.contains("stableId") && e["stableId"].is_number_unsigned()) {
            indexByStableId[e["stableId"].get<u64>()] = ents.size();
        }
        ents.push_back(e);
    }

    // Apply each enabled layer bottom-to-top.
    for (const Layer& layer : layers) {
        if (!layer.enabled) continue;

        for (const EntityDelta& d : layer.entities) {
            if (d.stableId == 0) continue;

            auto it = indexByStableId.find(d.stableId);
            const bool exists = (it != indexByStableId.end()) && !removed.count(d.stableId);

            if (d.destroyed) {
                if (it != indexByStableId.end()) removed.insert(d.stableId);
                continue;
            }

            usize pos;
            if (exists) {
                pos = it->second;
            } else if (d.created || it == indexByStableId.end()) {
                // Introduce the entity. A created delta that was previously
                // tombstoned by a lower layer is resurrected here.
                removed.erase(d.stableId);
                if (it != indexByStableId.end()) {
                    pos = it->second;               // slot already present, reuse
                } else {
                    json e;
                    e["id"] = d.stableId;           // stable id doubles as the
                    e["stableId"] = d.stableId;     // load-time "id" for refs
                    pos = ents.size();
                    indexByStableId[d.stableId] = pos;
                    ents.push_back(std::move(e));
                }
            } else {
                continue;  // stale delta against a removed entity; skip
            }

            // Apply this layer's component edits to the entity object.
            for (const ComponentDelta& c : d.components) {
                if (c.key.empty()) continue;
                if (c.json.empty()) {
                    ents[pos].erase(c.key);                    // remove component
                } else {
                    try {
                        ents[pos][c.key] = json::parse(c.json); // override / add
                    } catch (const std::exception& ex) {
                        ENJIN_LOG_WARN(Build, "LayerStack::Resolve: bad delta for key '%s': %s",
                                       c.key.c_str(), ex.what());
                    }
                }
            }
        }
    }

    // Rebuild the entities array, dropping tombstoned entities.
    json out = json::array();
    for (usize i = 0; i < ents.size(); ++i) {
        u64 sid = ents[i].value("stableId", u64{0});
        if (sid != 0 && removed.count(sid)) continue;
        out.push_back(std::move(ents[i]));
    }
    root["entities"] = std::move(out);
    root["entityCount"] = root["entities"].size();

    return root.dump();
}

DeserializationResult LayerStack::ResolveInto(ECS::World& world, const std::string& baseSceneJson) const {
    std::string merged = Resolve(baseSceneJson);
    SceneSerializer loader(&world);
    return loader.LoadFromString(merged, true);
}

} // namespace Scene
} // namespace Enjin
