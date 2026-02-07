#include "Enjin/Editor/PlayMode.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Debug/Profiler.h"

namespace Enjin {
namespace Editor {

void PlayMode::Initialize(ECS::World* world, Renderer::Camera* camera,
                          Renderer::CameraController* cameraController) {
    m_World = world;
    m_Camera = camera;
    m_CameraController = cameraController;

    m_Physics.SetWorld(world);

    m_ControllerSystem.SetWorld(world);
    m_ControllerSystem.SetCamera(camera);
    m_ControllerSystem.SetPhysics(&m_Physics);
    m_ControllerSystem.SetEnabled(false);

    m_FlowerSystem.SetWorld(world);
    m_FlowerSystem.SetCamera(camera);
    m_FlowerSystem.SetEnabled(false);

    // Initialize scripting engine
    if (m_ScriptEngine.Init()) {
        Scripting::RegisterAllBindings(m_ScriptEngine.GetASEngine());
        m_ScriptEngine.SetWorld(world);
        m_ScriptEngine.SetScriptDirectory("scripts");

        m_ScriptSystem.SetWorld(world);
        m_ScriptSystem.SetScriptEngine(&m_ScriptEngine);
        m_ScriptSystem.SetCoroutineScheduler(&m_CoroutineScheduler);
        m_ScriptSystem.SetEnabled(false);

        m_CoroutineScheduler.SetEngine(m_ScriptEngine.GetASEngine());
        m_EventBus.SetScriptEngine(&m_ScriptEngine);
        m_StateMachineSystem.SetScriptEngine(&m_ScriptEngine);
        m_TweenSystem.SetScriptEngine(&m_ScriptEngine);

        // Initialize visual script system
        m_VisualScriptSystem.SetWorld(world);

        ENJIN_LOG_INFO(Editor, "Script engine initialized");
    } else {
        ENJIN_LOG_WARN(Editor, "Failed to initialize script engine");
    }

    ENJIN_LOG_INFO(Editor, "PlayMode initialized");
}

void PlayMode::Play() {
    if (m_State == PlayState::Playing) {
        return;
    }

    ENJIN_LOG_INFO(Editor, "PlayMode::Play() starting...");

    // Save current editor state
    SaveEditorState();
    ENJIN_LOG_INFO(Editor, "PlayMode: SaveEditorState done");

    // Find the active game camera entity so controllers drive it instead of the editor camera
    ECS::Entity gameCam = ECS::CameraManager::GetActiveCamera(m_World);
    m_ControllerSystem.SetGameCameraEntity(gameCam);
    ENJIN_LOG_INFO(Editor, "PlayMode: Camera setup done (cam=%llu)", (unsigned long long)gameCam);

    // Enable controller system, flower system, scripting, and gameplay systems
    m_ControllerSystem.SetEnabled(true);
    ENJIN_LOG_INFO(Editor, "PlayMode: ControllerSystem enabled");
    m_FlowerSystem.Reset();
    m_FlowerSystem.SetEnabled(true);
    ENJIN_LOG_INFO(Editor, "PlayMode: FlowerSystem enabled");
    m_ScriptSystem.SetEnabled(true);
    ENJIN_LOG_INFO(Editor, "PlayMode: ScriptSystem enabled");
    m_HUDSystem.SetEnabled(true);
    m_QuestSystem.SetEnabled(true);
    m_FootstepSystem.SetEnabled(true);
    m_CinematicSystem.SetEnabled(true);
    ENJIN_LOG_INFO(Editor, "PlayMode: Gameplay systems enabled");
    m_TweenSystem.PlayAll(m_World);
    Scripting::SetBindingsWorld(m_World);
    Scripting::SetBindingsDialogueSystem(&m_DialogueSystem);
    Scripting::SetBindingsRenderSystem(m_RenderSystem);
    Scripting::SetBindingsPostProcessing(m_PostProcessing);
    ENJIN_LOG_INFO(Editor, "PlayMode: Script bindings set");

    // Wire EntityEventBus and SubtitleSystem to DialogueSystem
    m_DialogueSystem.SetEventBus(&m_EntityEventBus);
    m_DialogueSystem.SetSubtitleSystem(m_SubtitleSystem);
    ENJIN_LOG_INFO(Editor, "PlayMode: DialogueSystem integrations wired");
    m_ScriptSystem.InitializeAllScripts();
    ENJIN_LOG_INFO(Editor, "PlayMode: Scripts initialized");

    // Initialize visual script system
    m_VisualScriptSystem.Initialize();
    ENJIN_LOG_INFO(Editor, "PlayMode: VisualScriptSystem initialized");

    // Disable editor camera controller
    if (m_CameraController) {
        m_CameraController->SetEnabled(false);
    }

    // Do NOT capture mouse here — only focus mode (F11) captures the mouse.
    // This lets the user keep a visible cursor in the editor while play mode runs.

    m_State = PlayState::Playing;
    ENJIN_LOG_INFO(Editor, "Entered Play Mode");
}

void PlayMode::Pause() {
    if (m_State != PlayState::Playing) {
        return;
    }

    m_ControllerSystem.SetEnabled(false);
    m_FlowerSystem.SetEnabled(false);
    m_ScriptSystem.SetEnabled(false);
    m_HUDSystem.SetEnabled(false);
    m_QuestSystem.SetEnabled(false);
    m_FootstepSystem.SetEnabled(false);
    m_CinematicSystem.SetEnabled(false);
    Input::SetMouseCaptured(false);

    m_State = PlayState::Paused;
    ENJIN_LOG_INFO(Editor, "Play Mode Paused");
}

void PlayMode::Resume() {
    if (m_State != PlayState::Paused) {
        return;
    }

    m_ControllerSystem.SetEnabled(true);
    m_FlowerSystem.SetEnabled(true);
    m_ScriptSystem.SetEnabled(true);
    m_HUDSystem.SetEnabled(true);
    m_QuestSystem.SetEnabled(true);
    m_FootstepSystem.SetEnabled(true);
    m_CinematicSystem.SetEnabled(true);
    // Do NOT capture mouse here — only focus mode (F11) captures the mouse.

    m_State = PlayState::Playing;
    ENJIN_LOG_INFO(Editor, "Play Mode Resumed");
}

void PlayMode::Stop() {
    if (m_State == PlayState::Stopped) {
        return;
    }

    // Shutdown visual scripts and scripts first (before scene restore destroys entities)
    m_VisualScriptSystem.Shutdown();
    m_ScriptSystem.ShutdownAllScripts();
    m_ScriptSystem.SetEnabled(false);
    m_CoroutineScheduler.Clear();
    m_EventBus.Clear();
    m_EntityEventBus.Clear();

    // Disable controller, flower, and gameplay systems
    m_ControllerSystem.SetEnabled(false);
    m_ControllerSystem.SetGameCameraEntity(ECS::INVALID_ENTITY);
    m_FlowerSystem.SetEnabled(false);
    m_HUDSystem.SetEnabled(false);
    m_QuestSystem.SetEnabled(false);
    m_FootstepSystem.SetEnabled(false);
    m_CinematicSystem.SetEnabled(false);
    m_DialogueSystem.Clear();
    m_ObjectPool.DestroyAll(m_World);

    // Re-enable editor camera controller
    if (m_CameraController) {
        m_CameraController->SetEnabled(true);
    }

    // Release mouse
    Input::SetMouseCaptured(false);

    // Restore editor state
    RestoreEditorState();

    m_State = PlayState::Stopped;
    ENJIN_LOG_INFO(Editor, "Exited Play Mode");
}

void PlayMode::Update(f32 deltaTime) {
    // Escape is now handled by EditorLayer (which manages focus mode exit vs stop).

    // Update controller system when playing
    if (m_State == PlayState::Playing) {
        // Physics runs first to update rigidbody positions, then controllers overlay input
        {
            ENJIN_PROFILE_SCOPE("Physics");
            m_Physics.Update(deltaTime);
        }
        {
            ENJIN_PROFILE_SCOPE("ECS");
            m_ControllerSystem.Update(deltaTime);
            m_FlowerSystem.Update(deltaTime);
        }

        // Update scripts (handles hot-reload, lifecycle dispatch, coroutines)
        {
            ENJIN_PROFILE_SCOPE("Scripting");
            m_ScriptSystem.Update(deltaTime);
            m_CoroutineScheduler.EndOfFrame();
        }

        // Gameplay systems
        m_TweenSystem.Update(m_World, deltaTime);
        m_StateMachineSystem.Update(m_World, deltaTime);
        m_VisualScriptSystem.Update(deltaTime);
        m_DialogueSystem.Update(m_World, deltaTime);
        m_CinematicSystem.Update(m_World, m_Camera, deltaTime);
        m_QuestSystem.Update(m_World, deltaTime);
        m_FootstepSystem.Update(m_World, deltaTime);
        m_ObjectPool.Update(m_World, deltaTime);
        m_EntityEventBus.ProcessDeferred();

        // Regenerate resources
        auto resEntities = m_World->GetEntitiesWithComponent<ECS::ResourceComponent>();
        for (auto entity : resEntities) {
            auto* res = m_World->GetComponent<ECS::ResourceComponent>(entity);
            if (res) res->Regenerate(deltaTime);
        }
    }
}

void PlayMode::SaveEditorState() {
    if (!m_World || !m_Camera) {
        return;
    }

    // Save scene to JSON string
    Scene::SceneSerializer serializer(m_World);
    m_SavedSceneJson = serializer.SaveToString();

    // Save camera state
    m_SavedCameraPos = m_Camera->GetPosition();
    m_SavedCameraRot = m_Camera->GetRotation();
    m_SavedCameraFov = m_Camera->GetFOV();

    ENJIN_LOG_DEBUG(Editor, "Saved editor state");
}

void PlayMode::RestoreEditorState() {
    if (!m_World || !m_Camera) {
        return;
    }

    // Restore scene from saved JSON
    if (!m_SavedSceneJson.empty()) {
        Scene::SceneSerializer serializer(m_World);
        serializer.LoadFromString(m_SavedSceneJson, true);
    }

    // Restore camera state
    m_Camera->SetPosition(m_SavedCameraPos);
    m_Camera->SetRotation(m_SavedCameraRot);
    m_Camera->SetPerspective(m_SavedCameraFov, 16.0f / 9.0f, 0.1f, 1000.0f);

    // Sync camera controller
    if (m_CameraController) {
        m_CameraController->SyncFromCamera();
    }

    ENJIN_LOG_DEBUG(Editor, "Restored editor state");
}

} // namespace Editor
} // namespace Enjin
