#include "Enjin/Audio/AudioSystem.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Audio {

// ============================================================================
// FMOD Backend (Stub Implementation)
// ============================================================================

bool FMODBackend::Initialize() {
    ENJIN_LOG_INFO(Audio, "FMOD Backend: Initialize called (stub)");
    ENJIN_LOG_WARN(Audio, "FMOD Backend is a stub - implement with actual FMOD SDK for full functionality");
    m_Initialized = true;
    return true;
}

void FMODBackend::Shutdown() {
    ENJIN_LOG_INFO(Audio, "FMOD Backend: Shutdown");
    m_Initialized = false;
}

void FMODBackend::Update() {
    // FMOD::System::update() would go here
}

SoundHandle FMODBackend::LoadSound(const std::string& path, const SoundSettings& settings) {
    ENJIN_LOG_INFO(Audio, "FMOD Backend: LoadSound '%s' (stub)", path.c_str());
    return INVALID_SOUND;
}

void FMODBackend::UnloadSound(SoundHandle handle) {
    ENJIN_LOG_INFO(Audio, "FMOD Backend: UnloadSound %u (stub)", handle);
}

ChannelHandle FMODBackend::Play(SoundHandle sound, const Math::Vector3& position) {
    ENJIN_LOG_INFO(Audio, "FMOD Backend: Play sound %u (stub)", sound);
    return INVALID_CHANNEL;
}

void FMODBackend::Stop(ChannelHandle channel) {}
void FMODBackend::Pause(ChannelHandle channel) {}
void FMODBackend::Resume(ChannelHandle channel) {}

void FMODBackend::SetChannelVolume(ChannelHandle channel, f32 volume) {}
void FMODBackend::SetChannelPitch(ChannelHandle channel, f32 pitch) {}
void FMODBackend::SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) {}
void FMODBackend::SetChannelLoop(ChannelHandle channel, bool loop) {}
bool FMODBackend::IsChannelPlaying(ChannelHandle channel) const { return false; }

void FMODBackend::SetMasterVolume(f32 volume) {}
void FMODBackend::SetCategoryVolume(SoundType type, f32 volume) {}
void FMODBackend::SetListener(const AudioListener& listener) {}

void FMODBackend::SetParameter(const std::string& name, f32 value) {
    ENJIN_LOG_INFO(Audio, "FMOD Backend: SetParameter '%s' = %f (stub)", name.c_str(), value);
}

void FMODBackend::TriggerEvent(const std::string& eventPath) {
    ENJIN_LOG_INFO(Audio, "FMOD Backend: TriggerEvent '%s' (stub)", eventPath.c_str());
}

// ============================================================================
// Wwise Backend (Stub Implementation)
// ============================================================================

bool WwiseBackend::Initialize() {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: Initialize called (stub)");
    ENJIN_LOG_WARN(Audio, "Wwise Backend is a stub - implement with actual Wwise SDK for full functionality");
    m_Initialized = true;
    return true;
}

void WwiseBackend::Shutdown() {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: Shutdown");
    m_Initialized = false;
}

void WwiseBackend::Update() {
    // AK::SoundEngine::RenderAudio() would go here
}

SoundHandle WwiseBackend::LoadSound(const std::string& path, const SoundSettings& settings) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: LoadSound '%s' (stub)", path.c_str());
    return INVALID_SOUND;
}

void WwiseBackend::UnloadSound(SoundHandle handle) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: UnloadSound %u (stub)", handle);
}

ChannelHandle WwiseBackend::Play(SoundHandle sound, const Math::Vector3& position) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: Play sound %u (stub)", sound);
    return INVALID_CHANNEL;
}

void WwiseBackend::Stop(ChannelHandle channel) {}
void WwiseBackend::Pause(ChannelHandle channel) {}
void WwiseBackend::Resume(ChannelHandle channel) {}

