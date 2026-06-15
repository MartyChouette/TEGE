// Override-layer resolve test.
//
// Proves the core VWS primitive: a base scene plus one non-destructive layer
// resolves to a world where the layer's edits are applied, and disabling the
// layer yields the pristine base. Exercises all four delta kinds in one pass:
//   - override an existing component   (move entity A)
//   - add a component                  (light on entity B)
//   - tombstone an entity              (hide entity C)
//   - create an entity                 (spawn entity D)
// Plus a Layer JSON round-trip (the per-layer-file form).

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/LayerStack.h"

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

// Stable ids we control so deltas can address entities deterministically.
static constexpr u64 ID_A = 1001, ID_B = 1002, ID_C = 1003, ID_D = 2001;

int main() {
    std::printf("=== Layer Resolve Test ===\n\n");

    // ---------- Build the base scene: A, B, C ----------
    ECS::World src;
    auto a = src.CreateEntity();
    src.AddComponent<ECS::NameComponent>(a, ECS::NameComponent{"A"});
    { auto& t = src.AddComponent<ECS::TransformComponent>(a); t.position = Math::Vector3(1, 2, 3); }
    src.AddComponent<ECS::StableIdComponent>(a, ECS::StableIdComponent{ID_A});

    auto b = src.CreateEntity();
    src.AddComponent<ECS::NameComponent>(b, ECS::NameComponent{"B"});
    src.AddComponent<ECS::TransformComponent>(b);
    src.AddComponent<ECS::StableIdComponent>(b, ECS::StableIdComponent{ID_B});

    auto c = src.CreateEntity();
    src.AddComponent<ECS::NameComponent>(c, ECS::NameComponent{"C"});
    src.AddComponent<ECS::TransformComponent>(c);
    src.AddComponent<ECS::StableIdComponent>(c, ECS::StableIdComponent{ID_C});

    Scene::SceneSerializer saver(&src);
    std::string baseJson = saver.SaveToString();
    CHECK(!baseJson.empty(), "base scene serialized");

    // ---------- Build deltas by serializing the desired component states ----------
    // Override A.transform -> (9,9,9): mutate the live component then serialize it.
    src.GetComponent<ECS::TransformComponent>(a)->position = Math::Vector3(9, 9, 9);
    std::string aTransform = Scene::SceneSerializer::SerializeOneComponent(&src, a, "transform");

    // Add a light to B: attach then serialize.
    src.AddComponent<ECS::LightComponent>(b);
    std::string bLight = Scene::SceneSerializer::SerializeOneComponent(&src, b, "light");

    // Component states for the created entity D (built on a scratch entity).
    auto scratch = src.CreateEntity();
    src.AddComponent<ECS::NameComponent>(scratch, ECS::NameComponent{"Created"});
    { auto& t = src.AddComponent<ECS::TransformComponent>(scratch); t.position = Math::Vector3(5, 5, 5); }
    std::string dName = Scene::SceneSerializer::SerializeOneComponent(&src, scratch, "name");
    std::string dTransform = Scene::SceneSerializer::SerializeOneComponent(&src, scratch, "transform");

    CHECK(!aTransform.empty() && !bLight.empty() && !dName.empty(), "deltas serialized");

    // ---------- Assemble one layer ----------
    Scene::LayerStack stack;
    Scene::Layer layer;
    layer.name = "edits";

    { Scene::EntityDelta& d = layer.EntityFor(ID_A); d.components.push_back({"transform", aTransform}); }
    { Scene::EntityDelta& d = layer.EntityFor(ID_B); d.components.push_back({"light", bLight}); }
    { Scene::EntityDelta& d = layer.EntityFor(ID_C); d.destroyed = true; }
    { Scene::EntityDelta& d = layer.EntityFor(ID_D); d.created = true;
      d.components.push_back({"name", dName});
      d.components.push_back({"transform", dTransform}); }

    stack.layers.push_back(layer);

    // ---------- Resolve with the layer ENABLED ----------
    std::printf("Resolving with layer enabled...\n");
    {
        ECS::World world;
        auto r = stack.ResolveInto(world, baseJson);
        CHECK(r.success, "resolve(enabled) loaded");

        auto ra = FindByName(world, "A");
        CHECK(ra != ECS::INVALID_ENTITY, "A present");
        if (ra != ECS::INVALID_ENTITY) {
            auto* t = world.GetComponent<ECS::TransformComponent>(ra);
            CHECK(t && Vec3Near(t->position, 9, 9, 9), "A.transform overridden to (9,9,9)");
        }

        auto rb = FindByName(world, "B");
        CHECK(rb != ECS::INVALID_ENTITY, "B present");
        CHECK(rb != ECS::INVALID_ENTITY && world.HasComponent<ECS::LightComponent>(rb), "B gained a light");

        CHECK(FindByName(world, "C") == ECS::INVALID_ENTITY, "C tombstoned (absent)");

        auto rd = FindByName(world, "Created");
        CHECK(rd != ECS::INVALID_ENTITY, "D created (present)");
        if (rd != ECS::INVALID_ENTITY) {
            auto* t = world.GetComponent<ECS::TransformComponent>(rd);
            CHECK(t && Vec3Near(t->position, 5, 5, 5), "D.transform from delta");
            auto* sid = world.GetComponent<ECS::StableIdComponent>(rd);
            CHECK(sid && sid->id == ID_D, "D carries its stable id");
        }
    }

    // ---------- Resolve with the layer DISABLED -> pristine base ----------
    std::printf("Resolving with layer disabled...\n");
    {
        stack.layers[0].enabled = false;
        ECS::World world;
        auto r = stack.ResolveInto(world, baseJson);
        CHECK(r.success, "resolve(disabled) loaded");

        auto ra = FindByName(world, "A");
        CHECK(ra != ECS::INVALID_ENTITY, "A present (base)");
        if (ra != ECS::INVALID_ENTITY) {
            auto* t = world.GetComponent<ECS::TransformComponent>(ra);
            CHECK(t && Vec3Near(t->position, 1, 2, 3), "A.transform pristine (1,2,3)");
        }
        auto rb = FindByName(world, "B");
        CHECK(rb != ECS::INVALID_ENTITY && !world.HasComponent<ECS::LightComponent>(rb), "B has no light (base)");
        CHECK(FindByName(world, "C") != ECS::INVALID_ENTITY, "C present (base)");
        CHECK(FindByName(world, "Created") == ECS::INVALID_ENTITY, "D absent (base)");

        stack.layers[0].enabled = true;
    }

    // ---------- Layer JSON round-trip (per-layer file form) ----------
    std::printf("Round-tripping layer through JSON...\n");
    {
        std::string layerJson = stack.layers[0].ToJson();
        Scene::Layer back = Scene::Layer::FromJson(layerJson);
        CHECK(back.name == "edits", "layer name survives JSON");
        CHECK(back.entities.size() == 4, "layer has 4 entity deltas after JSON");

        const Scene::EntityDelta* cd = back.FindEntity(ID_C);
        CHECK(cd && cd->destroyed, "C delta still tombstone after JSON");
        const Scene::EntityDelta* dd = back.FindEntity(ID_D);
        CHECK(dd && dd->created && dd->components.size() == 2, "D delta still created w/ 2 comps");

        // Re-resolve from the JSON-restored layer; A should still move.
        Scene::LayerStack stack2;
        stack2.layers.push_back(back);
        ECS::World world;
        stack2.ResolveInto(world, baseJson);
        auto ra = FindByName(world, "A");
        auto* t = ra != ECS::INVALID_ENTITY ? world.GetComponent<ECS::TransformComponent>(ra) : nullptr;
        CHECK(t && Vec3Near(t->position, 9, 9, 9), "JSON-restored layer still overrides A");
    }

    std::printf("\n=== Results: %d checks, %d failures ===\n", g_Checks, g_Failures);
    std::printf(g_Failures == 0 ? "ALL PASSED\n" : "SOME TESTS FAILED\n");
    return g_Failures > 0 ? 1 : 0;
}
