// Driving a fluid volume from a recording instead of the solver.
//
// Playback hands the decoded frame to FluidSimulation rather than to a
// renderer, which is what makes every existing consumer work unchanged -- the
// Vulkan FluidRenderer, the web sprite path and BuildFluidSurface all read the
// grid, and none of them needs to know that nothing solved it.
#include "EnjinTest.h"
#include "Enjin/Effects/FluidPlaybackSystem.h"
#include "Enjin/Effects/FluidBake.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/ECS/Components/FluidPlayback.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

std::string TempDir() { return std::filesystem::temp_directory_path().string(); }

// Bake a short take to disk and return its file name, relative to TempDir().
std::string BakeToTemp(const char* name, bool looping = false) {
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.dimension = ECS::FluidDimension::Mode3D;
    v.fluidType = ECS::FluidType::Smoke;
    v.gridSize = 8;
    v.buoyancy = 1.0f;
    v.halfExtents = Math::Vector3(8.0f, 8.0f, 8.0f);
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);

    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 0.5f;
    s.settleTime = 0.5f;
    s.looping = looping;
    s.useSceneColliders = false;

    FluidBake bake;
    BakeFluid(&world, e, s, bake);
    bake.Save((std::filesystem::path(TempDir()) / name).string());
    return name;
}

ECS::Entity MakePlaybackVolume(ECS::World& world, const std::string& path) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.dimension = ECS::FluidDimension::Mode3D;
    v.gridSize = 8;
    v.halfExtents = Math::Vector3(8.0f, 8.0f, 8.0f);
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);
    ECS::FluidPlaybackComponent p;
    p.bakePath = path;
    world.AddComponent<ECS::FluidPlaybackComponent>(e, p);
    return e;
}

f32 TotalDensity(const FluidGridData* g) {
    if (!g) return 0.0f;
    f32 total = 0.0f;
    for (f32 d : g->density) total += d;
    return total;
}

} // namespace

ENJIN_TEST(FluidPlayback, test_a_recording_fills_the_volume_grid) {
    // Arrange
    const std::string name = BakeToTemp("enjin_test_playback_a.enjfluid");
    ECS::World world;
    ECS::Entity e = MakePlaybackVolume(world, name);

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());

    // Act
    playback.Update(1.0f / 30.0f, &world, sim);

    // Assert
    const FluidGridData* g = sim.GetGridData(e);
    ENJIN_ASSERT_TRUE(g != nullptr);
    ENJIN_EXPECT_EQ(g->N, static_cast<u32>(8));
    ENJIN_EXPECT_TRUE(TotalDensity(g) > 0.0f);

    std::remove((std::filesystem::path(TempDir()) / name).string().c_str());
}

ENJIN_TEST(FluidPlayback, test_a_played_back_volume_is_not_simulated) {
    // THE invariant. Solving a played-back frame would overwrite it with a
    // frame of real simulation before anything could draw it, so the recording
    // would never be seen at all.
    // Arrange
    const std::string name = BakeToTemp("enjin_test_playback_b.enjfluid");
    ECS::World world;
    ECS::Entity e = MakePlaybackVolume(world, name);

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());
    playback.Update(1.0f / 30.0f, &world, sim);

    const FluidGridData* g = sim.GetGridData(e);
    ENJIN_ASSERT_TRUE(g != nullptr);
    ENJIN_ASSERT_TRUE(g->playbackDriven);
    const std::vector<f32> afterPlayback = g->density;

    // Act: the solver runs and must leave it alone.
    sim.Update(1.0f / 30.0f, &world);

    // Assert
    const FluidGridData* after = sim.GetGridData(e);
    ENJIN_ASSERT_TRUE(after != nullptr);
    ENJIN_ASSERT_EQ(after->density.size(), afterPlayback.size());
    for (usize i = 0; i < afterPlayback.size(); ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(after->density[i], afterPlayback[i], 0.0001f);
    }

    std::remove((std::filesystem::path(TempDir()) / name).string().c_str());
}

ENJIN_TEST(FluidPlayback, test_playback_advances_with_time) {
    // Arrange
    const std::string name = BakeToTemp("enjin_test_playback_c.enjfluid");
    ECS::World world;
    ECS::Entity e = MakePlaybackVolume(world, name);

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());

    // Act
    playback.Update(0.0f, &world, sim);
    const std::vector<f32> first = sim.GetGridData(e)->density;
    for (int i = 0; i < 10; ++i) playback.Update(1.0f / 30.0f, &world, sim);
    const std::vector<f32> later = sim.GetGridData(e)->density;

    // Assert: a different frame, not the same one held.
    bool differs = false;
    for (usize i = 0; i < first.size() && !differs; ++i) {
        if (std::fabs(first[i] - later[i]) > 0.001f) differs = true;
    }
    ENJIN_EXPECT_TRUE(differs);

    std::remove((std::filesystem::path(TempDir()) / name).string().c_str());
}

