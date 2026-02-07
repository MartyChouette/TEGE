#include "Enjin/ECS/Systems/VisualScriptSystem.h"
#include "Enjin/ECS/Components/VisualScript.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

// ============================================================================
// LIFECYCLE
// ============================================================================

void VisualScriptSystem::Initialize() {
    if (!m_World) return;

    m_FirstUpdate = true;

    // Call OnCreate for all visual scripts
    for (Entity entity : m_World->GetAllEntities()) {
        auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
        if (script && script->enabled && !script->initialized) {
            m_Executor.ExecuteEvent(m_World, entity, script,
                                    VisualScriptEvent::OnCreate, 0.0f);
            script->initialized = true;
        }
    }

    ENJIN_LOG_INFO(Script, "VisualScriptSystem initialized");
}

void VisualScriptSystem::Shutdown() {
    if (!m_World) return;

    // Call OnDestroy and reset runtime state
    for (Entity entity : m_World->GetAllEntities()) {
        auto* script = m_World->GetComponent<VisualScriptComponent>(entity);
        if (script && script->enabled) {
            m_Executor.ExecuteEvent(m_World, entity, script,
                                    VisualScriptEvent::OnDestroy, 0.0f);
            script->ResetRuntimeState();
        }
    }

    ENJIN_LOG_INFO(Script, "VisualScriptSystem shut down");
}

// ============================================================================
// UPDATE
// ============================================================================

void VisualScriptSystem::Update(f32 deltaTime) {
    if (!m_World) return;

    for (Entity entity : m_World->GetAllEntities()) {
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
    }

    m_FirstUpdate = false;
}

// ============================================================================
// CUSTOM EVENTS
// ============================================================================

void VisualScriptSystem::BroadcastEvent(const std::string& eventName, f32 deltaTime) {
    if (!m_World) return;

    for (Entity entity : m_World->GetAllEntities()) {
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
