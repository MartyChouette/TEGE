#pragma once

#include "Enjin/Platform/Platform.h"

class asIScriptEngine;

namespace Enjin {
namespace Networking {
    class NewgroundsAPI;
}

namespace Scripting {

// Register Newgrounds API functions into AngelScript
// Provides: NG_Connect(), NG_IsConnected(), NG_UnlockMedal(), NG_PostScore(), NG_GetScores()
ENJIN_API void RegisterNewgroundsBindings(asIScriptEngine* engine);

// Set the NewgroundsAPI instance for bindings to use
void SetNewgroundsAPIInstance(Networking::NewgroundsAPI* api);

} // namespace Scripting
} // namespace Enjin
