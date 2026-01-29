#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace Enjin {
namespace Audio {

// ============================================================================
// Audio Types
// ============================================================================

// Handle to a loaded sound
using SoundHandle = u32;
using ChannelHandle = u32;
constexpr SoundHandle INVALID_SOUND = 0;
constexpr ChannelHandle INVALID_CHANNEL = 0;

// Sound type
enum class SoundType : u8 {
    SoundEffect,    // Short, one-shot sounds
    Music,          // Streaming music
    Ambient,        // Looping ambient sounds
    Voice           // Voice/dialogue
};

// Sound loading mode
enum class LoadMode : u8 {
    Memory,         // Load entire file into memory
    Streaming       // Stream from disk (for large files)
};

// 3D sound attenuation mode
enum class AttenuationMode : u8 {
    None,           // No distance attenuation
    Linear,         // Linear falloff
    InverseDistance,// 1/d falloff
    InverseDistanceClamped,
    Logarithmic     // More realistic falloff
};

// ============================================================================
// Sound Settings
// ============================================================================

struct SoundSettings {
    SoundType type = SoundType::SoundEffect;
    LoadMode loadMode = LoadMode::Memory;
    bool loop = false;
    f32 volume = 1.0f;
    f32 pitch = 1.0f;

    // 3D settings
    bool is3D = false;
    f32 minDistance = 1.0f;     // Distance where volume is 100%
    f32 maxDistance = 100.0f;   // Distance where volume is 0%
    AttenuationMode attenuation = AttenuationMode::InverseDistanceClamped;

    // Streaming settings
    u32 bufferSize = 65536;     // Streaming buffer size
};

// ============================================================================
// Audio Listener
// ============================================================================

struct AudioListener {
    Math::Vector3 position;
    Math::Vector3 velocity;
    Math::Vector3 forward = Math::Vector3(0, 0, -1);
    Math::Vector3 up = Math::Vector3(0, 1, 0);
};

// ============================================================================
// Audio Backend Interface
// ============================================================================

// Abstract backend interface - implement for FMOD, Wwise, or custom
class ENJIN_API IAudioBackend {
public:
    virtual ~IAudioBackend() = default;

    // Lifecycle
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    virtual void Update() = 0;

    // Sound loading
    virtual SoundHandle LoadSound(const std::string& path, const SoundSettings& settings) = 0;
    virtual void UnloadSound(SoundHandle handle) = 0;

    // Playback
    virtual ChannelHandle Play(SoundHandle sound, const Math::Vector3& position = Math::Vector3(0,0,0)) = 0;
    virtual void Stop(ChannelHandle channel) = 0;
    virtual void Pause(ChannelHandle channel) = 0;
    virtual void Resume(ChannelHandle channel) = 0;

    // Channel properties
    virtual void SetChannelVolume(ChannelHandle channel, f32 volume) = 0;
    virtual void SetChannelPitch(ChannelHandle channel, f32 pitch) = 0;
    virtual void SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) = 0;
    virtual void SetChannelLoop(ChannelHandle channel, bool loop) = 0;
    virtual bool IsChannelPlaying(ChannelHandle channel) const = 0;

    // Master volume
    virtual void SetMasterVolume(f32 volume) = 0;
    virtual void SetCategoryVolume(SoundType type, f32 volume) = 0;

    // Listener
    virtual void SetListener(const AudioListener& listener) = 0;

    // Backend name
    virtual const char* GetBackendName() const = 0;
};

// ============================================================================
// FMOD Backend (Stub)
// ============================================================================

// FMOD integration stub - users can implement with actual FMOD SDK
class ENJIN_API FMODBackend : public IAudioBackend {
public:
    bool Initialize() override;
    void Shutdown() override;
    void Update() override;

    SoundHandle LoadSound(const std::string& path, const SoundSettings& settings) override;
    void UnloadSound(SoundHandle handle) override;

    ChannelHandle Play(SoundHandle sound, const Math::Vector3& position) override;
    void Stop(ChannelHandle channel) override;
    void Pause(ChannelHandle channel) override;
    void Resume(ChannelHandle channel) override;

