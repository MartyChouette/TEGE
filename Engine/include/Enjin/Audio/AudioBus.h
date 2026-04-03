#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Math.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace Enjin::Audio {

// Forward declaration
class AudioMixer;

// A single bus in the audio routing hierarchy.
// Buses form a tree: Master → SFX/Music/Voice/UI → user sub-buses.
// Each bus has volume, mute, solo, and a VU level for metering.
struct ENJIN_API AudioBus {
    std::string name;
    f32 volume = 1.0f;           // Local volume (0-1)
    f32 targetVolume = 1.0f;     // For smooth transitions (snapshots, fades)
    f32 fadeSpeed = 5.0f;        // Volume interpolation speed (units/sec)
    bool muted = false;
    bool solo = false;

    // VU metering (updated per frame by the mixer)
    f32 vuLevel = 0.0f;          // Current RMS level (0-1)
    f32 vuPeak = 0.0f;           // Peak hold (decays slowly)
    u32 activeSoundCount = 0;    // Number of sounds playing through this bus

    // Per-bus parametric EQ (3-band: low shelf, mid bell, high shelf)
    struct EQBand {
        f32 frequency = 1000.0f;
        f32 gain = 0.0f;         // dB (-12 to +12)
        f32 q = 1.0f;            // Q factor (bell only)
    };
    EQBand eqLow  = {200.0f, 0.0f, 1.0f};
    EQBand eqMid  = {1000.0f, 0.0f, 1.0f};
    EQBand eqHigh = {5000.0f, 0.0f, 1.0f};
    bool eqEnabled = false;

    // Hierarchy
    AudioBus* parent = nullptr;
    std::vector<AudioBus*> children;

    // Compute effective volume (walks parent chain, respects mute)
    f32 GetEffectiveVolume() const {
        if (muted) return 0.0f;
        f32 v = volume;
        const AudioBus* bus = parent;
        while (bus) {
            if (bus->muted) return 0.0f;
            v *= bus->volume;
            bus = bus->parent;
        }
        return v;
    }
};

// Audio snapshot — state-based volume/effect presets that can be blended.
// Push "Dialogue" snapshot to duck music, push "Pause" to mute SFX, etc.
struct AudioSnapshot {
    std::string name;
    std::unordered_map<std::string, f32> busVolumes;  // bus name → target volume
    f32 transitionTime = 0.5f;
    i32 priority = 0;            // Higher priority snapshots override lower ones
};

// Built-in snapshot presets
namespace SnapshotPresets {
    inline AudioSnapshot Dialogue() {
        AudioSnapshot s;
        s.name = "Dialogue";
        s.busVolumes = {{"Music", 0.3f}, {"SFX", 0.5f}, {"UI", 0.3f}};
        s.transitionTime = 0.5f;
        s.priority = 10;
        return s;
    }
    inline AudioSnapshot Pause() {
        AudioSnapshot s;
        s.name = "Pause";
        s.busVolumes = {{"SFX", 0.1f}, {"Music", 0.2f}, {"Voice", 0.1f}};
        s.transitionTime = 0.3f;
        s.priority = 20;
        return s;
    }
    inline AudioSnapshot Combat() {
        AudioSnapshot s;
        s.name = "Combat";
        s.busVolumes = {{"SFX", 1.0f}, {"Music", 0.8f}};
        s.transitionTime = 1.0f;
        s.priority = 5;
        return s;
    }
    inline AudioSnapshot Cutscene() {
        AudioSnapshot s;
        s.name = "Cutscene";
        s.busVolumes = {{"SFX", 0.2f}, {"Music", 0.9f}, {"Voice", 1.0f}, {"UI", 0.0f}};
        s.transitionTime = 0.8f;
        s.priority = 15;
        return s;
    }
}

// Central audio mixer — owns the bus hierarchy and snapshot stack.
class ENJIN_API AudioMixer {
public:
    AudioMixer();
    ~AudioMixer() = default;

    // Update volume fades and snapshot transitions
    void Update(f32 deltaTime);

    // Bus management
    AudioBus* GetBus(const std::string& name);
    const AudioBus* GetBus(const std::string& name) const;
    AudioBus* CreateBus(const std::string& name, const std::string& parentName = "Master");
    AudioBus* GetMasterBus() { return &m_MasterBus; }

    // Convenience: get effective volume for a channel (backward-compatible)
    f32 GetEffectiveVolume(const std::string& busName) const;

    // Snapshot management
    void PushSnapshot(const AudioSnapshot& snapshot);
    void PopSnapshot(const std::string& name);
    bool IsSnapshotActive(const std::string& name) const;

    // VU level update (called by SimpleAudio with per-bus sound counts)
    void UpdateVU(const std::string& busName, f32 rmsLevel, u32 soundCount);

    // Mono downmix (accessibility)
    bool monoDownmix = false;

    // Get all buses for UI display
    const std::vector<AudioBus*>& GetAllBuses() const { return m_AllBuses; }

private:
    AudioBus m_MasterBus;
    std::unordered_map<std::string, AudioBus> m_Buses;  // Named buses (excludes Master)
    std::vector<AudioBus*> m_AllBuses;                   // Flat list for iteration

    // Snapshot stack (highest priority active wins per bus)
    std::vector<AudioSnapshot> m_ActiveSnapshots;

    void RebuildBusList();
};

// Music crossfader — manages smooth transitions between tracks.
class ENJIN_API MusicCrossfader {
public:
    enum class TransitionMode : u8 {
        LinearCrossfade,     // Both play simultaneously, volumes crossfade
        EqualPower,          // Perceptually smooth (sqrt curve)
        FadeOutThenIn,       // Gap between tracks
    };

    void Play(const std::string& clipPath, f32 fadeInTime = 2.0f, f32 fadeOutTime = 2.0f,
              TransitionMode mode = TransitionMode::EqualPower);
    void Stop(f32 fadeOutTime = 1.0f);
    void Update(f32 deltaTime);

    bool IsPlaying() const { return m_State != State::Idle; }
    const std::string& GetCurrentTrack() const { return m_TrackA; }

private:
    enum class State : u8 { Idle, FadingIn, Playing, Crossfading, FadingOut };
    State m_State = State::Idle;
    TransitionMode m_Mode = TransitionMode::EqualPower;

    std::string m_TrackA, m_TrackB;  // A = current, B = incoming
    f32 m_VolumeA = 0.0f, m_VolumeB = 0.0f;
    f32 m_FadeTimer = 0.0f, m_FadeDuration = 1.0f;
    u32 m_HandleA = 0, m_HandleB = 0;
};

} // namespace Enjin::Audio
