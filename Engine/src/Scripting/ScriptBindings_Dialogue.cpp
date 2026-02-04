#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

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
        asFUNCTION(Dialogue_Start), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Dialogue_Advance(uint64)",
        asFUNCTION(Dialogue_Advance), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Dialogue_Choose(uint64, uint)",
        asFUNCTION(Dialogue_Choose), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void Dialogue_SetVariable(uint64, const string&in, const string&in)",
        asFUNCTION(Dialogue_SetVariable), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetVariable(uint64, const string&in)",
        asFUNCTION(Dialogue_GetVariable), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Dialogue_IsActive(uint64)",
        asFUNCTION(Dialogue_IsActive), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetCurrentSpeaker(uint64)",
        asFUNCTION(Dialogue_GetCurrentSpeaker), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetCurrentText(uint64)",
        asFUNCTION(Dialogue_GetCurrentText), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "uint Dialogue_GetChoiceCount(uint64)",
        asFUNCTION(Dialogue_GetChoiceCount), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string Dialogue_GetChoiceText(uint64, uint)",
        asFUNCTION(Dialogue_GetChoiceText), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
