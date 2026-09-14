#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/ECS/Components/Gameplay.h"
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
// Per-entity persistence, declared from script (ENG-001, S6)
// ============================================================================
//
// SaveDataComponent existed, was serialized, and had an editor inspector -- and
// no script could touch it. Persistence could only be authored by hand in the
// editor, entity by entity, which is unusable for a game whose scene is
// generated and whose behaviour lives in AngelScript.
//
// That gap is what pushed run state into AngelScript globals, where the tier
// system cannot see it and no save can capture it. The decision (Marty,
// 2026-09-14) is that the state moves onto entities rather than the save system
// growing a script-state hook, so script has to be able to put it there.
//
// The custom-data accessors mirror the Meta_* family in this same file (Float /
// Int / Bool / String) rather than inventing a second shape. Storage underneath
// is the string pairs SaveDataComponent already uses and that already round-trip
// through the scene serializer.

static ECS::SaveDataComponent* ResolveSaveData(u64 entityId, bool createIfMissing) {
    if (!s_BindingsWorld) return nullptr;
    const ECS::Entity e = static_cast<ECS::Entity>(entityId);
    if (!s_BindingsWorld->IsValid(e)) return nullptr;
    if (!s_BindingsWorld->HasComponent<ECS::SaveDataComponent>(e)) {
        if (!createIfMissing) return nullptr;
        s_BindingsWorld->AddComponent<ECS::SaveDataComponent>(e);
    }
    return s_BindingsWorld->GetComponent<ECS::SaveDataComponent>(e);
}

static void SaveData_Set(u64 entityId, int tier) {
    if (tier < 0 || tier > 2) {
        ENJIN_LOG_WARN(Script, "SaveData_Set: tier %d is not SceneState(0), RunState(1) "
                       "or MetaProgression(2); ignored", tier);
        return;
    }
    auto* sd = ResolveSaveData(entityId, true);
    if (!sd) {
        ENJIN_LOG_WARN(Script, "SaveData_Set: entity %llu is not valid",
                       static_cast<unsigned long long>(entityId));
        return;
    }
    sd->tier = static_cast<ECS::PersistenceTier>(tier);

    // Said now rather than at save time. Without a stable id this entity's
    // record can be written and can never be matched back on load, and that
    // failure would not surface until someone tried to load.
    const ECS::Entity e = static_cast<ECS::Entity>(entityId);
    auto* sid = s_BindingsWorld->GetComponent<ECS::StableIdComponent>(e);
    if (!sid || sid->id == 0) {
        ENJIN_LOG_WARN(Script,
            "SaveData_Set: entity %llu has no stable id, so anything it saves cannot be "
            "restored. Entities authored into a scene have one; an entity spawned at "
            "runtime does not, and runtime-spawned persistence is not supported yet.",
            static_cast<unsigned long long>(entityId));
    }
}

static bool SaveData_Has(u64 entityId) {
    return ResolveSaveData(entityId, false) != nullptr;
}

static int SaveData_GetTier(u64 entityId) {
    auto* sd = ResolveSaveData(entityId, false);
    return sd ? static_cast<int>(sd->tier) : -1;
}

static void SaveData_AddTag(u64 entityId, const std::string& tag) {
    auto* sd = ResolveSaveData(entityId, true);
    if (sd && !sd->HasTag(tag)) sd->tags.push_back(tag);
}

static bool SaveData_HasTag(u64 entityId, const std::string& tag) {
    auto* sd = ResolveSaveData(entityId, false);
    return sd && sd->HasTag(tag);
}

static void SaveData_SetString(u64 entityId, const std::string& key, const std::string& value) {
    if (auto* sd = ResolveSaveData(entityId, true)) sd->SetData(key, value);
}
static std::string SaveData_GetString(u64 entityId, const std::string& key,
                                      const std::string& fallback) {
    auto* sd = ResolveSaveData(entityId, false);
    return sd ? sd->GetData(key, fallback) : fallback;
}

static void SaveData_SetFloat(u64 entityId, const std::string& key, f32 value) {
    if (auto* sd = ResolveSaveData(entityId, true)) sd->SetData(key, std::to_string(value));
}
static f32 SaveData_GetFloat(u64 entityId, const std::string& key, f32 fallback) {
    auto* sd = ResolveSaveData(entityId, false);
    if (!sd) return fallback;
    const std::string v = sd->GetData(key, "");
    if (v.empty()) return fallback;
    try { return std::stof(v); } catch (...) { return fallback; }
}

static void SaveData_SetInt(u64 entityId, const std::string& key, i32 value) {
    if (auto* sd = ResolveSaveData(entityId, true)) sd->SetData(key, std::to_string(value));
}
static i32 SaveData_GetInt(u64 entityId, const std::string& key, i32 fallback) {
    auto* sd = ResolveSaveData(entityId, false);
    if (!sd) return fallback;
    const std::string v = sd->GetData(key, "");
    if (v.empty()) return fallback;
    try { return std::stoi(v); } catch (...) { return fallback; }
}

static void SaveData_SetBool(u64 entityId, const std::string& key, bool value) {
    if (auto* sd = ResolveSaveData(entityId, true)) sd->SetData(key, value ? "1" : "0");
}
static bool SaveData_GetBool(u64 entityId, const std::string& key, bool fallback) {
    auto* sd = ResolveSaveData(entityId, false);
    if (!sd) return fallback;
    const std::string v = sd->GetData(key, "");
    if (v.empty()) return fallback;
    return v == "1" || v == "true";
}

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

    // Per-entity persistence (ENG-001 S6)
    AS_CHECK(engine->RegisterGlobalFunction("void SaveData_Set(uint64, int)",
        ENJIN_AS_FN(SaveData_Set), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveData_Has(uint64)",
        ENJIN_AS_FN(SaveData_Has), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int SaveData_GetTier(uint64)",
        ENJIN_AS_FN(SaveData_GetTier), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SaveData_AddTag(uint64, const string &in)",
        ENJIN_AS_FN(SaveData_AddTag), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveData_HasTag(uint64, const string &in)",
        ENJIN_AS_FN(SaveData_HasTag), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SaveData_SetString(uint64, const string &in, const string &in)",
        ENJIN_AS_FN(SaveData_SetString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string SaveData_GetString(uint64, const string &in, const string &in)",
        ENJIN_AS_FN(SaveData_GetString), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SaveData_SetFloat(uint64, const string &in, float)",
        ENJIN_AS_FN(SaveData_SetFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float SaveData_GetFloat(uint64, const string &in, float)",
        ENJIN_AS_FN(SaveData_GetFloat), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SaveData_SetInt(uint64, const string &in, int)",
        ENJIN_AS_FN(SaveData_SetInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int SaveData_GetInt(uint64, const string &in, int)",
        ENJIN_AS_FN(SaveData_GetInt), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void SaveData_SetBool(uint64, const string &in, bool)",
        ENJIN_AS_FN(SaveData_SetBool), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool SaveData_GetBool(uint64, const string &in, bool)",
        ENJIN_AS_FN(SaveData_GetBool), ENJIN_AS_CALL_CDECL));

    // Auto-save
    AS_CHECK(engine->RegisterGlobalFunction("void AutoSave_Enable(bool)",
        ENJIN_AS_FN(AutoSave_Enable), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void AutoSave_SetInterval(float)",
        ENJIN_AS_FN(AutoSave_SetInterval), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
