// Recording a fluid simulation and playing it back.
//
// Solving in the frame costs about 56 ms for one 48^3 volume, which is 18 fps
// for a single campfire and cannot be optimised away -- a grid pays for its
// whole box whether or not there is smoke in it. Solving offline costs nothing
// at runtime and removes every reason to keep the resolution low.
#include "EnjinTest.h"
#include "Enjin/Effects/FluidBake.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/FluidVolume.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using namespace Enjin;
using namespace Enjin::Effects;

namespace {

std::string TempPath(const char* name) {
    return (std::filesystem::temp_directory_path() / name).string();
}

ECS::Entity MakeSmokeVolume(ECS::World& world, u32 gridSize) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
    ECS::FluidVolumeComponent v;
    v.dimension = ECS::FluidDimension::Mode3D;
    v.fluidType = ECS::FluidType::Smoke;
    v.gridSize = gridSize;
    v.buoyancy = 1.0f;
    v.halfExtents = Math::Vector3(8.0f, 8.0f, 8.0f);
    world.AddComponent<ECS::FluidVolumeComponent>(e, v);
    return e;
}

} // namespace

// --- the codec --------------------------------------------------------------

ENJIN_TEST(FluidBakeCodec, test_a_round_trip_preserves_density_within_quantisation) {
    // Arrange: values spread across the range, plus the empty cells that
    // dominate a real field.
    std::vector<f32> density(64, 0.0f);
    density[3] = 1.0f;
    density[4] = 0.5f;
    density[20] = 0.25f;
    density[63] = 0.75f;

    // Act
    std::vector<u8> encoded;
    EncodeFluidDensity(density, 1.0f, encoded);
    std::vector<f32> decoded;
    const bool ok = DecodeFluidDensity(encoded, 1.0f, density.size(), decoded);

    // Assert: 8 bits over the range, so a cell is within half a step.
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_ASSERT_EQ(decoded.size(), density.size());
    for (usize i = 0; i < density.size(); ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(decoded[i], density[i], 1.0f / 255.0f);
    }
}

ENJIN_TEST(FluidBakeCodec, test_an_empty_field_encodes_to_almost_nothing) {
    // The whole reason this is cheap: most of a smoke field is empty, and it
    // is empty in long stretches because the scan walks whole rows.
    // Arrange
    const std::vector<f32> density(125000, 0.0f);

    // Act
    std::vector<u8> encoded;
    EncodeFluidDensity(density, 1.0f, encoded);

    // Assert: two bytes per run of 255, not 125,000 bytes and not 500 KB.
    ENJIN_EXPECT_TRUE(encoded.size() < 1200);

    std::vector<f32> decoded;
    ENJIN_ASSERT_TRUE(DecodeFluidDensity(encoded, 1.0f, density.size(), decoded));
    for (f32 d : decoded) ENJIN_EXPECT_FLOAT_NEAR(d, 0.0f, 0.0001f);
}

ENJIN_TEST(FluidBakeCodec, test_quantisation_is_scaled_against_the_takes_peak) {
    // A take peaking at 40 must not clip to 1. The peak is stored with the
    // bake precisely so decoding returns the right VALUES, not merely the
    // right shape.
    // Arrange
    std::vector<f32> density(16, 0.0f);
    density[0] = 40.0f;
    density[1] = 20.0f;

    // Act
    std::vector<u8> encoded;
    EncodeFluidDensity(density, 40.0f, encoded);
    std::vector<f32> decoded;
    ENJIN_ASSERT_TRUE(DecodeFluidDensity(encoded, 40.0f, density.size(), decoded));

    // Assert
    ENJIN_EXPECT_FLOAT_NEAR(decoded[0], 40.0f, 40.0f / 255.0f);
    ENJIN_EXPECT_FLOAT_NEAR(decoded[1], 20.0f, 40.0f / 255.0f);
}

