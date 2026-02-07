#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace Enjin {
namespace Audio {

SimpleAudio::~SimpleAudio() {
    Shutdown();
}

bool SimpleAudio::Initialize() {
    if (m_Initialized) return true;

    ENJIN_LOG_INFO(Audio, "SimpleAudio initialized");
    m_Initialized = true;
    return true;
}

void SimpleAudio::Shutdown() {
    if (!m_Initialized) return;

    StopAll();
    m_Clips.clear();
    m_Sounds.clear();

    m_Initialized = false;
    ENJIN_LOG_INFO(Audio, "SimpleAudio shutdown");
}

void SimpleAudio::SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up) {
    m_ListenerPosition = position;
    m_ListenerForward = forward;
    m_ListenerUp = up;
}

bool SimpleAudio::LoadWAV(const std::string& filepath, AudioClipData& clip) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Audio, "Failed to open WAV file: %s", filepath.c_str());
        return false;
    }

    // Read RIFF header
    char riffHeader[4];
    file.read(riffHeader, 4);
    if (std::memcmp(riffHeader, "RIFF", 4) != 0) {
        ENJIN_LOG_ERROR(Audio, "Not a valid WAV file (no RIFF header): %s", filepath.c_str());
        return false;
    }

    u32 fileSize;
    file.read(reinterpret_cast<char*>(&fileSize), 4);

    char waveHeader[4];
    file.read(waveHeader, 4);
    if (std::memcmp(waveHeader, "WAVE", 4) != 0) {
        ENJIN_LOG_ERROR(Audio, "Not a valid WAV file (no WAVE header): %s", filepath.c_str());
        return false;
    }

    // Find fmt and data chunks
    bool foundFmt = false;
    bool foundData = false;

    while (!file.eof() && !(foundFmt && foundData)) {
        char chunkId[4];
        u32 chunkSize;
        file.read(chunkId, 4);
        file.read(reinterpret_cast<char*>(&chunkSize), 4);

        if (!file.good()) break;

        // Cap chunk size to 100MB to prevent OOM on malformed files
        constexpr u32 MAX_CHUNK_SIZE = 100 * 1024 * 1024;
        if (chunkSize > MAX_CHUNK_SIZE) {
            ENJIN_LOG_ERROR(Audio, "WAV chunk size too large (%u bytes): %s", chunkSize, filepath.c_str());
            return false;
        }

        if (std::memcmp(chunkId, "fmt ", 4) == 0) {
            u16 audioFormat;
            file.read(reinterpret_cast<char*>(&audioFormat), 2);
            file.read(reinterpret_cast<char*>(&clip.channels), 2);
            file.read(reinterpret_cast<char*>(&clip.sampleRate), 4);

            u32 byteRate;
            u16 blockAlign;
            file.read(reinterpret_cast<char*>(&byteRate), 4);
            file.read(reinterpret_cast<char*>(&blockAlign), 2);
            file.read(reinterpret_cast<char*>(&clip.bitsPerSample), 2);

            // Skip any extra fmt data
            if (chunkSize > 16) {
                file.seekg(chunkSize - 16, std::ios::cur);
            }

            if (audioFormat != 1) {
                ENJIN_LOG_WARN(Audio, "WAV file is not PCM format (format=%u): %s", audioFormat, filepath.c_str());
            }

            foundFmt = true;
        } else if (std::memcmp(chunkId, "data", 4) == 0) {
            clip.pcmData.resize(chunkSize);
            file.read(reinterpret_cast<char*>(clip.pcmData.data()), chunkSize);
            foundData = true;
        } else {
            // Skip unknown chunk
            file.seekg(chunkSize, std::ios::cur);
        }
    }

    if (!foundFmt || !foundData) {
        ENJIN_LOG_ERROR(Audio, "Invalid WAV file (missing fmt or data chunk): %s", filepath.c_str());
        return false;
    }

    // Calculate duration
    u32 bytesPerSample = clip.bitsPerSample / 8;
    u32 bytesPerFrame = bytesPerSample * clip.channels;
    if (bytesPerFrame > 0 && clip.sampleRate > 0) {
        u32 totalFrames = static_cast<u32>(clip.pcmData.size()) / bytesPerFrame;
        clip.duration = static_cast<f32>(totalFrames) / static_cast<f32>(clip.sampleRate);
    }

    clip.loaded = true;
    clip.filepath = filepath;

    ENJIN_LOG_INFO(Audio, "Loaded WAV: %s (%.1fs, %uHz, %uch, %ubit)",
        filepath.c_str(), clip.duration, clip.sampleRate, clip.channels, clip.bitsPerSample);

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
    AudioClipData clipData;
    clipData.filepath = filepath;

    // Try to load WAV data
    if (filepath.size() > 4) {
        std::string ext = filepath.substr(filepath.size() - 4);
        if (ext == ".wav" || ext == ".WAV") {
            LoadWAV(filepath, clipData);
        } else {
            ENJIN_LOG_WARN(Audio, "Unsupported audio format (only WAV supported): %s", filepath.c_str());
        }
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
            if (sound.clip == clip) {
                sound.isPlaying = false;
            }
        }

        ENJIN_LOG_INFO(Audio, "Unloaded audio clip: %s", it->second.filepath.c_str());
        m_Clips.erase(it);
    }
}

