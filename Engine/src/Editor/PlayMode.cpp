#include "Enjin/Editor/PlayMode.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Effects/ParticleSystem.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Debug/Profiler.h"
#include "Enjin/ECS/Components/Gameplay.h"

// Extern for visual script node access to save system
extern Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;

namespace Enjin {
namespace Editor {

void PlayMode::Initialize(ECS::World* world, Renderer::Camera* camera,
                          Renderer::CameraController* cameraController,
                          Scene::SceneManager* sceneManager) {
    m_World = world;
    m_Camera = camera;
    m_CameraController = cameraController;

    // Read physics backend preference and project mode from SceneManager
    Physics::PhysicsBackendType backendType = Physics::PhysicsBackendType::Auto;
    Scene::ProjectMode projectMode = Scene::ProjectMode::Mode3D;
    if (sceneManager) {
        backendType = sceneManager->GetPhysicsBackendType();
        projectMode = sceneManager->GetProjectMode();
    }

    m_Physics = Physics::CreatePhysicsBackend(backendType, projectMode);
    if (m_Physics) m_Physics->SetWorld(world);

    m_Physics2D = Physics::CreatePhysicsBackend2D(backendType, projectMode);
    if (m_Physics2D) m_Physics2D->Initialize(world);

    m_ControllerSystem.SetWorld(world);
    m_ControllerSystem.SetCamera(camera);
    m_ControllerSystem.SetPhysics(m_Physics.get());
    m_ControllerSystem.SetPhysics2D(m_Physics2D.get());
    m_ControllerSystem.SetInputActionMap(&m_InputMap);
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

        // Initialize AI system
        m_AISystem.SetWorld(world);

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
    m_FlowerSystem.SetRenderSystem(m_RenderSystem);
    m_FlowerSystem.Reset();
    m_FlowerSystem.SetEnabled(true);
    m_FlowerSystem.SetGameCameraEntity(gameCam);
    ENJIN_LOG_INFO(Editor, "PlayMode: FlowerSystem enabled");
    m_ScriptSystem.SetEnabled(true);
    ENJIN_LOG_INFO(Editor, "PlayMode: ScriptSystem enabled");
    m_HUDSystem.SetEnabled(true);
    m_QuestSystem.SetEnabled(true);
    m_FootstepSystem.SetEnabled(true);
    m_CinematicSystem.SetEnabled(true);
    m_StreamingManager.SetEnabled(true);
    m_AISystem.SetEnabled(true);
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
    Scripting::SetBindingsPhysics2D(m_Physics2D.get());
    Scripting::SetBindingsNetworking(&m_NetworkSystem);
    Scripting::SetBindingsStreaming(&m_StreamingManager);
    Scripting::SetBindingsAudio(&m_SimpleAudio);
    Scripting::SetBindingsDestructible(&m_DestructibleSystem);
    Scripting::SetBindingsFlower(m_World);
    Scripting::SetBindingsProcedural(nullptr);
    Scripting::SetBindingsWeather(m_WeatherSystem);
    Scripting::SetBindingsSceneManager(m_SceneManager);
    Scripting::SetBindingsCoroutineScheduler(&m_CoroutineScheduler);
    Scripting::SetBindingsEventBus(&m_EventBus);
    Scripting::SetBindingsScriptEngine(&m_ScriptEngine);
    Scripting::SetBindingsSubtitles(m_SubtitleSystem);
    Scripting::SetBindingsAnnouncer(m_Announcer);
    Scripting::SetBindingsAccessibilitySettings(m_AccessibilitySettings);
    Scripting::SetBindingsPluginSystem(nullptr);  // No PluginSystem instance yet — null-safe
    Scripting::SetBindingsAudioGraphRuntime(&m_AudioGraphRuntime);
    s_VisualScriptSaveSystem = &m_TieredSaveSystem;
    ENJIN_LOG_INFO(Editor, "PlayMode: Script bindings set");

    // Initialize owned systems
    m_SimpleAudio.Initialize();
    m_SimpleAudio.SetWorld(m_World);
    m_DestructibleSystem.Initialize(m_World);
    m_AudioGraphRuntime.Initialize(&m_SimpleAudio);

    // Wire 2D physics collision callbacks to visual script system
    if (m_Physics2D) {
        m_Physics2D->SetOnCollisionEnter([this](const Physics::Contact2D& c) {
            m_VisualScriptSystem.OnCollisionEnter(c.entityA, c.entityB, 0.0f);
            m_VisualScriptSystem.OnCollisionEnter(c.entityB, c.entityA, 0.0f);
            // Contact damage and pickup processing
            ProcessContactDamage(c.entityA, c.entityB);
            ProcessPickup(c.entityA, c.entityB);
        });
        m_Physics2D->SetOnCollisionExit([this](const Physics::Contact2D& c) {
            m_VisualScriptSystem.OnCollisionExit(c.entityA, c.entityB, 0.0f);
            m_VisualScriptSystem.OnCollisionExit(c.entityB, c.entityA, 0.0f);
        });
        m_Physics2D->SetOnSensorEnter([this](const Physics::Contact2D& c) {
            m_VisualScriptSystem.OnTriggerEnter(c.entityA, c.entityB, 0.0f);
            m_VisualScriptSystem.OnTriggerEnter(c.entityB, c.entityA, 0.0f);
            // Sensors also trigger pickups and damage
            ProcessContactDamage(c.entityA, c.entityB);
            ProcessPickup(c.entityA, c.entityB);
        });
        m_Physics2D->SetOnSensorExit([this](const Physics::Contact2D& c) {
            m_VisualScriptSystem.OnTriggerExit(c.entityA, c.entityB, 0.0f);
            m_VisualScriptSystem.OnTriggerExit(c.entityB, c.entityA, 0.0f);
        });
    }

    // Wire EntityEventBus and SubtitleSystem to DialogueSystem
    m_DialogueSystem.SetEventBus(&m_EntityEventBus);
    m_DialogueSystem.SetSubtitleSystem(m_SubtitleSystem);
    ENJIN_LOG_INFO(Editor, "PlayMode: DialogueSystem integrations wired");
    m_ScriptSystem.InitializeAllScripts();
    ENJIN_LOG_INFO(Editor, "PlayMode: Scripts initialized");

    // Initialize visual script system
    m_VisualScriptSystem.SetPhysics(m_Physics.get());
    m_VisualScriptSystem.SetPhysics2D(m_Physics2D.get());
    m_VisualScriptSystem.SetNetworking(&m_NetworkSystem);
    m_VisualScriptSystem.SetScriptEngine(&m_ScriptEngine);
    m_VisualScriptSystem.SetStreaming(&m_StreamingManager);
    m_VisualScriptSystem.Initialize();
    ENJIN_LOG_INFO(Editor, "PlayMode: VisualScriptSystem initialized");

    // Initialize behavior tree system
    m_BehaviorTreeSystem.Initialize();
    ENJIN_LOG_INFO(Editor, "PlayMode: BehaviorTreeSystem initialized");

    // AI system is already set up (world set in Initialize) — just enabled above
    ENJIN_LOG_INFO(Editor, "PlayMode: AISystem enabled");

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
        m_RenderSystem->SetSkipMainPassRendering(true);
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
    m_StreamingManager.SetEnabled(false);
    m_AISystem.SetEnabled(false);
    // NOTE: TweenSystem, VisualScriptSystem, BehaviorTreeSystem, and DialogueSystem
    // do not have SetEnabled()/SetPaused() methods. They are effectively paused because
    // PlayMode::Update() only runs the gameplay block when m_State == PlayState::Playing.
    // TODO: If any of these systems gain background processing or timers, add pause here.
    // NetworkSystem intentionally keeps running during pause (lobby/connection maintenance).
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
    m_StreamingManager.SetEnabled(true);
    m_AISystem.SetEnabled(true);
    // Do NOT capture mouse here — only focus mode (F11) captures the mouse.

    m_State = PlayState::Playing;
    ENJIN_LOG_INFO(Editor, "Play Mode Resumed");
}

void PlayMode::Stop() {
    if (m_State == PlayState::Stopped) {
        return;
    }

    // Destroy pooled objects before shutting down scripts (scripts may reference pooled entities)
    m_ObjectPool.DestroyAll(m_World);

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

    // Shutdown owned runtime systems
    m_AudioGraphRuntime.Shutdown();
    m_SimpleAudio.Shutdown();
    m_DestructibleSystem.Shutdown();

    // Clear 2D physics callbacks
    if (m_Physics2D) {
        m_Physics2D->SetOnCollisionEnter(nullptr);
        m_Physics2D->SetOnCollisionExit(nullptr);
        m_Physics2D->SetOnSensorEnter(nullptr);
        m_Physics2D->SetOnSensorExit(nullptr);
    }

    // Clear save/gameplay system bindings
    s_VisualScriptSaveSystem = nullptr;
    Scripting::SetBindingsSubtitles(nullptr);
    Scripting::SetBindingsAnnouncer(nullptr);
    Scripting::SetBindingsAccessibilitySettings(nullptr);
    Scripting::SetBindingsSaveSystem(nullptr);
    Scripting::SetBindingsQuestSystem(nullptr);
    Scripting::SetBindingsCinematicSystem(nullptr);
    Scripting::SetBindingsObjectPool(nullptr);
    Scripting::SetBindingsPhysics(nullptr);
    Scripting::SetBindingsPhysics2D(nullptr);
    Scripting::SetBindingsNetworking(nullptr);
    Scripting::SetBindingsWeather(nullptr);
    Scripting::SetBindingsSceneManager(nullptr);
    Scripting::SetBindingsStreaming(nullptr);
    Scripting::SetBindingsAudio(nullptr);
    Scripting::SetBindingsDestructible(nullptr);
    Scripting::SetBindingsFlower(nullptr);
    Scripting::SetBindingsProcedural(nullptr);
    Scripting::SetBindingsPluginSystem(nullptr);
    Scripting::SetBindingsAudioGraphRuntime(nullptr);

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
    m_AISystem.SetEnabled(false);

    // Re-enable editor camera controller
    if (m_CameraController) {
        m_CameraController->SetEnabled(true);
    }

    // Release mouse
    Input::SetMouseCaptured(false);

    // Keep the scene as-is after stopping (no restore — play mode changes persist).
    // Compute diff so the user can see what changed during play.
    {
        Scene::SceneSerializer serializer(m_World);
        if (m_RenderSystem) {
            serializer.SetSkyboxConfig(m_RenderSystem->GetSkyboxConfig());
        }
        m_PlayedSceneJson = serializer.SaveToString();

        m_PlayModeDiff = ComputePlayModeDiff(m_SavedSceneJson, m_PlayedSceneJson);
        m_ShowDiffDialog = m_PlayModeDiff.HasChanges();
    }

    // Restore only camera state (so editor camera returns to its pre-play position)
    if (m_Camera) {
        m_Camera->SetPosition(m_SavedCameraPos);
        m_Camera->SetRotation(m_SavedCameraRot);
        m_Camera->SetPerspective(m_SavedCameraFov, 16.0f / 9.0f, 0.1f, 1000.0f);
    }

    // Re-enable main-pass shadow rendering for editor mode
    if (m_RenderSystem) {
        m_RenderSystem->SetSkipMainPassShadows(false);
        m_RenderSystem->SetSkipMainPassRendering(false);
    }

    m_State = PlayState::Stopped;
    ENJIN_LOG_INFO(Editor, "Exited Play Mode");
}

void PlayMode::Update(f32 deltaTime) {
    // Escape is now handled by EditorLayer (which manages focus mode exit vs stop).

    // Update controller system when playing
    if (m_State == PlayState::Playing) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        // Update input action map (polls input state for remappable actions)
        m_InputMap.Update(deltaTime);

        // Physics runs first to update rigidbody positions, then controllers overlay input
        auto t0 = std::chrono::high_resolution_clock::now();
        {
            ENJIN_PROFILE_SCOPE("Physics");
            if (m_Physics) m_Physics->Update(deltaTime);
            if (m_Physics2D) m_Physics2D->Update(deltaTime);
        }

        // Dispatch collision events to visual scripts
        if (m_Physics) {
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
        } // if m_Physics

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
        m_AISystem.Update(deltaTime);
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
        m_DestructibleSystem.Update(deltaTime);
        m_EntityEventBus.ProcessDeferred();

        // Weather (needs camera position for particle spawning around player)
        if (m_WeatherSystem && m_Camera) {
            m_WeatherSystem->Update(deltaTime, m_Camera->GetPosition());
        }

        // Particles
        if (m_ParticleSystem) {
            m_ParticleSystem->Update(deltaTime, m_World);
        }

        // Audio
        m_SimpleAudio.Update(deltaTime);
        m_SimpleAudio.UpdateAudioSources(deltaTime);
        m_AudioGraphRuntime.Update(deltaTime);

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

        // Health system (regen, invulnerability timers, death)
        UpdateHealthSystems(deltaTime);

        // Flush deferred entity destroys (from damage/pickup callbacks)
        FlushDeferredDestroys();

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

    usize entityCount = m_World->GetEntityCount();
    ENJIN_LOG_DEBUG(Editor, "Saved editor state (%zu entities, %zu bytes JSON)", entityCount, m_SavedSceneJson.size());
}

// Kept for potential future "restore scene on stop" feature
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

