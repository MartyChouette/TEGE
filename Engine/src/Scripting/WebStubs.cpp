// Web build: implementations for excluded editor/render code
#if ENJIN_PLATFORM_WEB
#include "Enjin/Platform/Platform.h"
#include <string>

class asIScriptEngine;

namespace Enjin {

// NOTE: RegisterRenderBindings / RegisterAudioGraphBindings are NO LONGER
// stubbed here — the real ScriptBindings_Render.cpp / _AudioGraph.cpp compile
// on web now. Stubbing a Register* function makes every script that MENTIONS
// one of its symbols fail to compile wholesale on web (the Playground died on
// "No matching symbol 'Render_SetRainActive'"). Register everything; let the
// null-guarded wrappers no-op where the backend lacks the feature.

// All VisualScript global system pointers (set by PlayMode on desktop)
namespace VisualScript {
    void* s_VisualScriptSaveSystem = nullptr;
    void* s_VisualScriptWeather = nullptr;
    void* s_VisualScriptSubtitleSystem = nullptr;
    void* s_VisualScriptAnnouncer = nullptr;
    void* s_VisualScriptWater = nullptr;
    void* s_VisualScriptHUD = nullptr;
    void* s_VisualScriptObjectPool = nullptr;
    void* s_VisualScriptElemental = nullptr;
    void* s_VisualScriptAudio = nullptr;
    void* s_VisualScriptAudioGraphRuntime = nullptr;
    void* s_VisualScriptPluginSystem = nullptr;
    void* s_VisualScriptPostProcessing = nullptr;
}

} // namespace Enjin

// NOTE: AudioEventGraphRuntime is NO LONGER stubbed here either, for the same
// reason as the Register* functions above. Audio/AudioEventGraphRuntime.cpp
// carries no platform guard and depends only on SimpleAudio and the logger, so
// it compiles on web and defines these four methods itself. Keeping the stubs
// meant two definitions and a duplicate-symbol link failure -- which only
// surfaced on a clean web build, because a stale build directory was still
// linking objects from before that file existed.

#endif
