#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Scene/LevelStreaming.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static Scene::StreamingManager* s_BindingsStreaming = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsStreaming(Scene::StreamingManager* mgr) {
    s_BindingsStreaming = mgr;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Level Streaming
// ============================================================================

static void Streaming_ForceLoad(const std::string& chunkId) {
    if (s_BindingsStreaming) s_BindingsStreaming->ForceLoadChunk(chunkId);
}

static void Streaming_ForceUnload(const std::string& chunkId) {
    if (s_BindingsStreaming) s_BindingsStreaming->ForceUnloadChunk(chunkId);
}

static int Streaming_GetState(const std::string& chunkId) {
    if (!s_BindingsStreaming) return 0;
    return static_cast<int>(s_BindingsStreaming->GetChunkState(chunkId));
}

static bool Streaming_IsLoaded(const std::string& chunkId) {
    return s_BindingsStreaming ?
        s_BindingsStreaming->GetChunkState(chunkId) == Scene::ChunkState::Loaded : false;
}

static int Streaming_GetLoadedCount() {
    return s_BindingsStreaming ? static_cast<int>(s_BindingsStreaming->GetLoadedChunkCount()) : 0;
}

static void Streaming_SetEnabled(bool enabled) {
    if (s_BindingsStreaming) s_BindingsStreaming->SetEnabled(enabled);
}

static void Streaming_SetMemoryBudgetMB(int mb) {
    if (s_BindingsStreaming) s_BindingsStreaming->SetMemoryBudgetBytes(mb > 0 ? static_cast<u64>(mb) * 1024ull * 1024ull : 0);
}

static int Streaming_GetMemoryBudgetMB() {
    return s_BindingsStreaming ? static_cast<int>(s_BindingsStreaming->GetMemoryBudgetBytes() / (1024ull * 1024ull)) : 0;
}

static float Streaming_GetResidentMB() {
    return s_BindingsStreaming ? static_cast<float>(s_BindingsStreaming->GetResidentBytes() / (1024.0 * 1024.0)) : 0.0f;
}

static int Streaming_GetBudgetEvictions() {
    return s_BindingsStreaming ? static_cast<int>(s_BindingsStreaming->GetBudgetEvictionCount()) : 0;
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterStreamingBindings(asIScriptEngine* engine) {
    // Chunk state enum
    AS_CHECK(engine->RegisterEnum("ChunkState"));
    AS_CHECK(engine->RegisterEnumValue("ChunkState", "CHUNK_UNLOADED", 0));
    AS_CHECK(engine->RegisterEnumValue("ChunkState", "CHUNK_LOADING", 1));
    AS_CHECK(engine->RegisterEnumValue("ChunkState", "CHUNK_LOADED", 2));
    AS_CHECK(engine->RegisterEnumValue("ChunkState", "CHUNK_UNLOADING", 3));

    AS_CHECK(engine->RegisterGlobalFunction("void Streaming_ForceLoad(const string &in)",
        ENJIN_AS_FN(Streaming_ForceLoad), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Streaming_ForceUnload(const string &in)",
        ENJIN_AS_FN(Streaming_ForceUnload), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Streaming_GetState(const string &in)",
        ENJIN_AS_FN(Streaming_GetState), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Streaming_IsLoaded(const string &in)",
        ENJIN_AS_FN(Streaming_IsLoaded), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Streaming_GetLoadedCount()",
        ENJIN_AS_FN(Streaming_GetLoadedCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Streaming_SetEnabled(bool)",
        ENJIN_AS_FN(Streaming_SetEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Streaming_SetMemoryBudgetMB(int)",
        ENJIN_AS_FN(Streaming_SetMemoryBudgetMB), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Streaming_GetMemoryBudgetMB()",
        ENJIN_AS_FN(Streaming_GetMemoryBudgetMB), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Streaming_GetResidentMB()",
        ENJIN_AS_FN(Streaming_GetResidentMB), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Streaming_GetBudgetEvictions()",
        ENJIN_AS_FN(Streaming_GetBudgetEvictions), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
