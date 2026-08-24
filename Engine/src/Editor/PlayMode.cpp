#include "Enjin/Editor/PlayMode.h"
#include <filesystem>
#include <random>
#include <type_traits>
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/Systems/DungeonGeneratorSystem.h"
#include "Enjin/ECS/Systems/RandomBagSystem.h"
#include "Enjin/ECS/Systems/ScatterSystem.h"
#include "Enjin/ECS/Systems/TerrainGeneratorSystem.h"
#include "Enjin/ECS/Systems/WFCSystem.h"
#include "Enjin/ECS/Components/Name.h"
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
extern Enjin::GUI::UISystem* s_VisualScriptUI;
extern Enjin::Accessibility::SubtitleSystem* s_VisualScriptSubtitleSystem;
extern Enjin::Accessibility::AccessibilityAnnouncer* s_VisualScriptAnnouncer;
extern Enjin::Audio::SimpleAudio* s_VisualScriptAudio;
extern Enjin::Renderer::PostProcessing* s_VisualScriptPostProcessing;
extern Enjin::Editor::AudioEventGraphRuntime* s_VisualScriptAudioGraphRuntime;
extern Enjin::Gameplay::ObjectPool* s_VisualScriptObjectPool;
extern Enjin::Effects::ElementalSystem* s_VisualScriptElemental;
extern Enjin::Gameplay::QuestSystem* s_VisualScriptQuestSystem;
extern Enjin::Gameplay::CinematicSystem* s_VisualScriptCinematic;

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

        // Initialize record & rewind system
        m_RecordRewindSystem.SetWorld(world);
        m_RecordRewindSystem.SetPhysics(m_Physics.get());
        m_RecordRewindSystem.SetPhysics2D(m_Physics2D.get());

        m_AudioReactiveSystem.SetWorld(world);
        m_AudioReactiveSystem.SetAudio(&m_SimpleAudio);
        m_AudioReactiveSystem.SetMIDI(&m_MIDIInput);

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
    if (m_State.load(std::memory_order_relaxed) == PlayState::Playing) {
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
            // Script bindings captured the (null) backend pointer at Initialize —
            // refresh it or Physics_Raycast/Teleport silently no-op this session
            Scripting::SetBindingsPhysics(m_Physics.get());
            ENJIN_LOG_INFO(Editor, "PlayMode: Created 3D physics backend on Play()");
        }
    }

    // Keep rewind system in sync with lazily-created physics backends
    m_RecordRewindSystem.SetPhysics(m_Physics.get());
    m_RecordRewindSystem.SetPhysics2D(m_Physics2D.get());

    // Find the active game camera entity so controllers drive it instead of the editor camera
    ECS::Entity gameCam = ECS::CameraManager::GetActiveCamera(m_World);
    m_ControllerSystem.SetGameCameraEntity(gameCam);
    // Never let a controller drive the editor fly camera in the editor. Without this, a character
    // with no game camera drags the editor camera around (it falls forever with the player).
    m_ControllerSystem.SetDriveEditorCameraFallback(false);
    if (gameCam == ECS::INVALID_ENTITY) {
        ENJIN_LOG_WARN(Editor, "PlayMode: no active game camera in the scene — controllers will not "
            "drive a camera. Add a Camera entity (or a controller via the templates which auto-create one).");
    }
    ENJIN_LOG_INFO(Editor, "PlayMode: Camera setup done (cam=%llu)", (unsigned long long)gameCam);

    // Resolve scene script paths against the project directory. Scene data
    // stores project-root-relative paths ("scripts/Foo.as"); the process CWD
    // is the exe directory, so without this no scene-attached script loads.
    // The script directory (for #include resolution: scripts/ and
    // scripts/enjin_api/) must be absolute for the same reason — the
    // Initialize-time default is the CWD-relative "scripts".
    if (m_SceneManager && !m_SceneManager->GetProjectPath().empty()) {
        std::filesystem::path projDir =
            std::filesystem::path(m_SceneManager->GetProjectPath()).parent_path();
        m_ScriptSystem.SetScriptRoot(projDir.string());
        m_ScriptEngine.SetScriptDirectory((projDir / "scripts").string());
        m_SimpleAudio.SetAssetRoot(projDir.string());
    }

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
    if (m_UISystem) m_UISystem->SetHUDEnabled(true);
    m_QuestSystem.SetEnabled(true);
    m_FootstepSystem.SetEnabled(true);
    m_CinematicSystem.SetEnabled(true);
    m_StreamingManager.SetEnabled(true);
    m_AISystem.SetEnabled(true);
    ENJIN_LOG_INFO(Editor, "PlayMode: Gameplay systems enabled");

    // Replay determinism: procgen components with seed 0 roll a random_device
    // seed inside Generate - invisible to the replay's scene snapshot, so a
    // replay would regenerate DIFFERENT content. Resolve them to concrete
    // seeds up front: generation below uses them, the snapshot captures them,
    // and Stop's scene restore returns the editor values to 0.
    if (m_World && !m_Replaying) {
        std::random_device rd;
        auto roll = [&rd]() { u32 s = rd(); return s ? s : 1u; };
        auto preRoll = [&](auto* tag) {
            using T = std::remove_pointer_t<decltype(tag)>;
            for (ECS::Entity e : m_World->GetEntitiesWithComponent<T>()) {
                auto* c = m_World->GetComponent<T>(e);
                if (c && c->seed == 0) c->seed = roll();
            }
        };
        preRoll(static_cast<ECS::DungeonGeneratorComponent*>(nullptr));
        preRoll(static_cast<ECS::ScatterComponent*>(nullptr));
        preRoll(static_cast<ECS::TerrainGeneratorComponent*>(nullptr));
        preRoll(static_cast<ECS::WFCComponent*>(nullptr));
        preRoll(static_cast<ECS::RandomBagComponent*>(nullptr));
    }

    ECS::DungeonGeneratorSystem::GenerateAll(m_World);   // fresh dungeons on play (generateOnStart)
    ECS::RandomBagSystem::ResetAll(m_World);             // fresh bags on play (clears editor test-draw state)
    ECS::ScatterSystem::GenerateAll(m_World);            // fresh scatter batches on play (generateOnStart)
    ECS::TerrainGeneratorSystem::GenerateAll(m_World);   // fresh terrain bakes on play (generateOnStart)
    ECS::WFCSystem::GenerateAll(m_World);                // fresh WFC layouts on play (generateOnStart)

    // Editor debug recording: inject a hidden whole-scene recorder so the play
    // session can be paused and stepped/scrubbed backward from the timeline.
    // Keyless (rewindKey -1): only the editor drives it, never gameplay input.
    // The entity is play-created, so Stop's scene restore removes it.
    // Fresh input recording for this session (replay playback reuses its own
    // stream instead). The scene snapshot is the compact form: mesh-by-reference,
    // no vertex data - replays are shared alongside the project, not instead of it.
    if (!m_Replaying) {
        m_ActiveRecording = Gameplay::ReplayData{};
        m_ActiveRecording.engineVersion = "0.9.7";
        if (m_World) {
            Scene::SceneSerializer ser(m_World);
            Scene::SerializationOptions opts;
            opts.prettyPrint = false;
            // Vertex data ON: with mesh references also on, imported meshes
            // still shrink to source refs - but authored/primitive meshes
            // (capsules, boxes, procgen output) keep their inline geometry.
            // includeVertexData=false stripped them, so replay playback showed
            // an invisible character whose collider still fell (2026-08-23).
            opts.includeVertexData = true;
            opts.useMeshReferences = true;
            m_ActiveRecording.sceneJson = ser.SaveToString(opts);
        }
        // Script Random()/RandomRange() drain a process-lifetime xorshift
        // stream - a replay in the same session would resume it mid-stream and
        // every script random would diverge. Seed it fresh per play session
        // and carry the seed in the replay.
        {
            std::random_device rd;
            u32 s = rd();
            if (s == 0) s = 1;
            m_ActiveRecording.rngSeed = s;
            Math::SetRandomSeed(s);
        }
    } else {
        Math::SetRandomSeed(m_ReplayData.rngSeed);
    }

    m_DebugRecorderEntity = ECS::INVALID_ENTITY;
    if (m_DebugRecordEnabled && m_World) {
        m_DebugRecorderEntity = m_World->CreateEntity();
        auto& name = m_World->AddComponent<ECS::NameComponent>(m_DebugRecorderEntity);
        name.name = "__DebugRecorder";
        auto& sr = m_World->AddComponent<ECS::SceneRewindComponent>(m_DebugRecorderEntity);
        sr.maxDuration = m_DebugRecordSeconds;
        sr.recordInterval = 1.0f / 30.0f;   // finer than the gameplay default: smoother stepping
        sr.rewindKey = -1;
        sr.cooldown = 0.0f;
        sr.charges = 0;
        ENJIN_LOG_INFO(Editor, "PlayMode: debug recorder active (%.0fs buffer, 30 snapshots/s)",
                       m_DebugRecordSeconds);
    }
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
    Scripting::SetBindingsRewindSystem(&m_RecordRewindSystem);
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
    // Push script-driven accessibility changes into the live consumers —
    // without this, Colorblind_SetMode etc. write the settings struct and
    // nothing in the editor ever reads it (works in player, dead in editor).
    Scripting::SetBindingsAccessibilityApplyCallback([this]() {
        if (!m_AccessibilitySettings) return;
        if (m_PostProcessing) {
            m_AccessibilitySettings->ApplyToPostProcessing(m_PostProcessing->GetSettings());
        }
        if (m_SubtitleSystem) {
            auto& sc = m_SubtitleSystem->GetConfig();
            sc.enabled = m_AccessibilitySettings->subtitlesEnabled;
            sc.fontSize = m_AccessibilitySettings->subtitleFontSize;
        }
        if (m_UISystem) {
            m_UISystem->SetFontScale(m_AccessibilitySettings->fontScale);
            m_UISystem->SetReducedMotion(m_AccessibilitySettings->reducedMotion);
            m_UISystem->SetSwitchAccessEnabled(m_AccessibilitySettings->switchAccessEnabled,
                                               m_AccessibilitySettings->switchScanSpeed);
            m_UISystem->SetDwellClickEnabled(m_AccessibilitySettings->dwellClickEnabled,
                                             m_AccessibilitySettings->dwellClickTime);
            m_UISystem->SetStickyDragEnabled(m_AccessibilitySettings->stickyDragEnabled);
        }
        m_ControllerSystem.SetReducedMotion(m_AccessibilitySettings->reducedMotion);
    });
    Scripting::SetBindingsPluginSystem(nullptr);  // No PluginSystem instance yet — null-safe
    Scripting::SetBindingsAudioGraphRuntime(&m_AudioGraphRuntime);
    m_MIDIInput.Initialize();
    Scripting::SetBindingsMIDI(&m_MIDIInput);
    Scripting::SetBindingsInputActionMap(&m_InputMap);
    s_VisualScriptSaveSystem = &m_TieredSaveSystem;
    s_VisualScriptWeather = m_WeatherSystem;
    s_VisualScriptElemental = m_ElementalSystem;
    Scripting::SetBindingsElemental(m_ElementalSystem);
    s_VisualScriptUI = m_UISystem;
    s_VisualScriptSubtitleSystem = m_SubtitleSystem;
    s_VisualScriptAnnouncer = m_Announcer;
    s_VisualScriptAudio = &m_SimpleAudio;
    s_VisualScriptPostProcessing = m_PostProcessing;
    s_VisualScriptWater = m_Water3D;
    s_VisualScriptAudioGraphRuntime = &m_AudioGraphRuntime;
    s_VisualScriptObjectPool = &m_ObjectPool;
    s_VisualScriptQuestSystem = &m_QuestSystem;
    s_VisualScriptCinematic = &m_CinematicSystem;
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
    // The UICanvas game-over screen (spawned by GameplayLoop, one UI source on all
    // platforms) dispatches "gameover_restart" — in the editor that means restart
    // the play session. Deferred via flag: Stop()/Play() can't run mid-dispatch.
    if (m_UISystem) {
        m_GameOverRestartListener = m_UISystem->GetEventBus().Listen("gameover_restart",
            [this](const GUI::UIEventData&) { m_RestartRequested = true; });

        // Bridge every UI event into the script event bus so game scripts can
        // react to authored buttons/sliders via Events_Listen("<onClickEvent>", ...).
        m_UISystem->GetEventBus().SetForwarder([this](const GUI::UIEventData& e) {
            Scripting::EventData data;
            data.SetString("source", "ui");
            data.SetEntity("canvas", static_cast<u64>(e.canvasEntity));
            data.SetInt("elementId", static_cast<i32>(e.elementId));
            data.SetFloat("value", e.floatValue);
            data.SetInt("checked", e.boolValue ? 1 : 0);
            data.SetString("text", e.stringValue);
            m_EventBus.Send(e.eventName, data);
        });
    }

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
    // Water-enter events (splash VFX / sound / score) go through the same bus.
    m_InteractiveWaterSystem.SetEventBus(&m_EntityEventBus);
    m_DialogueSystem.SetSubtitleSystem(m_SubtitleSystem);
    m_DialogueSystem.SetQuestSystem(&m_QuestSystem);
    m_DialogueSystem.SetCinematicSystem(&m_CinematicSystem);
    m_DialogueSystem.SetTieredSaveSystem(&m_TieredSaveSystem);
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
    m_VisualScriptSystem.SetDialogue(&m_DialogueSystem);
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

    m_State.store(PlayState::Playing, std::memory_order_relaxed);
    ENJIN_LOG_INFO(Editor, "Entered Play Mode");
}