    void SetChannelVolume(ChannelHandle channel, f32 volume) override;
    void SetChannelPitch(ChannelHandle channel, f32 pitch) override;
    void SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) override;
    void SetChannelLoop(ChannelHandle channel, bool loop) override;
    bool IsChannelPlaying(ChannelHandle channel) const override;

    void SetMasterVolume(f32 volume) override;
    void SetCategoryVolume(SoundType type, f32 volume) override;
    void SetListener(const AudioListener& listener) override;

    const char* GetBackendName() const override { return "FMOD (Stub)"; }

    // FMOD-specific features
    void SetParameter(const std::string& name, f32 value);
    void TriggerEvent(const std::string& eventPath);

private:
    bool m_Initialized = false;
    // Add FMOD::System*, FMOD::Studio::System* here when implementing
};

// ============================================================================
// Wwise Backend (Stub)
// ============================================================================

// Wwise integration stub - users can implement with actual Wwise SDK
class ENJIN_API WwiseBackend : public IAudioBackend {
public:
    bool Initialize() override;
    void Shutdown() override;
    void Update() override;

    SoundHandle LoadSound(const std::string& path, const SoundSettings& settings) override;
    void UnloadSound(SoundHandle handle) override;

    ChannelHandle Play(SoundHandle sound, const Math::Vector3& position) override;
    void Stop(ChannelHandle channel) override;
    void Pause(ChannelHandle channel) override;
    void Resume(ChannelHandle channel) override;

    void SetChannelVolume(ChannelHandle channel, f32 volume) override;
    void SetChannelPitch(ChannelHandle channel, f32 pitch) override;
    void SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) override;
    void SetChannelLoop(ChannelHandle channel, bool loop) override;
    bool IsChannelPlaying(ChannelHandle channel) const override;

    void SetMasterVolume(f32 volume) override;
    void SetCategoryVolume(SoundType type, f32 volume) override;
    void SetListener(const AudioListener& listener) override;

    const char* GetBackendName() const override { return "Wwise (Stub)"; }

    // Wwise-specific features
    void PostEvent(const std::string& eventName, u64 gameObjectId = 0);
    void SetRTPC(const std::string& name, f32 value, u64 gameObjectId = 0);
    void SetState(const std::string& stateGroup, const std::string& state);
    void SetSwitch(const std::string& switchGroup, const std::string& switchValue, u64 gameObjectId);
    void RegisterGameObject(u64 id, const std::string& name = "");
    void UnregisterGameObject(u64 id);

private:
    bool m_Initialized = false;
    // Add AK::SoundEngine pointers here when implementing
};

// ============================================================================
// Simple Audio Backend (Built-in, basic functionality)
// ============================================================================

class ENJIN_API SimpleAudioBackend : public IAudioBackend {
public:
    bool Initialize() override;
    void Shutdown() override;
    void Update() override;

    SoundHandle LoadSound(const std::string& path, const SoundSettings& settings) override;
    void UnloadSound(SoundHandle handle) override;

    ChannelHandle Play(SoundHandle sound, const Math::Vector3& position) override;
    void Stop(ChannelHandle channel) override;
    void Pause(ChannelHandle channel) override;
    void Resume(ChannelHandle channel) override;

    void SetChannelVolume(ChannelHandle channel, f32 volume) override;
    void SetChannelPitch(ChannelHandle channel, f32 pitch) override;
    void SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) override;
    void SetChannelLoop(ChannelHandle channel, bool loop) override;
    bool IsChannelPlaying(ChannelHandle channel) const override;

    void SetMasterVolume(f32 volume) override;
    void SetCategoryVolume(SoundType type, f32 volume) override;
    void SetListener(const AudioListener& listener) override;

    const char* GetBackendName() const override { return "Simple Audio"; }

private:
    struct LoadedSound {
        std::string path;
        SoundSettings settings;
        std::vector<u8> data;
        bool loaded = false;
    };

    struct PlayingChannel {
        SoundHandle soundHandle = INVALID_SOUND;
        Math::Vector3 position;
        f32 volume = 1.0f;
        f32 pitch = 1.0f;
        bool loop = false;
        bool playing = false;
        bool paused = false;
    };

    std::unordered_map<SoundHandle, LoadedSound> m_Sounds;
    std::unordered_map<ChannelHandle, PlayingChannel> m_Channels;
    SoundHandle m_NextSoundHandle = 1;
    ChannelHandle m_NextChannelHandle = 1;

    f32 m_MasterVolume = 1.0f;
    std::unordered_map<SoundType, f32> m_CategoryVolumes;
    AudioListener m_Listener;
    bool m_Initialized = false;
};

