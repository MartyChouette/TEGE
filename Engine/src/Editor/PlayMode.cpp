#include "Enjin/Editor/PlayMode.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Debug/Profiler.h"

// Extern for visual script node access to save system
extern Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;

namespace Enjin {
namespace Editor {

void PlayMode::Initialize(ECS::World* world, Renderer::Camera* camera,
                          Renderer::CameraController* cameraController) {
    m_World = world;
    m_Camera = camera;
    m_CameraController = cameraController;

    m_Physics = Physics::CreatePhysicsBackend();
    m_Physics->SetWorld(world);

    m_Physics2D = Physics::CreatePhysicsBackend2D();

    m_ControllerSystem.SetWorld(world);
    m_ControllerSystem.SetCamera(camera);
    m_ControllerSystem.SetPhysics(m_Physics.get());
    m_ControllerSystem.SetEnabled(false);

    m_FlowerSystem.SetWorld(world);
    m_FlowerSystem.SetCamera(camera);
    m_FlowerSystem.SetEnabled(false);

    // Initialize level streaming
    m_StreamingManager.SetWorld(world);
    m_StreamingManager.SetEnabled(false);

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

        // Initialize behavior tree system
        m_BehaviorTreeSystem.SetWorld(world);

        // Initialize network system
        m_NetworkSystem.SetWorld(world);

        ENJIN_LOG_INFO(Editor, "Script engine initialized");
    } else {
        ENJIN_LOG_WARN(Editor, "Failed to initialize script engine");
    }

    // Initialize tiered save system — load meta-progression on startup
    m_TieredSaveSystem.LoadMeta();

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
    m_StreamingManager.SetEnabled(true);
    ENJIN_LOG_INFO(Editor, "PlayMode: Gameplay systems enabled");
    m_TweenSystem.PlayAll(m_World);
    Scripting::SetBindingsWorld(m_World);
    Scripting::SetBindingsDialogueSystem(&m_DialogueSystem);
    Scripting::SetBindingsRenderSystem(m_RenderSystem);
    Scripting::SetBindingsPostProcessing(m_PostProcessing);
    Scripting::SetBindingsSaveSystem(&m_TieredSaveSystem);
    Scripting::SetBindingsQuestSystem(&m_QuestSystem);
    Scripting::SetBindingsCinematicSystem(&m_CinematicSystem);
    Scripting::SetBindingsObjectPool(&m_ObjectPool);
    Scripting::SetBindingsPhysics(m_Physics.get());
    Scripting::SetBindingsStreaming(&m_StreamingManager);
    s_VisualScriptSaveSystem = &m_TieredSaveSystem;
    ENJIN_LOG_INFO(Editor, "PlayMode: Script bindings set");

    // Wire EntityEventBus and SubtitleSystem to DialogueSystem
    m_DialogueSystem.SetEventBus(&m_EntityEventBus);
    m_DialogueSystem.SetSubtitleSystem(m_SubtitleSystem);
    ENJIN_LOG_INFO(Editor, "PlayMode: DialogueSystem integrations wired");
    m_ScriptSystem.InitializeAllScripts();
    ENJIN_LOG_INFO(Editor, "PlayMode: Scripts initialized");

    // Initialize visual script system
    m_VisualScriptSystem.SetPhysics(m_Physics.get());
    m_VisualScriptSystem.SetScriptEngine(&m_ScriptEngine);
    m_VisualScriptSystem.Initialize();
    ENJIN_LOG_INFO(Editor, "PlayMode: VisualScriptSystem initialized");

    // Initialize behavior tree system
    m_BehaviorTreeSystem.Initialize();
    ENJIN_LOG_INFO(Editor, "PlayMode: BehaviorTreeSystem initialized");

    // Enable network system if connected
    if (m_NetworkSystem.IsConnected()) {
        m_NetworkSystem.SetEnabled(true);
        ENJIN_LOG_INFO(Editor, "PlayMode: NetworkSystem enabled");
    }

    // Initialize quest flow components
    {
        auto qfEntities = m_World->GetEntitiesWithComponent<ECS::QuestFlowComponent>();
        for (auto entity : qfEntities) {
            auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(entity);
            if (qf) qf->ResetRuntimeState();
        }
        ENJIN_LOG_INFO(Editor, "PlayMode: QuestFlow initialized (%zu quests)", qfEntities.size());
    }

    // Disable editor camera controller
    if (m_CameraController) {
        m_CameraController->SetEnabled(false);
    }

    // Do NOT capture mouse here — only focus mode (F11) captures the mouse.
    // This lets the user keep a visible cursor in the editor while play mode runs.

    // Skip main-pass shadow rendering during play mode — the game view already
    // runs its own shadow pass via RenderShadowPassForCamera()
    if (m_RenderSystem) {
        m_RenderSystem->SetSkipMainPassShadows(true);
    }

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

