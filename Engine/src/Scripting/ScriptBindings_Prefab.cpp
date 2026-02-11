#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/Assets/Prefab.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// Prefab instantiation
// ============================================================================

static u64 Prefab_Instantiate(const std::string& path, float px, float py, float pz) {
    if (!s_BindingsWorld) return 0;
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
        asFUNCTION(Prefab_Instantiate), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "uint64 Prefab_InstantiateEx(const string &in, float, float, float, float, float, float, float, float, float)",
        asFUNCTION(Prefab_InstantiateEx), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "bool Prefab_IsPrefabInstance(uint64)",
        asFUNCTION(Prefab_IsPrefabInstance), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Prefab_Unpack(uint64)",
        asFUNCTION(Prefab_Unpack), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
