#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <fstream>
#include <unordered_set>

#include "miniaudio.h"

namespace Enjin {
namespace Audio {

// pImpl holding the ma_engine
struct SimpleAudio::Impl {
    ma_engine engine{};
    bool initialized = false;
};

SimpleAudio::SimpleAudio()
    : m_Impl(std::make_unique<Impl>()) {
}

SimpleAudio::~SimpleAudio() {
    Shutdown();
}

bool SimpleAudio::Initialize() {
    if (m_Initialized) return true;

    ma_result result = ma_engine_init(nullptr, &m_Impl->engine);
    if (result != MA_SUCCESS) {
        ENJIN_LOG_ERROR(Audio, "Failed to initialize miniaudio engine (error %d)", result);
        // Fall back to initialized-but-silent mode so the rest of the engine works
        m_Initialized = true;
        return true;
    }

    m_Impl->initialized = true;
    m_Initialized = true;
    ENJIN_LOG_INFO(Audio, "SimpleAudio initialized (miniaudio backend)");
    return true;
}

void SimpleAudio::Shutdown() {
    if (!m_Initialized) return;

    StopAll();
    m_Clips.clear();

    if (m_Impl && m_Impl->initialized) {
        ma_engine_uninit(&m_Impl->engine);
        m_Impl->initialized = false;
    }

    m_Initialized = false;
    ENJIN_LOG_INFO(Audio, "SimpleAudio shutdown");
}

void SimpleAudio::SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up) {
    m_ListenerPosition = position;
    m_ListenerForward = forward;
    m_ListenerUp = up;

    if (m_Impl && m_Impl->initialized) {
        ma_engine_listener_set_position(&m_Impl->engine, 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(&m_Impl->engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&m_Impl->engine, 0, up.x, up.y, up.z);
    }
}

bool SimpleAudio::LoadWAV(const std::string& filepath, AudioClipData& clip) {
    // With miniaudio, we don't need to manually parse WAV — ma_sound_init_from_file handles it.
    // Just verify the file exists and store the path.
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Audio, "Audio file not found: %s", filepath.c_str());
        return false;
    }

    // Get file size for duration estimate (miniaudio will decode it properly)
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    clip.loaded = true;
    clip.filepath = filepath;
    clip.duration = 0.0f; // Will be determined by miniaudio at play time

    ENJIN_LOG_INFO(Audio, "Registered audio file: %s (%lld bytes)", filepath.c_str(), (long long)fileSize);
    return true;
}

AudioClipHandle SimpleAudio::LoadClip(const std::string& filepath) {
    // Check if already loaded
    for (const auto& [handle, clip] : m_Clips) {
        if (clip.filepath == filepath) {
            return handle;
        }
    }

    AudioClipHandle handle = m_NextClipHandle++;
    if (m_NextClipHandle == 0) m_NextClipHandle = 1;
    AudioClipData clipData;
    clipData.filepath = filepath;

    // Verify file exists (miniaudio supports WAV, MP3, FLAC, and Vorbis natively)
    std::ifstream testFile(filepath, std::ios::binary);
    if (testFile.is_open()) {
        clipData.loaded = true;
        testFile.close();
    } else {
        ENJIN_LOG_WARN(Audio, "Audio file not found: %s", filepath.c_str());
    }

    m_Clips[handle] = std::move(clipData);
    ENJIN_LOG_INFO(Audio, "Registered audio clip: %s (handle: %u)", filepath.c_str(), handle);
    return handle;
}

void SimpleAudio::UnloadClip(AudioClipHandle clip) {
    auto it = m_Clips.find(clip);
    if (it != m_Clips.end()) {
        // Stop any sounds using this clip
        for (auto& [handle, sound] : m_Sounds) {
            if (sound.clip == clip && sound.isPlaying) {
                CleanupSound(sound);
            }
        }

        ENJIN_LOG_INFO(Audio, "Unloaded audio clip: %s", it->second.filepath.c_str());
        m_Clips.erase(it);
    }
}

void SimpleAudio::CleanupUnusedClips() {
    std::unordered_set<AudioClipHandle> referencedClips;
    for (const auto& [handle, sound] : m_Sounds) {
        referencedClips.insert(sound.clip);
    }

    for (auto it = m_Clips.begin(); it != m_Clips.end(); ) {
        if (referencedClips.find(it->first) == referencedClips.end()) {
            ENJIN_LOG_INFO(Audio, "Cleaning up unused audio clip: %s", it->second.filepath.c_str());
            it = m_Clips.erase(it);
        } else {
            ++it;
        }
    }
}

