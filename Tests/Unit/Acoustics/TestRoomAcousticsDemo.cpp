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
