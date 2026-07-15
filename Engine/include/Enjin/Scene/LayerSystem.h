#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Scene/LayerStack.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Scene {

// ---------------------------------------------------------------------------
// LayerSystem — the capture side of the override-layer (VWS) model.
//
// LayerStack (resolve side) answers "given a base + layers, what's the world?".
// LayerSystem answers "the user just edited the live world, fold that edit into
// the active layer". It is the choke point an editor routes edits through: the
// inspector or gizmo mutates a component on the live world, then calls
// RecordEdit(entity, key), and the change is captured into the active layer as
// a sparse delta. Nothing here touches the base scene; deletes stay reversible.
//
// Capture is exact and cheap: it serializes only the one edited component (via
// SceneSerializer::SerializeOneComponent), so there is no whole-scene snapshot
// and no JSON-roundtrip-the-world cost on every keystroke.
//
// Everything is addressed by StableIdComponent::id. Entities that don't have
// one yet (freshly created in the editor) get one assigned on first capture.
// ---------------------------------------------------------------------------
class ENJIN_API LayerSystem {
public:
    LayerSystem() = default;

    void SetWorld(ECS::World* w) { m_World = w; }
    ECS::World* GetWorld() const { return m_World; }

    // The pristine base scene JSON that layers resolve over.
    void SetBaseScene(const std::string& json) { m_BaseSceneJson = json; m_BaseIndexDirty = true; }
    const std::string& GetBaseScene() const { return m_BaseSceneJson; }

    LayerStack& Stack() { return m_Stack; }
    const LayerStack& Stack() const { return m_Stack; }

    // --- Layer management ---
    // Add a new (enabled) layer on top and make it active. Returns its index.
    int AddLayer(const std::string& name);
    void RemoveLayer(int index);
    void SetActiveLayer(int index);
    int ActiveLayerIndex() const { return m_ActiveLayer; }
    Layer* ActiveLayer();                 // null if none active

    // Ensure `e` has a StableIdComponent, assigning one if missing. Returns the
    // id (0 only if the world/entity is invalid).
    u64 EnsureStableId(ECS::Entity e);

    // --- Capture (no-ops if there is no active layer or no world) ---
    // Fold the entity's CURRENT value of one component into the active layer
    // (override if the base had it, add if it didn't).
    void RecordEdit(ECS::Entity e, const std::string& key);
    // Record that the active layer removes a component the base provided.
    void RecordRemoveComponent(ECS::Entity e, const std::string& key);
    // Mark `e` as introduced by the active layer and capture all of its current
    // components. Call after the editor has built the new entity.
    void RecordCreate(ECS::Entity e);
    // Tombstone a base-scene entity in the active layer.
    void RecordDestroy(ECS::Entity e);
    // Capture only the components that changed between a prior snapshot of the
    // entity and its current state. `beforeEntityJson` is a SerializeEntityToString
    // taken before an edit burst (e.g. at the top of an inspector frame). Added or
    // mutated components are folded into the active layer; components present in the
    // snapshot but gone now are recorded as removals. Nothing lands when nothing
    // changed, which keeps the layer a sparse delta even while the inspector redraws
    // an entity every frame. No-op with no active layer / no world.
    void RecordEntityChanges(ECS::Entity e, const std::string& beforeEntityJson);

    // Rebuild the world from base + the full stack (clears the world first).
    DeserializationResult ResolveIntoWorld();

    // --- Instant toggle (Phase 3) ---
    // Enable/disable a layer and apply the effect directly to the live world in
    // O(deltas of that layer): each (entity, component) the layer touches is
    // recomputed from base + the other enabled layers and upserted/removed in
    // place; created entities appear/disappear; tombstones destroy/resurrect.
    // No scene reload, so live entity handles and un-captured edits elsewhere
    // survive. The fallback is recomputed from the stack rather than read from a
    // pre-image cache captured at edit time — a cached pre-image goes stale the
    // moment a lower layer changes, the stack never does.
    // Returns false when the live apply was skipped (no world, bad index, or no
    // base scene captured); the enabled flag still flips so a later full
    // ResolveIntoWorld honors it.
    bool SetLayerEnabled(int index, bool enabled);

    // --- Persistence (Phase 5) ---
    // One file per layer under `dirPath` ("NN_name.layer", NN = stack position so
    // ordering survives the filesystem). Distinct layers in distinct files is the
    // Git story: parallel edit sessions never conflict on a shared scene file.
    // SaveLayers clears stale *.layer files in the directory first (a layer
    // deleted this session must not resurrect on the next load).
    bool SaveLayers(const std::string& dirPath) const;
    // Replaces the current stack with the directory's *.layer files (filename
    // order). Active layer becomes the topmost loaded layer, -1 when none.
    // Returns the number of layers loaded.
    int LoadLayers(const std::string& dirPath);

    // --- Merge-down (Phase 5) ---
    // Fold layer `index` into what's directly below it — layer index-1, or the
    // base scene when index is 0 — then drop the layer. The resolved output is
    // unchanged by construction; only the stack shape changes. Refuses (false)
    // when either side is locked or disabled: merging a muted or locked track
    // silently baking edits is never what the user meant.
    bool MergeDown(int index);

private:
    // Lazily parse m_BaseSceneJson into stableId -> entity-object text.
    void EnsureBaseIndex();
    // Winning value for (sid, key) across base + enabled layers, bottom-to-top.
    // Returns false when nobody provides the component (out untouched).
    bool WinningComponentJson(u64 sid, const std::string& key, std::string& out);
    // Full resolved entity object (base + enabled deltas) as JSON text.
    std::string ResolvedEntityJsonText(u64 sid);
    // Re-apply one entity delta's footprint to the live world (see SetLayerEnabled).
    void ReconcileEntity(const EntityDelta& d, std::unordered_map<u64, ECS::Entity>& liveBySid);
    // Insert-or-replace a component delta on an entity delta, keyed by component.
    static void UpsertComponent(EntityDelta& d, const std::string& key, const std::string& json);
    // Capture every serializable component currently on `e` into `d`.
    void CaptureAllComponents(EntityDelta& d, ECS::Entity e);

    ECS::World* m_World = nullptr;
    std::string m_BaseSceneJson;
    LayerStack  m_Stack;
    int         m_ActiveLayer = -1;

    // stableId -> base entity object text; rebuilt after SetBaseScene.
    std::unordered_map<u64, std::string> m_BaseIndex;
    bool m_BaseIndexDirty = true;
};

} // namespace Scene
} // namespace Enjin
