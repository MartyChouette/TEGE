// Untrusted-input hardening regression tests (2026-06-16 sweep findings).
//
// These exercise the REAL engine parse paths — SceneSerializer::LoadFromString,
// Layer::FromJson, LayerStack::Resolve — with hostile documents, not
// re-implemented copies of the checks:
//   1. JSON nesting-depth guard (stack-overflow DoS via recursive parser)
//   2. Layer entity/component-delta count caps (OOM before serializer caps fire)
//   3. layerVersion forward-compat rejection

#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/LayerStack.h"

#include <nlohmann/json.hpp>
#include <string>

using namespace Enjin;

// The caps under test (mirror LayerStack.cpp — keep in sync).
static constexpr usize kEntityCap = 100'000;
static constexpr usize kComponentDeltaCap = 256;

static std::string NestedArrays(usize depth) {
    std::string s;
    s.reserve(depth * 2 + 1);
    s.append(depth, '[');
    s += '0';
    s.append(depth, ']');
    return s;
}

// ===========================================================================
// 1. JSON nesting-depth guard
// ===========================================================================

ENJIN_TEST(JsonDepthGuard, ParseSceneJson_AcceptsReasonableNesting) {
    // 32 levels is deeper than any legitimate scene document.
    bool threw = false;
    try {
        Scene::ParseSceneJson(NestedArrays(32));
    } catch (const std::exception&) {
        threw = true;
    }
    ENJIN_EXPECT_FALSE(threw);
}