ENJIN_TEST(FluidBakeCodec, test_a_truncated_stream_is_refused_not_half_decoded) {
    // A short frame is smoke with a corrupt tail, which on screen reads as a
    // simulation bug and gets debugged in entirely the wrong place.
    // Arrange
    std::vector<f32> density(64, 0.0f);
    density[10] = 1.0f;
    std::vector<u8> encoded;
    EncodeFluidDensity(density, 1.0f, encoded);

    // Act: drop the last byte.
    ENJIN_ASSERT_TRUE(encoded.size() > 1);
    encoded.pop_back();
    std::vector<f32> decoded;
    const bool ok = DecodeFluidDensity(encoded, 1.0f, density.size(), decoded);

    // Assert
    ENJIN_EXPECT_FALSE(ok);
}

ENJIN_TEST(FluidBakeCodec, test_a_stream_claiming_more_cells_than_exist_is_refused) {
    // Arrange: a valid stream for 64 cells, decoded as though it were 32.
    std::vector<f32> density(64, 0.0f);
    density[40] = 1.0f;
    std::vector<u8> encoded;
    EncodeFluidDensity(density, 1.0f, encoded);

    // Act
    std::vector<f32> decoded;
    const bool ok = DecodeFluidDensity(encoded, 1.0f, 32, decoded);

    // Assert
    ENJIN_EXPECT_FALSE(ok);
}

// --- baking -----------------------------------------------------------------

ENJIN_TEST(FluidBakeRecording, test_a_bake_records_the_requested_number_of_frames) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 16);
    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 1.0f;
    s.settleTime = 0.2f;
    s.useSceneColliders = false;

    // Act
    FluidBake bake;
    const bool ok = BakeFluid(&world, e, s, bake);

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_EQ(bake.FrameCount(), static_cast<usize>(30));
    ENJIN_EXPECT_EQ(bake.gridSize, static_cast<u32>(16));
    ENJIN_EXPECT_FLOAT_NEAR(bake.Duration(), 1.0f, 0.01f);
    ENJIN_EXPECT_TRUE(bake.maxDensity > 0.0f);
}

ENJIN_TEST(FluidBakeRecording, test_the_settle_pass_is_discarded_not_recorded) {
    // A take that opens on an empty grid filling up is unusable for a loop and
    // ugly for a one-shot, so the settle frames are simulated and thrown away.
    // The proof is that the FIRST recorded frame already has smoke in it.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 16);
    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 0.2f;
    s.settleTime = 1.5f;
    s.useSceneColliders = false;

    // Act
    FluidBake bake;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, bake));

    std::vector<f32> first;
    ENJIN_ASSERT_TRUE(bake.SampleAt(0.0f, first));

    // Assert: a developed plume, not a nearly empty grid.
    f32 total = 0.0f;
    for (f32 d : first) total += d;
    ENJIN_EXPECT_TRUE(total > 1.0f);
}

ENJIN_TEST(FluidBakeRecording, test_progress_is_reported_from_zero_to_one) {
    // A 128^3 take is minutes of solving, and a tool that looks hung is a tool
    // people kill halfway through.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 8);
    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 0.3f;
    s.settleTime = 0.1f;
    s.useSceneColliders = false;

    f32 lowest = 2.0f, highest = -1.0f;
    int calls = 0;
    bool monotonic = true;
    f32 previous = -1.0f;

    // Act
    FluidBake bake;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, bake, [&](f32 p) {
        lowest = std::min(lowest, p);
        highest = std::max(highest, p);
        if (p < previous) monotonic = false;
        previous = p;
        ++calls;
    }));

    // Assert
    ENJIN_EXPECT_TRUE(calls > 0);
    ENJIN_EXPECT_TRUE(monotonic);
    ENJIN_EXPECT_TRUE(lowest >= 0.0f);
    ENJIN_EXPECT_FLOAT_NEAR(highest, 1.0f, 0.0001f);
}

