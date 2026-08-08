#pragma once
#include "Enjin/Platform/Platform.h"
#include <functional>

class asIScriptEngine;

namespace Enjin {
namespace ECS { class World; class DialogueSystem; class RenderSystem; }
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }
namespace Networking { class NetworkSystem; }
namespace Audio { class SimpleAudio; }
namespace Accessibility { class SubtitleSystem; class AccessibilityAnnouncer; struct RuntimeAccessibilitySettings; }
namespace Scene { class SceneManager; class StreamingManager; }
namespace Renderer { class PostProcessing; }
namespace Gameplay { class TieredSaveSystem; class QuestSystem; class CinematicSystem; class ObjectPool; class RecordRewindSystem; }
namespace Effects { class WeatherSystem; class DestructibleSystem; class ElementalSystem; }
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
void SetBindingsAudio(Audio::SimpleAudio* audio);
void SetBindingsSceneManager(Scene::SceneManager* mgr);
void SetBindingsCoroutineScheduler(CoroutineScheduler* scheduler);
void SetBindingsEventBus(ScriptEventBus* bus);
void SetBindingsScriptEngine(ScriptEngine* engine);
void SetBindingsDialogueSystem(ECS::DialogueSystem* system);
void SetBindingsRenderSystem(ECS::RenderSystem* renderSystem);
void SetBindingsPostProcessing(Renderer::PostProcessing* postProcessing);
void SetBindingsSaveSystem(Gameplay::TieredSaveSystem* sys);
void SetBindingsWeather(Effects::WeatherSystem* weather);
void SetBindingsQuestSystem(Gameplay::QuestSystem* quest);
void SetBindingsCinematicSystem(Gameplay::CinematicSystem* cinematic);
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

// Per-frame script binding housekeeping
void FlushDeferredEntityDestroys();
void ResetFrameEntityCreationCount();

} // namespace Scripting
} // namespace Enjin
