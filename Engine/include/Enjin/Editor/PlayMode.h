#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/Physics/SimplePhysics.h"
#include <string>

namespace Enjin {
namespace Editor {

// Play mode state
enum class PlayState {
    Stopped,    // Editor mode - editing scene
    Playing,    // Play mode - game running
    Paused      // Play mode but paused
};

// Manages transitioning between editor and play mode
class ENJIN_API PlayMode {
public:
    PlayMode() = default;
    ~PlayMode() = default;

    void Initialize(ECS::World* world, Renderer::Camera* camera,
                    Renderer::CameraController* cameraController);

    // State transitions
    void Play();      // Enter play mode
    void Pause();     // Pause play mode
    void Resume();    // Resume from pause
    void Stop();      // Exit play mode, restore editor state

    // Update (call every frame)
    void Update(f32 deltaTime);

    // State queries
    PlayState GetState() const { return m_State; }
    bool IsPlaying() const { return m_State == PlayState::Playing; }
    bool IsPaused() const { return m_State == PlayState::Paused; }
    bool IsStopped() const { return m_State == PlayState::Stopped; }

    // Get controller system for external configuration
    ECS::ControllerSystem* GetControllerSystem() { return &m_ControllerSystem; }

    // Get physics system
    Physics::SimplePhysics* GetPhysics() { return &m_Physics; }

private:
    void SaveEditorState();
    void RestoreEditorState();

    ECS::World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Renderer::CameraController* m_CameraController = nullptr;

    PlayState m_State = PlayState::Stopped;

    // Controller system for runtime
    ECS::ControllerSystem m_ControllerSystem;

    // Physics system
    Physics::SimplePhysics m_Physics;

    // Saved editor state (to restore when stopping)
    std::string m_SavedSceneJson;
    Math::Vector3 m_SavedCameraPos;
    Math::Quaternion m_SavedCameraRot;
    f32 m_SavedCameraFov = 45.0f;
};

} // namespace Editor
} // namespace Enjin