ENJIN_TEST(FluidBakeRecording, test_obstacles_are_honoured_during_a_bake) {
    // A bake exists to run against real level geometry. If it ignored the
    // colliders it would record smoke going through walls, at high fidelity.
    // Arrange: a slab across the middle of the volume.
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 16);

    ECS::Entity wall = world.CreateEntity();
    ECS::TransformComponent wxf;
    wxf.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(wall, wxf);
    ECS::BoxColliderComponent bc;
    bc.size = Math::Vector3(40.0f, 3.0f, 40.0f);   // world space
    world.AddComponent<ECS::BoxColliderComponent>(wall, bc);

    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 0.5f;
    s.settleTime = 4.0f;          // long enough for a free plume to top out
    s.useSceneColliders = true;

    // Act
    FluidBake bake;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, bake));
    std::vector<f32> last;
    ENJIN_ASSERT_TRUE(bake.SampleAt(bake.Duration(), last));

    // Assert: density below the slab, effectively none above it.
    const u32 N = bake.gridSize;
    const usize stride = static_cast<usize>(N) + 2;
    f32 below = 0.0f, above = 0.0f;
    for (u32 k = 1; k <= N; ++k)
        for (u32 j = 1; j <= N; ++j)
            for (u32 i = 1; i <= N; ++i) {
                const f32 d = last[i + stride * (j + stride * k)];
                if (j <= 6) below += d;
                else if (j >= 11) above += d;
            }
    ENJIN_EXPECT_TRUE(below > 1.0f);
    ENJIN_EXPECT_TRUE(above < below * 0.05f);
}

// --- playback ---------------------------------------------------------------

ENJIN_TEST(FluidBakePlayback, test_a_one_shot_holds_its_last_frame) {
    // Better than vanishing: a cinematic whose water blinks out of existence
    // on the final frame is worse than one that simply stops moving.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 8);
    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 0.3f;
    s.settleTime = 0.5f;
    s.looping = false;
    s.useSceneColliders = false;

    FluidBake bake;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, bake));

    // Act
    std::vector<f32> atEnd, wellPast;
    ENJIN_ASSERT_TRUE(bake.SampleAt(bake.Duration() - 0.001f, atEnd));
    ENJIN_ASSERT_TRUE(bake.SampleAt(bake.Duration() * 10.0f, wellPast));

    // Assert
    ENJIN_ASSERT_EQ(atEnd.size(), wellPast.size());
    for (usize i = 0; i < atEnd.size(); ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(wellPast[i], atEnd[i], 0.0001f);
    }
}

ENJIN_TEST(FluidBakePlayback, test_a_loop_wraps_to_the_beginning) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 8);
    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 0.4f;
    s.settleTime = 0.5f;
    s.looping = true;
    s.loopBlendFrames = 0;        // a hard cut, so the wrap is exact
    s.useSceneColliders = false;

    FluidBake bake;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, bake));

    // Act: one full duration later is the same frame again.
    std::vector<f32> early, wrapped;
    ENJIN_ASSERT_TRUE(bake.SampleAt(0.05f, early));
    ENJIN_ASSERT_TRUE(bake.SampleAt(0.05f + bake.Duration(), wrapped));

    // Assert
    ENJIN_ASSERT_EQ(early.size(), wrapped.size());
    for (usize i = 0; i < early.size(); ++i) {
        ENJIN_EXPECT_FLOAT_NEAR(wrapped[i], early[i], 0.0001f);
    }
}

ENJIN_TEST(FluidBakePlayback, test_sampling_an_empty_bake_fails_rather_than_returning_nothing) {
    // Arrange
    const FluidBake bake;
    std::vector<f32> out;

    // Act / Assert
    ENJIN_EXPECT_FALSE(bake.SampleAt(0.0f, out));
}

// --- the file ---------------------------------------------------------------

