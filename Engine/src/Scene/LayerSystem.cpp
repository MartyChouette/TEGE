#include "Enjin/Scene/LayerSystem.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Logging/Log.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace Scene {

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Layer management
// ---------------------------------------------------------------------------

int LayerSystem::AddLayer(const std::string& name) {
    Layer layer;
    layer.name = name;
    // Seed a distinct default color from a rotating palette so new layers are
    // visually distinguishable in the viewport highlight without any user setup.
    static const Math::Vector3 kPalette[] = {
        {0.40f, 0.80f, 1.00f}, // cyan
        {1.00f, 0.55f, 0.30f}, // orange
        {0.55f, 1.00f, 0.45f}, // green
        {1.00f, 0.45f, 0.75f}, // pink
        {0.75f, 0.60f, 1.00f}, // violet
        {1.00f, 0.90f, 0.35f}, // yellow
        {0.45f, 0.70f, 1.00f}, // blue
        {1.00f, 0.45f, 0.45f}, // red
    };
    layer.color = kPalette[m_Stack.layers.size() % (sizeof(kPalette) / sizeof(kPalette[0]))];
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

// ---------------------------------------------------------------------------
// Instant toggle (Phase 3)
// ---------------------------------------------------------------------------

void LayerSystem::EnsureBaseIndex() {
    if (!m_BaseIndexDirty) return;
    m_BaseIndex.clear();
    m_BaseIndexDirty = false;
    if (m_BaseSceneJson.empty()) return;

    try {
        json root = ParseSceneJson(m_BaseSceneJson);
        if (root.contains("entities") && root["entities"].is_array()) {
            for (auto& e : root["entities"]) {
                u64 sid = e.value("stableId", u64{0});
                if (sid != 0) m_BaseIndex[sid] = e.dump();
            }
        }
    } catch (const std::exception& ex) {
        ENJIN_LOG_WARN(Build, "LayerSystem::EnsureBaseIndex parse error: %s", ex.what());
    }
}

bool LayerSystem::WinningComponentJson(u64 sid, const std::string& key, std::string& out) {
    bool present = false;

    // Base first. destroyed/created flags don't strip component values here —
    // LayerStack::Resolve reuses the entity object across tombstone+resurrect,
    // so the fallback must too.
    auto bit = m_BaseIndex.find(sid);
    if (bit != m_BaseIndex.end()) {
        try {
            json obj = json::parse(bit->second);
            auto it = obj.find(key);
            if (it != obj.end()) { out = it->dump(); present = true; }
        } catch (const std::exception&) { /* index entries were dumped by us; unreachable */ }
    }

    // Then every enabled layer bottom-to-top; the topmost provider wins.
    for (const Layer& l : m_Stack.layers) {
        if (!l.enabled) continue;
        const EntityDelta* d = l.FindEntity(sid);
        if (!d) continue;
        for (const ComponentDelta& c : d->components) {
            if (c.key != key) continue;
            if (c.json.empty()) { present = false; }
            else { out = c.json; present = true; }
        }
    }
    return present;
}

std::string LayerSystem::ResolvedEntityJsonText(u64 sid) {
    json obj;
    auto bit = m_BaseIndex.find(sid);
    if (bit != m_BaseIndex.end()) {
        try { obj = json::parse(bit->second); } catch (const std::exception&) { obj = json::object(); }
    }
    if (obj.is_null() || obj.empty()) {
        obj = json::object();
        obj["id"] = sid;         // mirrors LayerStack::Resolve for layer-created
        obj["stableId"] = sid;   // entities
    }

    for (const Layer& l : m_Stack.layers) {
        if (!l.enabled) continue;
        const EntityDelta* d = l.FindEntity(sid);
        if (!d || d->destroyed) continue;
        for (const ComponentDelta& c : d->components) {
            if (c.key.empty()) continue;
            if (c.json.empty()) { obj.erase(c.key); continue; }
            try { obj[c.key] = ParseSceneJson(c.json); }
            catch (const std::exception& ex) {
                ENJIN_LOG_WARN(Build, "LayerSystem: bad delta for key '%s': %s", c.key.c_str(), ex.what());
            }
        }
    }
    return obj.dump();
}

void LayerSystem::ReconcileEntity(const EntityDelta& d, std::unordered_map<u64, ECS::Entity>& liveBySid) {
    const u64 sid = d.stableId;

    // Aliveness under the CURRENT stack state: base existence, then each enabled
    // layer's created/destroyed flags bottom-to-top (mirrors LayerStack::Resolve).
    bool alive = m_BaseIndex.count(sid) != 0;
    for (const Layer& l : m_Stack.layers) {
        if (!l.enabled) continue;
        const EntityDelta* ld = l.FindEntity(sid);
        if (!ld) continue;
        if (ld->destroyed)    alive = false;
        else if (ld->created) alive = true;
    }

    auto it = liveBySid.find(sid);
    ECS::Entity live = (it != liveBySid.end() && m_World->IsValid(it->second))
                       ? it->second : ECS::INVALID_ENTITY;

    if (!alive) {
        if (live != ECS::INVALID_ENTITY) {
            m_World->DestroyEntity(live);   // deferred; flushed at World::Update
            liveBySid.erase(sid);
        }
        return;
    }

    if (live == ECS::INVALID_ENTITY) {
        // (Re)introduce: created entity enabled, or tombstone lifted. Build the
        // fully resolved object and instantiate it.
        ECS::Entity e = SceneSerializer::DeserializeEntityFromString(m_World, ResolvedEntityJsonText(sid));
        if (m_World->IsValid(e)) {
            // DeserializeEntityFromString does not restore stable ids (only the
            // whole-scene path does) — re-attach the durable identity.
            m_World->AddComponent<ECS::StableIdComponent>(e, ECS::StableIdComponent{sid});
            liveBySid[sid] = e;
        }
        return;
    }

    // Alive and present: reconcile exactly the keys this layer touches. AddComponent
    // overwrites in place, so the entity handle and unrelated components are stable.
    for (const ComponentDelta& c : d.components) {
        if (c.key.empty() || c.key == "id" || c.key == "stableId") continue;
        std::string winning;
        if (WinningComponentJson(sid, c.key, winning)) {
            SceneSerializer::DeserializeOneComponent(m_World, live, c.key, winning);
        } else {
            SceneSerializer::RemoveOneComponent(m_World, live, c.key);
        }
    }
}

bool LayerSystem::SetLayerEnabled(int index, bool enabled) {
    if (index < 0 || index >= static_cast<int>(m_Stack.layers.size())) return false;
    Layer& layer = m_Stack.layers[index];
    if (layer.enabled == enabled) return true;

    // Flip first so aliveness/fallback computations see the new stack state.
    layer.enabled = enabled;

    if (!m_World) return false;
    if (m_BaseSceneJson.empty()) {
        ENJIN_LOG_WARN(Build, "LayerSystem::SetLayerEnabled: no base scene captured — flag flipped, live world not reconciled");
        return false;
    }
    EnsureBaseIndex();

    std::unordered_map<u64, ECS::Entity> liveBySid;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::StableIdComponent>()) {
        if (!m_World->IsValid(e)) continue;
        if (auto* s = m_World->GetComponent<ECS::StableIdComponent>(e)) {
            if (s->id != 0) liveBySid[s->id] = e;
        }
    }

    for (const EntityDelta& d : layer.entities) {
        if (d.stableId == 0) continue;
        ReconcileEntity(d, liveBySid);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Persistence (Phase 5)
// ---------------------------------------------------------------------------

namespace fs = std::filesystem;

// Layer names are user-typed; the filename derived from one must not be able to
// escape the layer directory or produce an unopenable path.
static std::string SanitizeLayerFileName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                        (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
        out.push_back(ok ? ch : '_');
    }
    if (out.empty()) out = "layer";
    return out;
}

bool LayerSystem::SaveLayers(const std::string& dirPath) const {
    if (dirPath.empty()) return false;

    std::error_code ec;
    fs::create_directories(dirPath, ec);
    if (ec) {
        ENJIN_LOG_ERROR(Build, "LayerSystem::SaveLayers: cannot create '%s': %s",
                        dirPath.c_str(), ec.message().c_str());
        return false;
    }

    // Drop stale .layer files so a layer deleted this session stays deleted.
    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (!ec && entry.is_regular_file() && entry.path().extension() == ".layer") {
            fs::remove(entry.path(), ec);
        }
    }

    bool allOk = true;
    for (usize i = 0; i < m_Stack.layers.size(); ++i) {
        const Layer& layer = m_Stack.layers[i];
        char prefix[8];
        std::snprintf(prefix, sizeof(prefix), "%02zu_", i);
        fs::path file = fs::path(dirPath) / (prefix + SanitizeLayerFileName(layer.name) + ".layer");

        std::ofstream f(file, std::ios::binary | std::ios::trunc);
        if (!f) {
            ENJIN_LOG_ERROR(Build, "LayerSystem::SaveLayers: cannot write '%s'", file.string().c_str());
            allOk = false;
            continue;
        }
        f << layer.ToJson(true);
    }
    return allOk;
}

