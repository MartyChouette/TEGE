#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>

namespace Enjin {
namespace Audio {

// Audio clip handle
using AudioClipHandle = u32;
constexpr AudioClipHandle INVALID_AUDIO_CLIP = 0;

// Sound instance (playing sound)
struct SoundInstance {
    AudioClipHandle clip = INVALID_AUDIO_CLIP;
    f32 volume = 1.0f;
    f32 pitch = 1.0f;
    f32 pan = 0.0f;        // -1 = left, 0 = center, 1 = right
    bool loop = false;
    bool is3D = false;
    Math::Vector3 position;
    f32 minDistance = 1.0f;
    f32 maxDistance = 500.0f;

    // State
    bool isPlaying = false;
    f32 playbackPosition = 0.0f;
};

// SoundHandle may already be defined by AudioSystem.h — guard against redefinition
#ifndef ENJIN_SOUND_HANDLE_DEFINED
#define ENJIN_SOUND_HANDLE_DEFINED
using SoundHandle = u32;
constexpr SoundHandle INVALID_SOUND = 0;
#endif

// Simple audio manager
// Note: This is a stub that can be extended with actual audio playback
// (miniaudio, OpenAL, FMOD, etc.)
class ENJIN_API SimpleAudio {
public:
    SimpleAudio() = default;
    ~SimpleAudio();

    bool Initialize();
    void Shutdown();

    void SetWorld(ECS::World* world) { m_World = world; }

    // Set listener position (usually camera)
    void SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up);

    // Load audio clip from file
    AudioClipHandle LoadClip(const std::string& filepath);

    // Unload audio clip
    void UnloadClip(AudioClipHandle clip);

    // S-L1: Remove clips that have no active sound instances referencing them
    void CleanupUnusedClips();

    // Play a sound
    SoundHandle Play(AudioClipHandle clip, f32 volume = 1.0f, f32 pitch = 1.0f, bool loop = false);

    // Play 3D sound at position
    SoundHandle Play3D(AudioClipHandle clip, const Math::Vector3& position,
                       f32 volume = 1.0f, f32 minDist = 1.0f, f32 maxDist = 500.0f);

    // Play one-shot (fire and forget)
    void PlayOneShot(AudioClipHandle clip, f32 volume = 1.0f);
    void PlayOneShot3D(AudioClipHandle clip, const Math::Vector3& position, f32 volume = 1.0f);

    // Control playing sounds
    void Stop(SoundHandle sound);
    void StopAll();
    void Pause(SoundHandle sound);
    void Resume(SoundHandle sound);
    void SetVolume(SoundHandle sound, f32 volume);
    void SetPitch(SoundHandle sound, f32 pitch);
    void SetPosition(SoundHandle sound, const Math::Vector3& position);

    // Query
    bool IsPlaying(SoundHandle sound) const;

    // Master volume
    void SetMasterVolume(f32 volume) { m_MasterVolume = Math::Clamp(volume, 0.0f, 1.0f); }
    f32 GetMasterVolume() const { return m_MasterVolume; }

    // Update (call every frame to update 3D audio, fade-outs, etc.)
    void Update(f32 deltaTime);

    // Update ECS audio sources
    void UpdateAudioSources(f32 deltaTime);

    // Callback for accessibility audio visual indicators (Task #38)
    // Called whenever a sound is played, with the clip filepath as label
    using SoundPlayedCallback = std::function<void(const std::string& soundName)>;
    void SetOnSoundPlayed(SoundPlayedCallback cb) { m_OnSoundPlayed = std::move(cb); }

private:
    f32 Calculate3DVolume(const Math::Vector3& soundPos, f32 minDist, f32 maxDist) const;

    ECS::World* m_World = nullptr;

    // Listener (camera) state
    Math::Vector3 m_ListenerPosition;
    Math::Vector3 m_ListenerForward = Math::Vector3(0, 0, -1);
    Math::Vector3 m_ListenerUp = Math::Vector3(0, 1, 0);

    f32 m_MasterVolume = 1.0f;

    // Loaded audio clip data
    struct AudioClipData {
        std::string filepath;
        std::vector<u8> pcmData;       // Raw PCM samples
        u32 sampleRate = 44100;
        u16 channels = 1;
        u16 bitsPerSample = 16;
        f32 duration = 0.0f;           // Duration in seconds
        bool loaded = false;           // True if PCM data is loaded
    };

    bool LoadWAV(const std::string& filepath, AudioClipData& clip);

    std::unordered_map<AudioClipHandle, AudioClipData> m_Clips;
    AudioClipHandle m_NextClipHandle = 1;

    // Playing sounds
    std::unordered_map<SoundHandle, SoundInstance> m_Sounds;
    SoundHandle m_NextSoundHandle = 1;

    bool m_Initialized = false;

    // Accessibility callback
    SoundPlayedCallback m_OnSoundPlayed;
};

} // namespace Audio
} // namespace Enjin