void SimpleAudio::CleanupSound(SoundInstance& sound) {
    if (sound.maSound) {
        auto* maS = static_cast<ma_sound*>(sound.maSound);
        ma_sound_stop(maS);
        ma_sound_uninit(maS);
        delete maS;
        sound.maSound = nullptr;
    }
    sound.isPlaying = false;
}

SoundHandle SimpleAudio::Play(AudioClipHandle clip, f32 volume, f32 pitch, bool loop,
                              AudioChannel channel) {
    auto clipIt = m_Clips.find(clip);
    if (clipIt == m_Clips.end()) {
        ENJIN_LOG_WARN(Audio, "Tried to play invalid clip: %u", clip);
        return INVALID_SOUND;
    }

    // Cap active sound count to prevent unbounded growth
    static constexpr usize MAX_ACTIVE_SOUNDS = 256;
    if (m_Sounds.size() >= MAX_ACTIVE_SOUNDS) {
        ENJIN_LOG_WARN(Audio, "Max active sounds reached (%zu), cannot play", MAX_ACTIVE_SOUNDS);
        return INVALID_SOUND;
    }

    SoundHandle handle = m_NextSoundHandle++;
    if (m_NextSoundHandle == 0) m_NextSoundHandle = 1;

    // Clamp parameters to valid ranges
    f32 clampedVolume = Math::Clamp(volume, 0.0f, 1.0f);
    f32 clampedPitch = Math::Clamp(pitch, 0.1f, 3.0f);

    SoundInstance sound;
    sound.clip = clip;
    sound.volume = clampedVolume;
    sound.pitch = clampedPitch;
    sound.loop = loop;
    sound.is3D = false;
    sound.channel = channel;
    sound.isPlaying = true;

    // Create miniaudio sound from file
    if (m_Impl && m_Impl->initialized && clipIt->second.loaded) {
        auto* maS = new ma_sound();
        ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION; // 2D sound — no listener attenuation
        ma_result result = ma_sound_init_from_file(
            &m_Impl->engine,
            clipIt->second.filepath.c_str(),
            flags,
            nullptr, nullptr,
            maS
        );

        if (result == MA_SUCCESS) {
            ma_sound_set_volume(maS, EffectiveVolume(clampedVolume, channel));
            ma_sound_set_pitch(maS, clampedPitch);
            ma_sound_set_looping(maS, loop ? MA_TRUE : MA_FALSE);
            ma_sound_start(maS);
            sound.maSound = maS;
        } else {
            ENJIN_LOG_ERROR(Audio, "Failed to play audio '%s' (error %d)",
                clipIt->second.filepath.c_str(), result);
            delete maS;
        }
    }

    m_Sounds[handle] = sound;

    ENJIN_LOG_DEBUG(Audio, "Playing sound (handle: %u, clip: %u, vol: %.2f)", handle, clip, volume);

    // Notify accessibility audio visual indicator system
    if (m_OnSoundPlayed) {
        const auto& filepath = clipIt->second.filepath;
        auto lastSlash = filepath.find_last_of("/\\");
        std::string label = (lastSlash != std::string::npos)
            ? filepath.substr(lastSlash + 1)
            : filepath;
        m_OnSoundPlayed(label);
    }

    return handle;
}

