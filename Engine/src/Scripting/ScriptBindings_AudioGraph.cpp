#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Audio/AudioEventGraph.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static Audio::AudioEventGraphRuntime* s_AudioGraphRuntime = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsAudioGraphRuntime(Audio::AudioEventGraphRuntime* runtime) {
    s_AudioGraphRuntime = runtime;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Audio Graph script wrappers
// ============================================================================

static void AS_AudioGraph_TriggerEvent(const std::string& name) {
    if (s_AudioGraphRuntime) s_AudioGraphRuntime->TriggerEvent(name);
}

static void AS_AudioGraph_SetParameter(const std::string& name, float value) {
    if (s_AudioGraphRuntime) s_AudioGraphRuntime->SetParameter(name, value);
}

static float AS_AudioGraph_GetParameter(const std::string& name) {
    if (s_AudioGraphRuntime) return s_AudioGraphRuntime->GetParameter(name);
    return 0.0f;
}

static void AS_AudioGraph_StopAll() {
    if (s_AudioGraphRuntime) s_AudioGraphRuntime->StopAll();
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterAudioGraphBindings(asIScriptEngine* engine) {
    AS_CHECK(engine->RegisterGlobalFunction(
        "void AudioGraph_TriggerEvent(const string &in)",
        asFUNCTION(AS_AudioGraph_TriggerEvent), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void AudioGraph_SetParameter(const string &in, float)",
        asFUNCTION(AS_AudioGraph_SetParameter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "float AudioGraph_GetParameter(const string &in)",
        asFUNCTION(AS_AudioGraph_GetParameter), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void AudioGraph_StopAll()",
        asFUNCTION(AS_AudioGraph_StopAll), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
