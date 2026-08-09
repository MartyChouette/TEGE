#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Input/MIDIInput.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

static InputSystem::MIDIInput* s_BindingsMIDI = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsMIDI(InputSystem::MIDIInput* midi) {
    s_BindingsMIDI = midi;
}

} // namespace Scripting
} // namespace Enjin

// ---------------------------------------------------------------------------
// Script wrapper functions
// ---------------------------------------------------------------------------

static u32 MIDI_GetDeviceCount() {
    return s_BindingsMIDI ? s_BindingsMIDI->GetDeviceCount() : 0;
}

static std::string MIDI_GetDeviceName(u32 index) {
    return s_BindingsMIDI ? s_BindingsMIDI->GetDeviceName(index) : std::string();
}

static bool MIDI_OpenDevice(u32 index) {
    return s_BindingsMIDI ? s_BindingsMIDI->OpenDevice(index) : false;
}

static void MIDI_CloseDevice() {
    if (s_BindingsMIDI) s_BindingsMIDI->CloseDevice();
}

static bool MIDI_IsDeviceOpen() {
    return s_BindingsMIDI ? s_BindingsMIDI->IsDeviceOpen() : false;
}

static bool MIDI_IsNoteOn(u8 note, u8 channel) {
    return s_BindingsMIDI ? s_BindingsMIDI->IsNoteOn(note, channel) : false;
}

static bool MIDI_IsNoteOff(u8 note, u8 channel) {
    return s_BindingsMIDI ? s_BindingsMIDI->IsNoteOff(note, channel) : false;
}

static u8 MIDI_GetNoteVelocity(u8 note, u8 channel) {
    return s_BindingsMIDI ? s_BindingsMIDI->GetNoteVelocity(note, channel) : 0;
}

static u8 MIDI_GetCC(u8 cc, u8 channel) {
    return s_BindingsMIDI ? s_BindingsMIDI->GetCC(cc, channel) : 0;
}

static u8 MIDI_GetCCValue(u8 cc, u8 channel) {
    return s_BindingsMIDI ? s_BindingsMIDI->GetCCValue(cc, channel) : 0;
}

static u32 MIDI_GetEventCount() {
    return s_BindingsMIDI ? static_cast<u32>(s_BindingsMIDI->GetEvents().size()) : 0;
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

namespace Enjin {
namespace Scripting {

void RegisterMIDIBindings(asIScriptEngine* engine) {
    #define AS_CHECK(expr) do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

    // Device management
    AS_CHECK(engine->RegisterGlobalFunction("uint32 MIDI_GetDeviceCount()",
        ENJIN_AS_FN(MIDI_GetDeviceCount), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string MIDI_GetDeviceName(uint32)",
        ENJIN_AS_FN(MIDI_GetDeviceName), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool MIDI_OpenDevice(uint32)",
        ENJIN_AS_FN(MIDI_OpenDevice), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void MIDI_CloseDevice()",
        ENJIN_AS_FN(MIDI_CloseDevice), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool MIDI_IsDeviceOpen()",
        ENJIN_AS_FN(MIDI_IsDeviceOpen), ENJIN_AS_CALL_CDECL));

    // Note queries (per-frame)
    AS_CHECK(engine->RegisterGlobalFunction("bool MIDI_IsNoteOn(uint8, uint8 = 0xFF)",
        ENJIN_AS_FN(MIDI_IsNoteOn), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool MIDI_IsNoteOff(uint8, uint8 = 0xFF)",
        ENJIN_AS_FN(MIDI_IsNoteOff), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint8 MIDI_GetNoteVelocity(uint8, uint8 = 0xFF)",
        ENJIN_AS_FN(MIDI_GetNoteVelocity), ENJIN_AS_CALL_CDECL));

    // CC queries
    AS_CHECK(engine->RegisterGlobalFunction("uint8 MIDI_GetCC(uint8, uint8 = 0xFF)",
        ENJIN_AS_FN(MIDI_GetCC), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("uint8 MIDI_GetCCValue(uint8, uint8 = 0)",
        ENJIN_AS_FN(MIDI_GetCCValue), ENJIN_AS_CALL_CDECL));

    // Event count
    AS_CHECK(engine->RegisterGlobalFunction("uint32 MIDI_GetEventCount()",
        ENJIN_AS_FN(MIDI_GetEventCount), ENJIN_AS_CALL_CDECL));

    #undef AS_CHECK

    ENJIN_LOG_INFO(Script, "MIDI bindings registered (12 functions)");
}

} // namespace Scripting
} // namespace Enjin
