#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace Audio {

SimpleAudio::~SimpleAudio() {
    Shutdown();
}

bool SimpleAudio::Initialize() {
    if (m_Initialized) return true;

    // TODO: Initialize actual audio backend (miniaudio, OpenAL, etc.)
    ENJIN_LOG_INFO(Audio, "SimpleAudio initialized (stub implementation)");

    m_Initialized = true;
    return true;
}

void SimpleAudio::Shutdown() {
    if (!m_Initialized) return;

    StopAll();
    m_Clips.clear();
    m_Sounds.clear();

    // TODO: Shutdown actual audio backend

    m_Initialized = false;
    ENJIN_LOG_INFO(Audio, "SimpleAudio shutdown");
}

void SimpleAudio::SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up) {
    m_ListenerPosition = position;
    m_ListenerForward = forward;
    m_ListenerUp = up;
}

AudioClipHandle SimpleAudio::LoadClip(const std::string& filepath) {
    // Check if already loaded
    for (const auto& [handle, path] : m_Clips) {
        if (path == filepath) {
            return handle;
        }
    }

    // TODO: Actually load audio file data
    AudioClipHandle handle = m_NextClipHandle++;
    m_Clips[handle] = filepath;

    ENJIN_LOG_INFO(Audio, "Loaded audio clip: %s (handle: %u)", filepath.c_str(), handle);
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

        ENJIN_LOG_INFO(Audio, "Unloaded audio clip: %s", it->second.c_str());
        m_Clips.erase(it);
    }
}

SoundHandle SimpleAudio::Play(AudioClipHandle clip, f32 volume, f32 pitch, bool loop) {
    if (m_Clips.find(clip) == m_Clips.end()) {
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

    // TODO: Actually start playing sound
    ENJIN_LOG_DEBUG(Audio, "Playing sound (handle: %u, clip: %u, vol: %.2f)", handle, clip, volume);

    return handle;
}

SoundHandle SimpleAudio::Play3D(AudioClipHandle clip, const Math::Vector3& position,
                                 f32 volume, f32 minDist, f32 maxDist) {
    if (m_Clips.find(clip) == m_Clips.end()) {
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
}

void SimpleAudio::StopAll() {
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
    // Remove finished non-looping sounds
    std::vector<SoundHandle> toRemove;

    for (auto& [handle, sound] : m_Sounds) {
        if (!sound.isPlaying) continue;

        // Update playback position (simulated)
        sound.playbackPosition += deltaTime;

        // TODO: Check if sound has finished (need clip duration)
        // For now, remove after 10 seconds if not looping
        if (!sound.loop && sound.playbackPosition > 10.0f) {
            toRemove.push_back(handle);
        }

        // Update 3D volume based on distance
        if (sound.is3D) {
            f32 spatialVolume = Calculate3DVolume(sound.position, sound.minDistance, sound.maxDistance);
            // TODO: Apply spatialVolume to actual audio
            (void)spatialVolume;
        }
    }

    for (SoundHandle h : toRemove) {
        m_Sounds.erase(h);
    }
}

void SimpleAudio::UpdateAudioSources(f32 deltaTime) {
    (void)deltaTime;

    if (!m_World) return;

    for (ECS::Entity entity : m_World->GetAllEntities()) {
        if (!m_World->HasComponent<ECS::AudioSourceComponent>(entity)) continue;

        auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);

        Math::Vector3 position = transform ? transform->position : Math::Vector3(0, 0, 0);

        // Handle playOnAwake (first frame)
        // TODO: Track if already triggered

        // Update playing sounds with new position
        if (audio->is3D && audio->isPlaying) {
            // TODO: Update sound position
        }
    }
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