SoundHandle SimpleAudio::Play3D(AudioClipHandle clip, const Math::Vector3& position,
                                 f32 volume, f32 minDist, f32 maxDist,
                                 AudioChannel channel) {
    auto clipIt = m_Clips.find(clip);
    if (clipIt == m_Clips.end()) {
        return INVALID_SOUND;
    }

    // Cap active sound count
    static constexpr usize MAX_ACTIVE_SOUNDS = 256;
    if (m_Sounds.size() >= MAX_ACTIVE_SOUNDS) {
        ENJIN_LOG_WARN(Audio, "Max active sounds reached (%zu), cannot play 3D sound", MAX_ACTIVE_SOUNDS);
        return INVALID_SOUND;
    }

    SoundHandle handle = m_NextSoundHandle++;
    if (m_NextSoundHandle == 0) m_NextSoundHandle = 1;

    // Clamp parameters to valid ranges
    f32 clampedVolume = Math::Clamp(volume, 0.0f, 1.0f);
    f32 clampedMinDist = std::max(minDist, 0.01f);
    f32 clampedMaxDist = std::max(maxDist, clampedMinDist + 0.01f);

    SoundInstance sound;
    sound.clip = clip;
    sound.volume = clampedVolume;
    sound.is3D = true;
    sound.channel = channel;
    sound.position = position;
    sound.minDistance = clampedMinDist;
    sound.maxDistance = clampedMaxDist;
    sound.isPlaying = true;

    // Create miniaudio 3D spatialized sound
    if (m_Impl && m_Impl->initialized && clipIt->second.loaded) {
        auto* maS = new ma_sound();
        ma_result result = ma_sound_init_from_file(
            &m_Impl->engine,
            clipIt->second.filepath.c_str(),
            0, // No flags — spatialization enabled by default
            nullptr, nullptr,
            maS
        );

        if (result == MA_SUCCESS) {
            ma_sound_set_volume(maS, EffectiveVolume(clampedVolume, channel));
            ma_sound_set_position(maS, position.x, position.y, position.z);
            ma_sound_set_min_distance(maS, clampedMinDist);
            ma_sound_set_max_distance(maS, clampedMaxDist);
            ma_sound_set_attenuation_model(maS, ma_attenuation_model_inverse);
            ma_sound_start(maS);
            sound.maSound = maS;
        } else {
            ENJIN_LOG_ERROR(Audio, "Failed to play 3D audio '%s' (error %d)",
                clipIt->second.filepath.c_str(), result);
            delete maS;
        }
    }

    m_Sounds[handle] = sound;

    ENJIN_LOG_DEBUG(Audio, "Playing 3D sound at (%.1f, %.1f, %.1f)", position.x, position.y, position.z);

    if (m_OnSoundPlayed) {
        const auto& filepath = clipIt->second.filepath;
        auto lastSlash = filepath.find_last_of("/\\");
        std::string label = (lastSlash != std::string::npos)
            ? filepath.substr(lastSlash + 1)
            : filepath;
        m_OnSoundPlayed(label);
    }

    return handle;
}

void SimpleAudio::PlayOneShot(AudioClipHandle clip, f32 volume, AudioChannel channel) {
    Play(clip, volume, 1.0f, false, channel);
}

void SimpleAudio::PlayOneShot3D(AudioClipHandle clip, const Math::Vector3& position, f32 volume) {
    Play3D(clip, position, volume);
}

void SimpleAudio::Stop(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        CleanupSound(it->second);
        m_Sounds.erase(it);
    }
}

void SimpleAudio::StopAll() {
    for (auto& [handle, sound] : m_Sounds) {
        CleanupSound(sound);
    }
    m_Sounds.clear();
}

void SimpleAudio::Pause(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end() && it->second.maSound) {
        ma_sound_stop(static_cast<ma_sound*>(it->second.maSound));
        it->second.isPlaying = false;
    }
}

void SimpleAudio::Resume(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end() && it->second.maSound) {
        ma_sound_start(static_cast<ma_sound*>(it->second.maSound));
        it->second.isPlaying = true;
    }
}

void SimpleAudio::SetVolume(SoundHandle sound, f32 volume) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.volume = Math::Clamp(volume, 0.0f, 1.0f);
        if (it->second.maSound) {
            ma_sound_set_volume(static_cast<ma_sound*>(it->second.maSound),
                EffectiveVolume(it->second.volume, it->second.channel));
        }
    }
}

void SimpleAudio::SetPitch(SoundHandle sound, f32 pitch) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.pitch = Math::Clamp(pitch, 0.1f, 3.0f);
        if (it->second.maSound) {
            ma_sound_set_pitch(static_cast<ma_sound*>(it->second.maSound), it->second.pitch);
        }
    }
}

void SimpleAudio::SetPosition(SoundHandle sound, const Math::Vector3& position) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.position = position;
        if (it->second.maSound) {
            ma_sound_set_position(static_cast<ma_sound*>(it->second.maSound),
                position.x, position.y, position.z);
        }
    }
}

bool SimpleAudio::IsPlaying(SoundHandle sound) const {
    auto it = m_Sounds.find(sound);
    if (it == m_Sounds.end()) return false;

    // Check miniaudio state if available
    if (it->second.maSound) {
        return ma_sound_is_playing(static_cast<ma_sound*>(it->second.maSound)) != 0;
    }
    return it->second.isPlaying;
}

void SimpleAudio::SetMasterVolume(f32 volume) {
    m_MasterVolume = Math::Clamp(volume, 0.0f, 1.0f);
    // Update all active sounds with new effective volume
    for (auto& [handle, sound] : m_Sounds) {
        if (sound.maSound) {
            ma_sound_set_volume(static_cast<ma_sound*>(sound.maSound),
                EffectiveVolume(sound.volume, sound.channel));
        }
    }
}

