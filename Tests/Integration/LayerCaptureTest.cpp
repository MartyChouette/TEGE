// Layer capture-session test.
//
// Simulates what an editor does: load a base scene into a live world, start a
// layer, then make edits to the live world and route each through LayerSystem's
// capture API. Finally resolve base + captured stack into a FRESH world and
// verify it reproduces the live edits. This proves the capture side produces
// deltas that resolve back to exactly what the user did.

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
        auto* nc = w.GetComponent<ECS::NameComponent>(e);
        if (nc && nc->name == name) return e;
    }
    return ECS::INVALID_ENTITY;
}

static bool Vec3Near(const Math::Vector3& v, f32 x, f32 y, f32 z) {
    return std::fabs(v.x - x) < 0.001f && std::fabs(v.y - y) < 0.001f && std::fabs(v.z - z) < 0.001f;
}

int main() {
    std::printf("=== Layer Capture-Session Test ===\n\n");

    // ---------- Build a base scene: A, B, C ----------
    ECS::World seed;
    auto sa = seed.CreateEntity();
    seed.AddComponent<ECS::NameComponent>(sa, ECS::NameComponent{"A"});
    { auto& t = seed.AddComponent<ECS::TransformComponent>(sa); t.position = Math::Vector3(1, 2, 3); }
    auto sb = seed.CreateEntity();
    seed.AddComponent<ECS::NameComponent>(sb, ECS::NameComponent{"B"});
    seed.AddComponent<ECS::TransformComponent>(sb);
    auto sc = seed.CreateEntity();
    seed.AddComponent<ECS::NameComponent>(sc, ECS::NameComponent{"C"});
    seed.AddComponent<ECS::TransformComponent>(sc);

    Scene::SceneSerializer saver(&seed);
    std::string baseJson = saver.SaveToString();   // assigns stable ids on the way out
    CHECK(!baseJson.empty(), "base serialized");

    // ---------- Load base into a LIVE world (the editor's working copy) ----------
    ECS::World live;
    Scene::SceneSerializer liveLoader(&live);
    auto lr = liveLoader.LoadFromString(baseJson, true);
    CHECK(lr.success, "base loaded into live world");

    Scene::LayerSystem sys;
    sys.SetWorld(&live);
    sys.SetBaseScene(baseJson);
    int idx = sys.AddLayer("session");
    CHECK(idx == 0 && sys.ActiveLayerIndex() == 0, "layer added and active");

    // ---------- Edit the live world, capturing each change ----------
    // 1. Move A and capture.
    auto a = FindByName(live, "A");
    live.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(9, 9, 9);
    sys.RecordEdit(a, "transform");

    // Re-edit the SAME component: should upsert, not duplicate.
    live.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(7, 7, 7);
    sys.RecordEdit(a, "transform");

    // 2. Add a light to B and capture.
    auto b = FindByName(live, "B");
    live.AddComponent<ECS::LightComponent>(b);
    sys.RecordEdit(b, "light");

    // 3. Tombstone C.
    auto c = FindByName(live, "C");
    sys.RecordDestroy(c);

    // 4. Create a new entity D and capture it whole.
    auto d = live.CreateEntity();
    live.AddComponent<ECS::NameComponent>(d, ECS::NameComponent{"Created"});
    { auto& t = live.AddComponent<ECS::TransformComponent>(d); t.position = Math::Vector3(5, 5, 5); }
    sys.RecordCreate(d);

    // ---------- Inspect the captured layer ----------
    Scene::Layer* layer = sys.ActiveLayer();
    CHECK(layer != nullptr, "active layer present");
    if (layer) {
        const Scene::EntityDelta* da = layer->FindEntity(live.GetComponent<ECS::StableIdComponent>(a)->id);
        CHECK(da && da->components.size() == 1, "A captured once (upsert, not duplicate)");

        const Scene::EntityDelta* dc = layer->FindEntity(live.GetComponent<ECS::StableIdComponent>(c)->id);
        CHECK(dc && dc->destroyed, "C captured as tombstone");

        u64 dId = live.GetComponent<ECS::StableIdComponent>(d)->id;
        CHECK(dId != 0, "created entity D got a stable id on capture");
        const Scene::EntityDelta* dd = layer->FindEntity(dId);
        CHECK(dd && dd->created && dd->components.size() >= 2, "D captured as created with its components");
    }

    // ---------- Resolve base + captured stack into a FRESH world ----------
    std::printf("Replaying captured layer into a fresh world...\n");
    ECS::World replay;
    auto rr = sys.Stack().ResolveInto(replay, baseJson);
    CHECK(rr.success, "captured stack resolved");

    auto ra = FindByName(replay, "A");
    CHECK(ra != ECS::INVALID_ENTITY, "A present in replay");
    if (ra != ECS::INVALID_ENTITY) {
        auto* t = replay.GetComponent<ECS::TransformComponent>(ra);
        CHECK(t && Vec3Near(t->position, 7, 7, 7), "A reflects latest captured move (7,7,7)");
    }
    auto rb = FindByName(replay, "B");
    CHECK(rb != ECS::INVALID_ENTITY && replay.HasComponent<ECS::LightComponent>(rb), "B has captured light");
    CHECK(FindByName(replay, "C") == ECS::INVALID_ENTITY, "C tombstoned in replay");
    auto rd = FindByName(replay, "Created");
    CHECK(rd != ECS::INVALID_ENTITY, "D created in replay");
    if (rd != ECS::INVALID_ENTITY) {
        auto* t = replay.GetComponent<ECS::TransformComponent>(rd);
        CHECK(t && Vec3Near(t->position, 5, 5, 5), "D transform captured");
    }

    // ---------- A captured layer survives its own JSON file ----------
    std::string layerFile = layer->ToJson();
    Scene::Layer restored = Scene::Layer::FromJson(layerFile);
    Scene::LayerStack stack2;
    stack2.layers.push_back(restored);
    ECS::World replay2;
    stack2.ResolveInto(replay2, baseJson);
    auto r2a = FindByName(replay2, "A");
    auto* t2 = r2a != ECS::INVALID_ENTITY ? replay2.GetComponent<ECS::TransformComponent>(r2a) : nullptr;
    CHECK(t2 && Vec3Near(t2->position, 7, 7, 7), "capture survives per-layer JSON file");
    CHECK(FindByName(replay2, "C") == ECS::INVALID_ENTITY, "tombstone survives per-layer JSON file");

    // ---------- RecordEntityChanges: the editor's snapshot/diff capture ----------
    // This is what the inspector does per frame: snapshot the entity on entry,
    // diff at the end. Only actually-changed components may land in the layer.
    std::printf("Testing RecordEntityChanges (snapshot/diff capture)...\n");
    {
        // Arrange: fresh layer on top; snapshot B (has transform + light from earlier).
        int diffIdx = sys.AddLayer("diff-session");
        CHECK(sys.ActiveLayerIndex() == diffIdx, "diff layer active");
        Scene::Layer* diffLayer = sys.ActiveLayer();
        u64 bSid = live.GetComponent<ECS::StableIdComponent>(b)->id;

        std::string snap = Scene::SceneSerializer::SerializeEntityToString(&live, b, /*includeVertexData=*/false);
        CHECK(!snap.empty(), "diff: snapshot taken");

        // Act 1: no edit at all -> Assert: nothing may land (per-frame no-op path).
        sys.RecordEntityChanges(b, snap);
        CHECK(diffLayer->FindEntity(bSid) == nullptr, "diff: unchanged entity records nothing");

        // Act 2: change ONE component (transform), leave light untouched.
        live.GetComponent<ECS::TransformComponent>(b)->position = Math::Vector3(4, 4, 4);
        sys.RecordEntityChanges(b, snap);

        // Assert: exactly the transform delta, not the untouched light.
        const Scene::EntityDelta* db = diffLayer->FindEntity(bSid);
        CHECK(db != nullptr, "diff: changed entity recorded");
        if (db) {
            CHECK(db->components.size() == 1, "diff: only the changed component captured");
            CHECK(!db->components.empty() && db->components[0].key == "transform",
                  "diff: captured key is 'transform'");
        }

        // Act 3: remove a component that was in the snapshot.
        std::string snap2 = Scene::SceneSerializer::SerializeEntityToString(&live, b, /*includeVertexData=*/false);
        live.RemoveComponent<ECS::LightComponent>(b);
        sys.RecordEntityChanges(b, snap2);

        // Assert: removal recorded as an empty-json delta.
        db = diffLayer->FindEntity(bSid);
        bool sawLightRemoval = false;
        if (db) {
            for (const auto& cdelta : db->components) {
                if (cdelta.key == "light" && cdelta.json.empty()) sawLightRemoval = true;
            }
        }
        CHECK(sawLightRemoval, "diff: removed component captured as empty-json delta");

        // Assert end-to-end: resolving base + both layers reproduces the live state
        // (B at (4,4,4) with no light — the diff layer's removal wins over the
        // first layer's light-add).
        ECS::World replay3;
        auto rr3 = sys.Stack().ResolveInto(replay3, baseJson);
        CHECK(rr3.success, "diff: stack with diff layer resolves");
        auto r3b = FindByName(replay3, "B");
        CHECK(r3b != ECS::INVALID_ENTITY, "diff: B present in replay");
        if (r3b != ECS::INVALID_ENTITY) {
            auto* t = replay3.GetComponent<ECS::TransformComponent>(r3b);
            CHECK(t && Vec3Near(t->position, 4, 4, 4), "diff: replayed B reflects diffed move (4,4,4)");
            CHECK(!replay3.HasComponent<ECS::LightComponent>(r3b), "diff: replayed B lost the removed light");
        }
    }

    std::printf("\n=== Results: %d checks, %d failures ===\n", g_Checks, g_Failures);
    std::printf(g_Failures == 0 ? "ALL PASSED\n" : "SOME TESTS FAILED\n");
    return g_Failures > 0 ? 1 : 0;
}
