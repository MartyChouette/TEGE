#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vector>
#include <string>
#include <mutex>

#ifdef ENJIN_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
// Prevent Windows macros from polluting Enjin identifiers
#undef LoadImage
#undef CreateWindow
#undef min
#undef max
#undef near
#undef far
#endif

namespace Enjin {
namespace InputSystem {

// MIDI message types (status byte high nibble)
enum class MIDIMessageType : u8 {
    NoteOff        = 0x80,
    NoteOn         = 0x90,
    PolyPressure   = 0xA0,
    ControlChange  = 0xB0,
    ProgramChange  = 0xC0,
    ChannelPressure= 0xD0,
    PitchBend      = 0xE0
};

// A single MIDI event captured from the device
struct MIDIEvent {
    MIDIMessageType type = MIDIMessageType::NoteOff;
    u8 channel  = 0;    // 0-15
    u8 data1    = 0;    // Note number or CC number (0-127)
    u8 data2    = 0;    // Velocity or CC value (0-127)
    f64 timestamp = 0.0; // Seconds since MIDI open
};

// Lightweight MIDI input manager
// Provides device enumeration, open/close, per-frame event polling
class ENJIN_API MIDIInput {
public:
    MIDIInput();
    ~MIDIInput();

    // Lifecycle
    bool Initialize();
    void Shutdown();

    // Call once per frame — moves buffered events into readable queue
    void Update();

    // Device enumeration
    u32 GetDeviceCount() const;
    std::string GetDeviceName(u32 index) const;

    // Open / close a specific device (only one at a time)
    bool OpenDevice(u32 index);
    void CloseDevice();
    bool IsDeviceOpen() const { return m_DeviceOpen; }
    std::string GetOpenDeviceName() const { return m_OpenDeviceName; }

    // Per-frame event access (valid until next Update())
    const std::vector<MIDIEvent>& GetEvents() const { return m_ReadEvents; }

    // Convenience queries on current frame's events
    bool IsNoteOn(u8 note, u8 channel = 0xFF) const;
    bool IsNoteOff(u8 note, u8 channel = 0xFF) const;
    u8   GetNoteVelocity(u8 note, u8 channel = 0xFF) const;
    u8   GetCC(u8 cc, u8 channel = 0xFF) const;

    // Persistent CC state (survives across frames)
    u8 GetCCValue(u8 cc, u8 channel = 0) const;

private:
    bool m_Initialized = false;
    bool m_DeviceOpen = false;
    std::string m_OpenDeviceName;

    // Double-buffered events: callback writes to m_WriteEvents, Update() swaps to m_ReadEvents
    std::vector<MIDIEvent> m_WriteEvents;
    std::vector<MIDIEvent> m_ReadEvents;
    std::mutex m_EventMutex;

    // Persistent CC values (16 channels x 128 controllers)
    u8 m_CCState[16][128] = {};

#ifdef ENJIN_PLATFORM_WINDOWS
    HMIDIIN m_MIDIHandle = nullptr;
    static void CALLBACK MidiInCallback(HMIDIIN hMidiIn, UINT wMsg,
                                         DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2);
#endif
};

} // namespace InputSystem
} // namespace Enjin
