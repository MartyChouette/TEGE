#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/ECS/Systems/FlowerSystem.h"
#include "Enjin/ECS/Systems/TweenSystem.h"
#include "Enjin/ECS/Systems/StateMachineSystem.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/ECS/Systems/BehaviorTreeSystem.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Physics/SimplePhysics.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Gameplay/HUDSystem.h"
#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/FootstepSystem.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Scene/LevelStreaming.h"
#include <string>

namespace Enjin {
namespace ECS { class RenderSystem; }
namespace Renderer { class PostProcessing; }
namespace Accessibility { class SubtitleSystem; }
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

    // Get flower system
    ECS::FlowerSystem* GetFlowerSystem() { return &m_FlowerSystem; }

    // Get physics system
    Physics::SimplePhysics* GetPhysics() { return &m_Physics; }

    // Get script systems
    Scripting::ScriptEngine* GetScriptEngine() { return &m_ScriptEngine; }
    Scripting::ScriptSystem* GetScriptSystem() { return &m_ScriptSystem; }
    Scripting::CoroutineScheduler* GetCoroutineScheduler() { return &m_CoroutineScheduler; }
    Scripting::ScriptEventBus* GetEventBus() { return &m_EventBus; }

    // Get gameplay systems
    ECS::EntityEventBus* GetEntityEventBus() { return &m_EntityEventBus; }
    Gameplay::HUDSystem* GetHUDSystem() { return &m_HUDSystem; }
    Gameplay::QuestSystem* GetQuestSystem() { return &m_QuestSystem; }
    Gameplay::FootstepSystem* GetFootstepSystem() { return &m_FootstepSystem; }
    Gameplay::ObjectPool* GetObjectPool() { return &m_ObjectPool; }
    Gameplay::CinematicSystem* GetCinematicSystem() { return &m_CinematicSystem; }
    Gameplay::TieredSaveSystem* GetTieredSaveSystem() { return &m_TieredSaveSystem; }
    ECS::TweenSystem* GetTweenSystem() { return &m_TweenSystem; }
    ECS::StateMachineSystem* GetStateMachineSystem() { return &m_StateMachineSystem; }
    ECS::DialogueSystem* GetDialogueSystem() { return &m_DialogueSystem; }
    ECS::VisualScriptSystem* GetVisualScriptSystem() { return &m_VisualScriptSystem; }
    ECS::BehaviorTreeSystem* GetBehaviorTreeSystem() { return &m_BehaviorTreeSystem; }

    Networking::NetworkSystem* GetNetworkSystem() { return &m_NetworkSystem; }
    Scene::StreamingManager* GetStreamingManager() { return &m_StreamingManager; }

    void SetRenderSystem(ECS::RenderSystem* rs) { m_RenderSystem = rs; }
    void SetPostProcessing(Renderer::PostProcessing* pp) { m_PostProcessing = pp; }
    void SetSubtitleSystem(Accessibility::SubtitleSystem* subs) { m_SubtitleSystem = subs; }

    // Play mode diff dialog
    bool HasPendingDiff() const { return m_ShowDiffDialog; }
    PlayModeDiff& GetDiff() { return m_PlayModeDiff; }
    void DismissDiff() { m_ShowDiffDialog = false; }
    const std::string& GetPlayedSceneJson() const { return m_PlayedSceneJson; }

private:
    void SaveEditorState();
    void RestoreEditorState();

    ECS::World* m_World = nullptr;
    Renderer::Camera* m_Camera = nullptr;
    Renderer::CameraController* m_CameraController = nullptr;

    PlayState m_State = PlayState::Stopped;

    // Controller system for runtime
    ECS::ControllerSystem m_ControllerSystem;

    // Flower system for runtime
    ECS::FlowerSystem m_FlowerSystem;

    // Physics system
    Physics::SimplePhysics m_Physics;

    // Scripting
    Scripting::ScriptEngine m_ScriptEngine;
    Scripting::ScriptSystem m_ScriptSystem;
    Scripting::CoroutineScheduler m_CoroutineScheduler;
    Scripting::ScriptEventBus m_EventBus;

    // C++ entity event bus
    ECS::EntityEventBus m_EntityEventBus;

    // Gameplay systems
    Gameplay::HUDSystem m_HUDSystem;
    Gameplay::QuestSystem m_QuestSystem;
    Gameplay::FootstepSystem m_FootstepSystem;
    Gameplay::ObjectPool m_ObjectPool;
    Gameplay::CinematicSystem m_CinematicSystem;

    // Tween system
    ECS::TweenSystem m_TweenSystem;

    // State machine system
    ECS::StateMachineSystem m_StateMachineSystem;

    // Dialogue system
    ECS::DialogueSystem m_DialogueSystem;

    // Visual script system
    ECS::VisualScriptSystem m_VisualScriptSystem;

    // Behavior tree system
    ECS::BehaviorTreeSystem m_BehaviorTreeSystem;

    // Network system
    Networking::NetworkSystem m_NetworkSystem;

    // Render system pointers (owned externally)
    ECS::RenderSystem* m_RenderSystem = nullptr;
    Renderer::PostProcessing* m_PostProcessing = nullptr;
    Accessibility::SubtitleSystem* m_SubtitleSystem = nullptr;

    // Level streaming
    Scene::StreamingManager m_StreamingManager;

    // Tiered save system
    Gameplay::TieredSaveSystem m_TieredSaveSystem;

    // Saved editor state (to restore when stopping)
    std::string m_SavedSceneJson;
    Math::Vector3 m_SavedCameraPos;
    Math::Quaternion m_SavedCameraRot;
    f32 m_SavedCameraFov = 45.0f;

    // Play mode diff
    PlayModeDiff m_PlayModeDiff;
    bool m_ShowDiffDialog = false;
    std::string m_PlayedSceneJson;  // Scene JSON captured at stop, before restore
};

} // namespace Editor
} // namespace Enjin
