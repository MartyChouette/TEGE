#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;
static ECS::DialogueSystem* s_BindingsDialogueSystem = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsDialogueSystem(ECS::DialogueSystem* system) {
    s_BindingsDialogueSystem = system;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Dialogue_ wrapper functions
// ============================================================================

static void Dialogue_Start(u64 entityId) {
    if (!s_BindingsDialogueSystem || !s_BindingsWorld) return;
    s_BindingsDialogueSystem->StartDialogue(s_BindingsWorld, static_cast<Entity>(entityId));
}

static void Dialogue_Advance(u64 entityId) {
    if (!s_BindingsDialogueSystem || !s_BindingsWorld) return;
    s_BindingsDialogueSystem->Advance(s_BindingsWorld, static_cast<Entity>(entityId));
}

static void Dialogue_Choose(u64 entityId, u32 choiceIndex) {
    if (!s_BindingsDialogueSystem || !s_BindingsWorld) return;
    s_BindingsDialogueSystem->SelectChoice(s_BindingsWorld, static_cast<Entity>(entityId), choiceIndex);
}

static void Dialogue_SetVariable(u64 entityId, const std::string& name, const std::string& value) {
    if (!s_BindingsDialogueSystem || !s_BindingsWorld) return;
    s_BindingsDialogueSystem->SetVariable(s_BindingsWorld, static_cast<Entity>(entityId), name, value);
}

static std::string Dialogue_GetVariable(u64 entityId, const std::string& name) {
    if (!s_BindingsDialogueSystem || !s_BindingsWorld) return "";
    return s_BindingsDialogueSystem->GetVariable(s_BindingsWorld, static_cast<Entity>(entityId), name);
}

static bool Dialogue_IsActive(u64 entityId) {
    if (!s_BindingsDialogueSystem || !s_BindingsWorld) return false;
    return s_BindingsDialogueSystem->IsActive(s_BindingsWorld, static_cast<Entity>(entityId));
}

static std::string Dialogue_GetCurrentSpeaker(u64 entityId) {
    if (!s_BindingsWorld) return "";
    auto* dlg = s_BindingsWorld->GetComponent<DialogueComponent>(static_cast<Entity>(entityId));
    if (!dlg) return "";
    return dlg->IsTreeMode() ? dlg->currentSpeaker : dlg->speakerName;
}

static std::string Dialogue_GetCurrentText(u64 entityId) {
    if (!s_BindingsWorld) return "";
    auto* dlg = s_BindingsWorld->GetComponent<DialogueComponent>(static_cast<Entity>(entityId));
    if (!dlg) return "";
    return dlg->IsTreeMode() ? dlg->GetTreeVisibleText() : dlg->GetVisibleText();
}

static u32 Dialogue_GetChoiceCount(u64 entityId) {
    if (!s_BindingsWorld) return 0;
    auto* dlg = s_BindingsWorld->GetComponent<DialogueComponent>(static_cast<Entity>(entityId));
    if (!dlg) return 0;
    if (dlg->IsTreeMode()) return static_cast<u32>(dlg->currentChoices.size());
    return static_cast<u32>(dlg->choices.size());
}

static std::string Dialogue_GetChoiceText(u64 entityId, u32 index) {
    if (!s_BindingsWorld) return "";
    auto* dlg = s_BindingsWorld->GetComponent<DialogueComponent>(static_cast<Entity>(entityId));
    if (!dlg) return "";
    if (dlg->IsTreeMode()) {
        if (index < static_cast<u32>(dlg->currentChoices.size()))
            return dlg->currentChoices[index].text;
    } else {
        if (index < static_cast<u32>(dlg->choices.size()))
            return dlg->choices[index].text;
    }
    return "";
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterDialogueBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Dialogue_Start(uint64)",
        ENJIN_AS_FN(Dialogue_Start), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Dialogue_Advance(uint64)",
        ENJIN_AS_FN(Dialogue_Advance), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Dialogue_Choose(uint64, uint)",
        ENJIN_AS_FN(Dialogue_Choose), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Dialogue_SetVariable(uint64, const string&in, const string&in)",
        ENJIN_AS_FN(Dialogue_SetVariable), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetVariable(uint64, const string&in)",
        ENJIN_AS_FN(Dialogue_GetVariable), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Dialogue_IsActive(uint64)",
        ENJIN_AS_FN(Dialogue_IsActive), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetCurrentSpeaker(uint64)",
        ENJIN_AS_FN(Dialogue_GetCurrentSpeaker), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetCurrentText(uint64)",
        ENJIN_AS_FN(Dialogue_GetCurrentText), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Dialogue_GetChoiceCount(uint64)",
        ENJIN_AS_FN(Dialogue_GetChoiceCount), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetChoiceText(uint64, uint)",
        ENJIN_AS_FN(Dialogue_GetChoiceText), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