int LayerSystem::LoadLayers(const std::string& dirPath) {
    m_Stack.layers.clear();
    m_ActiveLayer = -1;
    if (dirPath.empty()) return 0;

    std::error_code ec;
    if (!fs::is_directory(dirPath, ec) || ec) return 0;

    // Filename order (the NN_ prefix) is the stack order.
    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(dirPath, ec)) {
        if (!ec && entry.is_regular_file() && entry.path().extension() == ".layer") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        std::ifstream f(file, std::ios::binary);
        if (!f) {
            ENJIN_LOG_WARN(Build, "LayerSystem::LoadLayers: cannot read '%s'", file.string().c_str());
            continue;
        }
        std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        Layer layer = Layer::FromJson(text);
        // FromJson returns a default (nameless, empty) layer on parse/cap
        // failure — an authored empty layer still carries its name, so this
        // only drops genuinely broken files.
        if (layer.name.empty() && layer.entities.empty()) {
            ENJIN_LOG_WARN(Build, "LayerSystem::LoadLayers: skipping unparseable '%s'", file.string().c_str());
            continue;
        }
        m_Stack.layers.push_back(std::move(layer));
    }

    if (!m_Stack.layers.empty()) {
        m_ActiveLayer = static_cast<int>(m_Stack.layers.size()) - 1;
    }
    return static_cast<int>(m_Stack.layers.size());
}