void WwiseBackend::SetChannelVolume(ChannelHandle channel, f32 volume) {}
void WwiseBackend::SetChannelPitch(ChannelHandle channel, f32 pitch) {}
void WwiseBackend::SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) {}
void WwiseBackend::SetChannelLoop(ChannelHandle channel, bool loop) {}
bool WwiseBackend::IsChannelPlaying(ChannelHandle channel) const { return false; }

void WwiseBackend::SetMasterVolume(f32 volume) {}
void WwiseBackend::SetCategoryVolume(SoundType type, f32 volume) {}
void WwiseBackend::SetListener(const AudioListener& listener) {}

void WwiseBackend::PostEvent(const std::string& eventName, u64 gameObjectId) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: PostEvent '%s' on object %llu (stub)", eventName.c_str(), gameObjectId);
}

void WwiseBackend::SetRTPC(const std::string& name, f32 value, u64 gameObjectId) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: SetRTPC '%s' = %f on object %llu (stub)", name.c_str(), value, gameObjectId);
}

void WwiseBackend::SetState(const std::string& stateGroup, const std::string& state) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: SetState '%s' = '%s' (stub)", stateGroup.c_str(), state.c_str());
}

void WwiseBackend::SetSwitch(const std::string& switchGroup, const std::string& switchValue, u64 gameObjectId) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: SetSwitch '%s' = '%s' on object %llu (stub)",
                   switchGroup.c_str(), switchValue.c_str(), gameObjectId);
}

void WwiseBackend::RegisterGameObject(u64 id, const std::string& name) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: RegisterGameObject %llu '%s' (stub)", id, name.c_str());
}

void WwiseBackend::UnregisterGameObject(u64 id) {
    ENJIN_LOG_INFO(Audio, "Wwise Backend: UnregisterGameObject %llu (stub)", id);
}

// ============================================================================
// Simple Audio Backend (Built-in Implementation)
// ============================================================================

bool SimpleAudioBackend::Initialize() {
    ENJIN_LOG_INFO(Audio, "Simple Audio Backend: Initialize");

    // Initialize category volumes
    m_CategoryVolumes[SoundType::SoundEffect] = 1.0f;
    m_CategoryVolumes[SoundType::Music] = 1.0f;
    m_CategoryVolumes[SoundType::Ambient] = 1.0f;
    m_CategoryVolumes[SoundType::Voice] = 1.0f;

    m_Initialized = true;
    return true;
}

void SimpleAudioBackend::Shutdown() {
    ENJIN_LOG_INFO(Audio, "Simple Audio Backend: Shutdown");
    m_Sounds.clear();
    m_Channels.clear();
    m_Initialized = false;
}

void SimpleAudioBackend::Update() {
    // Update playing channels, check for finished sounds, etc.
    // In a real implementation, this would update the audio mixer

    // Clean up finished channels
    for (auto it = m_Channels.begin(); it != m_Channels.end();) {
        if (!it->second.playing && !it->second.paused) {
            it = m_Channels.erase(it);
        } else {
            ++it;
        }
    }
}

SoundHandle SimpleAudioBackend::LoadSound(const std::string& path, const SoundSettings& settings) {
    ENJIN_LOG_INFO(Audio, "Simple Audio Backend: LoadSound '%s'", path.c_str());

    SoundHandle handle = m_NextSoundHandle++;

    LoadedSound sound;
    sound.path = path;
    sound.settings = settings;
    sound.loaded = true;  // In real impl, would actually load the file

    // Note: Actual audio file loading would go here
    // For now, we just store the metadata

    m_Sounds[handle] = std::move(sound);
    return handle;
}

void SimpleAudioBackend::UnloadSound(SoundHandle handle) {
    ENJIN_LOG_INFO(Audio, "Simple Audio Backend: UnloadSound %u", handle);
    m_Sounds.erase(handle);
}