ENJIN_TEST(FluidBakeFile, test_a_bake_survives_a_save_and_load) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 8);
    FluidBakeSettings s;
    s.frameRate = 24.0f;
    s.duration = 0.4f;
    s.settleTime = 0.3f;
    s.looping = true;
    s.loopBlendFrames = 2;
    s.useSceneColliders = false;

    FluidBake baked;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, baked));

    const std::string path = TempPath("enjin_test_fluid_bake.enjfluid");

    // Act
    ENJIN_ASSERT_TRUE(baked.Save(path));
    FluidBake loaded;
    const bool ok = loaded.Load(path);

    // Assert: every field, because one unread field is a setting that silently
    // reverts to its default on load.
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_EQ(loaded.gridSize, baked.gridSize);
    ENJIN_EXPECT_EQ(loaded.is3D, baked.is3D);
    ENJIN_EXPECT_EQ(loaded.looping, baked.looping);
    ENJIN_EXPECT_EQ(loaded.loopBlendFrames, baked.loopBlendFrames);
    ENJIN_EXPECT_FLOAT_NEAR(loaded.frameRate, baked.frameRate, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(loaded.halfExtents.x, baked.halfExtents.x, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(loaded.halfExtents.y, baked.halfExtents.y, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(loaded.halfExtents.z, baked.halfExtents.z, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(loaded.maxDensity, baked.maxDensity, 0.0001f);
    ENJIN_ASSERT_EQ(loaded.FrameCount(), baked.FrameCount());

    // And the pixels, not just the header.
    std::vector<f32> a, b;
    ENJIN_ASSERT_TRUE(baked.SampleAt(0.1f, a));
    ENJIN_ASSERT_TRUE(loaded.SampleAt(0.1f, b));
    ENJIN_ASSERT_EQ(a.size(), b.size());
    for (usize i = 0; i < a.size(); ++i) ENJIN_EXPECT_FLOAT_NEAR(b[i], a[i], 0.0001f);

    std::remove(path.c_str());
}

ENJIN_TEST(FluidBakeFile, test_loading_a_file_that_is_not_a_bake_fails_cleanly) {
    // Arrange
    const std::string path = TempPath("enjin_test_not_a_bake.enjfluid");
    {
        std::ofstream f(path, std::ios::binary);
        const char junk[] = "this is not a fluid bake, it is a text file";
        f.write(junk, sizeof(junk));
    }

    // Act
    FluidBake bake;
    const bool ok = bake.Load(path);

    // Assert
    ENJIN_EXPECT_FALSE(ok);
    std::remove(path.c_str());
}

ENJIN_TEST(FluidBakeFile, test_loading_a_missing_file_fails_cleanly) {
    FluidBake bake;
    ENJIN_EXPECT_FALSE(bake.Load(TempPath("enjin_no_such_bake_exists.enjfluid")));
}

ENJIN_TEST(FluidBakeHeader, test_reading_the_header_alone_reports_the_whole_take) {
    // Arrange: a take with every header field set to something other than its
    // default, so a field that is never read cannot pass by accident.
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 8);
    FluidBakeSettings s;
    s.frameRate = 24.0f;
    s.duration = 0.5f;
    s.settleTime = 0.2f;
    s.looping = true;
    s.loopBlendFrames = 3;
    s.useSceneColliders = false;

    FluidBake baked;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, baked));
    const std::string path = TempPath("enjin_test_fluid_header.enjfluid");
    ENJIN_ASSERT_TRUE(baked.Save(path));

    // Act: the header only -- this is what a browser tooltip and the picker
    // read, and reading the frames to answer it would be megabytes per row.
    FluidBakeInfo info;
    const bool ok = ReadFluidBakeInfo(path, info);

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_EQ(info.gridSize, baked.gridSize);
    ENJIN_EXPECT_EQ(info.is3D, baked.is3D);
    ENJIN_EXPECT_EQ(info.looping, baked.looping);
    ENJIN_EXPECT_EQ(info.loopBlendFrames, baked.loopBlendFrames);
    ENJIN_EXPECT_FLOAT_NEAR(info.frameRate, baked.frameRate, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(info.maxDensity, baked.maxDensity, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(info.halfExtents.y, baked.halfExtents.y, 0.0001f);
    ENJIN_EXPECT_EQ(static_cast<usize>(info.frameCount), baked.FrameCount());
    // The number the UI actually prints, derived rather than stored.
    ENJIN_EXPECT_FLOAT_NEAR(info.Duration(), baked.Duration(), 0.0001f);

    std::remove(path.c_str());
}

ENJIN_TEST(FluidBakeHeader, test_a_file_that_is_not_a_recording_is_refused) {
    // A picker that lists whatever carries the extension would show junk as a
    // take with a grid of whatever those bytes happened to spell.
    const std::string path = TempPath("enjin_test_header_junk.enjfluid");
    {
        std::ofstream f(path, std::ios::binary);
        const char junk[] = "ENJFLUI-almost, but not quite, the magic";
        f.write(junk, sizeof(junk));
    }

    FluidBakeInfo info;
    ENJIN_EXPECT_FALSE(ReadFluidBakeInfo(path, info));
    std::remove(path.c_str());
}

ENJIN_TEST(FluidBakeHeader, test_a_truncated_header_is_refused_not_half_read) {
    // Arrange: a real recording cut off inside its header. Half-reading it
    // would report a take with a plausible grid and no frames, which reads as
    // a failed bake rather than a damaged file.
    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 8);
    FluidBakeSettings s;
    s.duration = 0.2f;
    s.settleTime = 0.0f;
    s.useSceneColliders = false;
    FluidBake baked;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, baked));

    const std::string full = TempPath("enjin_test_header_full.enjfluid");
    ENJIN_ASSERT_TRUE(baked.Save(full));

    std::vector<char> bytes;
    {
        std::ifstream in(full, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    ENJIN_ASSERT_TRUE(bytes.size() > 20);

    const std::string cut = TempPath("enjin_test_header_cut.enjfluid");
    {
        std::ofstream out(cut, std::ios::binary);
        out.write(bytes.data(), 20);   // magic and a little else: mid-header
    }

    // Act + Assert
    FluidBakeInfo info;
    ENJIN_EXPECT_FALSE(ReadFluidBakeInfo(cut, info));

    std::remove(full.c_str());
    std::remove(cut.c_str());
}

ENJIN_TEST(FluidBakeHeader, test_a_missing_file_is_refused) {
    FluidBakeInfo info;
    ENJIN_EXPECT_FALSE(ReadFluidBakeInfo(TempPath("enjin_no_such_header.enjfluid"), info));
}

ENJIN_TEST(FluidTakeDiscovery, test_recordings_under_assets_are_found_with_project_relative_paths) {
    // Arrange: a project-shaped directory with takes at two depths, plus a
    // file that merely looks like one.
    namespace fs = std::filesystem;
    const fs::path proj = fs::temp_directory_path() / "enjin_test_fluid_project";
    fs::remove_all(proj);
    fs::create_directories(proj / "assets" / "fluid");
    fs::create_directories(proj / "assets" / "cinematics" / "act1");

    ECS::World world;
    ECS::Entity e = MakeSmokeVolume(world, 8);
    FluidBakeSettings s;
    s.duration = 0.2f;
    s.settleTime = 0.0f;
    s.useSceneColliders = false;
    FluidBake baked;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, e, s, baked));
    ENJIN_ASSERT_TRUE(baked.Save((proj / "assets" / "fluid" / "Chimney.enjfluid").string()));
    ENJIN_ASSERT_TRUE(baked.Save((proj / "assets" / "cinematics" / "act1" / "Flood.enjfluid").string()));
    { std::ofstream f(proj / "assets" / "fluid" / "notes.txt"); f << "not a take"; }

    // Act
    std::string searched;
    const std::vector<FluidTakeEntry> takes = FindFluidRecordings(proj.string(), &searched);

    // Assert: both, found at any depth, and NOT the .txt.
    ENJIN_ASSERT_EQ(takes.size(), static_cast<usize>(2));
    // Project-relative and forward-slashed, which is the only form a scene can
    // carry across platforms.
    ENJIN_EXPECT_EQ(takes[0].relativePath, std::string("assets/cinematics/act1/Flood.enjfluid"));
    ENJIN_EXPECT_EQ(takes[1].relativePath, std::string("assets/fluid/Chimney.enjfluid"));
    ENJIN_EXPECT_TRUE(takes[0].readable);
    ENJIN_EXPECT_EQ(takes[1].info.gridSize, baked.gridSize);
    ENJIN_EXPECT_TRUE(searched.find("assets") != std::string::npos);

    fs::remove_all(proj);
}