void SimpleAudio::SetChannelVolume(AudioChannel channel, f32 volume) {
    auto idx = static_cast<usize>(channel);
    if (idx >= static_cast<usize>(AudioChannel::Count)) return;
    m_ChannelVolumes[idx] = Math::Clamp(volume, 0.0f, 1.0f);
    // Update all active sounds on this channel
    for (auto& [handle, sound] : m_Sounds) {
        if (sound.channel == channel && sound.maSound) {
            ma_sound_set_volume(static_cast<ma_sound*>(sound.maSound),
                EffectiveVolume(sound.volume, channel));
        }
    }
}

f32 SimpleAudio::GetChannelVolume(AudioChannel channel) const {
    auto idx = static_cast<usize>(channel);
    if (idx >= static_cast<usize>(AudioChannel::Count)) return 1.0f;
    return m_ChannelVolumes[idx];
}

void SimpleAudio::StopChannel(AudioChannel channel) {
    std::vector<SoundHandle> toRemove;
    for (auto& [handle, sound] : m_Sounds) {
        if (sound.channel == channel) {
            CleanupSound(sound);
            toRemove.push_back(handle);
        }
    }
    for (SoundHandle h : toRemove) {
        m_Sounds.erase(h);
    }
}

f32 SimpleAudio::EffectiveVolume(f32 instanceVolume, AudioChannel channel) const {
    auto idx = static_cast<usize>(channel);
    f32 channelVol = (idx < static_cast<usize>(AudioChannel::Count)) ? m_ChannelVolumes[idx] : 1.0f;
    return instanceVolume * channelVol * m_MasterVolume;
}

void SimpleAudio::Update(f32 deltaTime) {
    std::vector<SoundHandle> toRemove;

    for (auto& [handle, sound] : m_Sounds) {
        if (!sound.isPlaying && !sound.maSound) {
            toRemove.push_back(handle);
            continue;
        }

        // Check if miniaudio sound has finished
        if (sound.maSound) {
            auto* maS = static_cast<ma_sound*>(sound.maSound);
            if (ma_sound_at_end(maS)) {
                CleanupSound(sound);
                toRemove.push_back(handle);
            }
        }
    }

    for (SoundHandle h : toRemove) {
        m_Sounds.erase(h);
    }
}

void SimpleAudio::UpdateAudioSources(f32 deltaTime) {
    if (!m_World) return;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::AudioSourceComponent>()) {

        auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);

        Math::Vector3 position = transform ? transform->position : Math::Vector3(0, 0, 0);

        // Handle playOnAwake
        if (audio->playOnAwake && !audio->isPlaying && !audio->awakeTriggered) {
            if (!audio->clipPath.empty()) {
                AudioClipHandle clip = LoadClip(audio->clipPath);
                // Map ECS::AudioChannel to Audio::AudioChannel (same enum values)
                auto ch = static_cast<AudioChannel>(static_cast<u8>(audio->channel));
                // Music and UI channels force non-diegetic (2D) playback
                bool diegetic3D = audio->is3D &&
                    ch != AudioChannel::Music && ch != AudioChannel::UI;
                SoundHandle snd;
                if (diegetic3D) {
                    snd = Play3D(clip, position, audio->volume, audio->minDistance, audio->maxDistance, ch);
                } else {
                    snd = Play(clip, audio->volume, audio->pitch, audio->loop, ch);
                }
                audio->soundHandle = snd;
                audio->isPlaying = true;
            }
            audio->awakeTriggered = true;
        }

        // Update position for 3D sounds
        if (audio->is3D && audio->isPlaying && audio->soundHandle != INVALID_SOUND) {
            SetPosition(audio->soundHandle, position);
        }

        // Check if sound finished
        if (audio->isPlaying && audio->soundHandle != INVALID_SOUND) {
            if (!IsPlaying(audio->soundHandle)) {
                audio->isPlaying = false;
            }
        }
    }

    (void)deltaTime;
}

f32 SimpleAudio::Calculate3DVolume(const Math::Vector3& soundPos, f32 minDist, f32 maxDist) const {
    Math::Vector3 diff = soundPos - m_ListenerPosition;
    f32 distance = Math::Sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

    if (distance <= minDist) {
        return 1.0f;
    }
    if (distance >= maxDist) {
        return 0.0f;
    }

    return 1.0f - (distance - minDist) / (maxDist - minDist);
}

} // namespace Audio
} // namespace Enjin
