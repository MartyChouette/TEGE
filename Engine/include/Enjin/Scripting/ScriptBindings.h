#pragma once
#include "Enjin/Platform/Platform.h"

class asIScriptEngine;

namespace Enjin {
namespace ECS { class World; }
namespace Physics { class SimplePhysics; }
namespace Audio { class SimpleAudio; }
namespace Scene { class SceneManager; }
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

// Set subsystem pointers for script bindings
void SetBindingsWorld(ECS::World* world);
void SetBindingsPhysics(Physics::SimplePhysics* physics);
void SetBindingsAudio(Audio::SimpleAudio* audio);
void SetBindingsSceneManager(Scene::SceneManager* mgr);
void SetBindingsCoroutineScheduler(CoroutineScheduler* scheduler);
void SetBindingsEventBus(ScriptEventBus* bus);
void SetBindingsScriptEngine(ScriptEngine* engine);

} // namespace Scripting
} // namespace Enjin