// ---------------------------------------------------------------------------
// Merge-down (Phase 5)
// ---------------------------------------------------------------------------

bool LayerSystem::MergeDown(int index) {
    if (index < 0 || index >= static_cast<int>(m_Stack.layers.size())) return false;
    Layer& src = m_Stack.layers[index];

    if (!src.enabled || src.locked) {
        ENJIN_LOG_WARN(Build, "LayerSystem::MergeDown: layer '%s' is %s — not merging",
                       src.name.c_str(), src.locked ? "locked" : "disabled");
        return false;
    }

    if (index == 0) {
        // Fold into the base scene: resolve base + just this layer, adopt the
        // result as the new pristine base. The resolved world output is
        // identical before and after by construction.
        if (m_BaseSceneJson.empty()) {
            ENJIN_LOG_WARN(Build, "LayerSystem::MergeDown: no base scene captured — cannot merge '%s' down",
                           src.name.c_str());
            return false;
        }
        LayerStack solo;
        solo.layers.push_back(src);
        m_BaseSceneJson = solo.Resolve(m_BaseSceneJson);
        m_BaseIndexDirty = true;
        RemoveLayer(0);
        return true;
    }

    Layer& dst = m_Stack.layers[index - 1];
    if (!dst.enabled || dst.locked) {
        ENJIN_LOG_WARN(Build, "LayerSystem::MergeDown: target layer '%s' is %s — not merging",
                       dst.name.c_str(), dst.locked ? "locked" : "disabled");
        return false;
    }

    // Fold src's deltas over dst's, mirroring how Resolve would stack them.
    for (const EntityDelta& d : src.entities) {
        if (d.stableId == 0) continue;
        EntityDelta& tgt = dst.EntityFor(d.stableId);

        if (d.destroyed) {
            tgt.destroyed = true;
            tgt.created = false;
            tgt.components.clear();
            continue;
        }
        if (d.created) {
            tgt.created = true;
            tgt.destroyed = false;
        } else if (tgt.destroyed) {
            // dst tombstoned it and src edited without resurrecting: Resolve
            // skips such stale deltas, so folding drops them too.
            continue;
        }
        for (const ComponentDelta& c : d.components) {
            if (c.key.empty()) continue;
            UpsertComponent(tgt, c.key, c.json);
        }
    }
    RemoveLayer(index);
    return true;
}

} // namespace Scene
} // namespace Enjin
