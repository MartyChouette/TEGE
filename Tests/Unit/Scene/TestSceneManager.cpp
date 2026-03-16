#include "EnjinTest.h"
#include "Enjin/Scene/SceneManager.h"

using namespace Enjin;
using namespace Enjin::Scene;

// ===========================================================================
// SceneManager Defaults
// ===========================================================================

ENJIN_TEST(SceneManagerDefaults, ProjectName) {
    SceneManager sm;
    ENJIN_EXPECT_STR_EQ(sm.GetProjectName(), "Untitled Project");
}

ENJIN_TEST(SceneManagerDefaults, NoScenes) {
    SceneManager sm;
    ENJIN_EXPECT_EQ(sm.GetSceneCount(), (usize)0);
}

ENJIN_TEST(SceneManagerDefaults, ProjectMode) {
    SceneManager sm;
    ENJIN_EXPECT_EQ((int)sm.GetProjectMode(), (int)ProjectMode::Mode3D);
}

ENJIN_TEST(SceneManagerDefaults, NotTransitioning) {
    SceneManager sm;
    ENJIN_EXPECT_FALSE(sm.IsTransitioning());
    ENJIN_EXPECT_EQ((int)sm.GetTransitionState(), (int)TransitionState::None);
}

ENJIN_TEST(SceneManagerDefaults, WindowSettings) {
    SceneManager sm;
    ENJIN_EXPECT_STR_EQ(sm.GetWindowTitle(), "Enjin Game");
    ENJIN_EXPECT_EQ(sm.GetWindowWidth(), 1280u);
    ENJIN_EXPECT_EQ(sm.GetWindowHeight(), 720u);
    ENJIN_EXPECT_FALSE(sm.GetFullscreen());
}

ENJIN_TEST(SceneManagerDefaults, AudioSettings) {
    SceneManager sm;
    ENJIN_EXPECT_TRUE(sm.GetEnableHRTF());
    ENJIN_EXPECT_TRUE(sm.GetEnableOcclusion());
    ENJIN_EXPECT_TRUE(sm.GetEnableTransmission());
}

// ===========================================================================
// Scene Management
// ===========================================================================

ENJIN_TEST(SceneManagement, NewProject) {
    SceneManager sm;
    sm.NewProject("Test Game");
    ENJIN_EXPECT_STR_EQ(sm.GetProjectName(), "Test Game");
    ENJIN_EXPECT_EQ(sm.GetSceneCount(), (usize)0);
}

ENJIN_TEST(SceneManagement, AddScene) {
    SceneManager sm;
    sm.AddScene("Main Menu", "scenes/main_menu.enjin");
    ENJIN_EXPECT_EQ(sm.GetSceneCount(), (usize)1);
    auto* scene = sm.GetScene(0);
    ENJIN_ASSERT_NOT_NULL(scene);
    ENJIN_EXPECT_STR_EQ(scene->name, "Main Menu");
    ENJIN_EXPECT_STR_EQ(scene->path, "scenes/main_menu.enjin");
}

ENJIN_TEST(SceneManagement, AddMultipleScenes) {
    SceneManager sm;
    sm.AddScene("Level 1", "scenes/level1.enjin");
    sm.AddScene("Level 2", "scenes/level2.enjin");
    sm.AddScene("Level 3", "scenes/level3.enjin");
    ENJIN_EXPECT_EQ(sm.GetSceneCount(), (usize)3);
}

ENJIN_TEST(SceneManagement, RemoveScene) {
    SceneManager sm;
    sm.AddScene("Scene A", "a.enjin");
    sm.AddScene("Scene B", "b.enjin");
    sm.RemoveScene(0);
    ENJIN_EXPECT_EQ(sm.GetSceneCount(), (usize)1);
    ENJIN_EXPECT_STR_EQ(sm.GetScene(0)->name, "Scene B");
}

ENJIN_TEST(SceneManagement, GetSceneByName) {
    SceneManager sm;
    sm.AddScene("Menu", "menu.enjin");
    sm.AddScene("Game", "game.enjin");
    auto* found = sm.GetSceneByName("Game");
    ENJIN_ASSERT_NOT_NULL(found);
    ENJIN_EXPECT_STR_EQ(found->path, "game.enjin");
}