void PlayMode::Pause() {
    if (m_State != PlayState::Playing) {
        return;
    }

    m_ControllerSystem.SetEnabled(false);
    m_FlowerSystem.SetEnabled(false);
    m_ScriptSystem.SetEnabled(false);
    if (m_UISystem) m_UISystem->SetHUDEnabled(false);
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

    m_State.store(PlayState::Paused, std::memory_order_relaxed);
    ENJIN_LOG_INFO(Editor, "Play Mode Paused");
}

void PlayMode::Resume() {
    if (m_State != PlayState::Paused) {
        return;
    }

    m_ControllerSystem.SetEnabled(true);
    m_FlowerSystem.SetEnabled(true);
    m_ScriptSystem.SetEnabled(true);
    if (m_UISystem) m_UISystem->SetHUDEnabled(true);
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

    m_State.store(PlayState::Playing, std::memory_order_relaxed);
    ENJIN_LOG_INFO(Editor, "Play Mode Resumed");
}

void PlayMode::StartReplay(Gameplay::ReplayData&& data) {
    m_ReplayData = std::move(data);
    m_ReplayCursor = 0;
    m_Replaying = true;
    Input::SetReplayInjection(true);
    ENJIN_LOG_INFO(Editor, "Replay starting: %zu frames at %.4fs/frame",
                   m_ReplayData.frames.size(), m_ReplayData.fixedDt);
    Play();
}

