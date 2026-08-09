#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include <angelscript.h>
#include <string>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// The TieredSaveSystem pointer is set by PlayMode when entering play mode
static Gameplay::TieredSaveSystem* s_BindingsSaveSystem = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsSaveSystem(Gameplay::TieredSaveSystem* sys) {
    s_BindingsSaveSystem = sys;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Save/Load slot operations
// ============================================================================

static constexpr int kMaxSaveSlots = 20; // SC-H7: upper bound

static bool SaveGame_ToSlot(int slot) {
    if (!s_BindingsSaveSystem || !s_BindingsWorld || slot < 0 || slot >= kMaxSaveSlots) return false;
    return s_BindingsSaveSystem->SaveToSlot(static_cast<u32>(slot), s_BindingsWorld,
                                            s_BindingsSaveSystem->GetCurrentScene());
}

static bool SaveGame_FromSlot(int slot) {
    if (!s_BindingsSaveSystem || !s_BindingsWorld || slot < 0 || slot >= kMaxSaveSlots) return false;
    return s_BindingsSaveSystem->LoadFromSlot(static_cast<u32>(slot), s_BindingsWorld);
}

static bool SaveGame_DeleteSlot(int slot) {
    if (!s_BindingsSaveSystem || slot < 0 || slot >= kMaxSaveSlots) return false;
    return s_BindingsSaveSystem->DeleteSlot(static_cast<u32>(slot));
}

static void SaveGame_Checkpoint() {
    if (!s_BindingsSaveSystem || !s_BindingsWorld) return;
    s_BindingsSaveSystem->Checkpoint(s_BindingsWorld, s_BindingsSaveSystem->GetCurrentScene());
}

// ============================================================================
// Meta-progression
// ============================================================================

static void Meta_SetFloat(const std::string& key, float value) {
    if (s_BindingsSaveSystem) s_BindingsSaveSystem->SetMetaFloat(key, value);
}

static float Meta_GetFloat(const std::string& key, float fallback) {
    return s_BindingsSaveSystem ? s_BindingsSaveSystem->GetMetaFloat(key, fallback) : fallback;
}

static void Meta_SetInt(const std::string& key, int value) {
    if (s_BindingsSaveSystem) s_BindingsSaveSystem->SetMetaInt(key, value);
}

static int Meta_GetInt(const std::string& key, int fallback) {
    return s_BindingsSaveSystem ? s_BindingsSaveSystem->GetMetaInt(key, fallback) : fallback;
}

static void Meta_SetBool(const std::string& key, bool value) {
    if (s_BindingsSaveSystem) s_BindingsSaveSystem->SetMetaBool(key, value);
}

static bool Meta_GetBool(const std::string& key, bool fallback) {
    return s_BindingsSaveSystem ? s_BindingsSaveSystem->GetMetaBool(key, fallback) : fallback;
}

static void Meta_SetString(const std::string& key, const std::string& value) {
    if (s_BindingsSaveSystem) s_BindingsSaveSystem->SetMetaString(key, value);
}

static std::string Meta_GetString(const std::string& key, const std::string& fallback) {
    return s_BindingsSaveSystem ? s_BindingsSaveSystem->GetMetaString(key, fallback) : fallback;
}

static void Meta_Save() {
    if (s_BindingsSaveSystem) s_BindingsSaveSystem->SaveMeta();
}

// ============================================================================
// Auto-save configuration
// ============================================================================

static void AutoSave_Enable(bool enabled) {
    if (!s_BindingsSaveSystem) return;
    s_BindingsSaveSystem->GetAutoSaveConfig().enabled = enabled;
}

static void AutoSave_SetInterval(float seconds) {
    if (!s_BindingsSaveSystem) return;
    s_BindingsSaveSystem->GetAutoSaveConfig().intervalSeconds = seconds;
    s_BindingsSaveSystem->GetAutoSaveConfig().onTimedInterval = (seconds > 0.0f);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterSaveBindings(asIScriptEngine* engine) {
    // Save/Load
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveGame_ToSlot(int)",
        ENJIN_AS_FN(SaveGame_ToSlot), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveGame_FromSlot(int)",
        ENJIN_AS_FN(SaveGame_FromSlot), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveGame_DeleteSlot(int)",
        ENJIN_AS_FN(SaveGame_DeleteSlot), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SaveGame_Checkpoint()",
        ENJIN_AS_FN(SaveGame_Checkpoint), ENJIN_AS_CALL_CDECL));

    // Meta-progression
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetFloat(const string &in, float)",
        ENJIN_AS_FN(Meta_SetFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Meta_GetFloat(const string &in, float)",
        ENJIN_AS_FN(Meta_GetFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetInt(const string &in, int)",
        ENJIN_AS_FN(Meta_SetInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Meta_GetInt(const string &in, int)",
        ENJIN_AS_FN(Meta_GetInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetBool(const string &in, bool)",
        ENJIN_AS_FN(Meta_SetBool), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Meta_GetBool(const string &in, bool)",
        ENJIN_AS_FN(Meta_GetBool), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetString(const string &in, const string &in)",
        ENJIN_AS_FN(Meta_SetString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Meta_GetString(const string &in, const string &in)",
        ENJIN_AS_FN(Meta_GetString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_Save()",
        ENJIN_AS_FN(Meta_Save), ENJIN_AS_CALL_CDECL));

    // Auto-save
    AS_CHECK(engine->RegisterGlobalFunction("void AutoSave_Enable(bool)",
        ENJIN_AS_FN(AutoSave_Enable), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AutoSave_SetInterval(float)",
        ENJIN_AS_FN(AutoSave_SetInterval), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
