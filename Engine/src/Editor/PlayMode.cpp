#include "Enjin/Editor/PlayMode.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"

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

    ENJIN_LOG_INFO(Editor, "PlayMode initialized");
}

void PlayMode::Play() {
    if (m_State == PlayState::Playing) {
        return;
    }

    // Save current editor state
    SaveEditorState();

    // Find the active game camera entity so controllers drive it instead of the editor camera
    ECS::Entity gameCam = ECS::CameraManager::GetActiveCamera(m_World);
    m_ControllerSystem.SetGameCameraEntity(gameCam);

    // Enable controller system and flower system
    m_ControllerSystem.SetEnabled(true);
    m_FlowerSystem.SetEnabled(true);

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
    // Do NOT capture mouse here — only focus mode (F11) captures the mouse.

    m_State = PlayState::Playing;
    ENJIN_LOG_INFO(Editor, "Play Mode Resumed");
}

void PlayMode::Stop() {
    if (m_State == PlayState::Stopped) {
        return;
    }

    // Disable controller and flower systems
    m_ControllerSystem.SetEnabled(false);
    m_ControllerSystem.SetGameCameraEntity(ECS::INVALID_ENTITY);
    m_FlowerSystem.SetEnabled(false);

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
        m_Physics.Update(deltaTime);
        m_ControllerSystem.Update(deltaTime);
        m_FlowerSystem.Update(deltaTime);
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
