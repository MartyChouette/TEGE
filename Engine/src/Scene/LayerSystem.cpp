#include "Enjin/Scene/LayerSystem.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>

namespace Enjin {
namespace Scene {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Layer management
// ---------------------------------------------------------------------------

int LayerSystem::AddLayer(const std::string& name) {
    Layer layer;
    layer.name = name;
    m_Stack.layers.push_back(std::move(layer));
    m_ActiveLayer = static_cast<int>(m_Stack.layers.size()) - 1;
    return m_ActiveLayer;
}

void LayerSystem::RemoveLayer(int index) {
    if (index < 0 || index >= static_cast<int>(m_Stack.layers.size())) return;
    m_Stack.layers.erase(m_Stack.layers.begin() + index);
    if (m_ActiveLayer == index) {
        m_ActiveLayer = m_Stack.layers.empty() ? -1
                      : (index >= static_cast<int>(m_Stack.layers.size()) ? index - 1 : index);
    } else if (m_ActiveLayer > index) {
        --m_ActiveLayer;   // shift to track the same layer after the erase
    }
}

void LayerSystem::SetActiveLayer(int index) {
    if (index >= -1 && index < static_cast<int>(m_Stack.layers.size())) {
        m_ActiveLayer = index;
    }
}

Layer* LayerSystem::ActiveLayer() {
    if (m_ActiveLayer < 0 || m_ActiveLayer >= static_cast<int>(m_Stack.layers.size())) return nullptr;
    return &m_Stack.layers[m_ActiveLayer];
}

// ---------------------------------------------------------------------------
// Stable id
// ---------------------------------------------------------------------------

u64 LayerSystem::EnsureStableId(ECS::Entity e) {
    if (!m_World || !m_World->IsValid(e)) return 0;
    if (auto* s = m_World->GetComponent<ECS::StableIdComponent>(e)) {
        if (s->id == 0) s->id = ECS::GenerateStableId();
        return s->id;
    }
    u64 id = ECS::GenerateStableId();
    m_World->AddComponent<ECS::StableIdComponent>(e, ECS::StableIdComponent{id});
    return id;
}

// ---------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------

void LayerSystem::UpsertComponent(EntityDelta& d, const std::string& key, const std::string& jsonStr) {
    for (ComponentDelta& c : d.components) {
        if (c.key == key) { c.json = jsonStr; return; }
    }
    d.components.push_back(ComponentDelta{key, jsonStr});
}

void LayerSystem::CaptureAllComponents(EntityDelta& d, ECS::Entity e) {
    // SerializeEntityToString gives the full entity object: one key per
    // component, plus bookkeeping keys we don't treat as components.
    std::string entityJson = SceneSerializer::SerializeEntityToString(m_World, e);
    if (entityJson.empty()) return;

    try {
        json obj = json::parse(entityJson);
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const std::string& key = it.key();
            if (key == "id" || key == "stableId") continue;   // not component deltas
            UpsertComponent(d, key, it.value().dump());
        }
    } catch (const std::exception& ex) {
        ENJIN_LOG_WARN(Build, "LayerSystem::CaptureAllComponents parse error: %s", ex.what());
    }
}

void LayerSystem::RecordEdit(ECS::Entity e, const std::string& key) {
    Layer* layer = ActiveLayer();
    if (!layer || !m_World) return;
    u64 sid = EnsureStableId(e);
    if (sid == 0) return;

    std::string j = SceneSerializer::SerializeOneComponent(m_World, e, key);
    if (j.empty()) return;   // component absent or key unknown — nothing to capture

    UpsertComponent(layer->EntityFor(sid), key, j);
}

void LayerSystem::RecordRemoveComponent(ECS::Entity e, const std::string& key) {
    Layer* layer = ActiveLayer();
    if (!layer || !m_World) return;
    u64 sid = EnsureStableId(e);
    if (sid == 0) return;
    UpsertComponent(layer->EntityFor(sid), key, std::string{});   // empty json = remove
}

void LayerSystem::RecordCreate(ECS::Entity e) {
    Layer* layer = ActiveLayer();
    if (!layer || !m_World) return;
    u64 sid = EnsureStableId(e);
    if (sid == 0) return;

    EntityDelta& d = layer->EntityFor(sid);
    d.created = true;
    d.destroyed = false;
    CaptureAllComponents(d, e);
}

void LayerSystem::RecordDestroy(ECS::Entity e) {
    Layer* layer = ActiveLayer();
    if (!layer || !m_World) return;

    // Read the stable id directly; do not assign one to an entity we're deleting.
    auto* s = m_World->IsValid(e) ? m_World->GetComponent<ECS::StableIdComponent>(e) : nullptr;
    if (!s || s->id == 0) return;   // no durable identity => nothing the layer can address
    u64 sid = s->id;

    // If this layer created the entity, destroying it just drops the delta.
    // Otherwise it's a base entity: tombstone it.
    for (usize i = 0; i < layer->entities.size(); ++i) {
        if (layer->entities[i].stableId == sid) {
            if (layer->entities[i].created) {
                layer->entities.erase(layer->entities.begin() + i);
                return;
            }
            break;
        }
    }
    EntityDelta& d = layer->EntityFor(sid);
    d.destroyed = true;
    d.created = false;
    d.components.clear();
}

void LayerSystem::RecordEntityChanges(ECS::Entity e, const std::string& beforeEntityJson) {
    Layer* layer = ActiveLayer();
    if (!layer || !m_World || !m_World->IsValid(e)) return;

    // No vertex data: the diff only tracks inspector-editable component fields, and
    // the caller's `beforeEntityJson` was captured the same way. Excluding the mesh
    // vertex/index arrays keeps this cheap enough to run every inspector frame.
    std::string afterEntityJson = SceneSerializer::SerializeEntityToString(m_World, e, /*includeVertexData=*/false);
    if (afterEntityJson.empty()) return;

    json before, after;
    try {
        before = beforeEntityJson.empty() ? json::object() : json::parse(beforeEntityJson);
        after  = json::parse(afterEntityJson);
    } catch (const std::exception& ex) {
        ENJIN_LOG_WARN(Build, "LayerSystem::RecordEntityChanges parse error: %s", ex.what());
        return;
    }

    // Only touch the layer if something actually differs — otherwise a still
    // inspector would fold every drawn component into the delta every frame.
    if (before == after) return;

    u64 sid = EnsureStableId(e);
    if (sid == 0) return;
    EntityDelta& d = layer->EntityFor(sid);

    // Added or mutated components (skip the bookkeeping keys).
    for (auto it = after.begin(); it != after.end(); ++it) {
        const std::string& key = it.key();
        if (key == "id" || key == "stableId") continue;
        auto bit = before.find(key);
        if (bit == before.end() || *bit != it.value()) {
            UpsertComponent(d, key, it.value().dump());
        }
    }
    // Components that were present before the edit and are gone now = removals.
    for (auto it = before.begin(); it != before.end(); ++it) {
        const std::string& key = it.key();
        if (key == "id" || key == "stableId") continue;
        if (after.find(key) == after.end()) {
            UpsertComponent(d, key, std::string{});   // empty json = remove
        }
    }
}

// ---------------------------------------------------------------------------
// Resolve
// ---------------------------------------------------------------------------

DeserializationResult LayerSystem::ResolveIntoWorld() {
    if (!m_World) return DeserializationResult{};
    return m_Stack.ResolveInto(*m_World, m_BaseSceneJson);
}

} // namespace Scene
} // namespace Enjin