void PlayMode::Stop() {
    if (m_Replaying) {
        Input::SetReplayInjection(false);
        m_Replaying = false;
        m_ReplayCursor = 0;
    }

    if (m_State.load(std::memory_order_relaxed) == PlayState::Stopped) {
        return;
    }

    m_GameOverReady = false;

    // Stop all audio before destroying entities (prevents stale sound handles)
    m_SimpleAudio.StopAll();

    // Destroy pooled objects before shutting down scripts (scripts may reference pooled entities)
    m_ObjectPool.DestroyAll(m_World);

    // Tear down swarm proxy entities so they don't leak into the restored scene
    m_SwarmSystem.Reset(m_World);

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
    s_VisualScriptUI = nullptr;
    s_VisualScriptSubtitleSystem = nullptr;
    s_VisualScriptAnnouncer = nullptr;
    s_VisualScriptAudio = nullptr;
    s_VisualScriptPostProcessing = nullptr;
    s_VisualScriptAudioGraphRuntime = nullptr;
    s_VisualScriptObjectPool = nullptr;
    s_VisualScriptQuestSystem = nullptr;
    s_VisualScriptCinematic = nullptr;
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
    Scripting::SetBindingsAccessibilityApplyCallback(nullptr);
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
    Scripting::SetBindingsRewindSystem(nullptr);
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
    if (m_UISystem) m_UISystem->SetHUDEnabled(false);
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

    // Restore the scene to its pre-play state
    RestoreEditorState();

    // Cloth: the restore brings back component data, but the GPU vertex/index
    // buffers still hold the simulated (possibly torn) fabric. Rebuild fresh
    // grids so edit mode shows the authored cloth, not the last play's tears.
    Gameplay::ClothSystem::ResetAll(m_World);

    // Tear down the physics backends so the next Play rebuilds every body from the
    // restored pre-play transforms. The backend is only (re)created when !m_Physics,
    // so without this it persists across plays and its dynamic bodies keep their
    // simulated positions -- repeated plays of the stress test then start already
    // settled on the ground instead of falling. All holders below are re-pointed at
    // the fresh backend when Play() runs again, and every consuming system is
    // disabled in edit mode, so clearing the pointers here is safe.
    m_ControllerSystem.SetPhysics(nullptr);
    m_ControllerSystem.SetPhysics2D(nullptr);
    m_RecordRewindSystem.SetPhysics(nullptr);
    m_RecordRewindSystem.SetPhysics2D(nullptr);
    m_VisualScriptSystem.SetPhysics(nullptr);
    Scripting::SetBindingsPhysics(nullptr);
    m_Physics.reset();
    m_Physics2D.reset();

    // Re-enable main-pass shadow rendering for editor mode
    if (m_RenderSystem) {
        m_RenderSystem->SetSkipMainPassShadows(false);
    }

    // Drop the game-over restart listener (re-registered on next Play)
    if (m_UISystem && m_GameOverRestartListener != 0) {
        m_UISystem->GetEventBus().RemoveListener(m_GameOverRestartListener);
        m_GameOverRestartListener = 0;
    }

    m_State.store(PlayState::Stopped, std::memory_order_relaxed);
    ENJIN_LOG_INFO(Editor, "Exited Play Mode");
}

