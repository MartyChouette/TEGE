// Rapid scene switching (T9).
//
// A script asks for a scene with Scene_LoadScene, which only records a request;
// each runtime takes it at its safe point at the top of the frame and loads
// then. Three shapes were never exercised: a request made every frame, many
// requests in one frame, and cycling through scenes over and over. These pin
// what the request slot promises and that repeated loads replace the world
// rather than pile onto it.
//
// LoadSceneWithTransition has no caller yet, but it is public, and reading it
// predicted the one real failure: every call restarted the fade, so a request
// made each frame faded out forever and never loaded.

#include "EnjinTest.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"

#include <filesystem>
#include <string>

using namespace Enjin;
namespace fs = std::filesystem;

namespace {

// A project on disk with one scene per entry of `counts`, scene i holding
// counts[i] entities. Removed when it goes out of scope.
struct TempProject {
    fs::path dir;
    Scene::SceneManager sm;
    ECS::World world;

    explicit TempProject(std::initializer_list<int> counts) {
        dir = fs::temp_directory_path() / "enjin_test_scene_requests";
        fs::remove_all(dir);
        fs::create_directories(dir / "scenes");
        int i = 0;
        for (int n : counts) {
            ECS::World w;
            for (int e = 0; e < n; ++e) {
                const ECS::Entity ent = w.CreateEntity();
                w.AddComponent<ECS::TransformComponent>(ent, ECS::TransformComponent{});
            }
            Scene::SceneSerializer ser(&w);
            const std::string rel = "scenes/S" + std::to_string(i) + ".enjin";
            ser.Save((dir / rel).string());
            Scene::SceneEntry entry;
            entry.name = "S" + std::to_string(i);
            entry.path = rel;
            entry.buildIndex = i;
            sm.GetScenes().push_back(entry);
            ++i;
        }
        sm.SaveProject((dir / "p.enjinproject").string());
        sm.SetWorld(&world);
    }
    ~TempProject() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

}  // namespace

ENJIN_TEST(SceneRequests, test_scene_requests_flood_in_one_frame_is_one_load_of_the_last) {
    // Arrange
    Scene::SceneManager sm;

    // Act: a hundred requests before the safe point takes one
    for (int i = 0; i < 100; ++i) sm.RequestSceneChange("S" + std::to_string(i));
    std::string first, second;
    const auto r1 = sm.TakeSceneRequest(first);
    const auto r2 = sm.TakeSceneRequest(second);

    // Assert: one load, of the newest, and the slot is empty after
    ENJIN_EXPECT_TRUE(r1 == Scene::SceneManager::SceneRequest::Load);
    ENJIN_EXPECT_EQ(first, std::string("S99"));
    ENJIN_EXPECT_TRUE(r2 == Scene::SceneManager::SceneRequest::None);
}

ENJIN_TEST(SceneRequests, test_scene_requests_restart_after_load_wins) {
    // Arrange
    Scene::SceneManager sm;

    // Act
    sm.RequestSceneChange("S1");
    sm.RequestRestart();
    std::string name;
    const auto r = sm.TakeSceneRequest(name);

    // Assert: the newest request is the one honoured
    ENJIN_EXPECT_TRUE(r == Scene::SceneManager::SceneRequest::Restart);
}

ENJIN_TEST(SceneRequests, test_scene_requests_cycle_replaces_the_world_each_time) {
    // Arrange: three scenes of different sizes
    TempProject p({3, 7, 1});
    const usize expected[] = {3, 7, 1};

    // Act and assert: ten laps, each load leaving exactly that scene's entities
    bool allLoaded = true, allExact = true;
    for (int lap = 0; lap < 10; ++lap) {
        for (int s = 0; s < 3; ++s) {
            p.sm.RequestSceneChange("S" + std::to_string(s));
            std::string name;
            if (p.sm.TakeSceneRequest(name) != Scene::SceneManager::SceneRequest::Load) allLoaded = false;
            if (!p.sm.LoadScene(name)) allLoaded = false;
            p.world.Update(0.0f);   // flush deferred destruction, as a frame would
            if (p.world.GetEntityCount() != expected[s]) allExact = false;
        }
    }
    ENJIN_EXPECT_TRUE(allLoaded);
    ENJIN_EXPECT_TRUE(allExact);
    ENJIN_EXPECT_EQ(p.sm.GetCurrentSceneName(), std::string("S2"));
}

ENJIN_TEST(SceneRequests, test_scene_transition_requested_every_frame_still_loads) {
    // Arrange
    TempProject p({2, 5});
    const f32 dt = 1.0f / 60.0f;

    // Act: ask for S1 with a half-second fade on every frame for two seconds
    bool loaded = false;
    for (int frame = 0; frame < 120 && !loaded; ++frame) {
        p.sm.LoadSceneWithTransition("S1", Scene::TransitionType::FadeBlack, 0.5f);
        p.sm.UpdateTransition(dt);
        loaded = p.sm.GetCurrentSceneName() == "S1";
    }

    // Assert: the fade ran to the load instead of restarting each call
    ENJIN_EXPECT_TRUE(loaded);
}

ENJIN_TEST_MAIN()
