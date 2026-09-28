#include "Enjin/Input/MIDIInput.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace Enjin {
namespace InputSystem {

MIDIInput::MIDIInput() {
    std::memset(m_CCState, 0, sizeof(m_CCState));
}

MIDIInput::~MIDIInput() {
    Shutdown();
}

bool MIDIInput::Initialize() {
    if (m_Initialized) return true;
    m_Initialized = true;
    ENJIN_LOG_INFO(Editor, "MIDIInput initialized (%u devices found)", GetDeviceCount());
    return true;
}

void MIDIInput::Shutdown() {
    if (!m_Initialized) return;
    CloseDevice();
    m_Initialized = false;
}

void MIDIInput::Update() {
    std::lock_guard<std::mutex> lock(m_EventMutex);
    m_ReadEvents.swap(m_WriteEvents);
    m_WriteEvents.clear();
}

// ============================================================================
// Device enumeration
// ============================================================================

#ifdef ENJIN_PLATFORM_WINDOWS

u32 MIDIInput::GetDeviceCount() const {
    return static_cast<u32>(midiInGetNumDevs());
}

std::string MIDIInput::GetDeviceName(u32 index) const {
    MIDIINCAPSA caps = {};
    if (midiInGetDevCapsA(index, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
        return std::string(caps.szPname);
    }
    return "Unknown MIDI Device";
}

bool MIDIInput::OpenDevice(u32 index) {
    if (m_DeviceOpen) CloseDevice();

    MMRESULT result = midiInOpen(&m_MIDIHandle, index,
                                  reinterpret_cast<DWORD_PTR>(&MidiInCallback),
                                  reinterpret_cast<DWORD_PTR>(this),
                                  CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        ENJIN_LOG_ERROR(Editor, "MIDIInput: Failed to open device %u (error %u)", index, result);
        return false;
    }

    midiInStart(m_MIDIHandle);
    m_DeviceOpen = true;
    m_OpenDeviceName = GetDeviceName(index);
    std::memset(m_CCState, 0, sizeof(m_CCState));
    ENJIN_LOG_INFO(Editor, "MIDIInput: Opened device '%s'", m_OpenDeviceName.c_str());
    return true;
}

void MIDIInput::CloseDevice() {
    if (!m_DeviceOpen || !m_MIDIHandle) return;
    midiInStop(m_MIDIHandle);
    midiInClose(m_MIDIHandle);
    m_MIDIHandle = nullptr;
    m_DeviceOpen = false;
    m_OpenDeviceName.clear();
}

void CALLBACK MIDIInput::MidiInCallback(HMIDIIN /*hMidiIn*/, UINT wMsg,
                                          DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR /*dwParam2*/) {
    if (wMsg != MIM_DATA) return;

    auto* self = reinterpret_cast<MIDIInput*>(dwInstance);
    if (!self) return;

    self->InjectMessage(static_cast<u8>(dwParam1 & 0xFF),
                        static_cast<u8>((dwParam1 >> 8) & 0xFF),
                        static_cast<u8>((dwParam1 >> 16) & 0xFF));
}

#elif defined(__EMSCRIPTEN__)

// Web MIDI. The desktop player had MIDI through winmm and the web player had
// none, so MIDI_* scripts and MIDI bindings did nothing in a browser. The
// browser's MIDIAccess is asynchronous and asks the player first, so it is
// requested on first use rather than at boot: a game that never touches MIDI
// never shows the prompt.
namespace {
MIDIInput* s_WebMidiOwner = nullptr;
}

} // namespace InputSystem
} // namespace Enjin

