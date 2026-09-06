#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include <vector>
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/VisualScript/ScriptApiNodes.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

// ============================================================================
// LIFECYCLE
// ============================================================================

void VisualScriptSystem::SetScriptEngine(Scripting::ScriptEngine* engine) {
    m_Executor.SetScriptEngine(engine);
    // One-time script-API node codegen: by the time a runtime hands the VS
    // system its script engine, every binding is registered - reflect them
    // into visual-script nodes (idempotent; editor boot may have done it
    // already for the palette).
    if (engine && engine->GetASEngine())
        VisualScript::RegisterScriptApiNodes(engine->GetASEngine());
}

void VisualScriptSystem::Initialize() {
    if (!m_World) return;

    m_FirstUpdate = true;

    // Call OnCreate for all visual scripts.
    //
    // The entity list is COPIED and the component pointer is re-resolved after
    // the call. ExecuteEvent runs arbitrary graph code, and the visual-script
    // node set can create entities and add components -- which reallocates the
    // dense storage that GetEntitiesWithComponent returns a live reference to,
    // and invalidates `script` with it. Iterating that reference directly while
    // the executor mutates it is a use-after-free.
    //
    // Same pattern ScriptSystem uses for exactly the same reason.
    const std::vector<Entity> entities = m_World->GetEntitiesWithComponent<VisualScriptComponent>();
    for (Entity entity : entities) {
        if (!m_World->IsValid(entity)) continue;
        auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
        if (script && script->enabled && !script->initialized) {
            m_Executor.ExecuteEvent(m_World, entity, script,
                                    VisualScriptEvent::OnCreate, 0.0f);
            // Re-resolve: the call above may have moved the storage.
            if (auto* after = m_World->GetComponent<VisualScriptComponent>(entity)) {
                after->initialized = true;
            }
        }
    }

    ENJIN_LOG_INFO(Script, "VisualScriptSystem initialized");
}

void VisualScriptSystem::Shutdown() {
    if (!m_World) return;

    // Call OnDestroy and reset runtime state. Copied + re-resolved for the same
    // reason as Initialize above: OnDestroy graphs run arbitrary node code.
    const std::vector<Entity> entities = m_World->GetEntitiesWithComponent<VisualScriptComponent>();
    for (Entity entity : entities) {
        if (!m_World->IsValid(entity)) continue;
        auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
        if (script && script->enabled) {
            m_Executor.ExecuteEvent(m_World, entity, script,
                                    VisualScriptEvent::OnDestroy, 0.0f);
            if (auto* after = m_World->GetComponent<VisualScriptComponent>(entity)) {
                after->ResetRuntimeState();
            }
        }
    }

    ENJIN_LOG_INFO(Script, "VisualScriptSystem shut down");
}

// ============================================================================
// UPDATE
// ============================================================================

void VisualScriptSystem::Update(f32 deltaTime) {
    if (!m_Enabled || !m_World) return;

    // Copy entity list before iterating — script execution may destroy entities
    auto entities = m_World->GetEntitiesWithComponent<VisualScriptComponent>();
    std::vector<Entity> entitySnapshot(entities.begin(), entities.end());

    for (Entity entity : entitySnapshot) {
        // Verify entity still exists (scripts may destroy other entities)
        if (!m_World->IsValid(entity)) continue;

        auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
        if (!script || !script->enabled) continue;

        // Initialize if needed (for entities spawned after Initialize())
        if (!script->initialized) {
            m_Executor.ExecuteEvent(m_World, entity, script,
                                    VisualScriptEvent::OnCreate, deltaTime);
            script->initialized = true;
        }

        // OnStart - called once on first frame
        if (!script->started) {
            m_Executor.ExecuteEvent(m_World, entity, script,
                                    VisualScriptEvent::OnStart, deltaTime);
            script->started = true;
        }

        // OnUpdate - called every frame
        m_Executor.ExecuteEvent(m_World, entity, script,
                                VisualScriptEvent::OnUpdate, deltaTime);

        // Update latent nodes (Delay, etc.)
        m_Executor.UpdateLatentNodes(m_World, entity, script, deltaTime);
    }

    m_FirstUpdate = false;
}

// ============================================================================
// CUSTOM EVENTS
// ============================================================================

void VisualScriptSystem::BroadcastEvent(const std::string& eventName, f32 deltaTime) {
    if (!m_World) return;

    // Copy entity list — event handlers may destroy entities
    auto entities = m_World->GetEntitiesWithComponent<VisualScriptComponent>();
    std::vector<Entity> entitySnapshot(entities.begin(), entities.end());

    for (Entity entity : entitySnapshot) {
        if (!m_World->IsValid(entity)) continue;
        auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
        if (script && script->enabled) {
            m_Executor.ExecuteCustomEvent(m_World, entity, script, eventName, deltaTime);
        }
    }
}

void VisualScriptSystem::SendEvent(Entity entity, const std::string& eventName, f32 deltaTime) {
    if (!m_World) return;

    auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
    if (script && script->enabled) {
        m_Executor.ExecuteCustomEvent(m_World, entity, script, eventName, deltaTime);
    }
}

// ============================================================================
// COLLISION/TRIGGER EVENTS
// ============================================================================

void VisualScriptSystem::OnCollisionEnter(Entity entity, Entity other, f32 deltaTime) {
    if (!m_World) return;

    auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
    if (script && script->enabled) {
        m_Executor.ExecuteEvent(m_World, entity, script,
                                VisualScriptEvent::OnCollisionEnter, deltaTime, other);
    }
}

void VisualScriptSystem::OnCollisionExit(Entity entity, Entity other, f32 deltaTime) {
    if (!m_World) return;

    auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
    if (script && script->enabled) {
        m_Executor.ExecuteEvent(m_World, entity, script,
                                VisualScriptEvent::OnCollisionExit, deltaTime, other);
    }
}

void VisualScriptSystem::OnTriggerEnter(Entity entity, Entity other, f32 deltaTime) {
    if (!m_World) return;

    auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
    if (script && script->enabled) {
        m_Executor.ExecuteEvent(m_World, entity, script,
                                VisualScriptEvent::OnTriggerEnter, deltaTime, other);
    }
}

void VisualScriptSystem::OnTriggerExit(Entity entity, Entity other, f32 deltaTime) {
    if (!m_World) return;

    auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
    if (script && script->enabled) {
        m_Executor.ExecuteEvent(m_World, entity, script,
                                VisualScriptEvent::OnTriggerExit, deltaTime, other);
    }
}

} // namespace ECS
} // namespace Enjin
