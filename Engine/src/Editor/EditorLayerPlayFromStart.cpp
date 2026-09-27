// Play from Start: the whole game, as a player gets it, from the editor.
//
// Plain Play runs the open scene and nothing else, which is what a person
// editing a level wants and what it keeps doing. But it means the editor never
// ran a game's startup flow (splash, cutscene, title, gameplay) or its start
// scene, so the only way to see the boot sequence a player sees was to export a
// build (decided by Marty 2026-09-27: a separate Play from Start entry).
//
// This walks the project's startup flow the way both players do
// (BeginStartupFlow / AdvanceFlow in Player/src/main.cpp): the start scene
// first, then each step. A Scene step plays until its advance condition (a
// timer, any input, Flow_Advance(), or never, for gameplay); a Menu step shows
// the title menu with gameplay held until New Game. With no flow authored it is
// the classic boot: the start scene under its title menu. A scene change goes
// through the same stop -> open -> play the editor already uses for
// Scene_LoadScene. Stopping returns to the scene that was open when it began.
#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"

#include <filesystem>

namespace Enjin {
namespace Editor {

namespace {
std::string NormalizedPath(const std::string& p) {
    if (p.empty()) return {};
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::weakly_canonical(std::filesystem::path(p), ec);
    if (ec) abs = std::filesystem::path(p).lexically_normal();
    std::string s = abs.generic_string();
#if defined(_WIN32)
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
#endif
    return s;
}
} // namespace

std::string EditorLayer::FromStartScenePath(const std::string& projectRelative) const {
    if (projectRelative.empty()) return {};
    const std::string& root = m_SceneManager.GetProjectRoot();
    if (root.empty()) return {};
    return (std::filesystem::path(root) / projectRelative).lexically_normal().string();
}

bool EditorLayer::FromStartSceneIsOpen(const std::string& absolutePath) const {
    return NormalizedPath(absolutePath) == NormalizedPath(m_CurrentScenePath);
}

ECS::Entity EditorLayer::FindVisibleAuthoredMainMenu() const {
    if (!m_World) return ECS::INVALID_ENTITY;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<GUI::UICanvasComponent>()) {
        const auto* c = m_World->GetComponent<GUI::UICanvasComponent>(e);
        if (c && c->canvasName == "MainMenu" && c->visible) return e;
    }
    return ECS::INVALID_ENTITY;
}

void EditorLayer::RequestPlayFromStart() {
    if (!m_PlayMode.IsStopped() || m_FromStart.active) return;
    if (m_SceneManager.GetProjectPath().empty()) {
        ShowNotification("Play from Start needs a project", NotificationType::Warning);
        return;
    }

    // The start scene: the one marked, else the first in build order, as the
    // players choose it (SceneManager::LoadStartScene)
    const Scene::SceneEntry* start = nullptr;
    for (const auto& s : m_SceneManager.GetScenes())
        if (s.isStartScene) { start = &s; break; }
    if (!start) {
        for (const auto& s : m_SceneManager.GetScenes())
            if (s.buildIndex >= 0 && (!start || s.buildIndex < start->buildIndex)) start = &s;
    }
    if (!start) {
        ShowNotification("Play from Start: the project has no start scene", NotificationType::Warning);
        return;
    }

    // Switching scenes loads from disk, so an unsaved edit to the open scene
    // would be lost. Save it, as a build does; an untitled scene cannot be.
    if (m_SceneDirty) {
        if (m_CurrentScenePath.empty()) {
            ShowNotification("Save the scene before Play from Start", NotificationType::Warning);
            return;
        }
        SaveScene(m_CurrentScenePath);
    }

    m_FromStart = FromStartRun{};
    m_FromStart.active = true;
    m_FromStart.returnScene = m_CurrentScenePath;
    m_FromStart.steps = m_SceneManager.GetStartupFlow();
    if (m_FromStart.steps.empty()) {
        // The classic boot: the start scene under its title menu
        Scene::StartupFlowStep menu;
        menu.type = Scene::StartupStepType::Menu;
        m_FromStart.steps.push_back(menu);
    }
    m_FromStart.beginPending = true;

    m_PrePlayRenderSettings = Renderer::SceneRenderSettings::CaptureFromRuntime(
        m_RenderSystem, m_PostProcessing ? &m_PostProcessing->GetSettings() : nullptr);
    const std::string startPath = FromStartScenePath(start->path);
    if (FromStartSceneIsOpen(startPath)) {
        StartPlayMode();
    } else {
        m_RestartPlayScene = startPath;   // opened, then played, by the restart path
        m_RestartPlayPending = true;
    }
    ENJIN_LOG_INFO(Editor, "Play from Start: start scene '%s', %zu flow step(s)",
                   start->path.c_str(), m_FromStart.steps.size());
}

void EditorLayer::EndPlayFromStart() {
    if (!m_FromStart.active) return;
    const std::string back = m_FromStart.returnScene;
    m_FromStart = FromStartRun{};
    if (m_GameMenu.GetCurrentScreen() == GUI::MenuScreen::MainMenu) m_GameMenu.HideAll();
    if (!back.empty() && !FromStartSceneIsOpen(back)) OpenScene(back);
    ENJIN_LOG_INFO(Editor, "Play from Start: ended");
}

bool EditorLayer::FromStartHoldsGameplay() const {
    return m_FromStart.active && m_FromStart.onMenu;
}

void EditorLayer::AdvanceFromStart() {
    if (!m_FromStart.active) return;
    m_FromStart.index++;
    m_FromStart.onMenu = false;
    if (m_FromStart.index >= static_cast<i32>(m_FromStart.steps.size())) {
        // Ran off the end: whatever scene is open becomes gameplay, as in the players
        m_FromStart.finished = true;
        if (m_GameMenu.GetCurrentScreen() == GUI::MenuScreen::MainMenu) m_GameMenu.HideAll();
        ENJIN_LOG_INFO(Editor, "Play from Start: flow complete, gameplay");
        return;
    }
    const Scene::StartupFlowStep& step = m_FromStart.steps[static_cast<usize>(m_FromStart.index)];
    m_FlowAdvanceRequested = false;
    m_FromStart.menuAnswered = false;

    if (step.type == Scene::StartupStepType::Menu) {
        // The title menu over the open scene, gameplay held until New Game. An
        // authored MainMenu canvas is the title when the scene has one, as in
        // the players; otherwise the built-in menu.
        m_FromStart.onMenu = true;
        m_FromStart.builtInMenu = FindVisibleAuthoredMainMenu() == ECS::INVALID_ENTITY;
        if (m_FromStart.builtInMenu) m_GameMenu.ShowScreen(GUI::MenuScreen::MainMenu);
        Input::SetMouseCaptured(false);
        ENJIN_LOG_INFO(Editor, "Play from Start: step %d, title menu", m_FromStart.index);
        return;
    }

    if (m_GameMenu.GetCurrentScreen() == GUI::MenuScreen::MainMenu) m_GameMenu.HideAll();
    m_FromStart.timer = step.duration;
    const std::string path = FromStartScenePath(step.scene);
    if (!path.empty() && !FromStartSceneIsOpen(path)) {
        m_FromStart.waitingForScene = true;
        m_RestartPlayScene = path;
        m_RestartPlayPending = true;
        m_PendingPlayStop = true;
    }
    ENJIN_LOG_INFO(Editor, "Play from Start: step %d, scene '%s'", m_FromStart.index, step.scene.c_str());
}

void EditorLayer::UpdatePlayFromStart(f32 dt) {
    if (!m_FromStart.active) return;

    // A stop that is not one of ours (the toolbar, Escape, a Quit button) ends
    // the run and goes back to where it began
    const bool switching = m_RestartPlayPending || m_PendingPlayStop;
    if (m_PlayMode.IsStopped() && !switching) { EndPlayFromStart(); return; }
    if (!m_PlayMode.IsPlaying() && !m_PlayMode.IsPaused()) return;

    if (m_FromStart.beginPending) {
        if (!m_PlayMode.IsPlaying() || switching) return;
        m_FromStart.beginPending = false;
        m_FromStart.index = -1;
        AdvanceFromStart();
        return;
    }
    if (m_FromStart.waitingForScene) {
        if (switching || !m_PlayMode.IsPlaying()) return;
        m_FromStart.waitingForScene = false;
    }
    if (m_FromStart.finished || m_PlayMode.IsPaused()) return;
    if (m_FromStart.index < 0 || m_FromStart.index >= static_cast<i32>(m_FromStart.steps.size())) return;

    const Scene::StartupFlowStep& step = m_FromStart.steps[static_cast<usize>(m_FromStart.index)];
    if (step.type == Scene::StartupStepType::Menu) {
        // New Game on either menu. The authored canvas hides itself (PlayMode's
        // menu_newgame listener); the built-in one reports through the callback.
        const bool authoredGone = !m_FromStart.builtInMenu &&
                                  FindVisibleAuthoredMainMenu() == ECS::INVALID_ENTITY;
        if (m_FromStart.menuAnswered || authoredGone) AdvanceFromStart();
        return;
    }

    switch (step.advance) {
        case Scene::StartupAdvance::Timer:
            m_FromStart.timer -= dt;
            if (m_FromStart.timer <= 0.0f) AdvanceFromStart();
            break;
        case Scene::StartupAdvance::Input: {
            const bool touchNow = Input::GetActiveTouchCount() > 0;
            const bool touchStarted = touchNow && !m_FromStart.touchDown;
            m_FromStart.touchDown = touchNow;
            if (m_InputMap.IsActionPressedAnyFocus(InputSystem::GameAction::UIConfirm) ||
                m_InputMap.IsActionPressedAnyFocus(InputSystem::GameAction::UICancel) ||
                Input::IsMouseButtonPressed(MouseButton::Left) || touchStarted) {
                AdvanceFromStart();
            }
            break;
        }
        case Scene::StartupAdvance::Script:
            if (m_FlowAdvanceRequested) { m_FlowAdvanceRequested = false; AdvanceFromStart(); }
            break;
        case Scene::StartupAdvance::Gameplay:
        default:
            break;   // terminal: the game lives here
    }
}

} // namespace Editor
} // namespace Enjin