ENJIN_TEST(FluidPlayback, test_paused_playback_holds_its_frame) {
    // Arrange
    const std::string name = BakeToTemp("enjin_test_playback_d.enjfluid");
    ECS::World world;
    ECS::Entity e = MakePlaybackVolume(world, name);
    world.GetComponent<ECS::FluidPlaybackComponent>(e)->playing = false;

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());

    // Act
    playback.Update(0.0f, &world, sim);
    const std::vector<f32> first = sim.GetGridData(e)->density;
    for (int i = 0; i < 20; ++i) playback.Update(1.0f / 30.0f, &world, sim);
    const std::vector<f32> later = sim.GetGridData(e)->density;

    // Assert
    for (usize i = 0; i < first.size(); ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(later[i], first[i], 0.0001f);
    }

    std::remove((std::filesystem::path(TempDir()) / name).string().c_str());
}

ENJIN_TEST(FluidPlayback, test_one_recording_is_shared_between_volumes) {
    // Twenty chimneys playing the same take must hold ONE copy of it. At
    // 48^3 that is the difference between 7 MB and 140 MB for a town.
    // Arrange
    const std::string name = BakeToTemp("enjin_test_playback_e.enjfluid");
    ECS::World world;
    for (int n = 0; n < 8; ++n) MakePlaybackVolume(world, name);

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());

    // Act
    playback.Update(1.0f / 30.0f, &world, sim);

    // Assert
    ENJIN_EXPECT_EQ(playback.CachedBakeCount(), static_cast<usize>(1));

    std::remove((std::filesystem::path(TempDir()) / name).string().c_str());
}

ENJIN_TEST(FluidPlayback, test_a_missing_recording_is_reported_once_not_every_frame) {
    // A remembered failure is what stops a missing file being re-opened from
    // disk sixty times a second and the log filling with one line.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakePlaybackVolume(world, "enjin_no_such_recording.enjfluid");

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());

    // Act
    for (int i = 0; i < 10; ++i) playback.Update(1.0f / 30.0f, &world, sim);

    // Assert: tried once, remembered, and no grid conjured from nothing.
    const auto* p = world.GetComponent<ECS::FluidPlaybackComponent>(e);
    ENJIN_ASSERT_TRUE(p != nullptr);
    ENJIN_EXPECT_TRUE(p->loadAttempted);
    ENJIN_EXPECT_TRUE(p->loadFailed);
    ENJIN_EXPECT_EQ(playback.CachedBakeCount(), static_cast<usize>(1));  // the failure
    ENJIN_EXPECT_NULL(sim.GetGridData(e));
}

ENJIN_TEST(FluidPlayback, test_a_path_escaping_the_asset_root_is_refused) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakePlaybackVolume(world, "../../../etc/passwd");

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());

    // Act
    playback.Update(1.0f / 30.0f, &world, sim);

    // Assert
    const auto* p = world.GetComponent<ECS::FluidPlaybackComponent>(e);
    ENJIN_ASSERT_TRUE(p != nullptr);
    ENJIN_EXPECT_TRUE(p->loadFailed);
    ENJIN_EXPECT_NULL(sim.GetGridData(e));
}

ENJIN_TEST(FluidPlayback, test_an_empty_path_is_not_an_error) {
    // A freshly added component has nothing to play. That is the prompt to go
    // and bake something, not a failure to log about.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakePlaybackVolume(world, "");

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());

    // Act
    playback.Update(1.0f / 30.0f, &world, sim);

    // Assert
    const auto* p = world.GetComponent<ECS::FluidPlaybackComponent>(e);
    ENJIN_ASSERT_TRUE(p != nullptr);
    ENJIN_EXPECT_FALSE(p->loadFailed);
    ENJIN_EXPECT_EQ(playback.CachedBakeCount(), static_cast<usize>(0));
}

ENJIN_TEST(FluidPlayback, test_clearing_the_cache_releases_recordings) {
    // A cache keyed by path would otherwise hold a previous level's takes
    // alive for the whole session.
    // Arrange
    const std::string name = BakeToTemp("enjin_test_playback_f.enjfluid");
    ECS::World world;
    MakePlaybackVolume(world, name);

    FluidSimulation sim;
    FluidPlaybackSystem playback;
    playback.SetAssetRoot(TempDir());
    playback.Update(1.0f / 30.0f, &world, sim);
    ENJIN_ASSERT_EQ(playback.CachedBakeCount(), static_cast<usize>(1));

    // Act
    playback.ClearCache();

    // Assert
    ENJIN_EXPECT_EQ(playback.CachedBakeCount(), static_cast<usize>(0));

    std::remove((std::filesystem::path(TempDir()) / name).string().c_str());
}

ENJIN_TEST_MAIN()
