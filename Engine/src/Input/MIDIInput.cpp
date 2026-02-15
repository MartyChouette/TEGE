#include "Enjin/Input/MIDIInput.h"
#include "Enjin/Logging/Log.h"
#include <cstring>

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

    u8 status  = static_cast<u8>(dwParam1 & 0xFF);
    u8 data1   = static_cast<u8>((dwParam1 >> 8) & 0xFF);
    u8 data2   = static_cast<u8>((dwParam1 >> 16) & 0xFF);
    u8 msgType = status & 0xF0;
    u8 channel = status & 0x0F;

    MIDIEvent ev;
    ev.type    = static_cast<MIDIMessageType>(msgType);
    ev.channel = channel;
    ev.data1   = data1;
    ev.data2   = data2;

    // Update persistent CC state
    if (msgType == 0xB0) {
        self->m_CCState[channel][data1] = data2;
    }

    std::lock_guard<std::mutex> lock(self->m_EventMutex);
    self->m_WriteEvents.push_back(ev);
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

u8 MIDIInput::GetCCValue(u8 cc, u8 channel) const {
    if (channel >= 16 || cc >= 128) return 0;
    return m_CCState[channel][cc];
}

} // namespace InputSystem
} // namespace Enjin