ENJIN_TEST(JsonDepthGuard, ParseSceneJson_RejectsExtremeNesting) {
    // 100k levels would overflow the stack in a raw recursive parse; the
    // guard must throw (cleanly catchable) long before that.
    bool threw = false;
    try {
        Scene::ParseSceneJson(NestedArrays(100'000));
    } catch (const std::exception&) {
        threw = true;
    }
    ENJIN_EXPECT_TRUE(threw);
}

ENJIN_TEST(JsonDepthGuard, DeeplyNestedSceneRejectedWorldUntouched) {
    // Arrange: a world with one live entity, and a hostile scene document.
    ECS::World world;
    ECS::Entity survivor = world.CreateEntity();
    Scene::SceneSerializer serializer(&world);
    std::string hostile =
        "{\"formatVersion\":1,\"entities\":[],\"payload\":" + NestedArrays(100'000) + "}";

    // Act
    auto result = serializer.LoadFromString(hostile, /*clearExisting=*/true);

    // Assert: rejected as a parse error, and the world was NOT cleared
    // (LoadFromString only clears after a successful parse).
    ENJIN_EXPECT_FALSE(result.success);
    ENJIN_EXPECT_FALSE(result.error.empty());
    ENJIN_EXPECT_TRUE(world.IsValid(survivor));
}

ENJIN_TEST(JsonDepthGuard, ShallowSceneStillLoads) {
    ECS::World world;
    Scene::SceneSerializer serializer(&world);

    auto result = serializer.LoadFromString("{\"formatVersion\":1,\"entities\":[]}");

    ENJIN_EXPECT_TRUE(result.success);
}

ENJIN_TEST(JsonDepthGuard, DeeplyNestedLayerRejected) {
    std::string hostile =
        "{\"layerVersion\":1,\"name\":\"deep\",\"entities\":[{\"stableId\":1,"
        "\"components\":{\"transform\":" + NestedArrays(100'000) + "}}]}";

    Scene::Layer layer = Scene::Layer::FromJson(hostile);

    ENJIN_EXPECT_TRUE(layer.entities.empty());
}

// ===========================================================================
// 2. Layer count caps
// ===========================================================================

ENJIN_TEST(LayerCaps, EntityCountOverCapRejectsLayer) {
    // Arrange: cap + 1 minimal entity deltas.
    std::string j;
    j.reserve((kEntityCap + 1) * 18 + 64);
    j += "{\"layerVersion\":1,\"name\":\"evil\",\"entities\":[";
    for (usize i = 0; i <= kEntityCap; ++i) {
        if (i) j += ',';
        j += "{\"stableId\":" + std::to_string(i + 1) + "}";
    }
    j += "]}";

    Scene::Layer layer = Scene::Layer::FromJson(j);

    // Fail closed: the whole layer is rejected, not truncated.
    ENJIN_EXPECT_TRUE(layer.entities.empty());
}

ENJIN_TEST(LayerCaps, ComponentDeltaOverCapRejectsLayer) {
    std::string j = "{\"layerVersion\":1,\"name\":\"evil\",\"entities\":[{\"stableId\":1,\"components\":{";
    for (usize i = 0; i <= kComponentDeltaCap; ++i) {
        if (i) j += ',';
        j += "\"c" + std::to_string(i) + "\":{}";
    }
    j += "}}]}";

    Scene::Layer layer = Scene::Layer::FromJson(j);

    ENJIN_EXPECT_TRUE(layer.entities.empty());
}

ENJIN_TEST(LayerCaps, ResolveSkipsOversizedProgrammaticLayer) {
    // A layer built in memory (bypassing FromJson) must hit the same cap
    // inside Resolve itself.
    Scene::LayerStack stack;
    Scene::Layer big;
    big.name = "big";
    big.entities.reserve(kEntityCap + 1);
    for (usize i = 0; i <= kEntityCap; ++i) {
        Scene::EntityDelta d;
        d.stableId = i + 1;
        d.created = true;
        big.entities.push_back(std::move(d));
    }
    stack.layers.push_back(std::move(big));

    std::string resolved = stack.Resolve("{\"entities\":[]}");

    // The oversized layer is skipped: nothing was merged into the base.
    auto root = nlohmann::json::parse(resolved);
    ENJIN_EXPECT_EQ(root["entities"].size(), (usize)0);
}

ENJIN_TEST(LayerCaps, ResolveAppliesLayerWithinCaps) {
    // Control: a normal-sized layer still merges after the cap check.
    Scene::LayerStack stack;
    Scene::Layer small;
    small.name = "small";
    Scene::EntityDelta d;
    d.stableId = 42;
    d.created = true;
    small.entities.push_back(std::move(d));
    stack.layers.push_back(std::move(small));

    std::string resolved = stack.Resolve("{\"entities\":[]}");

    auto root = nlohmann::json::parse(resolved);
    ENJIN_EXPECT_EQ(root["entities"].size(), (usize)1);
    ENJIN_EXPECT_EQ(root["entities"][0].value("stableId", u64{0}), (u64)42);
}

ENJIN_TEST(LayerCaps, NormalLayerRoundTripsUnaffected) {
    // Guard against the caps breaking the ordinary save/load path.
    Scene::Layer layer;
    layer.name = "day-night";
    Scene::EntityDelta& d = layer.EntityFor(7);
    d.components.push_back(Scene::ComponentDelta{"transform", "{\"x\":1.0}"});

    Scene::Layer back = Scene::Layer::FromJson(layer.ToJson());

    ENJIN_EXPECT_STR_EQ(back.name, "day-night");
    ENJIN_ASSERT_TRUE(back.entities.size() == 1);
    ENJIN_EXPECT_EQ(back.entities[0].stableId, (u64)7);
    ENJIN_ASSERT_TRUE(back.entities[0].components.size() == 1);
    ENJIN_EXPECT_STR_EQ(back.entities[0].components[0].key, "transform");
}

// ===========================================================================
// 3. layerVersion forward-compat
// ===========================================================================

ENJIN_TEST(LayerVersion, FutureVersionRejected) {
    std::string j =
        "{\"layerVersion\":999,\"name\":\"future\",\"entities\":[{\"stableId\":1}]}";

    Scene::Layer layer = Scene::Layer::FromJson(j);

    // A file from a newer engine can't be interpreted reliably — fail closed.
    ENJIN_EXPECT_TRUE(layer.entities.empty());
}

ENJIN_TEST(LayerVersion, MissingVersionAcceptedAsCurrent) {
    // Hand-made files without the field keep working.
    std::string j = "{\"name\":\"handmade\",\"entities\":[{\"stableId\":5}]}";

    Scene::Layer layer = Scene::Layer::FromJson(j);

    ENJIN_EXPECT_STR_EQ(layer.name, "handmade");
    ENJIN_EXPECT_EQ(layer.entities.size(), (usize)1);
}

ENJIN_TEST_MAIN()
