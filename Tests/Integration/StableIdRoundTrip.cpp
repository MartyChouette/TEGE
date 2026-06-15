// StableId round-trip tests.
//
// The runtime Entity handle is remapped on every load, so it is NOT a durable
// address. StableIdComponent::id is. These tests pin the invariants the override
// -layer system depends on:
//   1. A stableId assigned in the editor survives a save/load unchanged, even
//      though the runtime Entity handle changes.
//   2. A legacy scene saved without a stableId gets one backfilled on load.
//   3. Distinct entities get distinct stableIds.

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <string>
#include <set>

using namespace Enjin;

static int g_Failures = 0;
static int g_Checks = 0;

#define CHECK(cond, label) do { \
    ++g_Checks; \
    if (!(cond)) { std::printf("  FAIL: %s\n", label); ++g_Failures; } \
} while(0)

// Find an entity by name in a world.
static ECS::Entity FindByName(ECS::World& w, const std::string& name) {
    for (auto e : w.GetAllEntities()) {
        auto* nc = w.GetComponent<ECS::NameComponent>(e);
        if (nc && nc->name == name) return e;
    }
    return ECS::INVALID_ENTITY;
}

static u64 StableIdOf(ECS::World& w, ECS::Entity e) {
    auto* sid = w.GetComponent<ECS::StableIdComponent>(e);
    return sid ? sid->id : 0;
}

// Test 1: a stableId set before save survives the round-trip even as the
// runtime Entity handle is remapped.
static void test_stableid_survives_roundtrip_with_remapped_handle() {
    // Arrange: an entity with an explicit stableId.
    ECS::World src;
    auto e = src.CreateEntity();
    src.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"Hero"});
    src.AddComponent<ECS::TransformComponent>(e);
    const u64 assigned = 0xABCDEF0123456789ull;
    src.AddComponent<ECS::StableIdComponent>(e, ECS::StableIdComponent{assigned});

    // Act: save, then load into a fresh world.
    Scene::SceneSerializer saver(&src);
    std::string json = saver.SaveToString();

    ECS::World dst;
    Scene::SceneSerializer loader(&dst);
    auto result = loader.LoadFromString(json, true);

    // Assert.
    CHECK(result.success, "test1: load succeeded");
    auto loaded = FindByName(dst, "Hero");
    CHECK(loaded != ECS::INVALID_ENTITY, "test1: entity found by name");
    CHECK(StableIdOf(dst, loaded) == assigned, "test1: stableId preserved across load");
    // The runtime handle is allowed to differ; the stableId is the stable part.
    // (We don't assert handle inequality — it's an implementation detail — only
    //  that the durable id matched regardless of what handle the load produced.)
}

// Test 2: a scene saved without any stableId field (legacy) gets a fresh,
// non-zero stableId backfilled on load.
static void test_legacy_scene_backfills_stableid() {
    // Arrange: hand-write minimal scene JSON with NO stableId on the entity.
    const char* legacy =
        "{\"formatVersion\":1,\"version\":\"1.0\",\"entityCount\":1,\"entities\":["
        "{\"id\":42,\"name\":{\"name\":\"Legacy\"}}"
        "]}";

    // Act.
    ECS::World dst;
    Scene::SceneSerializer loader(&dst);
    auto result = loader.LoadFromString(legacy, true);

    // Assert.
    CHECK(result.success, "test2: legacy load succeeded");
    auto e = FindByName(dst, "Legacy");
    CHECK(e != ECS::INVALID_ENTITY, "test2: legacy entity found");
    CHECK(dst.HasComponent<ECS::StableIdComponent>(e), "test2: stableId backfilled");
    CHECK(StableIdOf(dst, e) != 0, "test2: backfilled stableId is non-zero");
}

// Test 3: saving entities that lacked a stableId assigns distinct ones, and a
// re-save is stable (ids don't churn on every save).
static void test_save_assigns_distinct_and_stable_ids() {
    // Arrange: three entities, none with a stableId.
    ECS::World w;
    for (int i = 0; i < 3; ++i) {
        auto e = w.CreateEntity();
        w.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{"E" + std::to_string(i)});
    }

    // Act: first save assigns ids in-place.
    Scene::SceneSerializer saver(&w);
    std::string firstJson = saver.SaveToString();

    std::set<u64> ids;
    for (auto e : w.GetAllEntities()) {
        CHECK(w.HasComponent<ECS::StableIdComponent>(e), "test3: stableId assigned on save");
        ids.insert(StableIdOf(w, e));
    }

    // Assert: three distinct, non-zero ids.
    CHECK(ids.size() == 3, "test3: three distinct stableIds");
    CHECK(ids.count(0) == 0, "test3: no zero stableId");

    // Re-save must NOT churn ids (they already exist, so save leaves them).
    std::string secondJson = saver.SaveToString();
    std::set<u64> idsAfter;
    for (auto e : w.GetAllEntities()) idsAfter.insert(StableIdOf(w, e));
    CHECK(ids == idsAfter, "test3: stableIds stable across re-save");
}

int main() {
    std::printf("=== StableId Round-Trip Test ===\n\n");

    test_stableid_survives_roundtrip_with_remapped_handle();
    test_legacy_scene_backfills_stableid();
    test_save_assigns_distinct_and_stable_ids();

    std::printf("\n=== Results: %d checks, %d failures ===\n", g_Checks, g_Failures);
    std::printf(g_Failures == 0 ? "ALL PASSED\n" : "SOME TESTS FAILED\n");
    return g_Failures > 0 ? 1 : 0;
}
