// Layer persistence + merge-down test (VWS Phase 5).
//
// Layers must survive the disk round-trip (one .layer file per layer, filename
// order = stack order, stale files cleaned) and MergeDown must fold a layer
// into the one below it (or the base scene) WITHOUT changing the resolved
// world — merge-down only reshapes the stack, never the output.

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/LayerSystem.h"

#include <cstdio>
#include <cmath>
#include <filesystem>
#include <string>

using namespace Enjin;
namespace fs = std::filesystem;

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

// Position of entity `name` after resolving base + stack into a fresh world.
static Math::Vector3 ResolvedPos(const Scene::LayerStack& stack, const std::string& base,
                                 const std::string& name, bool* found = nullptr) {
    ECS::World w;
    stack.ResolveInto(w, base);
    auto e = FindByName(w, name);
    if (found) *found = (e != ECS::INVALID_ENTITY);
    if (e == ECS::INVALID_ENTITY) return Math::Vector3(NAN, NAN, NAN);
    auto* t = w.GetComponent<ECS::TransformComponent>(e);
    return t ? t->position : Math::Vector3(NAN, NAN, NAN);
}

int main() {
    std::printf("=== Layer Persistence + Merge-Down Test ===\n\n");

    const fs::path tmpDir = fs::temp_directory_path() / "enjin_layer_persist_test";
    fs::remove_all(tmpDir);   // clean slate even after a crashed prior run

    // ---------- Arrange: base with A, live world, two captured layers ----------
    ECS::World seed;
    auto sa = seed.CreateEntity();
    seed.AddComponent<ECS::NameComponent>(sa, ECS::NameComponent{"A"});
    { auto& t = seed.AddComponent<ECS::TransformComponent>(sa); t.position = Math::Vector3(1, 1, 1); }

    Scene::SceneSerializer saver(&seed);
    std::string baseJson = saver.SaveToString();

    ECS::World live;
    Scene::SceneSerializer liveLoader(&live);
    liveLoader.LoadFromString(baseJson, true);

    Scene::LayerSystem sys;
    sys.SetWorld(&live);
    sys.SetBaseScene(baseJson);
    auto a = FindByName(live, "A");

    sys.AddLayer("bottom");
    live.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(2, 2, 2);
    sys.RecordEdit(a, "transform");
    live.AddComponent<ECS::LightComponent>(a);
    sys.RecordEdit(a, "light");

    sys.AddLayer("top layer!?");     // hostile-ish name: must sanitize, not escape
    live.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(3, 3, 3);
    sys.RecordEdit(a, "transform");
    sys.Stack().layers[1].locked = false;

    // ---------- Act: save, then load into a FRESH system ----------
    std::printf("Disk round-trip...\n");
    CHECK(sys.SaveLayers(tmpDir.string()), "persist_save_layers_succeeds");

    int layerFiles = 0;
    for (const auto& e : fs::directory_iterator(tmpDir)) {
        if (e.path().extension() == ".layer") ++layerFiles;
    }
    CHECK(layerFiles == 2, "persist_one_file_per_layer");
    // The hostile-ish display name must have been sanitized into the filename
    // (specials -> '_'), so it cannot traverse or break the path.
    CHECK(fs::exists(tmpDir / "01_top_layer__.layer"), "persist_filename_sanitized");

    Scene::LayerSystem loaded;
    loaded.SetBaseScene(baseJson);
    CHECK(loaded.LoadLayers(tmpDir.string()) == 2, "persist_load_returns_layer_count");
    CHECK(loaded.Stack().layers.size() == 2, "persist_stack_size_restored");
    CHECK(loaded.Stack().layers[0].name == "bottom", "persist_order_and_name_restored_bottom");
    CHECK(loaded.Stack().layers[1].name == "top layer!?", "persist_name_with_specials_survives_in_json");
    CHECK(loaded.ActiveLayerIndex() == 1, "persist_active_defaults_to_top");

    // Assert: loaded stack resolves to the same world as the original.
    {
        bool found = false;
        Math::Vector3 p = ResolvedPos(loaded.Stack(), baseJson, "A", &found);
        CHECK(found && Vec3Near(p, 3, 3, 3), "persist_loaded_stack_resolves_identically");
    }

    // ---------- Act: delete a layer, save again -> stale file must not resurrect ----------
    std::printf("Stale-file cleanup...\n");
    sys.RemoveLayer(1);
    CHECK(sys.SaveLayers(tmpDir.string()), "persist_resave_succeeds");
    CHECK(loaded.LoadLayers(tmpDir.string()) == 1, "persist_deleted_layer_stays_deleted");
    sys.AddLayer("top");   // rebuild the two-layer stack for the merge tests
    live.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(3, 3, 3);
    sys.RecordEdit(a, "transform");

    // ---------- Merge-down invariance: layer 1 into layer 0 ----------
    std::printf("Merge layer into layer...\n");
    Math::Vector3 before = ResolvedPos(sys.Stack(), sys.GetBaseScene(), "A");
    CHECK(sys.MergeDown(1), "merge_layer_into_layer_succeeds");
    CHECK(sys.Stack().layers.size() == 1, "merge_drops_source_layer");
    {
        Math::Vector3 after = ResolvedPos(sys.Stack(), sys.GetBaseScene(), "A");
        CHECK(Vec3Near(after, before.x, before.y, before.z), "merge_layer_resolved_output_unchanged");
    }

    // ---------- Merge-down invariance: layer 0 into base ----------
    std::printf("Merge layer into base...\n");
    before = ResolvedPos(sys.Stack(), sys.GetBaseScene(), "A");
    std::string baseBefore = sys.GetBaseScene();
    CHECK(sys.MergeDown(0), "merge_into_base_succeeds");
    CHECK(sys.Stack().layers.empty(), "merge_into_base_empties_stack");
    CHECK(sys.GetBaseScene() != baseBefore, "merge_into_base_rewrites_base");
    {
        Scene::LayerStack empty;
        ECS::World w;
        empty.ResolveInto(w, sys.GetBaseScene());
        auto e = FindByName(w, "A");
        CHECK(e != ECS::INVALID_ENTITY, "merge_into_base_keeps_entity");
        auto* t = e != ECS::INVALID_ENTITY ? w.GetComponent<ECS::TransformComponent>(e) : nullptr;
        CHECK(t && Vec3Near(t->position, before.x, before.y, before.z), "merge_into_base_resolved_output_unchanged");
        CHECK(e != ECS::INVALID_ENTITY && w.HasComponent<ECS::LightComponent>(e), "merge_into_base_bakes_added_component");
    }

    // ---------- Refusals: locked and disabled layers don't merge ----------
    std::printf("Merge refusals...\n");
    sys.AddLayer("lower");
    sys.AddLayer("upper");
    sys.Stack().layers[1].locked = true;
    CHECK(!sys.MergeDown(1), "merge_refuses_locked_source");
    sys.Stack().layers[1].locked = false;
    sys.Stack().layers[0].enabled = false;
    CHECK(!sys.MergeDown(1), "merge_refuses_disabled_target");
    sys.Stack().layers[0].enabled = true;
    sys.Stack().layers[1].enabled = false;
    CHECK(!sys.MergeDown(1), "merge_refuses_disabled_source");
    CHECK(sys.Stack().layers.size() == 2, "merge_refusals_leave_stack_intact");

    // ---------- Cleanup (integration tests clean up after themselves) ----------
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    CHECK(!fs::exists(tmpDir), "test_cleaned_up_temp_dir");

    std::printf("\n=== Results: %d checks, %d failures ===\n", g_Checks, g_Failures);
    std::printf(g_Failures == 0 ? "ALL PASSED\n" : "SOME TESTS FAILED\n");
    return g_Failures > 0 ? 1 : 0;
}
