#include "Enjin/Scripting/NewgroundsBindings.h"
#include "Enjin/Networking/NewgroundsAPI.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <string>

namespace Enjin {
namespace Scripting {

static Networking::NewgroundsAPI* s_NGAPI = nullptr;

void SetNewgroundsAPIInstance(Networking::NewgroundsAPI* api) {
    s_NGAPI = api;
}

// ============================================================================
// AngelScript bound functions
// ============================================================================

static void NG_Connect(const std::string& appId, const std::string& key) {
    if (!s_NGAPI) return;
    s_NGAPI->Initialize(appId, key);
    s_NGAPI->StartSession();
}

static bool NG_IsConnected() {
    return s_NGAPI && s_NGAPI->IsConnected();
}

static bool NG_CheckSession() {
    if (!s_NGAPI) return false;
    return s_NGAPI->CheckSession();
}

static bool NG_UnlockMedal(int medalId) {
    if (!s_NGAPI) return false;
    return s_NGAPI->UnlockMedal(medalId);
}

static bool NG_PostScore(int boardId, int value) {
    if (!s_NGAPI) return false;
    return s_NGAPI->PostScore(boardId, value);
}

static std::string NG_GetPassportUrl() {
    if (!s_NGAPI) return "";
    return s_NGAPI->GetPassportUrl();
}

static std::string NG_GetUserName() {
    if (!s_NGAPI) return "";
    return s_NGAPI->GetSession().userName;
}

static bool NG_SaveSlot(int slotId, const std::string& data) {
    if (!s_NGAPI) return false;
    return s_NGAPI->SaveSlot(slotId, data);
}

static std::string NG_LoadSlot(int slotId) {
    if (!s_NGAPI) return "";
    return s_NGAPI->LoadSlot(slotId);
}

// ============================================================================
// Registration
// ============================================================================

void RegisterNewgroundsBindings(asIScriptEngine* engine) {
    if (!engine) return;

    int r = 0;

    r = engine->RegisterGlobalFunction(
        "void NG_Connect(const string &in appId, const string &in key)",
        asFUNCTION(NG_Connect), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "bool NG_IsConnected()",
        asFUNCTION(NG_IsConnected), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "bool NG_CheckSession()",
        asFUNCTION(NG_CheckSession), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "bool NG_UnlockMedal(int medalId)",
        asFUNCTION(NG_UnlockMedal), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "bool NG_PostScore(int boardId, int value)",
        asFUNCTION(NG_PostScore), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "string NG_GetPassportUrl()",
        asFUNCTION(NG_GetPassportUrl), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "string NG_GetUserName()",
        asFUNCTION(NG_GetUserName), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "bool NG_SaveSlot(int slotId, const string &in data)",
        asFUNCTION(NG_SaveSlot), asCALL_CDECL); (void)r;

    r = engine->RegisterGlobalFunction(
        "string NG_LoadSlot(int slotId)",
        asFUNCTION(NG_LoadSlot), asCALL_CDECL); (void)r;

    ENJIN_LOG_INFO(Script, "Newgrounds API bindings registered (9 functions)");
}

} // namespace Scripting
} // namespace Enjin
