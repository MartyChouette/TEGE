#include "Enjin/Editor/PlayMode.h"
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

    ENJIN_LOG_INFO(Editor, "PlayMode initialized");
}

void PlayMode::Play() {
    if (m_State == PlayState::Playing) {
        return;
    }

    // Save current editor state
    SaveEditorState();

    // Enable controller system
    m_ControllerSystem.SetEnabled(true);

    // Disable editor camera controller
    if (m_CameraController) {
        m_CameraController->SetEnabled(false);
    }

    // Capture mouse for gameplay
    Input::SetMouseCaptured(true);

    m_State = PlayState::Playing;
    ENJIN_LOG_INFO(Editor, "Entered Play Mode");
}

void PlayMode::Pause() {
    if (m_State != PlayState::Playing) {
        return;
    }

    m_ControllerSystem.SetEnabled(false);
    Input::SetMouseCaptured(false);

    m_State = PlayState::Paused;
    ENJIN_LOG_INFO(Editor, "Play Mode Paused");
}

void PlayMode::Resume() {
    if (m_State != PlayState::Paused) {
        return;
    }

    m_ControllerSystem.SetEnabled(true);
    Input::SetMouseCaptured(true);

    m_State = PlayState::Playing;
    ENJIN_LOG_INFO(Editor, "Play Mode Resumed");
}

void PlayMode::Stop() {
    if (m_State == PlayState::Stopped) {
        return;
    }

    // Disable controller system
    m_ControllerSystem.SetEnabled(false);

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
    // Handle Escape key to stop/pause
    if (Input::IsKeyPressed(KeyCode::Escape)) {
        if (m_State == PlayState::Playing) {
            Stop();
            return;
        }
    }

    // Update controller system when playing
    if (m_State == PlayState::Playing) {
        // Physics runs first to update rigidbody positions, then controllers overlay input
        m_Physics.Update(deltaTime);
        m_ControllerSystem.Update(deltaTime);
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