extern "C" {
EMSCRIPTEN_KEEPALIVE void enjin_midi_message(int status, int data1, int data2) {
    if (Enjin::InputSystem::s_WebMidiOwner)
        Enjin::InputSystem::s_WebMidiOwner->InjectMessage(static_cast<Enjin::u8>(status),
            static_cast<Enjin::u8>(data1), static_cast<Enjin::u8>(data2));
}
EMSCRIPTEN_KEEPALIVE void enjin_midi_settled(int deviceCount) {
    if (Enjin::InputSystem::s_WebMidiOwner)
        Enjin::InputSystem::s_WebMidiOwner->OnWebAccessSettled(deviceCount);
}
} // extern "C"

EM_JS(void, enjin_midi_js_request, (), {
    if (Module.enjinMidi) return;
    var m = Module.enjinMidi = { inputs: [], open: null, settled: false };
    if (!navigator.requestMIDIAccess) {
        m.settled = true;
        _enjin_midi_settled(-1);
        return;
    }
    navigator.requestMIDIAccess({ sysex: false }).then(function(access) {
        var refresh = function() {
            m.inputs = [];
            access.inputs.forEach(function(input) { m.inputs.push(input); });
        };
        refresh();
        access.onstatechange = refresh;
        m.settled = true;
        _enjin_midi_settled(m.inputs.length);
    }, function(err) {
        m.settled = true;
        console.warn('[Enjin] MIDI access refused: ' + err);
        _enjin_midi_settled(-1);
    });
});

EM_JS(int, enjin_midi_js_count, (), {
    return Module.enjinMidi ? Module.enjinMidi.inputs.length : 0;
});

// Writes the name as UTF-8 into out (cap bytes, NUL included); returns its length
EM_JS(int, enjin_midi_js_name, (int index, char* out, int cap), {
    var m = Module.enjinMidi;
    if (!m || index < 0 || index >= m.inputs.length || cap < 1) return -1;
    var bytes = new TextEncoder().encode(m.inputs[index].name || 'MIDI Device');
    var n = Math.min(bytes.length, cap - 1);
    HEAPU8.set(bytes.subarray(0, n), out);
    HEAPU8[out + n] = 0;
    return n;
});

EM_JS(int, enjin_midi_js_open, (int index), {
    var m = Module.enjinMidi;
    if (!m || index < 0 || index >= m.inputs.length) return 0;
    if (m.open) m.open.onmidimessage = null;
    var input = m.inputs[index];
    input.onmidimessage = function(e) {
        var d = e.data;
        if (d.length >= 1) _enjin_midi_message(d[0], d.length > 1 ? d[1] : 0, d.length > 2 ? d[2] : 0);
    };
    m.open = input;
    return 1;
});

EM_JS(void, enjin_midi_js_close, (), {
    var m = Module.enjinMidi;
    if (m && m.open) { m.open.onmidimessage = null; m.open = null; }
});

EM_JS(int, enjin_midi_js_settled, (), {
    return (Module.enjinMidi && Module.enjinMidi.settled) ? 1 : 0;
});

