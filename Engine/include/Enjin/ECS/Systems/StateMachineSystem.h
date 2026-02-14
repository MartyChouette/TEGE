#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"

namespace Enjin {
namespace Scripting { class ScriptEngine; }
namespace ECS {

class ENJIN_API StateMachineSystem {
public:
    StateMachineSystem() = default;
    ~StateMachineSystem() = default;

    // Enable/disable state machine updates
    void SetEnabled(bool enabled) { m_Enabled = enabled; }
    bool IsEnabled() const { return m_Enabled; }

    // Set script engine for state callbacks (optional — callbacks silently skipped if null)
    void SetScriptEngine(Scripting::ScriptEngine* engine) { m_ScriptEngine = engine; }

    // Evaluate transitions and advance state timers
    void Update(World* world, f32 deltaTime);

private:
    // Invoke a named callback on all enabled script attachments of an entity
    void InvokeCallback(World* world, Entity entity, const std::string& methodName);

    Scripting::ScriptEngine* m_ScriptEngine = nullptr;
    bool m_Enabled = true;
};

} // namespace ECS
} // namespace Enjin