ENJIN_TEST(FluidTakeDiscovery, test_a_file_that_is_not_a_take_is_listed_as_unreadable_not_hidden) {
    // Listing it with its header unread is the honest answer: the file IS
    // there, carrying the extension, and a picker that silently omits it
    // leaves a person looking for a take they can see in their file manager.
    namespace fs = std::filesystem;
    const fs::path proj = fs::temp_directory_path() / "enjin_test_fluid_project_junk";
    fs::remove_all(proj);
    fs::create_directories(proj / "assets" / "fluid");
    { std::ofstream f(proj / "assets" / "fluid" / "Broken.enjfluid", std::ios::binary); f << "junk"; }

    const std::vector<FluidTakeEntry> takes = FindFluidRecordings(proj.string());

    ENJIN_ASSERT_EQ(takes.size(), static_cast<usize>(1));
    ENJIN_EXPECT_FALSE(takes[0].readable);

    fs::remove_all(proj);
}

ENJIN_TEST(FluidTakeDiscovery, test_an_empty_result_still_reports_where_it_looked) {
    // The whole point of the out-parameter: "none found" and "wrong project"
    // are the same sentence without it.
    namespace fs = std::filesystem;
    const fs::path proj = fs::temp_directory_path() / "enjin_test_fluid_project_empty";
    fs::remove_all(proj);
    fs::create_directories(proj / "assets");

    std::string searched;
    const std::vector<FluidTakeEntry> takes = FindFluidRecordings(proj.string(), &searched);

    ENJIN_EXPECT_TRUE(takes.empty());
    ENJIN_EXPECT_TRUE(searched.find("assets") != std::string::npos);

    // And a project with no assets/ at all still names the directory, rather
    // than coming back with an empty string that prints as nothing.
    fs::remove_all(proj);
    fs::create_directories(proj);
    std::string searchedNoAssets;
    ENJIN_EXPECT_TRUE(FindFluidRecordings(proj.string(), &searchedNoAssets).empty());
    ENJIN_EXPECT_FALSE(searchedNoAssets.empty());

    fs::remove_all(proj);
}