    // Shutdown quest flows, behavior trees, visual scripts, and scripts first (before scene restore destroys entities)
    {
        auto qfEntities = m_World->GetEntitiesWithComponent<ECS::QuestFlowComponent>();
        for (auto entity : qfEntities) {
            auto* qf = m_World->GetComponent<ECS::QuestFlowComponent>(entity);
            if (qf) qf->ResetRuntimeState();
        }
    }
    m_BehaviorTreeSystem.Shutdown();
    m_VisualScriptSystem.Shutdown();
    m_ScriptSystem.ShutdownAllScripts();
    m_ScriptSystem.SetEnabled(false);
    m_CoroutineScheduler.Clear();
    m_EventBus.Clear();
    m_EntityEventBus.Clear();

    // Clear save/gameplay system bindings
    s_VisualScriptSaveSystem = nullptr;
    Scripting::SetBindingsSaveSystem(nullptr);
    Scripting::SetBindingsQuestSystem(nullptr);
    Scripting::SetBindingsCinematicSystem(nullptr);
    Scripting::SetBindingsObjectPool(nullptr);
    Scripting::SetBindingsPhysics(nullptr);
    Scripting::SetBindingsWeather(nullptr);
    Scripting::SetBindingsStreaming(nullptr);

    // Disable network system (but don't disconnect — lobby persists)
    m_NetworkSystem.SetEnabled(false);

    // Disable controller, flower, and gameplay systems
    m_ControllerSystem.SetEnabled(false);
    m_ControllerSystem.SetGameCameraEntity(ECS::INVALID_ENTITY);
    m_FlowerSystem.SetEnabled(false);
    m_HUDSystem.SetEnabled(false);
    m_QuestSystem.SetEnabled(false);
    m_FootstepSystem.SetEnabled(false);
    m_CinematicSystem.SetEnabled(false);
    m_StreamingManager.SetEnabled(false);
    m_StreamingManager.ClearChunks();
    m_DialogueSystem.Clear();
    m_ObjectPool.DestroyAll(m_World);

    // Re-enable editor camera controller
    if (m_CameraController) {
        m_CameraController->SetEnabled(true);
    }

    // Release mouse
    Input::SetMouseCaptured(false);

    // Capture current (played) scene state before restoring
    {
        Scene::SceneSerializer serializer(m_World);
        if (m_RenderSystem) {
            serializer.SetSkyboxConfig(m_RenderSystem->GetSkyboxConfig());
        }
        m_PlayedSceneJson = serializer.SaveToString();

        // Compute diff between saved and played states
        m_PlayModeDiff = ComputePlayModeDiff(m_SavedSceneJson, m_PlayedSceneJson);
        m_ShowDiffDialog = m_PlayModeDiff.HasChanges();
    }

    // Restore editor state
    RestoreEditorState();

    // Re-enable main-pass shadow rendering for editor mode
    if (m_RenderSystem) {
        m_RenderSystem->SetSkipMainPassShadows(false);
    }

