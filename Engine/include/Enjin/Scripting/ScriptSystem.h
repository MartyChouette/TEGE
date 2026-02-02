#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Script.h"

namespace Enjin {
namespace Scripting {

class ScriptEngine;
class CoroutineScheduler;

// ECS system that iterates entities with ScriptComponent and dispatches
// lifecycle callbacks to their AngelScript instances.
//
// Execution order per frame:
//   1. ScriptEngine::PollFileChanges() + ProcessHotReload()
//   2. New scripts: CreateInstance -> OnCreate -> OnEnable
//   3. Unstarted scripts: OnStart
//   4. Fixed timestep loop: OnFixedUpdate(fixedDt)
//   5. OnUpdate(deltaTime)
//   6. CoroutineScheduler::Update(deltaTime)
//   7. OnLateUpdate(deltaTime)
class ENJIN_API ScriptSystem {
public:
    ScriptSystem();
    ~ScriptSystem();

    void SetWorld(ECS::World* world) { m_World = world; }
    void SetScriptEngine(ScriptEngine* engine) { m_ScriptEngine = engine; }
    void SetCoroutineScheduler(CoroutineScheduler* scheduler) { m_Scheduler = scheduler; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Main update — call once per frame
    void Update(f32 deltaTime);

    // Fixed update — call from fixed timestep loop
    void FixedUpdate(f32 fixedDeltaTime);

    // Late update — call after main Update
    void LateUpdate(f32 deltaTime);

    // Initialize all scripts on entities (called when entering play mode)
    void InitializeAllScripts();

    // Shutdown all scripts (called when exiting play mode)
    void ShutdownAllScripts();

    // Dispatch collision callbacks
    void OnCollisionEnter(ECS::Entity entity, ECS::Entity other);
    void OnCollisionStay(ECS::Entity entity, ECS::Entity other);
    void OnCollisionExit(ECS::Entity entity, ECS::Entity other);
    void OnTriggerEnter(ECS::Entity entity, ECS::Entity other);
    void OnTriggerExit(ECS::Entity entity, ECS::Entity other);

private:
    // Initialize a single script attachment (create instance, call OnCreate)
    void InitScript(ECS::Entity entity, ECS::ScriptAttachment& script);

    // Call a void(void) lifecycle method on a script
    bool CallLifecycleMethod(ECS::ScriptAttachment& script, int methodId, const char* methodName);

    // Call a void(float) lifecycle method
    bool CallLifecycleMethodFloat(ECS::ScriptAttachment& script, int methodId, const char* methodName, f32 value);

    // Call a void(Entity) collision method
    bool CallCollisionMethod(ECS::ScriptAttachment& script, int methodId, const char* methodName, ECS::Entity other);

    // Cache method IDs for a script attachment
    void CacheMethodIds(ECS::ScriptAttachment& script);

    // Handle script error
    void HandleScriptError(ECS::ScriptAttachment& script, const char* methodName);

    ECS::World* m_World = nullptr;
    ScriptEngine* m_ScriptEngine = nullptr;
    CoroutineScheduler* m_Scheduler = nullptr;
    bool m_Enabled = false;

    // Fixed timestep accumulator
    f32 m_FixedTimeAccumulator = 0.0f;
    static constexpr f32 FIXED_TIMESTEP = 1.0f / 60.0f;
};

} // namespace Scripting
} // namespace Enjin