namespace Enjin {
namespace InputSystem {

u32 MIDIInput::GetDeviceCount() const {
    s_WebMidiOwner = const_cast<MIDIInput*>(this);
    enjin_midi_js_request();
    return static_cast<u32>(enjin_midi_js_count());
}

std::string MIDIInput::GetDeviceName(u32 index) const {
    char buf[256];
    if (enjin_midi_js_name(static_cast<int>(index), buf, sizeof(buf)) < 0) return "Unknown MIDI Device";
    return std::string(buf);
}

bool MIDIInput::OpenDevice(u32 index) {
    s_WebMidiOwner = this;
    enjin_midi_js_request();
    if (!enjin_midi_js_settled()) {
        // Opens when the player answers the browser's prompt
        m_PendingOpen = static_cast<i32>(index);
        return true;
    }
    if (m_DeviceOpen) CloseDevice();
    if (!enjin_midi_js_open(static_cast<int>(index))) {
        ENJIN_LOG_ERROR(Editor, "MIDIInput: No MIDI device %u in this browser (%d found)", index, enjin_midi_js_count());
        return false;
    }
    m_DeviceOpen = true;
    m_OpenDeviceName = GetDeviceName(index);
    std::memset(m_CCState, 0, sizeof(m_CCState));
    ENJIN_LOG_INFO(Editor, "MIDIInput: Opened device '%s'", m_OpenDeviceName.c_str());
    return true;
}

void MIDIInput::CloseDevice() {
    m_PendingOpen = -1;
    if (!m_DeviceOpen) return;
    enjin_midi_js_close();
    m_DeviceOpen = false;
    m_OpenDeviceName.clear();
}

void MIDIInput::OnWebAccessSettled(i32 deviceCount) {
    if (deviceCount < 0) {
        ENJIN_LOG_WARN(Editor, "MIDIInput: this browser has no MIDI access (unsupported, or the player refused it)");
        m_PendingOpen = -1;
        return;
    }
    ENJIN_LOG_INFO(Editor, "MIDIInput: browser MIDI access granted (%d devices)", deviceCount);
    if (m_PendingOpen >= 0) {
        const u32 index = static_cast<u32>(m_PendingOpen);
        m_PendingOpen = -1;
        OpenDevice(index);
    }
}

#else // Non-Windows stubs

u32 MIDIInput::GetDeviceCount() const { return 0; }
std::string MIDIInput::GetDeviceName(u32 /*index*/) const { return "N/A"; }
bool MIDIInput::OpenDevice(u32 /*index*/) { return false; }
void MIDIInput::CloseDevice() {}

#endif

// ============================================================================
// Query helpers (platform-independent)
// ============================================================================

bool MIDIInput::IsNoteOn(u8 note, u8 channel) const {
    for (const auto& ev : m_ReadEvents) {
        if (ev.type == MIDIMessageType::NoteOn && ev.data1 == note && ev.data2 > 0) {
            if (channel == 0xFF || ev.channel == channel) return true;
        }
    }
    return false;
}

bool MIDIInput::IsNoteOff(u8 note, u8 channel) const {
    for (const auto& ev : m_ReadEvents) {
        bool isOff = (ev.type == MIDIMessageType::NoteOff) ||
                     (ev.type == MIDIMessageType::NoteOn && ev.data2 == 0);
        if (isOff && ev.data1 == note) {
            if (channel == 0xFF || ev.channel == channel) return true;
        }
    }
    return false;
}

u8 MIDIInput::GetNoteVelocity(u8 note, u8 channel) const {
    for (const auto& ev : m_ReadEvents) {
        if (ev.type == MIDIMessageType::NoteOn && ev.data1 == note && ev.data2 > 0) {
            if (channel == 0xFF || ev.channel == channel) return ev.data2;
        }
    }
    return 0;
}

u8 MIDIInput::GetCC(u8 cc, u8 channel) const {
    for (const auto& ev : m_ReadEvents) {
        if (ev.type == MIDIMessageType::ControlChange && ev.data1 == cc) {
            if (channel == 0xFF || ev.channel == channel) return ev.data2;
        }
    }
    return 0;
}

void MIDIInput::InjectMessage(u8 status, u8 data1, u8 data2) {
    if (status < 0x80 || status >= 0xF0) return;   // running status / system messages
    const u8 msgType = status & 0xF0;
    const u8 channel = status & 0x0F;

    MIDIEvent ev;
    ev.type    = static_cast<MIDIMessageType>(msgType);
    ev.channel = channel;
    ev.data1   = data1 & 0x7F;
    ev.data2   = data2 & 0x7F;

    std::lock_guard<std::mutex> lock(m_EventMutex);
    if (msgType == 0xB0) m_CCState[channel][ev.data1] = ev.data2;
    m_WriteEvents.push_back(ev);
}

u8 MIDIInput::GetCCValue(u8 cc, u8 channel) const {
    if (channel >= 16 || cc >= 128) return 0;
    return m_CCState[channel][cc];
}

} // namespace InputSystem
} // namespace Enjin
