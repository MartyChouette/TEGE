// Instant layer-toggle test (VWS Phase 3).
//
// SetLayerEnabled must apply a layer's effect to the LIVE world in place:
// component values fall back to base / lower layers on disable and come back
// on enable, layer-created entities appear and disappear, tombstones destroy
// and resurrect — all WITHOUT a scene reload, so untouched entity handles and
// un-captured live edits survive.

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/LayerSystem.h"

#include <cstdio>
#include <cmath>
#include <string>

using namespace Enjin;

static int g_Failures = 0;
static int g_Checks = 0;

#define CHECK(cond, label) do { \
    ++g_Checks; \
    if (!(cond)) { std::printf("  FAIL: %s\n", label); ++g_Failures; } \
} while(0)

static ECS::Entity FindByName(ECS::World& w, const std::string& name) {
    for (auto e : w.GetAllEntities()) {
        if (!w.IsValid(e)) continue;
        auto* nc = w.GetComponent<ECS::NameComponent>(e);
        if (nc && nc->name == name) return e;
    }
    return ECS::INVALID_ENTITY;
}

static bool Vec3Near(const Math::Vector3& v, f32 x, f32 y, f32 z) {
    return std::fabs(v.x - x) < 0.001f && std::fabs(v.y - y) < 0.001f && std::fabs(v.z - z) < 0.001f;
}

