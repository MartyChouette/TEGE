// The dominant data-loss shape in this engine: a block of authored values written
// only when some OTHER field is on, and read back unconditionally.
//
// It always arrives with a reasonable-sounding comment -- "only when enabled so
// old scenes stay clean", "so a scene that never used one does not grow a block
// of parameters nobody set". The saving is a few lines of JSON. The cost is that
// the gate is itself a value someone toggles: set a sky, switch the type to None
// to check your lighting, save, and all 22 fields are gone. Turn 2D water off to
// see the level underneath: 13 gone. Untick "has collision" to test something:
// your hand-painted per-tile mask is gone, and re-ticking the box does not bring
// it back.
//
// The distinction that matters, and the one that made the MaterialComponent fix
// two lines instead of a 25x file-size regression:
//
//   dropping a field for equalling ITS OWN default   -> lossless, it reads back
//                                                       as that same default
//   gating a block on a DIFFERENT field              -> data loss
//
// These tests author a value, flip the gate, round-trip, and require the value to
// survive. Each one fails against the code as it was.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/Skybox.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include "Enjin/ECS/Components/Material.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {
bool Near(f32 a, f32 b, f32 eps = 0.001f) { return std::fabs(a - b) < eps; }
} // namespace

ENJIN_TEST(GatedSerialization, SkyboxSurvivesBeingSwitchedToNone) {
    // Arrange: a fully authored sky.
    World w;
    Scene::SceneSerializer out(&w);
    Renderer::SkyboxConfig sky;
    sky.type = Renderer::SkyboxType::Procedural;
    sky.topColor    = Vector3(0.12f, 0.31f, 0.74f);
    sky.bottomColor = Vector3(0.91f, 0.62f, 0.38f);
    sky.horizonColor = Vector3(0.77f, 0.55f, 0.41f);
    sky.cloudCoverage = 0.63f;
    sky.cloudScale = 3.7f;
    out.SetSkyboxConfig(sky);

    // Act: the author switches the sky off to look at their lighting, and saves.
    sky.type = Renderer::SkyboxType::None;
    out.SetSkyboxConfig(sky);
    const std::string json = out.SaveToString();

    World reloaded;
    Scene::SceneSerializer in(&reloaded);
    ENJIN_ASSERT_TRUE(in.LoadFromString(json).success);

    // Assert: switching it off is not the same as throwing it away.
    const auto& back = in.GetSkyboxConfig();
    ENJIN_EXPECT_TRUE(Near(back.topColor.x, 0.12f));
    ENJIN_EXPECT_TRUE(Near(back.topColor.z, 0.74f));
    ENJIN_EXPECT_TRUE(Near(back.bottomColor.y, 0.62f));
    ENJIN_EXPECT_TRUE(Near(back.horizonColor.x, 0.77f));
    ENJIN_EXPECT_TRUE(Near(back.cloudCoverage, 0.63f));
    ENJIN_EXPECT_TRUE(Near(back.cloudScale, 3.7f));
}

ENJIN_TEST(GatedSerialization, Water2DSurvivesBeingDisabled) {
    World w;
    Scene::SceneSerializer out(&w);
    Renderer::Water2DConfig water;
    water.enabled = true;
    water.waterLineY = -4.25f;
    water.surfaceColor = Vector3(0.21f, 0.63f, 0.72f);
    water.deepColor = Vector3(0.03f, 0.11f, 0.27f);
    water.opacity = 0.72f;
    out.SetWater2DConfig(water);

    water.enabled = false;   // "let me see the level underneath"
    out.SetWater2DConfig(water);
    const std::string json = out.SaveToString();

    World reloaded;
    Scene::SceneSerializer in(&reloaded);
    ENJIN_ASSERT_TRUE(in.LoadFromString(json).success);

    const auto& back = in.GetWater2DConfig();
    ENJIN_EXPECT_FALSE(back.enabled);                       // the toggle round-trips
    ENJIN_EXPECT_TRUE(Near(back.waterLineY, -4.25f));       // and so does the work
    ENJIN_EXPECT_TRUE(Near(back.surfaceColor.y, 0.63f));
    ENJIN_EXPECT_TRUE(Near(back.deepColor.z, 0.27f));
    ENJIN_EXPECT_TRUE(Near(back.opacity, 0.72f));
}

ENJIN_TEST(GatedSerialization, ACookieTunedThenSwitchedOffComesBack) {
    // Sixteen controls behind one checkbox.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Spot"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    LightComponent light;
    light.type = LightType::Spot;
    light.cookieEnabled = true;
    light.cookie.columns = 7.0f;
    light.cookie.rows = 3.0f;
    light.cookie.barWidth = 0.42f;
    light.cookie.resolution = 512;
    w.AddComponent<LightComponent>(e, light);

    // Turn the gobo off to compare, then save.
    w.GetComponent<LightComponent>(e)->cookieEnabled = false;

    const std::string entityJson =
        Scene::SceneSerializer::SerializeEntityToString(&w, e, false);
    World dst;
    Entity loaded = Scene::SceneSerializer::DeserializeEntityFromString(&dst, entityJson);
    ENJIN_ASSERT_TRUE(dst.IsValid(loaded));

    const auto* back = dst.GetComponent<LightComponent>(loaded);
    ENJIN_ASSERT_TRUE(back != nullptr);
    ENJIN_EXPECT_FALSE(back->cookieEnabled);
    ENJIN_EXPECT_TRUE(Near(back->cookie.columns, 7.0f));
    ENJIN_EXPECT_TRUE(Near(back->cookie.rows, 3.0f));
    ENJIN_EXPECT_TRUE(Near(back->cookie.barWidth, 0.42f));
    ENJIN_EXPECT_EQ(back->cookie.resolution, static_cast<u32>(512));
}

