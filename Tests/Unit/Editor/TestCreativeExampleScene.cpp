// Builds an example scene out of exactly what the creative tools make.
//
// Every object here is created by calling the SAME functions Creative Mode's
// Plants and Prop tools call, and the same ones the Build Palette calls -- so
// the scene is not a mock-up of what the tools produce, it IS what they produce.
// If the tools change, this scene changes with them.
//
// Writing it to disk is opt-in: set ENJIN_EXAMPLE_SCENE_OUT to a path and the
// test saves there as well as asserting. Without it this is a plain test that
// the shared placement code builds a coherent scene.
#include "EnjinTest.h"
#include "Enjin/Editor/ScenePlacement.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/Renderer/MeshFactory.h"

#include <cstdlib>
#include <string>

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

ECS::Entity Named(ECS::World& w, const char* name, const Math::Vector3& pos) {
    ECS::Entity e = w.CreateEntity();
    auto& xf = w.AddComponent<ECS::TransformComponent>(e);
    xf.position = pos;
    auto& nc = w.AddComponent<ECS::NameComponent>(e);
    nc.name = name;
    return e;
}

// A patch made the way the Plants tool makes one: add the volume, then size it
// from the drag rectangle.
ECS::Entity Patch(ECS::World& w, PlantKind kind, const Math::Vector3& centre,
                  f32 halfX, f32 halfZ) {
    ECS::Entity e = Named(w, PlantKindName(kind), centre);
    AddPlantVolume(&w, e, kind);
    SizePlantVolume(&w, e, kind, halfX, halfZ, 1.0f);
    return e;
}

// A prop made the way the Prop tool makes one: an entity with a transform, then
// CreateProp from the ground point it was clicked on.
ECS::Entity Prop(ECS::World& w, PropKind kind, const Math::Vector3& ground) {
    ECS::Entity e = Named(w, PropKindName(kind), ground);
    CreateProp(&w, e, kind, ground);
    return e;
}

} // namespace

ENJIN_TEST(CreativeExample, TheToolsBuildACoherentScene) {
    ECS::World world;

    // --- the things you would set up first -----------------------------------
    ECS::Entity cam = Named(world, "Camera", Math::Vector3(0.0f, 9.0f, 26.0f));
    {
        auto& c = world.AddComponent<ECS::CameraComponent>(cam);
        c.fieldOfView = 60.0f;
        c.nearPlane = 0.1f;
        c.farPlane = 500.0f;
    }
    ECS::Entity sun = Named(world, "Sun", Math::Vector3(0.0f, 20.0f, 10.0f));
    {
        auto& l = world.AddComponent<ECS::LightComponent>(sun);
        l.type = ECS::LightType::Directional;
        l.intensity = 1.6f;
    }

    // Ground to stand the rest on. A floor is what the Floor tool makes; this
    // one is a plain mesh because the point here is the PLACEMENT code.
    {
        ECS::Entity ground = Named(world, "Ground", Math::Vector3(0.0f, -0.5f, 0.0f));
        world.AddComponent<ECS::MeshComponent>(
            ground, Renderer::MeshFactory::CreateCube(1.0f));
        world.AddComponent<ECS::MaterialComponent>(ground);
        auto* xf = world.GetComponent<ECS::TransformComponent>(ground);
        xf->scale = Math::Vector3(60.0f, 1.0f, 60.0f);
    }

    // --- what the Plants tool makes ------------------------------------------
    ECS::Entity grass  = Patch(world, PlantKind::Grass,  Math::Vector3(-9.0f, 0.0f, 0.0f), 6.0f, 6.0f);
    ECS::Entity shrubs = Patch(world, PlantKind::Shrubs, Math::Vector3(3.0f, 0.0f, -7.0f), 4.0f, 3.0f);
    ECS::Entity trees  = Patch(world, PlantKind::Trees,  Math::Vector3(12.0f, 0.0f, -4.0f), 7.0f, 5.0f);

    // --- what the Prop tool makes --------------------------------------------
    Prop(world, PropKind::Ball,       Math::Vector3(-2.0f, 0.0f, 6.0f));
    Prop(world, PropKind::Light,      Math::Vector3(0.0f, 0.0f, 4.0f));
    Prop(world, PropKind::PhysicsBox, Math::Vector3(2.0f, 0.0f, 6.0f));
    Prop(world, PropKind::Barrel,     Math::Vector3(4.0f, 0.0f, 5.0f));
    Prop(world, PropKind::SpawnPoint, Math::Vector3(-5.0f, 0.0f, 8.0f));
    Prop(world, PropKind::Block,      Math::Vector3(6.0f, 0.0f, 7.0f));

    // --- what the Water tool makes, on its swimmable default -----------------
    {
        ECS::Entity pool = Named(world, "Water (swimmable)", Math::Vector3(-14.0f, 0.0f, 10.0f));
        auto& wv = world.AddComponent<ECS::WaterVolumeComponent>(pool);
        wv.halfExtents = Math::Vector3(6.0f, 2.0f, 5.0f);
        wv.waterType = ECS::WaterType::Lake;
    }

    // --- assertions ----------------------------------------------------------
    //
    // The densities are the shared curve's, so this asserts the scene agrees
    // with PlantDensity rather than with a number typed here.
    ENJIN_EXPECT_EQ(world.GetComponent<ECS::GrassVolumeComponent>(grass)->density,
                    PlantDensity(PlantKind::Grass, 6.0f, 6.0f, 1.0f));
    ENJIN_EXPECT_EQ(world.GetComponent<ECS::ShrubVolumeComponent>(shrubs)->density,
                    PlantDensity(PlantKind::Shrubs, 4.0f, 3.0f, 1.0f));
    ENJIN_EXPECT_EQ(world.GetComponent<ECS::TreeVolumeComponent>(trees)->density,
                    PlantDensity(PlantKind::Trees, 7.0f, 5.0f, 1.0f));

    // Three kinds of plant, six props, one swimmable pool, plus camera, sun and
    // ground.
    ENJIN_EXPECT_EQ(world.GetEntitiesWithComponent<ECS::GrassVolumeComponent>().size(),
                    static_cast<usize>(1));
    ENJIN_EXPECT_EQ(world.GetEntitiesWithComponent<ECS::WaterVolumeComponent>().size(),
                    static_cast<usize>(1));
    // Two lights: the sun, and the one the Prop tool placed.
    ENJIN_EXPECT_EQ(world.GetEntitiesWithComponent<ECS::LightComponent>().size(),
                    static_cast<usize>(2));

    // --- optionally write it out ---------------------------------------------
    if (const char* out = std::getenv("ENJIN_EXAMPLE_SCENE_OUT")) {
        Scene::SceneSerializer ser(&world);
        const auto result = ser.Save(out);
        if (!result.success) {
            std::printf("    could not write %s: %s\n", out, result.error.c_str());
        } else {
            std::printf("    wrote %s\n", out);
        }
        ENJIN_EXPECT_TRUE(result.success);
    }
}

ENJIN_TEST_MAIN()