void PlayMode::Update(f32 deltaTime) {
    // Escape is now handled by EditorLayer (which manages focus mode exit vs stop).

    // Deferred restart from the game-over screen's "Play Again" button
    if (m_RestartRequested) {
        m_RestartRequested = false;
        Stop();
        Play();
        return;
    }

    // Update controller system when playing
    if (m_State.load(std::memory_order_relaxed) == PlayState::Playing) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        // Replay determinism: recorded and replayed sessions step at the
        // recording's fixed dt so the same input stream lands on the same
        // simulation frames.
        if (m_Replaying) {
            if (m_ReplayCursor < m_ReplayData.frames.size()) {
                const auto& rf = m_ReplayData.frames[m_ReplayCursor++];
                deltaTime = rf.dt;   // replay the exact recorded dt stream
                bool keys[512]; bool mouse[8]; Math::Vector2 mpos;
                Gameplay::ReplayFrameToBuffers(rf, keys, mouse, mpos);
                Input::InjectFrameState(keys, mouse, mpos);
            } else {
                // Stream exhausted: hold here so the timeline can inspect it.
                Input::SetReplayInjection(false);
                Pause();
                ENJIN_LOG_INFO(Editor, "Replay finished (%zu frames) - paused at end",
                               m_ReplayData.frames.size());
                return;
            }
        } else {
            // Record this frame's input + its real dt (sparse; capped ~20 min).
            // Live play speed is untouched - determinism comes from replaying
            // the same dt sequence, not from forcing one.
            if (m_ActiveRecording.frames.size() < 72000u) {
                bool keys[512]; bool mouse[8]; Math::Vector2 mpos;
                Input::CaptureFrameState(keys, mouse, mpos);
                Gameplay::ReplayFrame frame;
                Gameplay::ReplayFrameFromBuffers(frame, keys, mouse, mpos);
                frame.dt = deltaTime;
                m_ActiveRecording.frames.push_back(std::move(frame));
            }
        }

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
            // TotK-style surface response: footstep/impact sound + particle from
            // the material of the surface walked on / struck. 3D physics path.
            m_SurfaceResponseSystem.Initialize(&m_SimpleAudio, m_RenderSystem, m_Physics.get(), m_Physics2D.get());
            m_SurfaceResponseSystem.Update(m_World, deltaTime);
            m_ClothSystem.Update(m_World, deltaTime,
                                 m_RenderSystem ? m_RenderSystem->GetWindSystem() : nullptr);
        }

        // Apply accessibility visual settings (colorblind filters, brightness,
        // contrast) to post-processing every frame. This was missing — colorblind
        // modes were defined but never pushed to the shader.
        if (m_AccessibilitySettings && m_PostProcessing) {
            m_AccessibilitySettings->ApplyToPostProcessing(m_PostProcessing->GetSettings());
        }
        // Font scale too — game scripts (Accessibility Demo) change it live.
        if (m_AccessibilitySettings && m_UISystem) {
            m_UISystem->SetFontScale(m_AccessibilitySettings->fontScale);
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

        // Record & Rewind (Braid / Sands of Time mechanic)
        m_RecordRewindSystem.Update(deltaTime);

        // Sync the 3D audio listener to the game camera. Without this the
        // listener stays wherever it was initialized (world origin) and every
        // positional sound attenuates/pans relative to spawn, not the player.
        {
            ECS::Entity cam = ECS::CameraManager::GetActiveCamera(m_World);
            if (cam != ECS::INVALID_ENTITY) {
                auto* camT = m_World->GetComponent<ECS::TransformComponent>(cam);
                if (camT) {
                    m_SimpleAudio.SetListenerPosition(camT->position,
                                                      camT->rotation.GetForward(),
                                                      camT->rotation.GetUp());
                }
            }
        }

        // Audio-reactive systems (beat sync, VU→visual, RTPC, threshold triggers)
        m_AudioReactiveSystem.Update(deltaTime);

        // Gameplay systems
        m_TweenSystem.Update(m_World, deltaTime);
        m_SwarmSystem.Update(m_World, deltaTime);
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
            // WARN level on purpose — the console's Warn filter isolates the perf diagnostics.
            ENJIN_LOG_WARN(Editor,
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
        Gameplay::GameplayLoop::CheckHazardOverlaps3D(m_World, m_DeferredDestroys);
        Gameplay::GameplayLoop::CheckPickupOverlaps3D(m_World, m_DeferredDestroys);
        Gameplay::GameplayLoop::CheckPickupOverlaps2D(m_World, m_DeferredDestroys);

        // Health system (regen, invulnerability timers, death)
        Gameplay::GameplayLoop::UpdateHealthSystems(m_World, deltaTime, m_DeferredDestroys);

        // Trigger zones (fills entitiesInside — required for reach-the-goal victory)
        Gameplay::GameplayLoop::UpdateTriggerZones(m_World);

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

    // Lightweight per-entity snapshot — just the gameplay-mutable state.
    // We deliberately avoid serializing the entire scene to JSON: roundtripping
    // mesh + skeleton + animation track data through nlohmann::json was producing
    // 100MB+ payloads for multi-mesh skeletal characters and OOMing on restore.
    m_SavedEntityState.clear();
    m_DestroyedEntityJson.clear();
    auto entities = m_World->GetAllEntities();
    for (auto entity : entities) {
        EntitySnapshot snap;
        if (auto* t = m_World->GetComponent<ECS::TransformComponent>(entity)) {
            snap.position = t->position;
            snap.rotation = t->rotation;
            snap.scale = t->scale;
            snap.visible = t->visible;
            snap.hadTransform = true;
        }
        if (auto* nc = m_World->GetComponent<ECS::NameComponent>(entity)) {
            snap.name = nc->name;
            snap.hadName = true;
        }
        if (auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity)) {
            snap.linearVelocity = rb->velocity;
            snap.angularVelocity = rb->angularVelocity;
            snap.hadRigidbody = true;
        }
        m_SavedEntityState[static_cast<u64>(entity)] = snap;
    }

    // Save camera state
    m_SavedCameraPos = m_Camera->GetPosition();
    m_SavedCameraRot = m_Camera->GetRotation();
    m_SavedCameraFov = m_Camera->GetFOV();

    // Instead of a full up-front scene serialize (the play-start hitch), install a
    // destroy-observer that serializes ONLY the pre-play entities that actually die
    // during play, at the moment they die. Stop recreates exactly these. The common
    // case (nothing destroyed) pays zero serialization cost on Play.
    m_World->SetEntityDestroyObserver([this](ECS::Entity e) {
        const u64 id = static_cast<u64>(e);
        // Only pre-play entities are restorable; play-created ones must stay gone.
        if (m_SavedEntityState.find(id) == m_SavedEntityState.end()) return;
        if (m_DestroyedEntityJson.count(id)) return;  // capture once, at first death
        m_DestroyedEntityJson[id] =
            Scene::SceneSerializer::SerializeEntityToString(m_World, e, /*includeVertexData=*/true);
    });

    ENJIN_LOG_DEBUG(Editor, "Saved editor state (%zu entities snapshotted)", m_SavedEntityState.size());
}