ENJIN_TEST(GatedSerialization, APaintedCollisionMaskSurvivesUntickingHasCollision) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Level"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    TilemapComponent tm;
    tm.width = 4;
    tm.height = 2;
    tm.tiles.assign(8, 1);
    tm.hasCollision = true;
    tm.collisionMask = { 1, 0, 1, 1, 0, 0, 1, 0 };   // painted by hand
    w.AddComponent<TilemapComponent>(e, tm);

    w.GetComponent<TilemapComponent>(e)->hasCollision = false;   // "just testing"

    const std::string entityJson =
        Scene::SceneSerializer::SerializeEntityToString(&w, e, true);
    World dst;
    Entity loaded = Scene::SceneSerializer::DeserializeEntityFromString(&dst, entityJson);
    ENJIN_ASSERT_TRUE(dst.IsValid(loaded));

    const auto* back = dst.GetComponent<TilemapComponent>(loaded);
    ENJIN_ASSERT_TRUE(back != nullptr);
    ENJIN_ASSERT_EQ(back->collisionMask.size(), static_cast<usize>(8));
    ENJIN_EXPECT_EQ(static_cast<i32>(back->collisionMask[0]), 1);
    ENJIN_EXPECT_EQ(static_cast<i32>(back->collisionMask[3]), 1);
    ENJIN_EXPECT_EQ(static_cast<i32>(back->collisionMask[6]), 1);
}

ENJIN_TEST(GatedSerialization, BakedLightmapPathsSurviveAnABToggle) {
    // These three paths point at the output of a bake that takes minutes.
    Renderer::SceneRenderSettings s;
    s.lightmapEnabled = true;
    s.lightmapStrength = 0.85f;
    s.lightmapPath[0] = "baked/lm_basis0.png";
    s.lightmapPath[1] = "baked/lm_basis1.png";
    s.lightmapPath[2] = "baked/lm_basis2.png";

    s.lightmapEnabled = false;   // A/B against unlit
    const auto j = Renderer::SerializeRenderSettings(s);
    Renderer::SceneRenderSettings back = Renderer::DeserializeRenderSettings(j);

    ENJIN_EXPECT_FALSE(back.lightmapEnabled);
    ENJIN_EXPECT_EQ(back.lightmapPath[0], std::string("baked/lm_basis0.png"));
    ENJIN_EXPECT_EQ(back.lightmapPath[2], std::string("baked/lm_basis2.png"));
    ENJIN_EXPECT_TRUE(Near(back.lightmapStrength, 0.85f));
}

ENJIN_TEST(GatedSerialization, AnOldSceneWithNoLightmapBlockStillLoadsEnabled) {
    // Back-compat: before the block became unconditional, its PRESENCE was the
    // enabled flag. A file written then must not come back with lightmaps off.
    Renderer::SceneRenderSettings s;
    s.lightmapEnabled = true;
    s.lightmapPath[0] = "a.png";
    s.lightmapPath[1] = "b.png";
    s.lightmapPath[2] = "c.png";
    auto j = Renderer::SerializeRenderSettings(s);
    j["lightmap"].erase("enabled");        // as an older engine wrote it

    Renderer::SceneRenderSettings back = Renderer::DeserializeRenderSettings(j);
    ENJIN_EXPECT_TRUE(back.lightmapEnabled);
}

ENJIN_TEST(GatedSerialization, DroppingAFieldForEqualsDefaultIsStillFine) {
    // The other half of the rule, so nobody "fixes" this by writing everything.
    // MaterialComponent omits a field that equals its own default and reads it
    // back as that same default, which is lossless -- and is what keeps a scene
    // file from growing 25x.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Plain"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    MaterialComponent mat;
    mat.baseColor = Vector3(0.4f, 0.7f, 0.2f);   // authored
    w.AddComponent<MaterialComponent>(e, mat);

    const std::string json = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);
    ENJIN_EXPECT_TRUE(json.find("metallic") == std::string::npos);   // omitted, equals default

    World dst;
    Entity loaded = Scene::SceneSerializer::DeserializeEntityFromString(&dst, json);
    ENJIN_ASSERT_TRUE(dst.IsValid(loaded));
    const auto* back = dst.GetComponent<MaterialComponent>(loaded);
    ENJIN_EXPECT_TRUE(Near(back->baseColor.y, 0.7f));               // authored survives
    ENJIN_EXPECT_TRUE(Near(back->metallic, MaterialComponent{}.metallic));  // omitted reads default
}

ENJIN_TEST_MAIN()
