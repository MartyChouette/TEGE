#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Gameplay/SimulationClock.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/CameraController.h"
#include "Enjin/ECS/Systems/ControllerSystem.h"
#include "Enjin/Gameplay/SurfaceResponseSystem.h"
#include "Enjin/Gameplay/ClothSystem.h"
#include "Enjin/ECS/Systems/FlowerSystem.h"
#include "Enjin/ECS/Systems/TweenSystem.h"
#include "Enjin/ECS/Systems/StateMachineSystem.h"
#include "Enjin/ECS/Systems/SwarmSystem.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/ECS/Systems/BehaviorTreeSystem.h"
#include "Enjin/ECS/Systems/AISystem.h"
#include "Enjin/Networking/NetworkSystem.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include "Enjin/Gameplay/Replay.h"
#include "Enjin/Audio/AudioReactiveSystem.h"
#include "Enjin/Editor/AudioEventGraph.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
#include "Enjin/Physics/PhysicsBackendFactory.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/Gameplay/QuestSystem.h"
#include "Enjin/Gameplay/QuestFlow.h"
#include "Enjin/Gameplay/FootstepSystem.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/Gameplay/CinematicSystem.h"
#include "Enjin/Gameplay/CameraDirector.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Editor/PlayModeDiff.h"
#include "Enjin/Editor/EditorSettings.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/Effects/Destructible.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/Input/InputAction.h"
#include "Enjin/Input/MIDIInput.h"
#include <atomic>
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Scene { class SceneManager; }
namespace ECS { class RenderSystem; }
namespace Renderer { class PostProcessing; }
namespace Effects { class WeatherSystem; class ParticleSystem; class Water3D; class FluidSimulation; class FluidTerrainCoupling; class CurlNoiseSystem; class ElementalSystem; }
namespace Accessibility { class SubtitleSystem; class AccessibilityAnnouncer; struct RuntimeAccessibilitySettings; class AlternativeInputManager; class AudioVisualIndicatorSystem; class ContentWarningSystem; }
namespace GUI { class UISystem; }
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
                    Renderer::CameraController* cameraController,
                    Scene::SceneManager* sceneManager = nullptr);

    // State transitions
    void Play();      // Enter play mode
    void Pause();     // Pause play mode
    void Resume();    // Resume from pause
    void Stop();      // Exit play mode, restore editor state

    // Update (call every frame)
    void Update(f32 deltaTime);

    // State queries
    PlayState GetState() const { return m_State.load(std::memory_order_relaxed); }
    bool IsPlaying() const { return m_State.load(std::memory_order_relaxed) == PlayState::Playing; }
    bool IsPaused() const { return m_State.load(std::memory_order_relaxed) == PlayState::Paused; }
    bool IsStopped() const { return m_State.load(std::memory_order_relaxed) == PlayState::Stopped; }

    // Get controller system for external configuration
    ECS::ControllerSystem* GetControllerSystem() { return &m_ControllerSystem; }

    // Get flower system
    ECS::FlowerSystem* GetFlowerSystem() { return &m_FlowerSystem; }

    // Get physics systems
    Physics::IPhysicsBackend* GetPhysics() { return m_Physics.get(); }
    Physics::IPhysicsBackend2D* GetPhysics2D() { return m_Physics2D.get(); }

    // Get script systems
    Scripting::ScriptEngine* GetScriptEngine() { return &m_ScriptEngine; }
    Scripting::ScriptSystem* GetScriptSystem() { return &m_ScriptSystem; }
    Scripting::CoroutineScheduler* GetCoroutineScheduler() { return &m_CoroutineScheduler; }
    Scripting::ScriptEventBus* GetEventBus() { return &m_EventBus; }

    // Get gameplay systems
    ECS::EntityEventBus* GetEntityEventBus() { return &m_EntityEventBus; }
    Gameplay::QuestSystem* GetQuestSystem() { return &m_QuestSystem; }
    Gameplay::FootstepSystem* GetFootstepSystem() { return &m_FootstepSystem; }
    Gameplay::ObjectPool* GetObjectPool() { return &m_ObjectPool; }
    Gameplay::CinematicSystem* GetCinematicSystem() { return &m_CinematicSystem; }
    Gameplay::CameraDirector* GetCameraDirector() { return &m_CameraDirector; }
    Gameplay::TieredSaveSystem* GetTieredSaveSystem() { return &m_TieredSaveSystem; }
    ECS::TweenSystem* GetTweenSystem() { return &m_TweenSystem; }
    ECS::StateMachineSystem* GetStateMachineSystem() { return &m_StateMachineSystem; }
    ECS::DialogueSystem* GetDialogueSystem() { return &m_DialogueSystem; }
    ECS::VisualScriptSystem* GetVisualScriptSystem() { return &m_VisualScriptSystem; }
    ECS::BehaviorTreeSystem* GetBehaviorTreeSystem() { return &m_BehaviorTreeSystem; }
    ECS::AISystem* GetAISystem() { return &m_AISystem; }

    Networking::NetworkSystem* GetNetworkSystem() { return &m_NetworkSystem; }
    Scene::StreamingManager* GetStreamingManager() { return &m_StreamingManager; }

    Gameplay::RecordRewindSystem* GetRecordRewindSystem() { return &m_RecordRewindSystem; }

    // Editor debug recording: when enabled, Play() injects a hidden scene-rewind
    // recorder (entity "__DebugRecorder") so every play session records the whole
    // scene, and the editor timeline can pause + step/scrub backward through it.
    // The same machinery games use for rewind mechanics, driven programmatically.
    void SetDebugRecording(bool enabled, f32 seconds) {
        m_DebugRecordEnabled = enabled;
        m_DebugRecordSeconds = seconds;
    }
    ECS::Entity GetDebugRecorderEntity() const { return m_DebugRecorderEntity; }

    // --- Replay (flagship #5 phase 2: shareable input-stream replays) ---
    // Every play session records its input stream (cheap: sparse key lists) plus
    // a compact scene snapshot; the pair replays the session deterministically
    // (same build/machine - see Replay.h). The editor exports/loads the files.
    const Gameplay::ReplayData& GetActiveRecording() const { return m_ActiveRecording; }
    bool HasRecording() const { return !m_ActiveRecording.frames.empty(); }
    // Begin replaying: the editor loads the replay's scene into the world first,
    // then calls this. Play() runs with input injection until frames run out,
    // then auto-pauses (step/scrub backward from there with the debug timeline).
    void StartReplay(Gameplay::ReplayData&& data);
    bool IsReplaying() const { return m_Replaying; }

    // Bookmarks: marked moments (F8 during play, or script exceptions). They
    // ride in the replay file; while paused, the transport jumps the scene to
    // a mark via the debug recorder's snapshot ring.
    void MarkBookmark(const std::string& label);
    const std::vector<Gameplay::ReplayBookmark>& GetSessionBookmarks() const {
        return m_Replaying ? m_ReplayData.bookmarks : m_ActiveRecording.bookmarks;
    }
    f32 GetSessionElapsed() const { return m_SessionElapsed; }

    Audio::SimpleAudio* GetSimpleAudio() { return &m_SimpleAudio; }
    Effects::DestructibleSystem* GetDestructibleSystem() { return &m_DestructibleSystem; }
    Effects::InteractiveWaterSystem* GetInteractiveWaterSystem() { return &m_InteractiveWaterSystem; }
    InputSystem::InputActionMap* GetInputActionMap() { return &m_InputMap; }
    Editor::AudioEventGraphRuntime* GetAudioGraphRuntime() { return &m_AudioGraphRuntime; }

    void SetRenderSystem(ECS::RenderSystem* rs) { m_RenderSystem = rs; }
    void SetPostProcessing(Renderer::PostProcessing* pp) { m_PostProcessing = pp; }
    void SetSubtitleSystem(Accessibility::SubtitleSystem* subs) { m_SubtitleSystem = subs; }
    void SetAnnouncer(Accessibility::AccessibilityAnnouncer* ann) { m_Announcer = ann; }
    void SetAccessibilitySettings(Accessibility::RuntimeAccessibilitySettings* settings) { m_AccessibilitySettings = settings; }
    void SetAlternativeInput(Accessibility::AlternativeInputManager* altInput) { m_AlternativeInput = altInput; }
    void SetAudioIndicators(Accessibility::AudioVisualIndicatorSystem* indicators) { m_AudioIndicators = indicators; }
    void SetContentWarnings(Accessibility::ContentWarningSystem* warnings) { m_ContentWarnings = warnings; }
    void SetUISystem(GUI::UISystem* uiSystem) { m_UISystem = uiSystem; }
    GUI::UISystem* GetUISystem() const { return m_UISystem; }
    void SetWeatherSystem(Effects::WeatherSystem* ws) { m_WeatherSystem = ws; }
    void SetElementalSystem(Effects::ElementalSystem* es) { m_ElementalSystem = es; }
    void SetParticleSystem(Effects::ParticleSystem* ps) { m_ParticleSystem = ps; }
    void SetSceneManager(Scene::SceneManager* sm) { m_SceneManager = sm; }
    void SetWater3D(Effects::Water3D* w) { m_Water3D = w; }
    void SetFluidSimulation(Effects::FluidSimulation* fs) { m_FluidSimulation = fs; }
    void SetFluidTerrainCoupling(Effects::FluidTerrainCoupling* ftc) { m_FluidTerrainCoupling = ftc; }
    void SetCurlNoiseSystem(Effects::CurlNoiseSystem* cn) { m_CurlNoiseSystem = cn; }
    void SetEditorSettings(EditorSettings* settings) { m_EditorSettings = settings; }

    // Game over state (set by UpdateGameOverState in Update)
    bool IsGameOverReady() const { return m_GameOverReady; }

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

    std::atomic<PlayState> m_State = PlayState::Stopped; // ED-C2 fix: atomic for thread-safe state queries

    // Controller system for runtime
    ECS::ControllerSystem m_ControllerSystem;

    // Flower system for runtime
    ECS::FlowerSystem m_FlowerSystem;

    // Surface response: material footstep/impact sound + particle (TotK-style)
    Gameplay::SurfaceResponseSystem m_SurfaceResponseSystem;

    // Grid cloth simulation (flags/capes/curtains, tearable)
    Gameplay::ClothSystem m_ClothSystem;

    // Physics systems (created via factory)
    Gameplay::SimulationClock m_SimClock;
    std::unique_ptr<Physics::IPhysicsBackend> m_Physics;
    std::unique_ptr<Physics::IPhysicsBackend2D> m_Physics2D;

    // Scripting
    Scripting::ScriptEngine m_ScriptEngine;
    Scripting::ScriptSystem m_ScriptSystem;
    Scripting::CoroutineScheduler m_CoroutineScheduler;
    Scripting::ScriptEventBus m_EventBus;

    // C++ entity event bus
    ECS::EntityEventBus m_EntityEventBus;

    // Gameplay systems
    Gameplay::QuestSystem m_QuestSystem;
    Gameplay::FootstepSystem m_FootstepSystem;
    Gameplay::ObjectPool m_ObjectPool;
    Gameplay::CinematicSystem m_CinematicSystem;
    Gameplay::CameraDirector m_CameraDirector;

    // Tween system
    ECS::TweenSystem m_TweenSystem;

    // Swarm system (data-oriented crowd proxies)
    ECS::SwarmSystem m_SwarmSystem;

    // Record & Rewind (Braid / Sands of Time mechanic)
    Gameplay::RecordRewindSystem m_RecordRewindSystem;
    bool m_DebugRecordEnabled = true;
    f32  m_DebugRecordSeconds = 30.0f;
    ECS::Entity m_DebugRecorderEntity = ECS::INVALID_ENTITY;
    Gameplay::ReplayData m_ActiveRecording;   // input stream of the current/last session
    Gameplay::ReplayData m_ReplayData;        // stream being played back
    bool  m_Replaying = false;
    usize m_ReplayCursor = 0;
    f32   m_SessionElapsed = 0.0f;            // seconds simulated this session (record + replay)
    u32   m_PausedScriptCount = 0;            // breakpoint-pause transition guard
    u32   m_LastExceptionCount = 0;           // script-exception watermark for auto-bookmarks

    // Audio-reactive system (beat sync, VU→visual, RTPC, threshold triggers)
    Audio::AudioReactiveSystem m_AudioReactiveSystem;

    // State machine system
    ECS::StateMachineSystem m_StateMachineSystem;

    // Dialogue system
    ECS::DialogueSystem m_DialogueSystem;

    // Visual script system
    ECS::VisualScriptSystem m_VisualScriptSystem;

    // Behavior tree system
    ECS::BehaviorTreeSystem m_BehaviorTreeSystem;

    // AI system
    ECS::AISystem m_AISystem;

    // Network system
    Networking::NetworkSystem m_NetworkSystem;

    // Render system pointers (owned externally)
    ECS::RenderSystem* m_RenderSystem = nullptr;
    Renderer::PostProcessing* m_PostProcessing = nullptr;
    Accessibility::SubtitleSystem* m_SubtitleSystem = nullptr;
    Accessibility::AccessibilityAnnouncer* m_Announcer = nullptr;
    Accessibility::RuntimeAccessibilitySettings* m_AccessibilitySettings = nullptr;
    Accessibility::AlternativeInputManager* m_AlternativeInput = nullptr;
    Accessibility::AudioVisualIndicatorSystem* m_AudioIndicators = nullptr;
    Accessibility::ContentWarningSystem* m_ContentWarnings = nullptr;
    GUI::UISystem* m_UISystem = nullptr;
    u32 m_GameOverRestartListener = 0;   // UI event bus listener id (0 = none)
    bool m_RestartRequested = false;     // Set by "gameover_restart"; handled in Update()
    Effects::WeatherSystem* m_WeatherSystem = nullptr;
    Effects::ParticleSystem* m_ParticleSystem = nullptr;
    Scene::SceneManager* m_SceneManager = nullptr;
    Effects::Water3D* m_Water3D = nullptr;
    Effects::FluidSimulation* m_FluidSimulation = nullptr;
    Effects::FluidTerrainCoupling* m_FluidTerrainCoupling = nullptr;
    Effects::CurlNoiseSystem* m_CurlNoiseSystem = nullptr;
    Effects::ElementalSystem* m_ElementalSystem = nullptr;
    EditorSettings* m_EditorSettings = nullptr;

    // Audio system (owned by PlayMode for script bindings)
    Audio::SimpleAudio m_SimpleAudio;

    // Destructible system (owned by PlayMode for script bindings)
    Effects::DestructibleSystem m_DestructibleSystem;

    // Interactive water system
    Effects::InteractiveWaterSystem m_InteractiveWaterSystem;

    // Input action map for remappable input
    InputSystem::InputActionMap m_InputMap;

    // Audio event graph runtime
    Editor::AudioEventGraphRuntime m_AudioGraphRuntime;

    // MIDI input
    InputSystem::MIDIInput m_MIDIInput;

    // Level streaming
    Scene::StreamingManager m_StreamingManager;

    // Tiered save system
    Gameplay::TieredSaveSystem m_TieredSaveSystem;

    // Frame timing profiler (logs breakdown every N frames during play)
    f32 m_ProfileAccumPhysics = 0.0f;
    f32 m_ProfileAccumECS = 0.0f;
    f32 m_ProfileAccumScripting = 0.0f;
    f32 m_ProfileAccumGameplay = 0.0f;
    f32 m_ProfileAccumTotal = 0.0f;
    u32 m_ProfileFrameCount = 0;

    // Saved editor state (to restore when stopping).
    // Lightweight per-entity gameplay-mutable snapshot — NOT a full scene
    // serialization. Roundtripping the entire scene through JSON every play/stop
    // cycle was OOMing on heavy skeletal meshes (140MB+ JSON for a multi-mesh
    // Mixamo character). The snapshot captures only what gameplay can mutate.
    struct EntitySnapshot {
        Math::Vector3 position;
        Math::Quaternion rotation;
        Math::Vector3 scale;
        bool visible = true;
        bool hadTransform = false;
        std::string name;       // For detecting entity ID recycling
        bool hadName = false;
        // Rigidbody velocities: without these, physics-driven motion leaks across
        // Play/Stop — the body keeps its last linear/angular velocity, so the next
        // Play starts already spinning/drifting from the previous run.
        Math::Vector3 linearVelocity = Math::Vector3(0.0f);
        Math::Vector3 angularVelocity = Math::Vector3(0.0f);
        bool hadRigidbody = false;
    };
    std::unordered_map<u64, EntitySnapshot> m_SavedEntityState;
    // Incremental capture (adr-0004 sibling — kills the play-start hitch): instead of
    // serializing the whole scene to JSON on every Play, a World destroy-observer
    // serializes ONLY the pre-play entities that actually die during play, keyed by
    // their original entity handle, at the moment they die. Stop recreates exactly
    // these. Empty when nothing was destroyed (the common case), so Play pays nothing.
    std::unordered_map<u64, std::string> m_DestroyedEntityJson;
    Math::Vector3 m_SavedCameraPos;
    Math::Quaternion m_SavedCameraRot;
    f32 m_SavedCameraFov = 45.0f;

    // Deferred entity destruction list (shared with GameplayLoop functions)
    std::vector<ECS::Entity> m_DeferredDestroys;

    // Game over
    bool m_GameOverReady = false;

    // Play mode diff
    PlayModeDiff m_PlayModeDiff;
    bool m_ShowDiffDialog = false;
    std::string m_PlayedSceneJson;  // Scene JSON captured at stop, before restore
};

} // namespace Editor
} // namespace Enjin
