#pragma once
#include "Enjin/Platform/Platform.h"

class asIScriptEngine;

namespace Enjin {
namespace ECS { class World; class DialogueSystem; class RenderSystem; }
namespace Physics { class IPhysicsBackend; class IPhysicsBackend2D; }
namespace Networking { class NetworkSystem; }
namespace Audio { class SimpleAudio; }
namespace Scene { class SceneManager; class StreamingManager; }
namespace Renderer { class PostProcessing; }
namespace Gameplay { class TieredSaveSystem; class QuestSystem; class CinematicSystem; class ObjectPool; }
namespace Effects { class WeatherSystem; class DestructibleSystem; }
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
void SetBindingsStreaming(Scene::StreamingManager* mgr);
void SetBindingsFlower(ECS::World* world);
void SetBindingsPhysics2D(Physics::IPhysicsBackend2D* physics2d);
void SetBindingsNetworking(Networking::NetworkSystem* net);

} // namespace Scripting
} // namespace Enjin
