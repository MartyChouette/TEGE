// Which scene am I in?
//
// BUG-0003 (Marty, 2026-09-13). `m_CurrentSceneName` was assigned in exactly one
// place -- inside `SceneManager::LoadScene` -- and two of the three ways a scene
// actually becomes current do not go through it. The editor's
// `OpenSceneImmediate` deserializes directly; the Player's deferred script
// scene-request hands the PATH to the flow transition. So
// `Scene_GetCurrentScene()` answered "" for a whole play session, and the
// sharpest case is the obvious one: a script calls `Scene_LoadScene` and then
// asks where it is.
//
// Empty is the worst possible answer here, because it is indistinguishable from
// "no scene is loaded" -- the same magic-empty shape as a fallback that could be
// a real value.
//
// The two call sites are one line each and are visible in the diff. What is
// worth pinning is the resolution: the callers hold different things (a
// scene-list name, a relative path, an absolute path), and requiring each to
// convert is how this got missed three times over.

#include "EnjinTest.h"
#include "Enjin/Scene/SceneManager.h"

#include <cstdio>
#include <string>

using namespace Enjin;

namespace {

Scene::SceneManager MakeManager() {
    Scene::SceneManager sm;
    Scene::SceneEntry week;
    week.name = "SdriftWeek";
    week.path = "scenes/SdriftWeek.enjin";
    sm.GetScenes().push_back(week);

    Scene::SceneEntry drive;
    drive.name = "DriftStage";
    drive.path = "scenes/sub/DriftStage.enjin";
    sm.GetScenes().push_back(drive);
    return sm;
}

}  // namespace

ENJIN_TEST(CurrentSceneName, StartsEmptyBecauseNothingIsLoaded) {
    Scene::SceneManager sm = MakeManager();
    ENJIN_EXPECT_EQ(sm.GetCurrentSceneName(), std::string(""));
}

ENJIN_TEST(CurrentSceneName, AListedNameIsKeptAsIs) {
    Scene::SceneManager sm = MakeManager();
    sm.NoteSceneBecameCurrent("SdriftWeek");
    ENJIN_EXPECT_EQ(sm.GetCurrentSceneName(), std::string("SdriftWeek"));
}

// The editor hands over a path, not a name. It must still come back as the name
// a script would use with Scene_LoadScene, or the answer is useless for the one
// thing anybody asks it.
ENJIN_TEST(CurrentSceneName, APathResolvesBackToItsSceneName) {
    Scene::SceneManager sm = MakeManager();
    sm.NoteSceneBecameCurrent("scenes/sub/DriftStage.enjin");
    std::printf("    from exact path -> '%s'\n", sm.GetCurrentSceneName().c_str());
    ENJIN_EXPECT_EQ(sm.GetCurrentSceneName(), std::string("DriftStage"));
}

// An absolute path, which is what the editor's file dialog produces, does not
// match any entry's relative path. Falling back to the filename still lands on
// the right scene.
ENJIN_TEST(CurrentSceneName, AnAbsolutePathFallsBackToTheFilename) {
    Scene::SceneManager sm = MakeManager();
    sm.NoteSceneBecameCurrent("D:/TEGE_Projects/Sdrift/scenes/SdriftWeek.enjin");
    std::printf("    from absolute path -> '%s'\n", sm.GetCurrentSceneName().c_str());
    ENJIN_EXPECT_EQ(sm.GetCurrentSceneName(), std::string("SdriftWeek"));
}

// A scene nobody registered still answers with something. A name that is not in
// the list is wrong-ish; an empty string is actively misleading, because it
// reads as "no scene loaded".
ENJIN_TEST(CurrentSceneName, AnUnlistedSceneStillAnswersWithSomething) {
    Scene::SceneManager sm = MakeManager();
    sm.NoteSceneBecameCurrent("scenes/Untracked.enjin");
    std::printf("    unlisted -> '%s'\n", sm.GetCurrentSceneName().c_str());
    ENJIN_EXPECT_TRUE(!sm.GetCurrentSceneName().empty());
    ENJIN_EXPECT_EQ(sm.GetCurrentSceneName(), std::string("Untracked"));
}

ENJIN_TEST_MAIN()
