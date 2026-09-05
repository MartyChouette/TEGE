#include "Enjin/Scripting/ScriptComponentAccess.h"
#include "Enjin/Logging/Log.h"

#include <mutex>
#include <string>
#include <unordered_set>

namespace Enjin {
namespace Scripting {

namespace {

std::mutex g_Mutex;
std::unordered_set<std::string> g_Warned;

} // namespace

void WarnMissingScriptComponent(u64 entityId, const char* componentName,
                                const char* bindingName) {
    // Once per entity + component. A setter in OnUpdate would otherwise print
    // the same true sentence sixty times a second and bury everything else.
    std::string key = std::to_string(entityId);
    key += '/';
    key += componentName ? componentName : "?";

    {
        std::lock_guard<std::mutex> lock(g_Mutex);
        if (!g_Warned.insert(std::move(key)).second) return;
    }

    ENJIN_LOG_WARN(Script,
                   "%s: entity %llu has no %s - the call did nothing. "
                   "Add the component in the editor, or check the entity is the one you meant.",
                   bindingName ? bindingName : "script binding",
                   static_cast<unsigned long long>(entityId),
                   componentName ? componentName : "component");
}

} // namespace Scripting
} // namespace Enjin
