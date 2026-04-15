// Web build: implementations for excluded editor/render code
#if ENJIN_PLATFORM_WEB
#include "Enjin/Platform/Platform.h"
#include <string>

class asIScriptEngine;

namespace Enjin {

// Script binding stubs
namespace Scripting {
    void RegisterRenderBindings(asIScriptEngine*) {}
    void RegisterAudioGraphBindings(asIScriptEngine*) {}
}

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

// AudioEventGraphRuntime method stubs
#include "Enjin/Editor/AudioEventGraph.h"
namespace Enjin { namespace Editor {
    void AudioEventGraphRuntime::TriggerEvent(const std::string&) {}
    void AudioEventGraphRuntime::SetParameter(const std::string&, float) {}
    float AudioEventGraphRuntime::GetParameter(const std::string&) const { return 0.0f; }
    void AudioEventGraphRuntime::StopAll() {}
}}

#endif