// ============================================================================
// Audio Manager (Main API)
// ============================================================================

class ENJIN_API AudioManager {
public:
    // Singleton access
    static AudioManager& Get();

    // Lifecycle
    bool Initialize(std::unique_ptr<IAudioBackend> backend = nullptr);
    void Shutdown();
    void Update();

    // Check if initialized
    bool IsInitialized() const { return m_Backend != nullptr; }

    // Get backend
    IAudioBackend* GetBackend() { return m_Backend.get(); }
    const char* GetBackendName() const;

    // High-level API
    SoundHandle LoadSound(const std::string& path, const SoundSettings& settings = SoundSettings{});
    void UnloadSound(SoundHandle handle);

    ChannelHandle PlaySound(SoundHandle sound);
    ChannelHandle PlaySoundAt(SoundHandle sound, const Math::Vector3& position);
    void StopChannel(ChannelHandle channel);
    void StopAllSounds();

    // One-shot helpers (loads, plays, and manages lifetime)
    void PlayOneShot(const std::string& path, f32 volume = 1.0f);
    void PlayOneShotAt(const std::string& path, const Math::Vector3& position, f32 volume = 1.0f);

    // Music
    void PlayMusic(const std::string& path, f32 fadeInTime = 0.0f);
    void StopMusic(f32 fadeOutTime = 0.0f);
    void SetMusicVolume(f32 volume);

    // Volume controls
    void SetMasterVolume(f32 volume);
    void SetSFXVolume(f32 volume);
    void SetMusicVolumeCategory(f32 volume);
    void SetVoiceVolume(f32 volume);

    f32 GetMasterVolume() const { return m_MasterVolume; }

    // Listener
    void SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up);
    void SetListenerVelocity(const Math::Vector3& velocity);

    // Preload sounds
    void PreloadSounds(const std::vector<std::string>& paths);
    void UnloadAllSounds();

private:
    AudioManager() = default;
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    std::unique_ptr<IAudioBackend> m_Backend;

    // Sound cache for one-shot sounds
    std::unordered_map<std::string, SoundHandle> m_SoundCache;

    // Music state
    ChannelHandle m_MusicChannel = INVALID_CHANNEL;
    SoundHandle m_CurrentMusic = INVALID_SOUND;
    f32 m_MusicFadeTime = 0.0f;
    f32 m_MusicFadeProgress = 1.0f;
    f32 m_MusicTargetVolume = 1.0f;
    bool m_MusicFadingOut = false;

    f32 m_MasterVolume = 1.0f;
    AudioListener m_Listener;
};

// ============================================================================
// Audio Component (for ECS)
// ============================================================================

struct AudioSourceComponent {
    // Sound to play
    std::string soundPath;
    SoundHandle loadedSound = INVALID_SOUND;

    // Playback settings
    f32 volume = 1.0f;
    f32 pitch = 1.0f;
    bool loop = false;
    bool playOnStart = false;
    bool is3D = true;

    // 3D settings
    f32 minDistance = 1.0f;
    f32 maxDistance = 50.0f;
    AttenuationMode attenuation = AttenuationMode::InverseDistanceClamped;

    // Runtime state
    ChannelHandle currentChannel = INVALID_CHANNEL;
    bool isPlaying = false;
};

// ============================================================================
// Audio Utilities
// ============================================================================

namespace AudioUtils {
    // Convert decibels to linear volume
    ENJIN_API f32 DbToLinear(f32 db);

    // Convert linear volume to decibels
    ENJIN_API f32 LinearToDb(f32 linear);

    // Calculate 3D volume based on distance
    ENJIN_API f32 Calculate3DVolume(f32 distance, f32 minDistance, f32 maxDistance, AttenuationMode mode);

    // Crossfade helper
    ENJIN_API f32 CrossfadeVolume(f32 progress, bool fadeIn);
}

} // namespace Audio
} // namespace Enjin