ChannelHandle SimpleAudioBackend::Play(SoundHandle sound, const Math::Vector3& position) {
    auto it = m_Sounds.find(sound);
    if (it == m_Sounds.end()) {
        ENJIN_LOG_WARN(Audio, "Simple Audio Backend: Cannot play invalid sound %u", sound);
        return INVALID_CHANNEL;
    }

    ChannelHandle handle = m_NextChannelHandle++;

    PlayingChannel channel;
    channel.soundHandle = sound;
    channel.position = position;
    channel.volume = it->second.settings.volume;
    channel.pitch = it->second.settings.pitch;
    channel.loop = it->second.settings.loop;
    channel.playing = true;
    channel.paused = false;

    m_Channels[handle] = channel;

    ENJIN_LOG_INFO(Audio, "Simple Audio Backend: Playing sound %u on channel %u", sound, handle);
    return handle;
}

void SimpleAudioBackend::Stop(ChannelHandle channel) {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        it->second.playing = false;
        it->second.paused = false;
    }
}

void SimpleAudioBackend::Pause(ChannelHandle channel) {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        it->second.paused = true;
    }
}

void SimpleAudioBackend::Resume(ChannelHandle channel) {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        it->second.paused = false;
    }
}

void SimpleAudioBackend::SetChannelVolume(ChannelHandle channel, f32 volume) {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        it->second.volume = std::clamp(volume, 0.0f, 1.0f);
    }
}

void SimpleAudioBackend::SetChannelPitch(ChannelHandle channel, f32 pitch) {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        it->second.pitch = std::clamp(pitch, 0.1f, 4.0f);
    }
}

void SimpleAudioBackend::SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        it->second.position = position;
    }
}

void SimpleAudioBackend::SetChannelLoop(ChannelHandle channel, bool loop) {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        it->second.loop = loop;
    }
}

bool SimpleAudioBackend::IsChannelPlaying(ChannelHandle channel) const {
    auto it = m_Channels.find(channel);
    if (it != m_Channels.end()) {
        return it->second.playing && !it->second.paused;
    }
    return false;
}

void SimpleAudioBackend::SetMasterVolume(f32 volume) {
    m_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
}

void SimpleAudioBackend::SetCategoryVolume(SoundType type, f32 volume) {
    m_CategoryVolumes[type] = std::clamp(volume, 0.0f, 1.0f);
}

void SimpleAudioBackend::SetListener(const AudioListener& listener) {
    m_Listener = listener;
}

// ============================================================================
// Audio Manager
// ============================================================================

AudioManager& AudioManager::Get() {
    static AudioManager instance;
    return instance;
}

AudioManager::~AudioManager() {
    Shutdown();
}

bool AudioManager::Initialize(std::unique_ptr<IAudioBackend> backend) {
    if (m_Backend) {
        ENJIN_LOG_WARN(Audio, "AudioManager already initialized");
        return true;
    }

    // Use provided backend or default to SimpleAudioBackend
    if (backend) {
        m_Backend = std::move(backend);
    } else {
        m_Backend = std::make_unique<SimpleAudioBackend>();
    }

    if (!m_Backend->Initialize()) {
        ENJIN_LOG_ERROR(Audio, "Failed to initialize audio backend");
        m_Backend.reset();
        return false;
    }

    ENJIN_LOG_INFO(Audio, "AudioManager initialized with backend: %s", m_Backend->GetBackendName());
    return true;
}

void AudioManager::Shutdown() {
    if (!m_Backend) return;

    StopAllSounds();
    UnloadAllSounds();

    m_Backend->Shutdown();
    m_Backend.reset();

    ENJIN_LOG_INFO(Audio, "AudioManager shutdown");
}

void AudioManager::Update() {
    if (!m_Backend) return;

    m_Backend->Update();

    // Handle music fade
    if (m_MusicFadeTime > 0.0f && m_MusicFadeProgress < 1.0f) {
        // Note: deltaTime would need to be passed in or tracked
        // For now, this is a simplified implementation
    }
}

const char* AudioManager::GetBackendName() const {
    return m_Backend ? m_Backend->GetBackendName() : "None";
}