SoundHandle SimpleAudio::Play(AudioClipHandle clip, f32 volume, f32 pitch, bool loop) {
    auto clipIt = m_Clips.find(clip);
    if (clipIt == m_Clips.end()) {
        ENJIN_LOG_WARN(Audio, "Tried to play invalid clip: %u", clip);
        return INVALID_SOUND;
    }

    SoundHandle handle = m_NextSoundHandle++;

    SoundInstance sound;
    sound.clip = clip;
    sound.volume = volume;
    sound.pitch = pitch;
    sound.loop = loop;
    sound.is3D = false;
    sound.isPlaying = true;

    m_Sounds[handle] = sound;

#ifdef _WIN32
    // Use Windows PlaySound API for simple WAV playback
    const auto& clipData = clipIt->second;
    if (clipData.loaded && !clipData.pcmData.empty()) {
        // PlaySound from memory requires the full WAV file, not just PCM data
        // Use PlaySound from file for simplicity
        DWORD flags = SND_FILENAME | SND_ASYNC | SND_NODEFAULT;
        if (loop) flags |= SND_LOOP;
        PlaySoundA(clipData.filepath.c_str(), nullptr, flags);
    }
#endif

    ENJIN_LOG_DEBUG(Audio, "Playing sound (handle: %u, clip: %u, vol: %.2f)", handle, clip, volume);
    return handle;
}

SoundHandle SimpleAudio::Play3D(AudioClipHandle clip, const Math::Vector3& position,
                                 f32 volume, f32 minDist, f32 maxDist) {
    auto clipIt = m_Clips.find(clip);
    if (clipIt == m_Clips.end()) {
        return INVALID_SOUND;
    }

    SoundHandle handle = m_NextSoundHandle++;

    SoundInstance sound;
    sound.clip = clip;
    sound.volume = volume;
    sound.is3D = true;
    sound.position = position;
    sound.minDistance = minDist;
    sound.maxDistance = maxDist;
    sound.isPlaying = true;

    m_Sounds[handle] = sound;

    // Calculate spatial volume
    f32 spatialVolume = Calculate3DVolume(position, minDist, maxDist) * volume * m_MasterVolume;

#ifdef _WIN32
    const auto& clipData = clipIt->second;
    if (clipData.loaded && spatialVolume > 0.01f) {
        PlaySoundA(clipData.filepath.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
#else
    (void)spatialVolume;
#endif

    ENJIN_LOG_DEBUG(Audio, "Playing 3D sound at (%.1f, %.1f, %.1f)", position.x, position.y, position.z);
    return handle;
}

void SimpleAudio::PlayOneShot(AudioClipHandle clip, f32 volume) {
    Play(clip, volume, 1.0f, false);
}

void SimpleAudio::PlayOneShot3D(AudioClipHandle clip, const Math::Vector3& position, f32 volume) {
    Play3D(clip, position, volume);
}

void SimpleAudio::Stop(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.isPlaying = false;
        m_Sounds.erase(it);
    }

#ifdef _WIN32
    // Stop currently playing sound
    PlaySoundA(nullptr, nullptr, 0);
#endif
}

void SimpleAudio::StopAll() {
#ifdef _WIN32
    PlaySoundA(nullptr, nullptr, 0);
#endif
    m_Sounds.clear();
}

void SimpleAudio::Pause(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.isPlaying = false;
    }
}

void SimpleAudio::Resume(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.isPlaying = true;
    }
}

void SimpleAudio::SetVolume(SoundHandle sound, f32 volume) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.volume = Math::Clamp(volume, 0.0f, 1.0f);
    }
}

void SimpleAudio::SetPitch(SoundHandle sound, f32 pitch) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.pitch = Math::Clamp(pitch, 0.1f, 3.0f);
    }
}

void SimpleAudio::SetPosition(SoundHandle sound, const Math::Vector3& position) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.position = position;
    }
}

bool SimpleAudio::IsPlaying(SoundHandle sound) const {
    auto it = m_Sounds.find(sound);
    return it != m_Sounds.end() && it->second.isPlaying;
}

void SimpleAudio::Update(f32 deltaTime) {
    std::vector<SoundHandle> toRemove;

    for (auto& [handle, sound] : m_Sounds) {
        if (!sound.isPlaying) continue;

        sound.playbackPosition += deltaTime * sound.pitch;

        // Check if sound has finished using actual clip duration
        auto clipIt = m_Clips.find(sound.clip);
        f32 clipDuration = 10.0f; // Default fallback
        if (clipIt != m_Clips.end() && clipIt->second.loaded) {
            clipDuration = clipIt->second.duration;
        }

        if (!sound.loop && sound.playbackPosition >= clipDuration) {
            toRemove.push_back(handle);
        } else if (sound.loop && sound.playbackPosition >= clipDuration) {
            sound.playbackPosition -= clipDuration;
        }

        // Update 3D volume attenuation
        if (sound.is3D) {
            f32 spatialVolume = Calculate3DVolume(sound.position, sound.minDistance, sound.maxDistance);
            // Volume attenuation is tracked for query purposes
            // Real-time volume changes would need a more advanced backend (XAudio2, etc.)
            (void)spatialVolume;
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
                SoundHandle snd;
                if (audio->is3D) {
                    snd = Play3D(clip, position, audio->volume, audio->minDistance, audio->maxDistance);
                } else {
                    snd = Play(clip, audio->volume, audio->pitch, audio->loop);
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

    // Linear falloff
    return 1.0f - (distance - minDist) / (maxDist - minDist);
}

} // namespace Audio
} // namespace Enjin