ENJIN_TEST(FluidBakeRecording, test_a_bake_solves_only_the_volume_it_is_recording) {
    // A bake records ONE volume, but FluidSimulation::Update iterates the
    // world -- so without a solo filter, baking one chimney in a ten-volume
    // level solves all ten for the whole take: ten times the wait for exactly
    // the same file.
    // Arrange: two volumes, only one of which is being recorded.
    ECS::World world;
    ECS::Entity target = MakeSmokeVolume(world, 16);
    ECS::Entity other = MakeSmokeVolume(world, 16);

    FluidBakeSettings s;
    s.frameRate = 30.0f;
    s.duration = 0.3f;
    s.settleTime = 0.3f;
    s.useSceneColliders = false;

    // Act
    FluidBake bake;
    ENJIN_ASSERT_TRUE(BakeFluid(&world, target, s, bake));

    // Assert: the recorded volume has frames with smoke in them...
    std::vector<f32> frame;
    ENJIN_ASSERT_TRUE(bake.SampleAt(0.0f, frame));
    f32 total = 0.0f;
    for (f32 d : frame) total += d;
    ENJIN_EXPECT_TRUE(total > 1.0f);

    // ...and the other volume was never touched, so its component still reads
    // as never having been initialised by this bake.
    const auto* otherVol = world.GetComponent<ECS::FluidVolumeComponent>(other);
    ENJIN_ASSERT_TRUE(otherVol != nullptr);
    ENJIN_EXPECT_FALSE(otherVol->simulationInitialized);

    const auto* targetVol = world.GetComponent<ECS::FluidVolumeComponent>(target);
    ENJIN_ASSERT_TRUE(targetVol != nullptr);
    ENJIN_EXPECT_TRUE(targetVol->simulationInitialized);
}

ENJIN_TEST_MAIN()