SoundHandle AudioManager::LoadSound(const std::string& path, const SoundSettings& settings) {
    if (!m_Backend) return INVALID_SOUND;
    return m_Backend->LoadSound(path, settings);
}

void AudioManager::UnloadSound(SoundHandle handle) {
    if (!m_Backend) return;
    m_Backend->UnloadSound(handle);
}

ChannelHandle AudioManager::PlaySound(SoundHandle sound) {
    if (!m_Backend) return INVALID_CHANNEL;
    return m_Backend->Play(sound, Math::Vector3(0, 0, 0));
}

ChannelHandle AudioManager::PlaySoundAt(SoundHandle sound, const Math::Vector3& position) {
    if (!m_Backend) return INVALID_CHANNEL;
    return m_Backend->Play(sound, position);
}

void AudioManager::StopChannel(ChannelHandle channel) {
    if (!m_Backend) return;
    m_Backend->Stop(channel);
}

void AudioManager::StopAllSounds() {
    if (!m_Backend) return;
    // In a full implementation, we'd track all playing channels
    // For now, stop music channel if playing
    if (m_MusicChannel != INVALID_CHANNEL) {
        m_Backend->Stop(m_MusicChannel);
        m_MusicChannel = INVALID_CHANNEL;
    }
}

void AudioManager::PlayOneShot(const std::string& path, f32 volume) {
    if (!m_Backend) return;

    // Check cache
    auto it = m_SoundCache.find(path);
    SoundHandle handle;

    if (it != m_SoundCache.end()) {
        handle = it->second;
    } else {
        SoundSettings settings;
        settings.type = SoundType::SoundEffect;
        settings.loadMode = LoadMode::Memory;
        handle = m_Backend->LoadSound(path, settings);
        m_SoundCache[path] = handle;
    }

    ChannelHandle channel = m_Backend->Play(handle, Math::Vector3(0, 0, 0));
    m_Backend->SetChannelVolume(channel, volume);
}

void AudioManager::PlayOneShotAt(const std::string& path, const Math::Vector3& position, f32 volume) {
    if (!m_Backend) return;

    auto it = m_SoundCache.find(path);
    SoundHandle handle;

    if (it != m_SoundCache.end()) {
        handle = it->second;
    } else {
        SoundSettings settings;
        settings.type = SoundType::SoundEffect;
        settings.loadMode = LoadMode::Memory;
        settings.is3D = true;
        handle = m_Backend->LoadSound(path, settings);
        m_SoundCache[path] = handle;
    }

    ChannelHandle channel = m_Backend->Play(handle, position);
    m_Backend->SetChannelVolume(channel, volume);
}

void AudioManager::PlayMusic(const std::string& path, f32 fadeInTime) {
    if (!m_Backend) return;

    // Stop current music
    if (m_MusicChannel != INVALID_CHANNEL) {
        m_Backend->Stop(m_MusicChannel);
    }
    if (m_CurrentMusic != INVALID_SOUND) {
        m_Backend->UnloadSound(m_CurrentMusic);
    }

    // Load new music
    SoundSettings settings;
    settings.type = SoundType::Music;
    settings.loadMode = LoadMode::Streaming;
    settings.loop = true;

    m_CurrentMusic = m_Backend->LoadSound(path, settings);
    m_MusicChannel = m_Backend->Play(m_CurrentMusic, Math::Vector3(0, 0, 0));

    if (fadeInTime > 0.0f) {
        m_MusicFadeTime = fadeInTime;
        m_MusicFadeProgress = 0.0f;
        m_MusicFadingOut = false;
        m_Backend->SetChannelVolume(m_MusicChannel, 0.0f);
    } else {
        m_Backend->SetChannelVolume(m_MusicChannel, m_MusicTargetVolume);
    }
}

void AudioManager::StopMusic(f32 fadeOutTime) {
    if (!m_Backend || m_MusicChannel == INVALID_CHANNEL) return;

    if (fadeOutTime > 0.0f) {
        m_MusicFadeTime = fadeOutTime;
        m_MusicFadeProgress = 0.0f;
        m_MusicFadingOut = true;
    } else {
        m_Backend->Stop(m_MusicChannel);
        m_MusicChannel = INVALID_CHANNEL;
    }
}

