// The demo scene, held to its own claim.
//
// Examples/RoomAcoustics exists to be listened to: three rooms of identical
// size and shape, differing only in what they are made of, with no ReverbZone
// anywhere in the scene. The claim it makes by existing is that walking between
// them sounds different, and that the difference comes from the geometry rather
// than from anybody's slider.
//
// A demo that quietly stopped demonstrating that would be worse than no demo,
// because it would be evidence for something untrue -- and nobody notices a
// rooms-sound-the-same bug by glancing at a scene file. So the scene is loaded
// here and measured, and the numbers it produces are the assertion.

#include "EnjinTest.h"
#include "Enjin/Acoustics/AcousticsSystem.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Material.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <set>
#include <string>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::Math::Vector3;

namespace {

// The scene lives beside the engine, not beside the test binary, so the path is
// resolved by walking up from the working directory until it appears. Hunting
// for it is better than a fixed relative path that works from one build layout
// and silently skips from another.
std::string FindScene() {
    const char* candidates[] = {
        "Examples/RoomAcoustics/scenes/Main.enjin",
        "../Examples/RoomAcoustics/scenes/Main.enjin",
        "../../Examples/RoomAcoustics/scenes/Main.enjin",
        "../../../Examples/RoomAcoustics/scenes/Main.enjin",
        "../../../../Examples/RoomAcoustics/scenes/Main.enjin",
    };
    for (const char* path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    return std::string();
}

AcousticsSettings DemoSettings() {
    AcousticsSettings s;
    s.trace.rayCount = 512;
    return s;
}

// Measure the room around a point, the way the running game does.
RoomResponse MeasureAt(AcousticsSystem& system, const Vector3& where) {
    // Rebuild first, then trace: the system does at most one per call.
    system.Update(where, 2.0f);
    system.Update(where, 0.016f);
    return system.Measurement();
}

} // namespace

ENJIN_TEST(RoomAcousticsDemo, TheSceneLoads) {
    // Arrange
    const std::string path = FindScene();
    if (path.empty()) {
        ENJIN_SKIP("Examples/RoomAcoustics/scenes/Main.enjin not found from the test's "
                   "working directory");
        return;
    }

    // Act
    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    const auto result = serializer.LoadFromString(buffer.str());

    // Assert
    ENJIN_EXPECT_TRUE(result.success);
    ENJIN_EXPECT_TRUE(result.entities.size() > 30);
}

ENJIN_TEST(RoomAcousticsDemo, TheRoomsAreActuallyMadeOfDifferentThings) {
    // Arrange
    const std::string path = FindScene();
    if (path.empty()) { ENJIN_SKIP("demo scene not found"); return; }

    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    ENJIN_ASSERT_TRUE(serializer.LoadFromString(buffer.str()).success);

    // Act: how many DISTINCT things the scene is made of, and whether the
    // extremes are among them.
    //
    // Counting a specific number of carpet surfaces is what this used to do,
    // and it broke the moment the basement's walls became fabric -- a change
    // that made the demo better. A test that has to be edited every time the
    // thing it guards improves is guarding the wrong property.
    std::set<u8> kinds;
    usize surfaced = 0;
    for (ECS::Entity e : world.GetEntitiesWithComponent<ECS::MaterialComponent>()) {
        const auto* m = world.GetComponent<ECS::MaterialComponent>(e);
        if (!m || m->surfaceMaterial == ECS::SurfaceMaterial::Default) continue;
        kinds.insert(static_cast<u8>(m->surfaceMaterial));
        ++surfaced;
    }

    // Assert: this is the round trip that was missing entirely. surfaceMaterial
    // sat on the component, was read by the acoustics, and was serialized by
    // nothing -- so a scene could not author it and it would not survive a save.
    // A demo built on a field that does not persist loads as three identical
    // rooms and proves the opposite of its point.
    std::printf("    %zu surfaces across %zu materials\n",
                surfaced, kinds.size());
    ENJIN_EXPECT_TRUE(surfaced >= 30);
    ENJIN_EXPECT_TRUE(kinds.size() >= 4);
    ENJIN_EXPECT_TRUE(kinds.count(static_cast<u8>(ECS::SurfaceMaterial::Tile)) == 1);
    ENJIN_EXPECT_TRUE(kinds.count(static_cast<u8>(ECS::SurfaceMaterial::Carpet)) == 1);
}

// What the demo is for.
ENJIN_TEST(RoomAcousticsDemo, TheThreeRoomsMeasureAsThreeDifferentRooms) {
    // Arrange
    const std::string path = FindScene();
    if (path.empty()) { ENJIN_SKIP("demo scene not found"); return; }

    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    ENJIN_ASSERT_TRUE(serializer.LoadFromString(buffer.str()).success);

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(DemoSettings());

    // Act: stand in the middle of each room, as a player would.
    const RoomResponse kitchen = MeasureAt(system, Vector3(-21.0f, 1.6f, 0.0f));
    ENJIN_ASSERT_TRUE(kitchen.Valid());
    const f32 kitchenRT = kitchen.rt60[1];

    system.Update(Vector3(0.0f, 1.6f, 0.0f), 0.016f);
    const RoomResponse hall = system.Measurement();
    ENJIN_ASSERT_TRUE(hall.Valid());
    const f32 hallRT = hall.rt60[1];

    system.Update(Vector3(21.0f, 1.6f, 0.0f), 0.016f);
    const RoomResponse basement = system.Measurement();
    ENJIN_ASSERT_TRUE(basement.Valid());
    const f32 basementRT = basement.rt60[1];

    // Assert
    std::printf("    tiled kitchen  %.2f s   (%.2f / %.2f / %.2f)\n",
                kitchenRT, kitchen.rt60[0], kitchen.rt60[1], kitchen.rt60[2]);
    std::printf("    drywall hall   %.2f s   (%.2f / %.2f / %.2f)\n",
                hallRT, hall.rt60[0], hall.rt60[1], hall.rt60[2]);
    std::printf("    carpet basement %.2f s  (%.2f / %.2f / %.2f)\n",
                basementRT, basement.rt60[0], basement.rt60[1], basement.rt60[2]);

    // Three rooms of identical size, ordered hardest to softest, with nothing
    // authored. If these ever converge, the demo is showing the opposite of
    // what it claims and this says so.
    ENJIN_EXPECT_TRUE(kitchenRT > hallRT);
    ENJIN_EXPECT_TRUE(hallRT > basementRT);
    ENJIN_EXPECT_TRUE(kitchenRT > basementRT * 2.0f);
}

ENJIN_TEST(RoomAcousticsDemo, TheCarpetedRoomIsDullRatherThanQuiet) {
    // Arrange
    const std::string path = FindScene();
    if (path.empty()) { ENJIN_SKIP("demo scene not found"); return; }

    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    ENJIN_ASSERT_TRUE(serializer.LoadFromString(buffer.str()).success);

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(DemoSettings());

    // Act
    const RoomResponse basement = MeasureAt(system, Vector3(21.0f, 1.6f, 0.0f));
    ENJIN_ASSERT_TRUE(basement.Valid());

    // Assert: carpet returns most of the low end and swallows the top, so the
    // room must ring longer at the bottom than at the top. A basement whose
    // three bands matched would just be a quieter room, and the per-band work
    // would be reaching nobody.
    ENJIN_EXPECT_TRUE(basement.rt60[0] > basement.rt60[2] * 1.5f);
}

// The half of a room a decay time cannot describe.
//
// A tail says how live a room is. The discrete reflections say where its walls
// are and what they are made of -- and until they were wired to the mixer the
// engine only ever produced the tail, so every room was the same wash at a
// different length. Tile returns the top of a clap almost intact; a curtain
// returns the bottom of it and nothing else. If that stops being true, the
// rooms collapse back into one room played at different speeds.
ENJIN_TEST(RoomAcousticsDemo, TheReflectionsCarryTheMaterialNotJustTheTail) {
    // Arrange
    const std::string path = FindScene();
    if (path.empty()) { ENJIN_SKIP("demo scene not found"); return; }

    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    ENJIN_ASSERT_TRUE(serializer.LoadFromString(buffer.str()).success);

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(DemoSettings());

    // Act
    MeasureAt(system, Vector3(-21.0f, 1.6f, 0.0f));
    const auto kitchen = system.Reflections();

    system.Update(Vector3(21.0f, 1.6f, 0.0f), 0.016f);
    const auto basement = system.Reflections();

    // Assert: there have to BE reflections at all. An empty set is what the
    // engine effectively had for as long as nothing called the tracer.
    ENJIN_ASSERT_TRUE(kitchen.Any());
    ENJIN_ASSERT_TRUE(basement.Any());

    // Brightest tap in each room, measured as how much of the top end comes
    // back relative to the middle. This is the number a listener hears as
    // "hard surface" versus "soft furnishing".
    auto brightest = [](const EarlyReflectionResult& r) {
        f32 best = 0.0f;
        for (const auto& t : r.taps) {
            const f32 mid = t.gain[1] > 1.0e-9f ? t.gain[1] : 1.0e-9f;
            const f32 ratio = t.gain[2] / mid;
            if (ratio > best) best = ratio;
        }
        return best;
    };
    auto loudest = [](const EarlyReflectionResult& r) {
        f32 best = 0.0f;
        for (const auto& t : r.taps) if (t.gain[1] > best) best = t.gain[1];
        return best;
    };

    std::printf("    kitchen  %zu taps, loudest %.3f, brightest hi/mid %.2f\n",
                kitchen.taps.size(), loudest(kitchen), brightest(kitchen));
    std::printf("    basement %zu taps, loudest %.3f, brightest hi/mid %.2f\n",
                basement.taps.size(), loudest(basement), brightest(basement));

    // Where a single reflection actually carries the material, and where it
    // does not.
    //
    // In the MIDS one bounce barely tells them apart, and that is not a bug.
    // Heavy carpet on concrete absorbs 0.30 of the mid energy, so it returns
    // sqrt(0.70) = 0.84 of the amplitude against tile's sqrt(0.99) = 0.995 --
    // about 16% down, over the same 3.2 m floor bounce. Asserting a 1.5x split
    // here would have been asserting a number the physics does not produce, and
    // the only way to pass it would have been to make the materials wrong.
    //
    // What one bounce DOES carry is the top end: tile absorbs 0.02 up there and
    // carpet absorbs 0.60, so the same reflection comes back bright off one and
    // dull off the other. That is the cue a listener reads instantly as "hard
    // room" -- and it is why the rooms have to differ in the tail as well, since
    // the tail is where a hundred of these multiply together.
    ENJIN_EXPECT_TRUE(loudest(kitchen) > loudest(basement));
    ENJIN_EXPECT_TRUE(brightest(kitchen) > brightest(basement) * 1.2f);
}

// A reflection has to arrive AFTER the sound that caused it.
ENJIN_TEST(RoomAcousticsDemo, TheFirstReflectionArrivesWhenTheGeometrySaysItShould) {
    // Arrange
    const std::string path = FindScene();
    if (path.empty()) { ENJIN_SKIP("demo scene not found"); return; }

    ECS::World world;
    Scene::SceneSerializer serializer(&world);
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    ENJIN_ASSERT_TRUE(serializer.LoadFromString(buffer.str()).success);

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(DemoSettings());

    // Act: stood in the middle of the hall, which is 10 x 3.4 x 8 metres.
    MeasureAt(system, Vector3(0.0f, 1.6f, 0.0f));
    const auto hall = system.Reflections();
    ENJIN_ASSERT_TRUE(hall.Any());

    f32 earliest = 1.0e9f;
    for (const auto& t : hall.taps) if (t.delay < earliest) earliest = t.delay;

    // Assert: the nearest surface to that point is the ceiling, 1.8 m up, so
    // the shortest path out and back is 3.6 m -- about 10.5 ms at 343 m/s. The
    // floor, 1.6 m down, gives 9.3 ms. Nothing can come back sooner than that
    // without passing through a wall.
    std::printf("    first reflection in the hall: %.1f ms\n", earliest * 1000.0f);
    ENJIN_EXPECT_TRUE(earliest > 0.008f);
    // And it has to be early enough to read as THIS room rather than as reverb:
    // the longest dimension is 10 m, so a first-order path cannot exceed ~60 ms.
    ENJIN_EXPECT_TRUE(earliest < 0.060f);
}

ENJIN_TEST(RoomAcousticsDemo, NothingInTheSceneAuthorsAReverb) {
    // Arrange
    const std::string path = FindScene();
    if (path.empty()) { ENJIN_SKIP("demo scene not found"); return; }

    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    // Assert: the whole point is that the difference comes from the geometry. A
    // ReverbZone appearing in here later -- added to "fix" a room that sounded
    // wrong -- would make the demo prove nothing at all, quietly.
    ENJIN_EXPECT_TRUE(text.find("reverbZone") == std::string::npos);
    ENJIN_EXPECT_TRUE(text.find("decayTime") == std::string::npos);
}

ENJIN_TEST_MAIN()
