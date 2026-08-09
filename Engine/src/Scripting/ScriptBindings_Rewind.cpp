#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Gameplay/RecordRewindSystem.h"
#include <angelscript.h>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

static Gameplay::RecordRewindSystem* s_BindingsRewind = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsRewindSystem(Gameplay::RecordRewindSystem* system) {
    s_BindingsRewind = system;
}

// ============================================================================
// Wrapper functions
// ============================================================================

static void Rewind_StartEntity(u64 id) {
    if (s_BindingsRewind) s_BindingsRewind->StartEntityRewind(static_cast<ECS::Entity>(id));
}

static void Rewind_StopEntity(u64 id) {
    if (s_BindingsRewind) s_BindingsRewind->StopEntityRewind(static_cast<ECS::Entity>(id));
}

static bool Rewind_IsEntityRewinding(u64 id) {
    if (!s_BindingsWorld) return false;
    auto* rr = s_BindingsWorld->GetComponent<ECS::RecordRewindComponent>(static_cast<ECS::Entity>(id));
    return rr ? rr->rewinding : false;
}

static void Rewind_StartScene() {
    if (s_BindingsRewind) s_BindingsRewind->StartSceneRewind();
}

static void Rewind_StopScene() {
    if (s_BindingsRewind) s_BindingsRewind->StopSceneRewind();
}

static bool Rewind_IsSceneRewinding() {
    return s_BindingsRewind ? s_BindingsRewind->IsSceneRewinding() : false;
}

static void Rewind_SeekScene(float time) {
    if (s_BindingsRewind) s_BindingsRewind->SeekSceneToTime(time);
}

static float Rewind_GetRecordedDuration() {
    return s_BindingsRewind ? s_BindingsRewind->GetSceneRecordedDuration() : 0.0f;
}

static float Rewind_GetCurrentTime() {
    return s_BindingsRewind ? s_BindingsRewind->GetSceneCurrentTime() : 0.0f;
}

static bool Rewind_IsAnyRewinding() {
    return s_BindingsRewind ? s_BindingsRewind->IsAnyRewinding() : false;
}

static void Rewind_SetEntityChannels(u64 id, u32 channelMask) {
    if (!s_BindingsWorld) return;
    auto* rr = s_BindingsWorld->GetComponent<ECS::RecordRewindComponent>(static_cast<ECS::Entity>(id));
    if (rr) rr->channels = channelMask;
}

// ============================================================================
// Registration
// ============================================================================

void RegisterRewindBindings(asIScriptEngine* engine) {
    // Entity rewind (Braid-style)
    AS_CHECK(engine->RegisterGlobalFunction("void Rewind_StartEntity(uint64)", ENJIN_AS_FN(Rewind_StartEntity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rewind_StopEntity(uint64)", ENJIN_AS_FN(Rewind_StopEntity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Rewind_IsEntityRewinding(uint64)", ENJIN_AS_FN(Rewind_IsEntityRewinding), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rewind_SetEntityChannels(uint64, uint)", ENJIN_AS_FN(Rewind_SetEntityChannels), ENJIN_AS_CALL_CDECL));

    // Scene rewind (Sands of Time-style)
    AS_CHECK(engine->RegisterGlobalFunction("void Rewind_StartScene()", ENJIN_AS_FN(Rewind_StartScene), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rewind_StopScene()", ENJIN_AS_FN(Rewind_StopScene), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Rewind_IsSceneRewinding()", ENJIN_AS_FN(Rewind_IsSceneRewinding), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Rewind_SeekScene(float)", ENJIN_AS_FN(Rewind_SeekScene), ENJIN_AS_CALL_CDECL));

    // Query
    AS_CHECK(engine->RegisterGlobalFunction("float Rewind_GetRecordedDuration()", ENJIN_AS_FN(Rewind_GetRecordedDuration), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Rewind_GetCurrentTime()", ENJIN_AS_FN(Rewind_GetCurrentTime), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Rewind_IsAnyRewinding()", ENJIN_AS_FN(Rewind_IsAnyRewinding), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
