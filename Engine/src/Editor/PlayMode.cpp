#include "Enjin/Editor/PlayMode.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Effects/ParticleSystem.h"
#include "Enjin/Effects/Water.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Scene/SceneManager.h"
#include "Enjin/Platform/Input.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/FlashAPIShim.h"
#include "Enjin/Debug/Profiler.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Accessibility/AlternativeInput.h"
#include "Enjin/Renderer/PostProcessing.h"
#include "Enjin/Accessibility/AudioVisualIndicator.h"
#include "Enjin/Accessibility/ContentWarning.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/Effects/FluidSimulation.h"
#include "Enjin/Effects/FluidTerrainCoupling.h"
#include "Enjin/Effects/CurlNoiseSystem.h"
#include "Enjin/Effects/ElementalSystem.h"

// Extern for visual script node access to systems
extern Enjin::Gameplay::TieredSaveSystem* s_VisualScriptSaveSystem;
extern Enjin::Effects::WeatherSystem* s_VisualScriptWeather;
extern Enjin::Effects::Water3D* s_VisualScriptWater;
extern Enjin::Gameplay::HUDSystem* s_VisualScriptHUD;
extern Enjin::Accessibility::SubtitleSystem* s_VisualScriptSubtitleSystem;
extern Enjin::Accessibility::AccessibilityAnnouncer* s_VisualScriptAnnouncer;
extern Enjin::Audio::SimpleAudio* s_VisualScriptAudio;
extern Enjin::Renderer::PostProcessing* s_VisualScriptPostProcessing;
extern Enjin::Editor::AudioEventGraphRuntime* s_VisualScriptAudioGraphRuntime;
extern Enjin::Gameplay::ObjectPool* s_VisualScriptObjectPool;
extern Enjin::Effects::ElementalSystem* s_VisualScriptElemental;

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
    if (m_ScriptEngine.Initialize()) {
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
    m_GameOverReady = false;

    // Save current editor state
    SaveEditorState();
    ENJIN_LOG_INFO(Editor, "PlayMode: SaveEditorState done");

    // Re-create physics backends if project mode changed since Initialize()
    // (e.g., user created a 2D template after editor startup initialized with Mode3D)
    if (m_SceneManager) {
        auto backendType = m_SceneManager->GetPhysicsBackendType();
        auto projectMode = m_SceneManager->GetProjectMode();
        bool need2D = (projectMode == Scene::ProjectMode::Mode2D || projectMode == Scene::ProjectMode::Mixed);
        bool need3D = (projectMode == Scene::ProjectMode::Mode3D || projectMode == Scene::ProjectMode::Mixed);

        if (need2D && !m_Physics2D) {
            m_Physics2D = Physics::CreatePhysicsBackend2D(backendType, projectMode);
            if (m_Physics2D) m_Physics2D->Initialize(m_World);
            m_ControllerSystem.SetPhysics2D(m_Physics2D.get());
            ENJIN_LOG_INFO(Editor, "PlayMode: Created 2D physics backend on Play()");
        }
        if (need3D && !m_Physics) {
            m_Physics = Physics::CreatePhysicsBackend(backendType, projectMode);
            if (m_Physics) m_Physics->SetWorld(m_World);
            m_ControllerSystem.SetPhysics(m_Physics.get());
            ENJIN_LOG_INFO(Editor, "PlayMode: Created 3D physics backend on Play()");
        }
    }

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
    Scripting::SetFlashShimSaveSystem(&m_TieredSaveSystem);
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
    m_MIDIInput.Initialize();
    Scripting::SetBindingsMIDI(&m_MIDIInput);
    Scripting::SetBindingsInputActionMap(&m_InputMap);
    s_VisualScriptSaveSystem = &m_TieredSaveSystem;
    s_VisualScriptWeather = m_WeatherSystem;
    s_VisualScriptElemental = m_ElementalSystem;
    Scripting::SetBindingsElemental(m_ElementalSystem);
    s_VisualScriptHUD = &m_HUDSystem;
    s_VisualScriptSubtitleSystem = m_SubtitleSystem;
    s_VisualScriptAnnouncer = m_Announcer;
    s_VisualScriptAudio = &m_SimpleAudio;
    s_VisualScriptPostProcessing = m_PostProcessing;
    s_VisualScriptWater = m_Water3D;
    s_VisualScriptAudioGraphRuntime = &m_AudioGraphRuntime;
    s_VisualScriptObjectPool = &m_ObjectPool;
    ENJIN_LOG_INFO(Editor, "PlayMode: Script bindings set");

    // Initialize owned systems
    m_SimpleAudio.Initialize();
    m_SimpleAudio.SetWorld(m_World);
#ifdef ENJIN_AUDIO_STEAM_AUDIO
    // Apply HRTF setting from editor
    if (m_EditorSettings) {
        m_SimpleAudio.SetHRTFEnabled(m_EditorSettings->enableHRTF);
        m_SimpleAudio.SetOcclusionEnabled(m_EditorSettings->enableOcclusion);
        m_SimpleAudio.SetTransmissionEnabled(m_EditorSettings->enableTransmission);
    }
    // Build audio scene geometry from colliders for occlusion
    m_SimpleAudio.BuildSteamAudioScene();
#endif
    m_DestructibleSystem.Initialize(m_World);
    m_AudioGraphRuntime.Initialize(&m_SimpleAudio);
    if (m_CurlNoiseSystem) m_CurlNoiseSystem->Initialize(m_World);

    // Wire 2D physics collision callbacks to visual script system and gameplay processing
    Gameplay::GameplayLoop::Wire2DCollisionCallbacks(
        m_Physics2D.get(), m_World, &m_VisualScriptSystem, m_DeferredDestroys);

    // Wire announcer to UISystem for screen reader support (Task #36)
    if (m_UISystem && m_Announcer) {
        m_UISystem->SetAnnouncerCallback([this](const std::string& text) {
            m_Announcer->Announce(text, Accessibility::AnnouncePriority::Normal);
        });
    }

    // Apply motor accessibility settings to UISystem (Task #34, #40)
    if (m_UISystem && m_AccessibilitySettings) {
        m_UISystem->SetSwitchAccessEnabled(m_AccessibilitySettings->switchAccessEnabled,
                                            m_AccessibilitySettings->switchScanSpeed);
        m_UISystem->SetDwellClickEnabled(m_AccessibilitySettings->dwellClickEnabled,
                                          m_AccessibilitySettings->dwellClickTime);
        m_UISystem->SetStickyDragEnabled(m_AccessibilitySettings->stickyDragEnabled);
        m_UISystem->SetReducedMotion(m_AccessibilitySettings->reducedMotion);
        m_UISystem->SetFontScale(m_AccessibilitySettings->fontScale);
    }

    // Wire audio visual indicators to SimpleAudio callbacks (Task #38)
    if (m_AudioIndicators && m_AudioIndicators->GetConfig().enabled) {
        m_SimpleAudio.SetOnSoundPlayed([this](const std::string& soundName) {
            if (m_AudioIndicators) {
                m_AudioIndicators->ShowIndicator(soundName,
                    Enjin::Math::Vector3(0.4f, 0.8f, 1.0f), 1.5f);
            }
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
    m_VisualScriptSystem.SetSceneManager(m_SceneManager);
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
    // runs its own shadow pass via RenderShadowPassForCamera(), and the editor
    // viewport reuses those shadow maps. Main pass geometry still renders so the
    // user can fly the editor camera and inspect the scene at runtime.
    if (m_RenderSystem) {
        m_RenderSystem->SetSkipMainPassShadows(true);
    }

    m_State = PlayState::Playing;

    // Start recording snapshots for rewind
    StartRecording();

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
    m_TweenSystem.SetEnabled(false);
    m_VisualScriptSystem.SetEnabled(false);
    m_BehaviorTreeSystem.SetEnabled(false);
    m_DialogueSystem.SetEnabled(false);
    m_StateMachineSystem.SetEnabled(false);
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
    m_TweenSystem.SetEnabled(true);
    m_VisualScriptSystem.SetEnabled(true);
    m_BehaviorTreeSystem.SetEnabled(true);
    m_DialogueSystem.SetEnabled(true);
    m_StateMachineSystem.SetEnabled(true);
    // Do NOT capture mouse here — only focus mode (F11) captures the mouse.

    m_State = PlayState::Playing;
    ENJIN_LOG_INFO(Editor, "Play Mode Resumed");
}

void PlayMode::Stop() {
    if (m_State == PlayState::Stopped) {
        return;
    }

    m_GameOverReady = false;

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
    if (m_CurlNoiseSystem) m_CurlNoiseSystem->Shutdown();
    m_DestructibleSystem.Shutdown();

    // Clear 2D physics callbacks
    if (m_Physics2D) {
        m_Physics2D->SetOnCollisionEnter(nullptr);
        m_Physics2D->SetOnCollisionExit(nullptr);
        m_Physics2D->SetOnSensorEnter(nullptr);
        m_Physics2D->SetOnSensorExit(nullptr);
    }

    // Destroy character controllers created during play (prevents memory leak
    // across play/stop cycles — CharacterVirtual instances are per-entity and
    // must be cleaned up when the scene is restored)
    if (m_Physics) m_Physics->DestroyAllCharacterControllers();

    // Clear save/gameplay system bindings
    s_VisualScriptSaveSystem = nullptr;
    s_VisualScriptWeather = nullptr;
    s_VisualScriptElemental = nullptr;
    s_VisualScriptWater = nullptr;
    s_VisualScriptHUD = nullptr;
    s_VisualScriptSubtitleSystem = nullptr;
    s_VisualScriptAnnouncer = nullptr;
    s_VisualScriptAudio = nullptr;
    s_VisualScriptPostProcessing = nullptr;
    s_VisualScriptAudioGraphRuntime = nullptr;
    s_VisualScriptObjectPool = nullptr;
    Scripting::SetBindingsWorld(nullptr);
    Scripting::SetBindingsRenderSystem(nullptr);
    Scripting::SetBindingsDialogueSystem(nullptr);
    Scripting::SetBindingsCoroutineScheduler(nullptr);
    Scripting::SetBindingsEventBus(nullptr);
    Scripting::SetBindingsScriptEngine(nullptr);
    Scripting::SetBindingsPostProcessing(nullptr);
    Scripting::SetBindingsSubtitles(nullptr);
    Scripting::SetBindingsAnnouncer(nullptr);
    Scripting::SetBindingsAccessibilitySettings(nullptr);
    Scripting::SetBindingsSaveSystem(nullptr);
    Scripting::SetFlashShimSaveSystem(nullptr);
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
    m_MIDIInput.Shutdown();
    Scripting::SetBindingsMIDI(nullptr);
    Scripting::SetBindingsInputActionMap(nullptr);

    // Clear accessibility wiring
    if (m_UISystem) {
        m_UISystem->SetAnnouncerCallback(nullptr);
        m_UISystem->SetSwitchAccessEnabled(false);
        m_UISystem->SetDwellClickEnabled(false);
        m_UISystem->SetStickyDragEnabled(false);
    }
    m_SimpleAudio.SetOnSoundPlayed(nullptr);

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

    // Stop recording and free rewind snapshots
    StopRecording();
    ExitRewind();
    m_Snapshots.clear();
    m_Snapshots.shrink_to_fit();  // Release memory

    // Restore the scene to its pre-play state
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
        // Skip all gameplay updates while scrubbing the rewind timeline
        if (m_Rewinding) return;

        auto frameStart = std::chrono::high_resolution_clock::now();

        // Update input action map (polls input state for remappable actions)
        m_InputMap.Update(deltaTime);
        m_MIDIInput.Update();

        // Physics runs first to update rigidbody positions, then controllers overlay input
        auto t0 = std::chrono::high_resolution_clock::now();
        {
            ENJIN_PROFILE_SCOPE("Physics");
            if (m_Physics) m_Physics->Update(deltaTime);
            if (m_Physics2D) m_Physics2D->Update(deltaTime);
        }

        // Dispatch 3D collision events to visual scripts and gameplay systems
        Gameplay::GameplayLoop::DispatchCollisionEvents3D(
            m_World, m_Physics.get(), &m_VisualScriptSystem, deltaTime, m_DeferredDestroys);

        auto t1 = std::chrono::high_resolution_clock::now();

        {
            ENJIN_PROFILE_SCOPE("ECS");
            m_ControllerSystem.Update(deltaTime);
            m_FlowerSystem.Update(deltaTime);
        }

        // Apply accessibility visual settings (colorblind filters, brightness,
        // contrast) to post-processing every frame. This was missing — colorblind
        // modes were defined but never pushed to the shader.
        if (m_AccessibilitySettings && m_PostProcessing) {
            m_AccessibilitySettings->ApplyToPostProcessing(m_PostProcessing->GetSettings());
        }

        // Re-sync controller-driven positions to Box2D and fire sensor events.
        // Controllers move entities after the physics step, so without this
        // second sync, sensor overlaps (damage, pickups) are 1 frame late.
        if (m_Physics2D) m_Physics2D->SyncAndProcessEvents();

        auto t2 = std::chrono::high_resolution_clock::now();

        // Update scripts (handles hot-reload, lifecycle dispatch, coroutines)
        {
            ENJIN_PROFILE_SCOPE("Scripting");
            m_ScriptSystem.Update(deltaTime);
            m_CoroutineScheduler.EndOfFrame();
            Scripting::FlushDeferredEntityDestroys();
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
        m_InteractiveWaterSystem.Update(m_World, deltaTime);
        // Update interactive water mesh for rendering
        for (auto entity : m_World->GetEntitiesWithComponent<Effects::InteractiveWaterComponent>()) {
            auto* water = m_World->GetComponent<Effects::InteractiveWaterComponent>(entity);
            auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);
            if (!water || !transform || !water->initialized) continue;
            auto mesh = m_InteractiveWaterSystem.GenerateMesh(*water, *transform);
            if (m_World->HasComponent<ECS::MeshComponent>(entity))
                *m_World->GetComponent<ECS::MeshComponent>(entity) = std::move(mesh);
            else
                m_World->AddComponent<ECS::MeshComponent>(entity, std::move(mesh));
        }
        // Update Water3D animated surfaces
        if (m_Water3D) {
            m_Water3D->Update(deltaTime);
            for (auto entity : m_World->GetEntitiesWithComponent<ECS::Water3DComponent>()) {
                auto* water3d = m_World->GetComponent<ECS::Water3DComponent>(entity);
                if (!water3d || !water3d->meshCreated) continue;
                m_Water3D->Initialize(water3d->settings);
                m_Water3D->UpdateEntityMesh(m_World, entity);
            }
        }
        m_EntityEventBus.ProcessDeferred();

        // Record state snapshot at regular intervals
        if (m_Recording) {
            m_RecordTimer += deltaTime;
            m_MaxRecordTime += deltaTime;
            if (m_RecordTimer >= m_RecordInterval) {
                m_RecordTimer -= m_RecordInterval;
                Scene::SceneSerializer serializer(m_World);
                StateSnapshot snap;
                snap.timestamp = m_MaxRecordTime;
                snap.sceneJson = serializer.SaveToString();
                m_Snapshots.push_back(std::move(snap));

                // Cap at ~600 snapshots (40 seconds at 15fps) to avoid unbounded memory
                if (m_Snapshots.size() > 600) {
                    m_Snapshots.erase(m_Snapshots.begin());
                }
            }
        }

        // Fluid simulation + terrain coupling + curl noise
        if (m_FluidSimulation) m_FluidSimulation->Update(deltaTime, m_World);
        if (m_FluidTerrainCoupling && m_FluidSimulation) m_FluidTerrainCoupling->Update(deltaTime, m_World, *m_FluidSimulation);
        if (m_CurlNoiseSystem) m_CurlNoiseSystem->Update(deltaTime);

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

        // Accessibility systems update (Tasks #37, #38)
        if (m_AlternativeInput) m_AlternativeInput->Update(deltaTime);
        if (m_AudioIndicators) m_AudioIndicators->Update(deltaTime);
        if (m_Announcer) m_Announcer->Update(deltaTime);
        if (m_SubtitleSystem) m_SubtitleSystem->Update(deltaTime);

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

        // Hazard overlap check (spikes, lava — bypasses Box2D sensor system)
        Gameplay::GameplayLoop::CheckHazardOverlaps(m_World, deltaTime, m_DeferredDestroys);
        // Enemy contact damage (Box2D kinematic-kinematic sensor events unreliable)
        Gameplay::GameplayLoop::CheckEnemyOverlaps2D(m_World, deltaTime, m_DeferredDestroys);

        // Pickup overlap (manual AABB — Box2D sensor events unreliable for kinematic-kinematic,
        // Jolt CharacterVirtual doesn't fire collision events with static bodies)
        Gameplay::GameplayLoop::CheckPickupOverlaps3D(m_World, m_DeferredDestroys);
        Gameplay::GameplayLoop::CheckPickupOverlaps2D(m_World, m_DeferredDestroys);

        // Health system (regen, invulnerability timers, death)
        Gameplay::GameplayLoop::UpdateHealthSystems(m_World, deltaTime, m_DeferredDestroys);

        // Game over state (player death / victory detection)
        m_GameOverReady = Gameplay::GameplayLoop::UpdateGameOverState(m_World, deltaTime);

        // Flush deferred entity destroys (from damage/pickup callbacks)
        Gameplay::GameplayLoop::FlushDeferredDestroys(m_World, m_DeferredDestroys);

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

// --- Record & Rewind ---

void PlayMode::StartRecording() {
    m_Recording = true;
    m_Snapshots.clear();
    m_RecordTimer = 0.0f;
    m_MaxRecordTime = 0.0f;
    ENJIN_LOG_INFO(Editor, "PlayMode: Recording started (%.0f snapshots/sec)", 1.0f / m_RecordInterval);
}

void PlayMode::StopRecording() {
    m_Recording = false;
    ENJIN_LOG_INFO(Editor, "PlayMode: Recording stopped (%zu snapshots, %.1f sec)",
        m_Snapshots.size(), m_MaxRecordTime);
}

void PlayMode::EnterRewind() {
    if (m_Snapshots.empty()) return;
    m_Rewinding = true;
    m_RewindFrame = static_cast<i32>(m_Snapshots.size()) - 1;
    Pause();  // Pause gameplay while rewinding
    ENJIN_LOG_INFO(Editor, "PlayMode: Entered rewind mode (%zu frames)", m_Snapshots.size());
}

void PlayMode::ExitRewind() {
    m_Rewinding = false;
    m_RewindFrame = -1;
    ENJIN_LOG_INFO(Editor, "PlayMode: Exited rewind mode");
}

void PlayMode::SeekToFrame(i32 frameIndex) {
    if (frameIndex < 0 || frameIndex >= static_cast<i32>(m_Snapshots.size())) return;
    m_RewindFrame = frameIndex;

    // Load the snapshot into the world
    Scene::SceneSerializer serializer(m_World);
    serializer.LoadFromString(m_Snapshots[frameIndex].sceneJson, true);
}

// Gameplay processing methods (ProcessContactDamage, ProcessPickup,
// UpdateHealthSystems, FlushDeferredDestroys) are now in
// Enjin::Gameplay::GameplayLoop (Engine/src/Gameplay/GameplayLoop.cpp).

} // namespace Editor
} // namespace Enjin