    m_State = PlayState::Stopped;
    ENJIN_LOG_INFO(Editor, "Exited Play Mode");
}

void PlayMode::Update(f32 deltaTime) {
    // Escape is now handled by EditorLayer (which manages focus mode exit vs stop).

    // Update controller system when playing
    if (m_State == PlayState::Playing) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        // Physics runs first to update rigidbody positions, then controllers overlay input
        auto t0 = std::chrono::high_resolution_clock::now();
        {
            ENJIN_PROFILE_SCOPE("Physics");
            m_Physics->Update(deltaTime);
        }

        // Dispatch collision events to visual scripts
        {
            const auto& collisionEvents = m_Physics->GetPendingCollisionEvents();
            for (const auto& evt : collisionEvents) {
                if (evt.isTrigger) {
                    if (evt.type == Physics::CollisionEvent::Type::Enter) {
                        m_VisualScriptSystem.OnTriggerEnter(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnTriggerEnter(evt.entityB, evt.entityA, deltaTime);
                    } else {
                        m_VisualScriptSystem.OnTriggerExit(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnTriggerExit(evt.entityB, evt.entityA, deltaTime);
                    }
                } else {
                    if (evt.type == Physics::CollisionEvent::Type::Enter) {
                        m_VisualScriptSystem.OnCollisionEnter(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnCollisionEnter(evt.entityB, evt.entityA, deltaTime);
                    } else {
                        m_VisualScriptSystem.OnCollisionExit(evt.entityA, evt.entityB, deltaTime);
                        m_VisualScriptSystem.OnCollisionExit(evt.entityB, evt.entityA, deltaTime);
                    }
                }
            }
            m_Physics->ClearPendingCollisionEvents();
        }
        auto t1 = std::chrono::high_resolution_clock::now();

        {
            ENJIN_PROFILE_SCOPE("ECS");
            m_ControllerSystem.Update(deltaTime);
            m_FlowerSystem.Update(deltaTime);
        }
        auto t2 = std::chrono::high_resolution_clock::now();

        // Update scripts (handles hot-reload, lifecycle dispatch, coroutines)
        {
            ENJIN_PROFILE_SCOPE("Scripting");
            m_ScriptSystem.Update(deltaTime);
            m_CoroutineScheduler.EndOfFrame();
        }
        auto t3 = std::chrono::high_resolution_clock::now();

        // Gameplay systems
        m_TweenSystem.Update(m_World, deltaTime);
        m_StateMachineSystem.Update(m_World, deltaTime);
        m_VisualScriptSystem.Update(deltaTime);
        m_BehaviorTreeSystem.Update(deltaTime);
        m_DialogueSystem.Update(m_World, deltaTime);
        m_CinematicSystem.Update(m_World, m_Camera, deltaTime);
        m_QuestSystem.Update(m_World, deltaTime);

        // Quest flow graphs
        {
            for (auto entity : m_World->GetEntitiesWithComponent<ECS::QuestFlowComponent>()) {
                Gameplay::AdvanceQuestFlow(m_World, entity, deltaTime);
            }
        }

        m_FootstepSystem.Update(m_World, deltaTime);
        m_ObjectPool.Update(m_World, deltaTime);
        m_EntityEventBus.ProcessDeferred();

        // Networking
        {
            ENJIN_PROFILE_SCOPE("Networking");
            m_NetworkSystem.Update(deltaTime);
        }

        // Tiered save system (auto-save timer, play time tracking)
        m_TieredSaveSystem.Update(deltaTime, m_World, m_TieredSaveSystem.GetCurrentScene());

        auto t4 = std::chrono::high_resolution_clock::now();

        // Accumulate frame timing for periodic profiling output
        auto toMs = [](auto a, auto b) { return std::chrono::duration<f32, std::milli>(b - a).count(); };
        m_ProfileAccumPhysics   += toMs(t0, t1);
        m_ProfileAccumECS       += toMs(t1, t2);
        m_ProfileAccumScripting += toMs(t2, t3);
        m_ProfileAccumGameplay  += toMs(t3, t4);
        m_ProfileAccumTotal     += toMs(frameStart, t4);
        m_ProfileFrameCount++;

        // Log every 120 frames
        if (m_ProfileFrameCount >= 120) {
            f32 n = static_cast<f32>(m_ProfileFrameCount);
            ENJIN_LOG_INFO(Editor,
                "PlayMode Update avg (%.0f frames): Total=%.2fms  Physics=%.2fms  ECS=%.2fms  Script=%.2fms  Gameplay=%.2fms",
                n,
                m_ProfileAccumTotal / n,
                m_ProfileAccumPhysics / n,
                m_ProfileAccumECS / n,
                m_ProfileAccumScripting / n,
                m_ProfileAccumGameplay / n);
            m_ProfileAccumPhysics = m_ProfileAccumECS = m_ProfileAccumScripting = m_ProfileAccumGameplay = m_ProfileAccumTotal = 0.0f;
            m_ProfileFrameCount = 0;
        }

        // Update level streaming
        if (m_Camera) {
            m_StreamingManager.Update(m_Camera->GetPosition(), deltaTime);
        }

        // Regenerate resources
        for (auto entity : m_World->GetEntitiesWithComponent<ECS::ResourceComponent>()) {
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
    if (m_RenderSystem) {
        serializer.SetSkyboxConfig(m_RenderSystem->GetSkyboxConfig());
    }
    m_SavedSceneJson = serializer.SaveToString();

    // Save camera state
    m_SavedCameraPos = m_Camera->GetPosition();
    m_SavedCameraRot = m_Camera->GetRotation();
    m_SavedCameraFov = m_Camera->GetFOV();

    usize entityCount = m_World->GetAllEntities().size();
    ENJIN_LOG_DEBUG(Editor, "Saved editor state (%zu entities, %zu bytes JSON)", entityCount, m_SavedSceneJson.size());
}

void PlayMode::RestoreEditorState() {
    if (!m_World || !m_Camera) {
        return;
    }

    // Restore scene from saved JSON
    if (!m_SavedSceneJson.empty()) {
        Scene::SceneSerializer serializer(m_World);
        auto result = serializer.LoadFromString(m_SavedSceneJson, true);
        if (!result.success) {
            ENJIN_LOG_ERROR(Editor, "Failed to restore editor state: %s", result.error.c_str());
        }

        // Apply skybox config that was saved with the scene
        if (m_RenderSystem) {
            m_RenderSystem->SetSkybox(serializer.GetSkyboxConfig());
        }

        usize entityCount = m_World->GetAllEntities().size();
        ENJIN_LOG_DEBUG(Editor, "Restored editor state (%zu entities)", entityCount);
    } else {
        ENJIN_LOG_WARN(Editor, "No saved scene JSON to restore — editor state was empty");
    }

    // Restore camera state
    m_Camera->SetPosition(m_SavedCameraPos);
    m_Camera->SetRotation(m_SavedCameraRot);
    m_Camera->SetPerspective(m_SavedCameraFov, 16.0f / 9.0f, 0.1f, 1000.0f);

    // Sync camera controller
    if (m_CameraController) {
        m_CameraController->SyncFromCamera();
    }
}

} // namespace Editor
} // namespace Enjin