void PlayMode::RestoreEditorState() {
    if (!m_World || !m_Camera) {
        return;
    }

    // Capture any destroys still queued from the final play frame while the observer
    // is still installed (their data is intact until the flush actually runs), then
    // stop observing so nothing captures during the restore mutations below.
    m_World->FlushPendingDestructions();
    m_World->ClearEntityDestroyObserver();

    // Restore each SURVIVING tracked entity's gameplay-mutable state in place
    // (transform + visible). Static asset data (mesh, skeleton, animator) is left
    // untouched, which avoids the old multi-megabyte JSON roundtrip AND the reload
    // use-after-free that came from re-adding AnimatorComponent on reloaded entities.
    // Entities destroyed during play are handled by the recreate pass below; ID
    // recycling means a valid handle whose name changed is a different entity that
    // reused the slot, so treat the original as destroyed (skip it here).
    usize restored = 0;
    for (const auto& [eid, snap] : m_SavedEntityState) {
        ECS::Entity entity = static_cast<ECS::Entity>(eid);
        if (!m_World->IsValid(entity)) continue;   // destroyed — recreated below
        if (snap.hadName) {
            auto* nc = m_World->GetComponent<ECS::NameComponent>(entity);
            if (!nc || nc->name != snap.name) continue;  // slot reused by another entity
        }
        if (snap.hadTransform) {
            if (auto* t = m_World->GetComponent<ECS::TransformComponent>(entity)) {
                t->position = snap.position;
                t->rotation = snap.rotation;
                t->scale = snap.scale;
                t->visible = snap.visible;
                ++restored;
            }
        }
        if (snap.hadRigidbody) {
            if (auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(entity)) {
                rb->velocity = snap.linearVelocity;        // else physics motion leaks into the next Play
                rb->angularVelocity = snap.angularVelocity;
            }
        }
    }

    // Recreate the pre-play entities that died during play, from their incrementally
    // captured JSON. No World::Clear + full reload (that was the crash-historied path
    // and the source of the play-start/stop cost) — each entity comes back through the
    // normal per-entity add path (DeserializeEntityFromString -> AddComponent ->
    // OnEntityAdded rebuilds its render buffers), and we stamp its pre-play transform
    // back on so it returns to where it started rather than where it died.
    usize recreated = 0;
    for (const auto& [eid, json] : m_DestroyedEntityJson) {
        if (json.empty()) continue;
        ECS::Entity created = Scene::SceneSerializer::DeserializeEntityFromString(m_World, json);
        if (!m_World->IsValid(created)) continue;
        auto snapIt = m_SavedEntityState.find(eid);
        if (snapIt != m_SavedEntityState.end() && snapIt->second.hadTransform) {
            if (auto* t = m_World->GetComponent<ECS::TransformComponent>(created)) {
                const auto& snap = snapIt->second;
                t->position = snap.position;
                t->rotation = snap.rotation;
                t->scale = snap.scale;
                t->visible = snap.visible;
            }
            if (snapIt->second.hadRigidbody) {
                if (auto* rb = m_World->GetComponent<ECS::RigidbodyComponent>(created)) {
                    rb->velocity = snapIt->second.linearVelocity;
                    rb->angularVelocity = snapIt->second.angularVelocity;
                }
            }
        }
        ++recreated;
    }
    m_DestroyedEntityJson.clear();

    // The recreate pass added entities/components; point the render caches at the
    // current storages so the editor (which never calls World::Update in edit mode)
    // draws the recreated entities immediately.
    if (m_RenderSystem && recreated > 0) m_RenderSystem->RefreshStorageCache();

    if (recreated > 0) {
        ENJIN_LOG_INFO(Editor, "Restored editor state (%zu reset, %zu recreated from incremental capture)", restored, recreated);
    } else {
        ENJIN_LOG_INFO(Editor, "Restored editor state (%zu entities, lightweight)", restored);
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

// Gameplay processing methods (ProcessContactDamage, ProcessPickup,
// UpdateHealthSystems, FlushDeferredDestroys) are now in
// Enjin::Gameplay::GameplayLoop (Engine/src/Gameplay/GameplayLoop.cpp).

} // namespace Editor
} // namespace Enjin
