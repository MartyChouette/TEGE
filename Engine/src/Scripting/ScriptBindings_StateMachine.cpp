#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// SM_ wrapper functions
// ============================================================================

static void SM_AddState(u64 entityId, const std::string& name) {
    if (!s_BindingsWorld) return;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (!sm) return;
    // Don't add duplicate
    for (const auto& s : sm->states) {
        if (s.name == name) return;
    }
    SMState s;
    s.name = name;
    sm->states.push_back(s);
    // Auto-set current state if this is the first one
    if (sm->states.size() == 1 && sm->currentState.empty()) {
        sm->currentState = name;
    }
}

static void SM_AddTransition(u64 entityId, const std::string& fromState, const std::string& toState) {
    if (!s_BindingsWorld) return;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (!sm) return;
    for (auto& s : sm->states) {
        if (s.name == fromState) {
            SMTransition t;
            t.toState = toState;
            s.transitions.push_back(t);
            return;
        }
    }
}

static void SM_SetState(u64 entityId, const std::string& name) {
    if (!s_BindingsWorld) return;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (sm) sm->SetState(name);
}

static std::string SM_GetCurrentState(u64 entityId) {
    if (!s_BindingsWorld) return "";
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    return sm ? sm->currentState : std::string("");
}

static std::string SM_GetPreviousState(u64 entityId) {
    if (!s_BindingsWorld) return "";
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    return sm ? sm->previousState : std::string("");
}

static f32 SM_GetStateTime(u64 entityId) {
    if (!s_BindingsWorld) return 0.0f;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    return sm ? sm->stateTime : 0.0f;
}

static void SM_SendTrigger(u64 entityId, const std::string& trigger) {
    if (!s_BindingsWorld) return;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (sm) sm->SendTrigger(trigger);
}

static void SM_SetBool(u64 entityId, const std::string& param, bool value) {
    if (!s_BindingsWorld) return;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (sm) sm->SetBool(param, value);
}

static bool SM_GetBool(u64 entityId, const std::string& param) {
    if (!s_BindingsWorld) return false;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    return sm ? sm->GetBool(param) : false;
}

static void SM_SetFloat(u64 entityId, const std::string& param, f32 value) {
    if (!s_BindingsWorld) return;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (sm) sm->SetFloat(param, value);
}

static f32 SM_GetFloat(u64 entityId, const std::string& param) {
    if (!s_BindingsWorld) return 0.0f;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    return sm ? sm->GetFloat(param) : 0.0f;
}

static void SM_SetInt(u64 entityId, const std::string& param, i32 value) {
    if (!s_BindingsWorld) return;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (sm) sm->SetInt(param, value);
}

static i32 SM_GetInt(u64 entityId, const std::string& param) {
    if (!s_BindingsWorld) return 0;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    return sm ? sm->GetInt(param) : 0;
}

static bool SM_HasState(u64 entityId, const std::string& name) {
    if (!s_BindingsWorld) return false;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    return sm ? sm->HasState(name) : false;
}

// Helper to find a mutable SMState by name
static SMState* FindSMState(u64 entityId, const std::string& stateName) {
    if (!s_BindingsWorld) return nullptr;
    auto* sm = s_BindingsWorld->GetComponent<StateMachineComponent>(static_cast<Entity>(entityId));
    if (!sm) return nullptr;
    for (auto& s : sm->states) {
        if (s.name == stateName) return &s;
    }
    return nullptr;
}

static void SM_SetOnEnter(u64 entityId, const std::string& stateName, const std::string& funcName) {
    auto* state = FindSMState(entityId, stateName);
    if (state) state->onEnter = funcName;
}

static void SM_SetOnUpdate(u64 entityId, const std::string& stateName, const std::string& funcName) {
    auto* state = FindSMState(entityId, stateName);
    if (state) state->onUpdate = funcName;
}

static void SM_SetOnExit(u64 entityId, const std::string& stateName, const std::string& funcName) {
    auto* state = FindSMState(entityId, stateName);
    if (state) state->onExit = funcName;
}

static std::string SM_GetOnEnter(u64 entityId, const std::string& stateName) {
    auto* state = FindSMState(entityId, stateName);
    return state ? state->onEnter : std::string("");
}

static std::string SM_GetOnUpdate(u64 entityId, const std::string& stateName) {
    auto* state = FindSMState(entityId, stateName);
    return state ? state->onUpdate : std::string("");
}

static std::string SM_GetOnExit(u64 entityId, const std::string& stateName) {
    auto* state = FindSMState(entityId, stateName);
    return state ? state->onExit : std::string("");
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterStateMachineBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_AddState(uint64, const string&in)",
        asFUNCTION(SM_AddState), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_AddTransition(uint64, const string&in, const string&in)",
        asFUNCTION(SM_AddTransition), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetState(uint64, const string&in)",
        asFUNCTION(SM_SetState), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetCurrentState(uint64)",
        asFUNCTION(SM_GetCurrentState), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetPreviousState(uint64)",
        asFUNCTION(SM_GetPreviousState), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float SM_GetStateTime(uint64)",
        asFUNCTION(SM_GetStateTime), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SendTrigger(uint64, const string&in)",
        asFUNCTION(SM_SendTrigger), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetBool(uint64, const string&in, bool)",
        asFUNCTION(SM_SetBool), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool SM_GetBool(uint64, const string&in)",
        asFUNCTION(SM_GetBool), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetFloat(uint64, const string&in, float)",
        asFUNCTION(SM_SetFloat), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float SM_GetFloat(uint64, const string&in)",
        asFUNCTION(SM_GetFloat), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetInt(uint64, const string&in, int)",
        asFUNCTION(SM_SetInt), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int SM_GetInt(uint64, const string&in)",
        asFUNCTION(SM_GetInt), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool SM_HasState(uint64, const string&in)",
        asFUNCTION(SM_HasState), asCALL_CDECL));

    // State callback setters/getters
    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetOnEnter(uint64, const string&in, const string&in)",
        asFUNCTION(SM_SetOnEnter), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetOnUpdate(uint64, const string&in, const string&in)",
        asFUNCTION(SM_SetOnUpdate), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetOnExit(uint64, const string&in, const string&in)",
        asFUNCTION(SM_SetOnExit), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetOnEnter(uint64, const string&in)",
        asFUNCTION(SM_GetOnEnter), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetOnUpdate(uint64, const string&in)",
        asFUNCTION(SM_GetOnUpdate), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetOnExit(uint64, const string&in)",
        asFUNCTION(SM_GetOnExit), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
