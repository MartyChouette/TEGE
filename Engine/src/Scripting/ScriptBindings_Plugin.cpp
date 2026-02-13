#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Plugin/PluginSystem.h"
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

static bool AS_Plugin_Load(const std::string& name) {
    if (s_PluginSystem) return s_PluginSystem->LoadPlugin(name);
    return false;
}

static void AS_Plugin_Unload(const std::string& name) {
    if (s_PluginSystem) s_PluginSystem->UnloadPlugin(name);
}

void RegisterPluginBindings(asIScriptEngine* engine) {
    engine->RegisterGlobalFunction("bool Plugin_IsLoaded(const string &in)", asFUNCTION(AS_Plugin_IsLoaded), asCALL_CDECL);
    engine->RegisterGlobalFunction("string Plugin_GetVersion(const string &in)", asFUNCTION(AS_Plugin_GetVersion), asCALL_CDECL);
    engine->RegisterGlobalFunction("bool Plugin_Load(const string &in)", asFUNCTION(AS_Plugin_Load), asCALL_CDECL);
    engine->RegisterGlobalFunction("void Plugin_Unload(const string &in)", asFUNCTION(AS_Plugin_Unload), asCALL_CDECL);
}

} // namespace Scripting
} // namespace Enjin
