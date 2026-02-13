#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/System.h"
#include "Enjin/ECS/World.h"
#include "Enjin/VisualScript/VisualScriptExecutor.h"

namespace Enjin {
namespace ECS {

// ============================================================================
// VISUAL SCRIPT SYSTEM
// ============================================================================

// System that executes VisualScriptComponents during play mode
class ENJIN_API VisualScriptSystem : public ISystem {
public:
    VisualScriptSystem() = default;
    ~VisualScriptSystem() override = default;

    // Set the world to operate on
    void SetWorld(World* world) { m_World = world; }

    // Set physics system for physics query nodes
    void SetPhysics(Physics::IPhysicsBackend* physics) { m_Executor.SetPhysics(physics); }

    // Set 2D physics system for 2D physics query nodes
    void SetPhysics2D(Physics::IPhysicsBackend2D* physics2d) { m_Executor.SetPhysics2D(physics2d); }

    // Set script engine for interop nodes
    void SetScriptEngine(Scripting::ScriptEngine* engine) { m_Executor.SetScriptEngine(engine); }

    // Set networking system for multiplayer nodes
    void SetNetworking(Networking::NetworkSystem* net) { m_Executor.SetNetworking(net); }

    // Set streaming manager for level streaming nodes
    void SetStreaming(Scene::StreamingManager* sm) { m_Executor.SetStreaming(sm); }

    // Called once when play mode starts
    void Initialize();

    // Called once when play mode stops
    void Shutdown();

    // ISystem interface
    void Update(f32 deltaTime) override;

    // Access the executor (for debugging/stats)
    VisualScript::VisualScriptExecutor& GetExecutor() { return m_Executor; }
    const VisualScript::VisualScriptExecutor& GetExecutor() const { return m_Executor; }

    // Trigger a custom event on all visual scripts
    void BroadcastEvent(const std::string& eventName, f32 deltaTime);

    // Trigger a custom event on a specific entity
    void SendEvent(Entity entity, const std::string& eventName, f32 deltaTime);

    // Trigger collision/trigger events on an entity
    void OnCollisionEnter(Entity entity, Entity other, f32 deltaTime);
    void OnCollisionExit(Entity entity, Entity other, f32 deltaTime);
    void OnTriggerEnter(Entity entity, Entity other, f32 deltaTime);
    void OnTriggerExit(Entity entity, Entity other, f32 deltaTime);

private:
    World* m_World = nullptr;
    VisualScript::VisualScriptExecutor m_Executor;

    // Track first update to call OnStart
    bool m_FirstUpdate = true;
};

} // namespace ECS
} // namespace Enjin
