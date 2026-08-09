#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

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
        ENJIN_AS_FN(SM_AddState), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_AddTransition(uint64, const string&in, const string&in)",
        ENJIN_AS_FN(SM_AddTransition), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetState(uint64, const string&in)",
        ENJIN_AS_FN(SM_SetState), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetCurrentState(uint64)",
        ENJIN_AS_FN(SM_GetCurrentState), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetPreviousState(uint64)",
        ENJIN_AS_FN(SM_GetPreviousState), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float SM_GetStateTime(uint64)",
        ENJIN_AS_FN(SM_GetStateTime), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SendTrigger(uint64, const string&in)",
        ENJIN_AS_FN(SM_SendTrigger), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetBool(uint64, const string&in, bool)",
        ENJIN_AS_FN(SM_SetBool), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool SM_GetBool(uint64, const string&in)",
        ENJIN_AS_FN(SM_GetBool), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetFloat(uint64, const string&in, float)",
        ENJIN_AS_FN(SM_SetFloat), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float SM_GetFloat(uint64, const string&in)",
        ENJIN_AS_FN(SM_GetFloat), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetInt(uint64, const string&in, int)",
        ENJIN_AS_FN(SM_SetInt), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int SM_GetInt(uint64, const string&in)",
        ENJIN_AS_FN(SM_GetInt), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool SM_HasState(uint64, const string&in)",
        ENJIN_AS_FN(SM_HasState), ENJIN_AS_CALL_CDECL));

    // State callback setters/getters
    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetOnEnter(uint64, const string&in, const string&in)",
        ENJIN_AS_FN(SM_SetOnEnter), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetOnUpdate(uint64, const string&in, const string&in)",
        ENJIN_AS_FN(SM_SetOnUpdate), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void SM_SetOnExit(uint64, const string&in, const string&in)",
        ENJIN_AS_FN(SM_SetOnExit), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetOnEnter(uint64, const string&in)",
        ENJIN_AS_FN(SM_GetOnEnter), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetOnUpdate(uint64, const string&in)",
        ENJIN_AS_FN(SM_GetOnUpdate), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string SM_GetOnExit(uint64, const string&in)",
        ENJIN_AS_FN(SM_GetOnExit), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
