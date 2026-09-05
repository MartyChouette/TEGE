// RT/PT probe-scene EMIT TOOL (registered as a test target, but it is a
// tool, not a correctness test — same pattern as TestAdr3SceneEmit).
//
// Builds a complete Enjin project whose content lights up every ray-tracing
// path: a reflective floor and metallic sphere row (RT reflections), an
// emissive block (GI / path tracer light transport), pillars near the ground
// (RT shadows + AO contact darkening), and mixed-roughness dielectrics
// (denoiser stress). No existing project enables RT, which is why the whole
// RT stack has never executed on real content.
//
// Gated: without ENJIN_RT_PROBE_DIR set, the test is a no-op pass. To emit:
//
//   $env:ENJIN_RT_PROBE_DIR = 'D:\TEGE_Projects\_RTProbe'
//   ctest -C Release -R TestRTSceneEmit
//
// Then drive the capability matrix:
//
//   pwsh -File tools/probes/rt_matrix.ps1          (stamps variants, captures, validates)

#include "EnjinTest.h"

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace Enjin;
namespace fs = std::filesystem;

ENJIN_TEST(RTSceneEmit, EmitsRayTracingProbeProject) {
    // Arrange: opt-in only — no env var means this run is a normal test pass.
    const char* outDir = std::getenv("ENJIN_RT_PROBE_DIR");
    if (!outDir || !*outDir) {
        return;
    }

    fs::path projDir = outDir;
    std::error_code ec;
    fs::create_directories(projDir / "scenes", ec);
    ENJIN_ASSERT_FALSE(static_cast<bool>(ec));

    // Act: build the probe world.
    ECS::World world;

    auto makeEntity = [&](const char* name, Math::Vector3 pos, Math::Vector3 scale,
                          Math::Vector3 baseColor, f32 metallic, f32 roughness) {
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(e, name);
        auto& t = world.AddComponent<ECS::TransformComponent>(e);
        t.position = pos;
        t.scale = scale;
        auto& mat = world.AddComponent<ECS::MaterialComponent>(e);
        mat.baseColor = baseColor;
        mat.metallic = metallic;
        mat.roughness = roughness;
        return e;
    };

    // Reflective floor — the mirror every RT reflection variant reads from
    {
        ECS::Entity floor = makeEntity("Floor", Math::Vector3(0.0f, -0.1f, 0.0f),
                                       Math::Vector3(24.0f, 0.2f, 24.0f),
                                       Math::Vector3(0.6f, 0.6f, 0.65f), 0.9f, 0.15f);
        world.AddComponent<ECS::MeshComponent>(floor, Renderer::MeshFactory::CreateCube(1.0f));
    }

    // Metallic sphere row across the roughness range (reflection sharpness sweep)
    for (int i = 0; i < 5; ++i) {
        f32 rough = static_cast<f32>(i) / 4.0f;
        ECS::Entity s = makeEntity("MetalSphere", Math::Vector3(-4.0f + i * 2.0f, 1.0f, 0.0f),
                                   Math::Vector3(1.0f, 1.0f, 1.0f),
                                   Math::Vector3(0.9f, 0.85f, 0.7f), 1.0f, rough);
        world.AddComponent<ECS::MeshComponent>(s, Renderer::MeshFactory::CreateSphere(0.8f, 32, 32));
    }

    // Two dielectric spheres (diffuse GI receivers, denoiser stress)
    for (int i = 0; i < 2; ++i) {
        ECS::Entity s = makeEntity("DielectricSphere", Math::Vector3(-2.0f + i * 4.0f, 1.0f, -3.0f),
                                   Math::Vector3(1.0f, 1.0f, 1.0f),
                                   i == 0 ? Math::Vector3(0.85f, 0.2f, 0.2f)
                                          : Math::Vector3(0.2f, 0.4f, 0.85f),
                                   0.0f, 0.6f);
        world.AddComponent<ECS::MeshComponent>(s, Renderer::MeshFactory::CreateSphere(0.8f, 32, 32));
    }

    // Emissive block — a real area light source for GI and the path tracer
    {
        ECS::Entity em = makeEntity("EmissiveBlock", Math::Vector3(0.0f, 2.5f, -5.0f),
                                    Math::Vector3(3.0f, 1.5f, 0.3f),
                                    Math::Vector3(1.0f, 0.85f, 0.6f), 0.0f, 1.0f);
        auto* mat = world.GetComponent<ECS::MaterialComponent>(em);
        mat->emissiveColor = Math::Vector3(1.0f, 0.85f, 0.6f);
        mat->emissiveStrength = 8.0f;
        world.AddComponent<ECS::MeshComponent>(em, Renderer::MeshFactory::CreateCube(1.0f));
    }

    // Pillars near the ground: RT shadow casters + AO contact corners
    for (int i = 0; i < 3; ++i) {
        ECS::Entity p = makeEntity("Pillar", Math::Vector3(3.5f, 1.25f, -1.5f + i * 2.0f),
                                   Math::Vector3(0.6f, 2.5f, 0.6f),
                                   Math::Vector3(0.75f, 0.72f, 0.68f), 0.0f, 0.8f);
        world.AddComponent<ECS::MeshComponent>(p, Renderer::MeshFactory::CreateCube(1.0f));
    }

    // Vegetation volumes (GPU-procedural instancing — exercises the RT
    // vegetation TLAS path) + a water plane (transmission material + the
    // FLAG_WATER_SURFACE raster/RT parity case)
    {
        ECS::Entity grass = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(grass, "GrassPatch");
        auto& t = world.AddComponent<ECS::TransformComponent>(grass);
        t.position = Math::Vector3(-6.5f, 0.0f, 2.0f);
        auto& g = world.AddComponent<ECS::GrassVolumeComponent>(grass);
        g.halfExtents = Math::Vector3(3.0f, 0.0f, 3.0f);
        g.density = 400;
        g.bladeHeight = 0.5f;

        ECS::Entity shrubs = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(shrubs, "ShrubPatch");
        auto& st = world.AddComponent<ECS::TransformComponent>(shrubs);
        st.position = Math::Vector3(6.5f, 0.0f, 2.0f);
        auto& sh = world.AddComponent<ECS::ShrubVolumeComponent>(shrubs);
        sh.halfExtents = Math::Vector3(2.5f, 0.0f, 2.5f);
        sh.density = 40;

        ECS::Entity trees = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(trees, "TreeGrove");
        auto& tt = world.AddComponent<ECS::TransformComponent>(trees);
        tt.position = Math::Vector3(-7.5f, 0.0f, -6.0f);
        auto& tv = world.AddComponent<ECS::TreeVolumeComponent>(trees);
        tv.halfExtents = Math::Vector3(3.0f, 0.0f, 3.0f);
        tv.density = 6;

        ECS::Entity water = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(water, "WaterPool");
        auto& wt = world.AddComponent<ECS::TransformComponent>(water);
        wt.position = Math::Vector3(7.5f, 0.05f, -5.0f);
        auto& w = world.AddComponent<ECS::Water3DComponent>(water);
        w.settings.position = Math::Vector3(7.5f, 0.05f, -5.0f);
        w.settings.width = 7.0f;
        w.settings.depth = 7.0f;
    }

    // Sun with shadows + a colored point light (multi-light RT paths)
    {
        ECS::Entity sun = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(sun, "Sun");
        auto& t = world.AddComponent<ECS::TransformComponent>(sun);
        t.position = Math::Vector3(0.0f, 10.0f, 0.0f);
        t.rotation = Math::Quaternion(Math::Vector3(1.0f, 0.0f, 0.0f), -0.9f);
        auto& lc = world.AddComponent<ECS::LightComponent>(sun);
        lc.type = ECS::LightType::Directional;
        lc.intensity = 2.0f;
        lc.castShadows = true;

        ECS::Entity point = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(point, "FillPoint");
        auto& pt = world.AddComponent<ECS::TransformComponent>(point);
        pt.position = Math::Vector3(-3.0f, 3.0f, 3.0f);
        auto& plc = world.AddComponent<ECS::LightComponent>(point);
        plc.type = ECS::LightType::Point;
        plc.color = Math::Vector3(0.4f, 0.6f, 1.0f);
        plc.intensity = 3.0f;
        plc.range = 12.0f;
    }

    // Game camera framing the whole arrangement
    {
        ECS::Entity cam = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(cam, "Camera");
        auto& t = world.AddComponent<ECS::TransformComponent>(cam);
        t.position = Math::Vector3(0.0f, 3.5f, 9.0f);
        t.rotation = Math::Quaternion(Math::Vector3(1.0f, 0.0f, 0.0f), -0.25f);
        auto& cc = world.AddComponent<ECS::CameraComponent>(cam);
        cc.isActive = true;
    }

    // Procedural sky: the path tracer samples the scene skybox for miss rays,
    // so the probe needs a real one (no skybox = correctly black sky in PT)
    Scene::SceneSerializer serializer(&world);
    {
        Renderer::SkyboxConfig sky;
        sky.type = Renderer::SkyboxType::Procedural;
        sky.topColor = Math::Vector3(0.25f, 0.45f, 0.85f);
        sky.horizonColor = Math::Vector3(0.7f, 0.8f, 0.95f);
        sky.bottomColor = Math::Vector3(0.45f, 0.42f, 0.4f);
        serializer.SetSkyboxConfig(sky);

        // Low ambient so ray-traced shadows/AO/GI actually read in captures —
        // the default 1.0 flat-fill ambient washes out exactly the shadowing the
        // hybrid effects produce.
        Renderer::SceneRenderSettings rs = serializer.GetRenderSettings();
        rs.ambientIntensity = 0.25f;
        rs.ambientColor = Math::Vector3(0.12f, 0.13f, 0.18f);
        serializer.SetRenderSettings(rs);
    }
    auto saveResult = serializer.Save((projDir / "scenes" / "main.enjin").string());
    ENJIN_ASSERT_TRUE(saveResult.success);

    {
        std::ofstream mf(projDir / "RTProbe.enjinproject");
        ENJIN_ASSERT_TRUE(mf.good());
        mf << "{\n"
              "  \"projectName\": \"RTProbe\",\n"
              "  \"scenes\": [\n"
              "    { \"name\": \"main\", \"path\": \"scenes/main.enjin\", "
              "\"isStartScene\": true, \"buildIndex\": 0 }\n"
              "  ]\n"
              "}\n";
    }

    // Assert: the emitted scene round-trips with the expected content mix.
    ECS::World verify;
    Scene::SceneSerializer verifySer(&verify);
    auto loadResult = verifySer.Load((projDir / "scenes" / "main.enjin").string());
    ENJIN_ASSERT_TRUE(loadResult.success);
    u32 meshes = 0, lights = 0, emissive = 0, vegetation = 0, water = 0;
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::MeshComponent>()) { (void)e; meshes++; }
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::LightComponent>()) { (void)e; lights++; }
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::MaterialComponent>()) {
        auto* m = verify.GetComponent<ECS::MaterialComponent>(e);
        if (m && m->emissiveStrength > 0.0f) emissive++;
    }
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::GrassVolumeComponent>()) { (void)e; vegetation++; }
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::ShrubVolumeComponent>()) { (void)e; vegetation++; }
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::TreeVolumeComponent>()) { (void)e; vegetation++; }
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::Water3DComponent>()) { (void)e; water++; }
    ENJIN_ASSERT_EQ(meshes, static_cast<u32>(12));  // floor + 5 metal + 2 dielectric + emissive + 3 pillars
    ENJIN_ASSERT_EQ(lights, static_cast<u32>(2));
    ENJIN_ASSERT_EQ(emissive, static_cast<u32>(1));
    ENJIN_ASSERT_EQ(vegetation, static_cast<u32>(3));
    ENJIN_ASSERT_EQ(water, static_cast<u32>(1));
}

ENJIN_TEST_MAIN()
