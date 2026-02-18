#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include <angelscript.h>
#include <string>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

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
        asFUNCTION(SaveGame_ToSlot), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveGame_FromSlot(int)",
        asFUNCTION(SaveGame_FromSlot), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveGame_DeleteSlot(int)",
        asFUNCTION(SaveGame_DeleteSlot), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SaveGame_Checkpoint()",
        asFUNCTION(SaveGame_Checkpoint), asCALL_CDECL));

    // Meta-progression
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetFloat(const string &in, float)",
        asFUNCTION(Meta_SetFloat), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Meta_GetFloat(const string &in, float)",
        asFUNCTION(Meta_GetFloat), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetInt(const string &in, int)",
        asFUNCTION(Meta_SetInt), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Meta_GetInt(const string &in, int)",
        asFUNCTION(Meta_GetInt), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetBool(const string &in, bool)",
        asFUNCTION(Meta_SetBool), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Meta_GetBool(const string &in, bool)",
        asFUNCTION(Meta_GetBool), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_SetString(const string &in, const string &in)",
        asFUNCTION(Meta_SetString), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Meta_GetString(const string &in, const string &in)",
        asFUNCTION(Meta_GetString), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Meta_Save()",
        asFUNCTION(Meta_Save), asCALL_CDECL));

    // Auto-save
    AS_CHECK(engine->RegisterGlobalFunction("void AutoSave_Enable(bool)",
        asFUNCTION(AutoSave_Enable), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AutoSave_SetInterval(float)",
        asFUNCTION(AutoSave_SetInterval), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