ENJIN_TEST(SceneManagement, GetSceneByNameNotFound) {
    SceneManager sm;
    sm.AddScene("Menu", "menu.enjin");
    auto* found = sm.GetSceneByName("NonExistent");
    ENJIN_EXPECT_NULL(found);
}

ENJIN_TEST(SceneManagement, GetSceneIndex) {
    SceneManager sm;
    sm.AddScene("A", "a.enjin");
    sm.AddScene("B", "b.enjin");
    sm.AddScene("C", "c.enjin");
    ENJIN_EXPECT_EQ(sm.GetSceneIndex("B"), 1);
    ENJIN_EXPECT_EQ(sm.GetSceneIndex("Missing"), -1);
}

// ===========================================================================
// Project Settings
// ===========================================================================

ENJIN_TEST(ProjectSettings, SetProjectMode) {
    SceneManager sm;
    sm.SetProjectMode(ProjectMode::Mode2D);
    ENJIN_EXPECT_EQ((int)sm.GetProjectMode(), (int)ProjectMode::Mode2D);
    sm.SetProjectMode(ProjectMode::Mixed);
    ENJIN_EXPECT_EQ((int)sm.GetProjectMode(), (int)ProjectMode::Mixed);
}

ENJIN_TEST(ProjectSettings, WindowConfig) {
    SceneManager sm;
    sm.SetWindowTitle("My Game");
    sm.SetWindowWidth(1920);
    sm.SetWindowHeight(1080);
    sm.SetFullscreen(true);
    ENJIN_EXPECT_STR_EQ(sm.GetWindowTitle(), "My Game");
    ENJIN_EXPECT_EQ(sm.GetWindowWidth(), 1920u);
    ENJIN_EXPECT_EQ(sm.GetWindowHeight(), 1080u);
    ENJIN_EXPECT_TRUE(sm.GetFullscreen());
}

ENJIN_TEST(ProjectSettings, AudioConfig) {
    SceneManager sm;
    sm.SetEnableHRTF(false);
    sm.SetEnableOcclusion(false);
    ENJIN_EXPECT_FALSE(sm.GetEnableHRTF());
    ENJIN_EXPECT_FALSE(sm.GetEnableOcclusion());
}

// ===========================================================================
// Transition
// ===========================================================================

ENJIN_TEST(Transition, TransitionAlpha) {
    SceneManager sm;
    ENJIN_EXPECT_FLOAT_EQ(sm.GetTransitionAlpha(), 0.0f);
}

// ===========================================================================
// Collision Group Names
// ===========================================================================

ENJIN_TEST(CollisionGroups, Initial32Groups) {
    SceneManager sm;
    ENJIN_EXPECT_EQ(sm.GetCollisionGroupNames().size(), (size_t)32);
    ENJIN_EXPECT_STR_EQ(sm.GetCollisionGroupNames()[0], "Default");
}

ENJIN_TEST(CollisionGroups, SetGroupNames) {
    SceneManager sm;
    auto& groups = sm.GetCollisionGroupNames();
    groups[1] = "Player";
    groups[2] = "Enemy";
    groups[3] = "Environment";
    ENJIN_EXPECT_EQ(sm.GetCollisionGroupNames().size(), (size_t)32);
    ENJIN_EXPECT_STR_EQ(sm.GetCollisionGroupNames()[1], "Player");
}

// ===========================================================================
// SceneEntry
// ===========================================================================

ENJIN_TEST(SceneEntry, Defaults) {
    SceneEntry entry;
    ENJIN_EXPECT_EQ(entry.buildIndex, -1);
    ENJIN_EXPECT_FALSE(entry.isStartScene);
}

// ===========================================================================
// Transition Enums
// ===========================================================================

ENJIN_TEST(TransitionEnums, Types) {
    ENJIN_EXPECT_EQ((int)TransitionType::Instant, 0);
    ENJIN_EXPECT_EQ((int)TransitionType::FadeBlack, 1);
    ENJIN_EXPECT_EQ((int)TransitionType::FadeWhite, 2);
    ENJIN_EXPECT_EQ((int)TransitionType::CrossFade, 3);
}

ENJIN_TEST_MAIN()