void AudioManager::SetMusicVolume(f32 volume) {
    m_MusicTargetVolume = std::clamp(volume, 0.0f, 1.0f);
    if (m_Backend && m_MusicChannel != INVALID_CHANNEL) {
        m_Backend->SetChannelVolume(m_MusicChannel, m_MusicTargetVolume);
    }
}

void AudioManager::SetMasterVolume(f32 volume) {
    m_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (m_Backend) {
        m_Backend->SetMasterVolume(m_MasterVolume);
    }
}

void AudioManager::SetSFXVolume(f32 volume) {
    if (m_Backend) {
        m_Backend->SetCategoryVolume(SoundType::SoundEffect, volume);
    }
}

void AudioManager::SetMusicVolumeCategory(f32 volume) {
    if (m_Backend) {
        m_Backend->SetCategoryVolume(SoundType::Music, volume);
    }
}

void AudioManager::SetVoiceVolume(f32 volume) {
    if (m_Backend) {
        m_Backend->SetCategoryVolume(SoundType::Voice, volume);
    }
}

void AudioManager::SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up) {
    m_Listener.position = position;
    m_Listener.forward = forward;
    m_Listener.up = up;

    if (m_Backend) {
        m_Backend->SetListener(m_Listener);
    }
}

void AudioManager::SetListenerVelocity(const Math::Vector3& velocity) {
    m_Listener.velocity = velocity;

    if (m_Backend) {
        m_Backend->SetListener(m_Listener);
    }
}

void AudioManager::PreloadSounds(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        if (m_SoundCache.find(path) == m_SoundCache.end()) {
            SoundSettings settings;
            SoundHandle handle = LoadSound(path, settings);
            m_SoundCache[path] = handle;
        }
    }
}

void AudioManager::UnloadAllSounds() {
    if (!m_Backend) return;

    for (auto& pair : m_SoundCache) {
        m_Backend->UnloadSound(pair.second);
    }
    m_SoundCache.clear();

    if (m_CurrentMusic != INVALID_SOUND) {
        m_Backend->UnloadSound(m_CurrentMusic);
        m_CurrentMusic = INVALID_SOUND;
    }
}

// ============================================================================
// Audio Utilities
// ============================================================================

namespace AudioUtils {

f32 DbToLinear(f32 db) {
    return std::pow(10.0f, db / 20.0f);
}

f32 LinearToDb(f32 linear) {
    if (linear <= 0.0f) return -100.0f;  // Effectively silent
    return 20.0f * std::log10(linear);
}

f32 Calculate3DVolume(f32 distance, f32 minDistance, f32 maxDistance, AttenuationMode mode) {
    if (distance <= minDistance) return 1.0f;
    if (distance >= maxDistance) return 0.0f;

    f32 range = maxDistance - minDistance;
    f32 normalizedDist = (distance - minDistance) / range;

    switch (mode) {
        case AttenuationMode::None:
            return 1.0f;

        case AttenuationMode::Linear:
            return 1.0f - normalizedDist;

        case AttenuationMode::InverseDistance:
            return minDistance / distance;

        case AttenuationMode::InverseDistanceClamped:
            return std::clamp(minDistance / distance, 0.0f, 1.0f);

        case AttenuationMode::Logarithmic:
            // Logarithmic falloff for more realistic sound
            return std::max(0.0f, 1.0f - std::log10(1.0f + 9.0f * normalizedDist));
    }

    return 1.0f;
}

f32 CrossfadeVolume(f32 progress, bool fadeIn) {
    // Use equal-power crossfade for smooth transitions
    f32 angle = progress * 3.14159f * 0.5f;
    if (fadeIn) {
        return std::sin(angle);
    } else {
        return std::cos(angle);
    }
}

} // namespace AudioUtils

} // namespace Audio
} // namespace Enjin