int main() {
    std::printf("=== Layer Instant-Toggle Test ===\n\n");

    // ---------- Arrange: base scene with A (at 1,2,3) and B ----------
    ECS::World seed;
    auto sa = seed.CreateEntity();
    seed.AddComponent<ECS::NameComponent>(sa, ECS::NameComponent{"A"});
    { auto& t = seed.AddComponent<ECS::TransformComponent>(sa); t.position = Math::Vector3(1, 2, 3); }
    auto sb = seed.CreateEntity();
    seed.AddComponent<ECS::NameComponent>(sb, ECS::NameComponent{"B"});
    seed.AddComponent<ECS::TransformComponent>(sb);
    seed.AddComponent<ECS::LightComponent>(sb);   // base-provided light (removal target)

    Scene::SceneSerializer saver(&seed);
    std::string baseJson = saver.SaveToString();
    CHECK(!baseJson.empty(), "base serialized");

    ECS::World live;
    Scene::SceneSerializer liveLoader(&live);
    CHECK(liveLoader.LoadFromString(baseJson, true).success, "base loaded into live world");

    Scene::LayerSystem sys;
    sys.SetWorld(&live);
    sys.SetBaseScene(baseJson);

    auto a = FindByName(live, "A");
    auto b = FindByName(live, "B");
    CHECK(a != ECS::INVALID_ENTITY && b != ECS::INVALID_ENTITY, "A and B live");

    // Un-captured live edit on B: must survive every toggle below untouched.
    live.GetComponent<ECS::TransformComponent>(b)->position = Math::Vector3(50, 50, 50);

    // ---------- Layer 1: move A, add light to A, remove B's light, create D ----------
    sys.AddLayer("L1");
    live.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(9, 9, 9);
    sys.RecordEdit(a, "transform");

    live.AddComponent<ECS::LightComponent>(a);
    sys.RecordEdit(a, "light");

    live.RemoveComponent<ECS::LightComponent>(b);
    sys.RecordRemoveComponent(b, "light");

    auto d = live.CreateEntity();
    live.AddComponent<ECS::NameComponent>(d, ECS::NameComponent{"D"});
    { auto& t = live.AddComponent<ECS::TransformComponent>(d); t.position = Math::Vector3(4, 4, 4); }
    sys.RecordCreate(d);
    u64 dSid = live.GetComponent<ECS::StableIdComponent>(d)->id;

    // ---------- Act: disable L1 ----------
    std::printf("Disable L1...\n");
    CHECK(sys.SetLayerEnabled(0, false), "L1 disabled with live apply");
    live.FlushPendingDestructions();

    // Assert: base values restored on the SAME entity handles (no reload).
    CHECK(live.IsValid(a), "toggle_disable_keeps_entity_handle: A handle still valid");
    {
        auto* t = live.GetComponent<ECS::TransformComponent>(a);
        CHECK(t && Vec3Near(t->position, 1, 2, 3), "toggle_disable_restores_base_transform");
    }
    CHECK(!live.HasComponent<ECS::LightComponent>(a), "toggle_disable_removes_layer_added_component");
    CHECK(live.HasComponent<ECS::LightComponent>(b), "toggle_disable_restores_removed_base_component");
    CHECK(FindByName(live, "D") == ECS::INVALID_ENTITY, "toggle_disable_destroys_layer_created_entity");
    {
        auto* t = live.GetComponent<ECS::TransformComponent>(b);
        CHECK(t && Vec3Near(t->position, 50, 50, 50), "toggle_disable_preserves_uncaptured_live_edit");
    }

    // ---------- Act: re-enable L1 ----------
    std::printf("Re-enable L1...\n");
    CHECK(sys.SetLayerEnabled(0, true), "L1 re-enabled with live apply");
    live.FlushPendingDestructions();

    {
        auto* t = live.GetComponent<ECS::TransformComponent>(a);
        CHECK(t && Vec3Near(t->position, 9, 9, 9), "toggle_enable_restores_layer_transform");
    }
    CHECK(live.HasComponent<ECS::LightComponent>(a), "toggle_enable_reapplies_layer_added_component");
    CHECK(!live.HasComponent<ECS::LightComponent>(b), "toggle_enable_reapplies_component_removal");
    auto d2 = FindByName(live, "D");
    CHECK(d2 != ECS::INVALID_ENTITY, "toggle_enable_recreates_layer_created_entity");
    if (d2 != ECS::INVALID_ENTITY) {
        auto* sid = live.GetComponent<ECS::StableIdComponent>(d2);
        CHECK(sid && sid->id == dSid, "toggle_enable_recreated_entity_keeps_stable_id");
        auto* t = live.GetComponent<ECS::TransformComponent>(d2);
        CHECK(t && Vec3Near(t->position, 4, 4, 4), "toggle_enable_recreated_entity_has_captured_transform");
    }

    // ---------- Layer 2 above: same key on A -> stacked fallback ----------
    std::printf("Stacked fallback (L2 over L1)...\n");
    sys.AddLayer("L2");
    live.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(7, 7, 7);
    sys.RecordEdit(a, "transform");

    // Act: disable L2 -> A falls back to L1's value, not base.
    CHECK(sys.SetLayerEnabled(1, false), "L2 disabled with live apply");
    {
        auto* t = live.GetComponent<ECS::TransformComponent>(a);
        CHECK(t && Vec3Near(t->position, 9, 9, 9), "toggle_disable_top_falls_back_to_lower_layer");
    }
    // Act: re-enable L2 -> topmost wins again.
    CHECK(sys.SetLayerEnabled(1, true), "L2 re-enabled with live apply");
    {
        auto* t = live.GetComponent<ECS::TransformComponent>(a);
        CHECK(t && Vec3Near(t->position, 7, 7, 7), "toggle_enable_top_wins_over_lower_layer");
    }

    // ---------- Tombstone: L2 destroys base entity B ----------
    std::printf("Tombstone toggle...\n");
    sys.SetActiveLayer(1);
    sys.RecordDestroy(b);
    live.DestroyEntity(b);   // the editor would do this alongside the capture
    live.FlushPendingDestructions();
    CHECK(FindByName(live, "B") == ECS::INVALID_ENTITY, "B destroyed in live world");

    // Act: disable L2 -> tombstone lifted, B resurrects from base + L1 deltas
    // (L1 removed B's light, so the resurrected B must NOT have one).
    CHECK(sys.SetLayerEnabled(1, false), "L2 (tombstone) disabled with live apply");
    live.FlushPendingDestructions();
    auto b2 = FindByName(live, "B");
    CHECK(b2 != ECS::INVALID_ENTITY, "toggle_disable_tombstone_resurrects_base_entity");
    if (b2 != ECS::INVALID_ENTITY) {
        CHECK(!live.HasComponent<ECS::LightComponent>(b2), "toggle_resurrected_entity_respects_lower_layer_removal");
    }

    // Act: re-enable L2 -> tombstone applies again.
    CHECK(sys.SetLayerEnabled(1, true), "L2 (tombstone) re-enabled with live apply");
    live.FlushPendingDestructions();
    CHECK(FindByName(live, "B") == ECS::INVALID_ENTITY, "toggle_enable_tombstone_destroys_entity_again");

    // ---------- No base captured: flag flips, live apply refused ----------
    std::printf("No-base guard...\n");
    Scene::LayerSystem bare;
    bare.SetWorld(&live);
    bare.AddLayer("orphan");
    CHECK(!bare.SetLayerEnabled(0, false), "toggle_without_base_reports_no_live_apply");
    CHECK(!bare.Stack().layers[0].enabled, "toggle_without_base_still_flips_flag");

    std::printf("\n=== Results: %d checks, %d failures ===\n", g_Checks, g_Failures);
    std::printf(g_Failures == 0 ? "ALL PASSED\n" : "SOME TESTS FAILED\n");
    return g_Failures > 0 ? 1 : 0;
}
