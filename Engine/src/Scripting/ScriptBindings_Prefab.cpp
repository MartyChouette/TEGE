#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Assets/Prefab.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;
extern bool ValidateScriptAssetPath(const std::string& path, const char* funcName);

// S4 fix: shares per-frame entity creation cap with Scene bindings
extern bool CheckEntityCreationCap(const char* funcName);

// ============================================================================
// Prefab instantiation
// ============================================================================

static u64 Prefab_Instantiate(const std::string& path, float px, float py, float pz) {
    if (!s_BindingsWorld) return 0;
    if (!ValidateScriptAssetPath(path, "Prefab_Instantiate")) return 0;
    if (!CheckEntityCreationCap("Prefab_Instantiate")) return 0;
    auto prefab = Assets::PrefabManager::Get().LoadPrefab(path);
    if (!prefab) return 0;
    return Assets::PrefabManager::Get().Instantiate(
        s_BindingsWorld, *prefab,
        Math::Vector3(px, py, pz));
}

static u64 Prefab_InstantiateEx(const std::string& path,
                                 float px, float py, float pz,
                                 float rx, float ry, float rz,
                                 float sx, float sy, float sz) {
    if (!s_BindingsWorld) return 0;
    if (!ValidateScriptAssetPath(path, "Prefab_InstantiateEx")) return 0;
    if (!CheckEntityCreationCap("Prefab_InstantiateEx")) return 0;
    auto prefab = Assets::PrefabManager::Get().LoadPrefab(path);
    if (!prefab) return 0;
    return Assets::PrefabManager::Get().Instantiate(
        s_BindingsWorld, *prefab,
        Math::Vector3(px, py, pz),
        Math::Vector3(rx, ry, rz),
        Math::Vector3(sx, sy, sz));
}

static bool Prefab_IsPrefabInstance(u64 entity) {
    if (!s_BindingsWorld) return false;
    return Assets::PrefabUtils::IsPrefabInstance(s_BindingsWorld, entity);
}

static void Prefab_Unpack(u64 entity) {
    if (!s_BindingsWorld) return;
    Assets::PrefabManager::Get().UnpackInstance(s_BindingsWorld, entity);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterPrefabBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Prefab_Instantiate(const string &in, float, float, float)",
        ENJIN_AS_FN(Prefab_Instantiate), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Prefab_InstantiateEx(const string &in, float, float, float, float, float, float, float, float, float)",
        ENJIN_AS_FN(Prefab_InstantiateEx), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Prefab_IsPrefabInstance(uint64)",
        ENJIN_AS_FN(Prefab_IsPrefabInstance), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Prefab_Unpack(uint64)",
        ENJIN_AS_FN(Prefab_Unpack), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
