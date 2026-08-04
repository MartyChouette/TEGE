// adr-0003 probe-scene EMIT TOOL (registered as a test target, but it is a
// tool, not a correctness test).
//
// Builds a complete Enjin project containing TWO skinned, animated meshes —
// the minimum content that can trigger the adr-0003 hazard: RenderSystem's
// UpdateBoneDescriptor rewrites binding 7 on the live descriptor set whenever
// the bone buffer changes between draws, so two entities with distinct bone
// buffers alternating in one frame is the reproduction case. None of the
// existing projects contain any skinned content, which is why this generator
// exists.
//
// Gated: without ENJIN_ADR3_PROBE_DIR set, the test is a no-op pass, so normal
// ctest runs never touch the filesystem outside temp. To emit:
//
//   $env:ENJIN_ADR3_PROBE_DIR = 'D:\TEGE_Projects\_Adr3Probe'
//   ctest -C Release -R TestAdr3SceneEmit
//
// Then probe:
//
//   EnjinEditor.exe D:\TEGE_Projects\_Adr3Probe\Adr3Probe.enjinproject --play
//   (under validation via the _play_probe.ps1 pattern)

#include "EnjinTest.h"

#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/Scene/SceneSerializer.h"

#include "GoldenGLTFGenerator.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace Enjin;
namespace fs = std::filesystem;

ENJIN_TEST(Adr3SceneEmit, EmitsTwoSkinnedMeshProbeProject) {
    // Arrange: opt-in only — no env var means this run is a normal test pass.
    const char* outDir = std::getenv("ENJIN_ADR3_PROBE_DIR");
    if (!outDir || !*outDir) {
        return;
    }

    fs::path projDir = outDir;
    std::error_code ec;
    fs::create_directories(projDir / "scenes", ec);
    ENJIN_ASSERT_FALSE(static_cast<bool>(ec));

    std::string gltf = GoldenGLTF::WriteGoldenGLTF(
        fs::temp_directory_path() / "enjin_adr3_emit");
    ENJIN_ASSERT_FALSE(gltf.empty());

    // Act: import the same skinned+animated glTF twice — two entity trees,
    // each with its own Skeleton and (at render time) its own bone buffer.
    // SceneImporter auto-plays the animation; SceneSerializer persists and
    // resumes it on load.
    ECS::World world;
    Assets::ImportOptions opts;
    opts.generateColliders = false;
    Assets::ImportResult r1 = Assets::SceneImporter::ImportGLTF(gltf, &world, opts);
    ENJIN_ASSERT_TRUE(r1.success);
    Assets::ImportResult r2 = Assets::SceneImporter::ImportGLTF(gltf, &world, opts);
    ENJIN_ASSERT_TRUE(r2.success);

    // Offset the second instance's roots so both meshes are visible.
    for (ECS::Entity e : r2.entities) {
        if (ECS::GetParent(&world, e) != ECS::INVALID_ENTITY) continue;
        auto* t = world.GetComponent<ECS::TransformComponent>(e);
        if (t) t->position.x += 2.0f;
    }

    // Camera framing both triangles (they sit near the origin and at x=+2).
    ECS::Entity cam = world.CreateEntity();
    auto& camT = world.AddComponent<ECS::TransformComponent>(cam);
    camT.position = Math::Vector3(1.0f, 0.5f, 5.0f);
    world.AddComponent<ECS::CameraComponent>(cam);

    // A light so the meshes aren't pitch black.
    ECS::Entity light = world.CreateEntity();
    auto& lightT = world.AddComponent<ECS::TransformComponent>(light);
    lightT.position = Math::Vector3(1.0f, 3.0f, 3.0f);
    auto& lc = world.AddComponent<ECS::LightComponent>(light);
    lc.intensity = 2.0f;

    Scene::SceneSerializer serializer(&world);
    auto saveResult = serializer.Save((projDir / "scenes" / "main.enjin").string());
    ENJIN_ASSERT_TRUE(saveResult.success);

    // Minimal project manifest — SceneManager::NormalizeSceneList repairs any
    // missing invariants on load.
    {
        std::ofstream mf(projDir / "Adr3Probe.enjinproject");
        ENJIN_ASSERT_TRUE(mf.good());
        mf << "{\n"
              "  \"projectName\": \"Adr3Probe\",\n"
              "  \"scenes\": [\n"
              "    { \"name\": \"main\", \"path\": \"scenes/main.enjin\", "
              "\"isStartScene\": true, \"buildIndex\": 0 }\n"
              "  ]\n"
              "}\n";
    }

    // Assert: the emitted scene actually contains two animator components.
    ECS::World verify;
    Scene::SceneSerializer verifySer(&verify);
    auto loadResult = verifySer.Load((projDir / "scenes" / "main.enjin").string());
    ENJIN_ASSERT_TRUE(loadResult.success);
    u32 animatorCount = 0;
    for (ECS::Entity e : verify.GetEntitiesWithComponent<ECS::AnimatorComponent>()) {
        (void)e;
        animatorCount++;
    }
    ENJIN_ASSERT_EQ(animatorCount, static_cast<u32>(2));
}

ENJIN_TEST_MAIN()
