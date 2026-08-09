#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Plugin/PluginSystem.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>

namespace Enjin {
namespace Scripting {

static Plugin::PluginSystem* s_PluginSystem = nullptr;

void SetBindingsPluginSystem(Plugin::PluginSystem* system) {
    s_PluginSystem = system;
}

static bool AS_Plugin_IsLoaded(const std::string& name) {
    if (s_PluginSystem) return s_PluginSystem->IsLoaded(name);
    return false;
}

static std::string AS_Plugin_GetVersion(const std::string& name) {
    if (s_PluginSystem) {
        auto* entry = s_PluginSystem->FindPlugin(name);
        if (entry) return entry->manifest.version;
    }
    return "";
}

// SC-C1: Plugin_Load/Unload removed from script bindings — loading native code
// from untrusted scripts is a complete sandbox escape. These remain available
// only to C++ editor code via PluginSystem directly.
static bool AS_Plugin_Load(const std::string&) {
    ENJIN_LOG_WARN(Script, "Plugin_Load is disabled from scripts (security)");
    return false;
}

static void AS_Plugin_Unload(const std::string&) {
    ENJIN_LOG_WARN(Script, "Plugin_Unload is disabled from scripts (security)");
}

void RegisterPluginBindings(asIScriptEngine* engine) {
    engine->RegisterGlobalFunction("bool Plugin_IsLoaded(const string &in)", ENJIN_AS_FN(AS_Plugin_IsLoaded), ENJIN_AS_CALL_CDECL);
    engine->RegisterGlobalFunction("string Plugin_GetVersion(const string &in)", ENJIN_AS_FN(AS_Plugin_GetVersion), ENJIN_AS_CALL_CDECL);
    engine->RegisterGlobalFunction("bool Plugin_Load(const string &in)", ENJIN_AS_FN(AS_Plugin_Load), ENJIN_AS_CALL_CDECL);
    engine->RegisterGlobalFunction("void Plugin_Unload(const string &in)", ENJIN_AS_FN(AS_Plugin_Unload), ENJIN_AS_CALL_CDECL);
}

} // namespace Scripting
} // namespace Enjin