        usize entityCount = m_World->GetEntityCount();
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

// ============================================================================
// Gameplay Processing — Contact Damage, Pickups, Health
// ============================================================================

void PlayMode::ProcessContactDamage(ECS::Entity entityA, ECS::Entity entityB) {
    if (!m_World) return;

    // Check both orderings: A damages B, or B damages A
    auto tryDamage = [this](ECS::Entity damager, ECS::Entity target) {
        auto* dmg = m_World->GetComponent<ECS::DamageComponent>(damager);
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(target);
        if (!dmg || !hp || hp->isDead) return;

        // Check damageOnce — skip if already damaged this entity
        if (dmg->damageOnce) {
            for (auto e : dmg->damagedEntities) {
                if (e == target) return;
            }
            dmg->damagedEntities.push_back(target);
        }

        // Check invulnerability
        if (hp->isInvulnerable || hp->invulnerabilityTimer > 0.0f) return;

        // Apply damage (shield absorbs first)
        f32 remaining = dmg->damage;
        if (hp->currentShield > 0.0f) {
            f32 absorbed = Math::Min(remaining, hp->currentShield);
            hp->currentShield -= absorbed;
            remaining -= absorbed;
        }
        hp->currentHealth -= remaining;
        hp->timeSinceLastDamage = 0.0f;

        // Start invulnerability window
        if (hp->invulnerabilityTime > 0.0f) {
            hp->invulnerabilityTimer = hp->invulnerabilityTime;
        }

        // Knockback (apply to character controller velocity if present)
        if (dmg->knockbackForce > 0.0f) {
            auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(target);
            if (ctrl) {
                auto* dmgT = m_World->GetComponent<ECS::TransformComponent>(damager);
                auto* tgtT = m_World->GetComponent<ECS::TransformComponent>(target);
                if (dmgT && tgtT) {
                    f32 dir = (tgtT->position.x > dmgT->position.x) ? 1.0f : -1.0f;
                    ctrl->velocity.x = dir * dmg->knockbackForce;
                    ctrl->velocity.y = dmg->knockbackForce * 0.5f;
                    ctrl->isGrounded = false;
                }
            }
        }

        // Check death
        if (hp->currentHealth <= 0.0f) {
            hp->currentHealth = 0.0f;
            hp->isDead = true;
        }

        // Destroy damager if configured
        if (dmg->destroyOnHit) {
            m_DeferredDestroys.push_back(damager);
        }
    };

    tryDamage(entityA, entityB);
    tryDamage(entityB, entityA);
}

void PlayMode::ProcessPickup(ECS::Entity entityA, ECS::Entity entityB) {
    if (!m_World) return;

    auto tryPickup = [this](ECS::Entity pickupEntity, ECS::Entity collector) {
        auto* pickup = m_World->GetComponent<ECS::PickupComponent>(pickupEntity);
        if (!pickup || pickup->isCollected) return;

        // Only characters with a controller or health can collect pickups
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(collector);
        bool isPlayer = m_World->GetComponent<ECS::Platformer2DController>(collector) != nullptr ||
                        m_World->GetComponent<ECS::TopDown2DController>(collector) != nullptr ||
                        m_World->GetComponent<ECS::FirstPersonController>(collector) != nullptr ||
                        m_World->GetComponent<ECS::ThirdPersonController>(collector) != nullptr;
        if (!isPlayer) return;

        // Apply pickup effect
        switch (pickup->type) {
            case ECS::PickupComponent::PickupType::Health:
                if (hp) {
                    hp->currentHealth = Math::Min(hp->currentHealth + pickup->value, hp->maxHealth);
                }
                break;
            case ECS::PickupComponent::PickupType::Coin:
            case ECS::PickupComponent::PickupType::Ammo:
            case ECS::PickupComponent::PickupType::Key:
            case ECS::PickupComponent::PickupType::Powerup:
            case ECS::PickupComponent::PickupType::Custom:
                // These are handled by scripts/visual scripts via OnTriggerEnter
                // For now, mark as collected so it disappears
                break;
        }

        pickup->isCollected = true;

        if (pickup->destroyOnPickup && !pickup->canRespawn) {
            m_DeferredDestroys.push_back(pickupEntity);
        } else if (pickup->destroyOnPickup) {
            // Hide but keep for respawn — make invisible
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(pickupEntity);
            if (transform) transform->visible = false;
            pickup->respawnTimer = pickup->respawnTime;
        }
    };

    tryPickup(entityA, entityB);
    tryPickup(entityB, entityA);
}

void PlayMode::UpdateHealthSystems(f32 deltaTime) {
    if (!m_World) return;

    for (auto entity : m_World->GetEntitiesWithComponent<ECS::HealthComponent>()) {
        auto* hp = m_World->GetComponent<ECS::HealthComponent>(entity);
        if (!hp) continue;

        // Update invulnerability timer
        if (hp->invulnerabilityTimer > 0.0f) {
            hp->invulnerabilityTimer -= deltaTime;
        }

        // Track time since last damage (for regen delay)
        hp->timeSinceLastDamage += deltaTime;

        // Health regeneration
        if (!hp->isDead && hp->regenRate > 0.0f && hp->timeSinceLastDamage >= hp->regenDelay) {
            hp->currentHealth = Math::Min(hp->currentHealth + hp->regenRate * deltaTime, hp->maxHealth);
        }

        // Shield regeneration
        if (!hp->isDead && hp->shieldRegenRate > 0.0f && hp->timeSinceLastDamage >= hp->shieldRegenDelay) {
            hp->currentShield = Math::Min(hp->currentShield + hp->shieldRegenRate * deltaTime, hp->maxShield);
        }

        // Death handling — destroy non-player entities, respawn players
        if (hp->isDead) {
            auto* ctrl = m_World->GetComponent<ECS::Platformer2DController>(entity);
            if (ctrl) {
                // Player death: respawn at Y=2 above origin
                hp->isDead = false;
                hp->currentHealth = hp->maxHealth;
                hp->currentShield = hp->maxShield;
                hp->invulnerabilityTimer = 1.0f;  // Brief invulnerability after respawn
                auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
                if (transform) {
                    transform->position = Math::Vector3(0.0f, 2.0f, 0.0f);
                }
                ctrl->velocity = Math::Vector3(0.0f);
                ctrl->isGrounded = false;
            } else {
                // Non-player death: destroy entity
                m_DeferredDestroys.push_back(entity);
            }
        }
    }

    // Pickup respawn
    for (auto entity : m_World->GetEntitiesWithComponent<ECS::PickupComponent>()) {
        auto* pickup = m_World->GetComponent<ECS::PickupComponent>(entity);
        if (!pickup || !pickup->isCollected || !pickup->canRespawn) continue;

        pickup->respawnTimer -= deltaTime;
        if (pickup->respawnTimer <= 0.0f) {
            pickup->isCollected = false;
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (transform) transform->visible = true;
        }
    }
}

void PlayMode::FlushDeferredDestroys() {
    if (!m_World || m_DeferredDestroys.empty()) return;

    for (auto entity : m_DeferredDestroys) {
        // Verify entity still exists before destroying
        if (m_World->GetComponent<ECS::TransformComponent>(entity)) {
            m_World->DestroyEntity(entity);
        }
    }
    m_DeferredDestroys.clear();
}

} // namespace Editor
} // namespace Enjin
