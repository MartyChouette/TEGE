#pragma once
#include "Enjin/Platform/Platform.h"

class asIScriptEngine;

namespace Enjin {
namespace ECS { class World; class DialogueSystem; class RenderSystem; }
namespace Physics { class SimplePhysics; }
namespace Audio { class SimpleAudio; }
namespace Scene { class SceneManager; }
namespace Renderer { class PostProcessing; }
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

// Set subsystem pointers for script bindings
void SetBindingsWorld(ECS::World* world);
void SetBindingsPhysics(Physics::SimplePhysics* physics);
void SetBindingsAudio(Audio::SimpleAudio* audio);
void SetBindingsSceneManager(Scene::SceneManager* mgr);
void SetBindingsCoroutineScheduler(CoroutineScheduler* scheduler);
void SetBindingsEventBus(ScriptEventBus* bus);
void SetBindingsScriptEngine(ScriptEngine* engine);
void SetBindingsDialogueSystem(ECS::DialogueSystem* system);
void SetBindingsRenderSystem(ECS::RenderSystem* renderSystem);
void SetBindingsPostProcessing(Renderer::PostProcessing* postProcessing);

} // namespace Scripting
} // namespace Enjin
