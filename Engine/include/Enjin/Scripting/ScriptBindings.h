#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <functional>
#include <string>

class asIScriptEngine;

namespace Enjin {
namespace ECS { class World; class DialogueSystem; class RenderSystem; }
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }
namespace Networking { class NetworkSystem; }
namespace Audio { class SimpleAudio; }
namespace Accessibility { class SubtitleSystem; class AccessibilityAnnouncer; struct RuntimeAccessibilitySettings; }
namespace Scene { class SceneManager; class StreamingManager; }
namespace Renderer { class PostProcessing; class Camera; }
namespace Gameplay { class TieredSaveSystem; class QuestSystem; class CinematicSystem; class ObjectPool; class RecordRewindSystem; class CameraDirector; }
namespace Effects { class WeatherSystem; class WindSystem; class DestructibleSystem; class ElementalSystem; class WorldTimeSystem; class SeasonalWeatherSystem; }
namespace Procedural { class LevelGenerator; }
namespace Plugin { class PluginSystem; }
namespace Audio { class AudioEventGraphRuntime; }
namespace InputSystem { class MIDIInput; class InputActionMap; }
namespace Scripting {

class ScriptEngine;
class CoroutineScheduler;
class ScriptEventBus;

// Register all engine types and functions into the AngelScript engine
ENJIN_API void RegisterAllBindings(asIScriptEngine* engine);

// Walk every registered enum/type/function on a bound engine and emit an
// AngelScript "stub" (declarations only, no bodies) for editor IntelliSense.
// The real implementations stay in C++; this file just lets an editor index
// the API. Call on an engine that has already had RegisterAllBindings() run.
ENJIN_API std::string GenerateApiStub(asIScriptEngine* engine);

// Individual registration functions
void RegisterMathTypes(asIScriptEngine* engine);
void RegisterEntityTypes(asIScriptEngine* engine);
void RegisterInputBindings(asIScriptEngine* engine);
void RegisterPhysicsBindings(asIScriptEngine* engine);
void RegisterAudioBindings(asIScriptEngine* engine);
void RegisterSceneBindings(asIScriptEngine* engine);
void RegisterTimeBindings(asIScriptEngine* engine);
void RegisterDebugBindings(asIScriptEngine* engine);
void RegisterComponentBindings(asIScriptEngine* engine);
void RegisterCoroutineBindings(asIScriptEngine* engine);
void RegisterEventBindings(asIScriptEngine* engine);
void RegisterTweenBindings(asIScriptEngine* engine);
void RegisterStateMachineBindings(asIScriptEngine* engine);
void RegisterDialogueBindings(asIScriptEngine* engine);
void RegisterRenderBindings(asIScriptEngine* engine);
void RegisterNoiseBindings(asIScriptEngine* engine);
void RegisterSaveBindings(asIScriptEngine* engine);
void RegisterWeatherBindings(asIScriptEngine* engine);
void RegisterGameplayBindings(asIScriptEngine* engine);
void RegisterUIBindings(asIScriptEngine* engine);
void RegisterParticleBindings(asIScriptEngine* engine);
void RegisterWaterBindings(asIScriptEngine* engine);
void RegisterGameplayComponentBindings(asIScriptEngine* engine);
void RegisterPrefabBindings(asIScriptEngine* engine);
void RegisterStreamingBindings(asIScriptEngine* engine);
void RegisterFlowerBindings(asIScriptEngine* engine);
void RegisterPhysics2DBindings(asIScriptEngine* engine);
void RegisterNetworkBindings(asIScriptEngine* engine);
void RegisterSpriteBindings(asIScriptEngine* engine);
void RegisterHUDBindings(asIScriptEngine* engine);
void RegisterTextBindings(asIScriptEngine* engine);
void RegisterElementalBindings(asIScriptEngine* engine);
void RegisterRewindBindings(asIScriptEngine* engine);
void RegisterAudioReactiveBindings(asIScriptEngine* engine);

// Set subsystem pointers for script bindings
void SetBindingsWorld(ECS::World* world);
void SetBindingsPhysics(Physics::IPhysicsBackend* physics);
// Push the active render camera + viewport so screen-space script queries (Physics_RaycastScreen) work.
void SetBindingsRenderView(const Renderer::Camera* camera, f32 viewportWidth, f32 viewportHeight);
// Screen-point pick through that same render view (same ray as the script-facing
// Physics_RaycastScreen). 0 = nothing hit / no camera / web. Drives the
// OnMouseEnter/OnMouseExit/OnClick script callbacks in ScriptSystem.
ENJIN_API u64 BindingsPickEntityAtScreen(f32 screenX, f32 screenY);
void SetBindingsAudio(Audio::SimpleAudio* audio);
void SetBindingsSceneManager(Scene::SceneManager* mgr);
void SetBindingsFlowAdvanceFlag(bool* flag);
void SetBindingsCoroutineScheduler(CoroutineScheduler* scheduler);
void SetBindingsEventBus(ScriptEventBus* bus);
// Release every Events_Listen callback/object the bus holds. Call on script
// teardown (Play->Stop) so the AddRef'd delegates don't survive to the engine's
// shutdown GC (which then can't collect them: "GC cannot destroy $func") and so
// listeners don't leak across play sessions.
// Advance the script-visible clock. ScriptSystem::Update calls this, so every
// runtime gets it without another hand-maintained list. Without it
// Time_GetTime(), Time_GetDeltaTime() and Time_GetFrameCount() return 0 forever.
void TickBindingsTime(f32 deltaTime, f32 fixedDeltaTime = 0.0f);
void ResetBindingsTime();

void ClearBindingsEventListeners();

// Drop just one entity's listeners, for a despawn. ScriptSystem's destroy
// observer calls this; without it a dead entity's callbacks kept firing.
void RemoveBindingsEventListenersForEntity(u64 entityId);
void SetBindingsScriptEngine(ScriptEngine* engine);
void SetBindingsDialogueSystem(ECS::DialogueSystem* system);
void SetBindingsRenderSystem(ECS::RenderSystem* renderSystem);
void SetBindingsPostProcessing(Renderer::PostProcessing* postProcessing);
void SetBindingsSaveSystem(Gameplay::TieredSaveSystem* sys);
void SetBindingsWeather(Effects::WeatherSystem* weather);
void SetBindingsWind(Effects::WindSystem* wind);
void SetBindingsWorldTime(Effects::WorldTimeSystem* time,
                          Effects::SeasonalWeatherSystem* seasonal);
void SetBindingsQuestSystem(Gameplay::QuestSystem* quest);
void SetBindingsCinematicSystem(Gameplay::CinematicSystem* cinematic);
void SetBindingsCameraDirector(Gameplay::CameraDirector* director);
void SetBindingsObjectPool(Gameplay::ObjectPool* pool);
void SetBindingsDestructible(Effects::DestructibleSystem* destructible);
void SetBindingsRewindSystem(Gameplay::RecordRewindSystem* system);
void SetBindingsElemental(Effects::ElementalSystem* system);
void SetBindingsStreaming(Scene::StreamingManager* mgr);
void SetBindingsFlower(ECS::World* world);
void SetBindingsPhysics2D(Physics::IPhysicsBackend2D* physics2d);
void SetBindingsNetworking(Networking::NetworkSystem* net);
void SetBindingsProcedural(Procedural::LevelGenerator* generator);

// Accessibility binding setters
void SetBindingsSubtitles(Accessibility::SubtitleSystem* subtitles);
void SetBindingsAnnouncer(Accessibility::AccessibilityAnnouncer* announcer);
void SetBindingsAccessibilitySettings(Accessibility::RuntimeAccessibilitySettings* settings);
void SetBindingsAccessibilitySaveCallback(std::function<void()> callback);
void SetBindingsDyslexiaFontCallback(std::function<void(bool)> callback);
void SetBindingsAccessibilityApplyCallback(std::function<void()> callback);

// VisualScript node forwarders — reuse the AngelScript accessibility wiring
// (settings pointer + apply/save callbacks). See ScriptBindings_Accessibility.cpp.
void VSAccessSetFontScale(f32 v);          f32  VSAccessGetFontScale();
void VSAccessSetReducedMotion(bool v);     bool VSAccessGetReducedMotion();
void VSAccessSetDisableScreenShake(bool v); bool VSAccessGetDisableScreenShake();
void VSAccessSetContrast(f32 v);           f32  VSAccessGetContrast();
void VSAccessSetColorblindStrength(f32 v); f32  VSAccessGetColorblindStrength();
void VSAccessSetSubtitles(bool v);         bool VSAccessGetSubtitles();
void VSAccessSetDyslexiaFont(bool v);      bool VSAccessGetDyslexiaFont();
void VSAccessSetScreenReader(bool v);      bool VSAccessGetScreenReader();
void VSAccessSave();

// VisualScript input-action node forwarders — reuse the AngelScript action-map
// wiring. See ScriptBindings_InputAction.cpp.
bool VSInputActionIsDown(i32 action);
bool VSInputActionIsPressed(i32 action);
f32  VSInputActionGetValue(i32 action);
i32  VSInputActionCount();
std::string VSInputActionName(i32 index);
std::string VSInputBindingName(i32 index);
void VSInputRebind(i32 actionIndex, i32 keyCode);
i32  VSInputPollKey();
void VSInputResetBindings();

// VisualScript navmesh node forwarders — reuse the AngelScript navmesh wiring
// (navmesh + pathfinder pointers + last-path buffer). See ScriptBindings_AI.cpp.
bool VSNavmeshHasNavmesh();
bool VSNavmeshIsPointOn(f32 x, f32 y, f32 z);
i32  VSNavmeshFindPath(f32 sx, f32 sy, f32 sz, f32 ex, f32 ey, f32 ez);
bool VSNavmeshPathExists(f32 sx, f32 sy, f32 sz, f32 ex, f32 ey, f32 ez);
void VSNavmeshGetWaypoint(i32 index, f32& x, f32& y, f32& z);

// Registration for accessibility bindings (defined in ScriptBindings_Accessibility.cpp)
void RegisterAccessibilityBindings(asIScriptEngine* engine);

// Registration for AI bindings (defined in ScriptBindings_AI.cpp)
void RegisterAIBindings(asIScriptEngine* engine);

// Registration for procedural generation bindings (defined in ScriptBindings_Procedural.cpp)
void RegisterProceduralBindings(asIScriptEngine* engine);

// Registration for plugin bindings (defined in ScriptBindings_Plugin.cpp)
void RegisterPluginBindings(asIScriptEngine* engine);

// Registration for audio event graph bindings (defined in ScriptBindings_AudioGraph.cpp)
void RegisterAudioGraphBindings(asIScriptEngine* engine);

// Plugin system setter
void SetBindingsPluginSystem(Plugin::PluginSystem* system);

// Audio event graph runtime setter
void SetBindingsAudioGraphRuntime(Audio::AudioEventGraphRuntime* runtime);

// Registration for MIDI input bindings (defined in ScriptBindings_MIDI.cpp)
void RegisterMIDIBindings(asIScriptEngine* engine);

// MIDI input setter
void SetBindingsMIDI(InputSystem::MIDIInput* midi);

// Registration for input action map bindings (defined in ScriptBindings_InputAction.cpp)
void RegisterInputActionBindings(asIScriptEngine* engine);

// Input action map setter
void SetBindingsInputActionMap(InputSystem::InputActionMap* map);

// Global time scale (slow-mo/hitstop). Runtimes multiply the dt they hand to
// GAMEPLAY systems by this; UI/editor/frame limiter stay unscaled. Reset to 1
// on play start so an effect never leaks across sessions.
ENJIN_API f32  GetTimeScale();
ENJIN_API void SetTimeScale(f32 scale);

// Per-frame script binding housekeeping
void FlushDeferredEntityDestroys();
void ResetFrameEntityCreationCount();

} // namespace Scripting
} // namespace Enjin
