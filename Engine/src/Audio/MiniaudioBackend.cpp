#include "miniaudio.h"

#include "Enjin/Audio/MiniaudioBackend.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>

namespace Enjin {
namespace Audio {

// ============================================================================
// pImpl holding the ma_engine
// ============================================================================

struct MiniaudioBackend::Impl {
    ma_engine engine{};
    bool initialized = false;
};

// ============================================================================
// Constructor / Destructor
// ============================================================================

MiniaudioBackend::MiniaudioBackend()
    : m_Impl(std::make_unique<Impl>()) {
}

MiniaudioBackend::~MiniaudioBackend() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================

bool MiniaudioBackend::Initialize() {
    if (m_Impl->initialized) {
        ENJIN_LOG_WARN(Audio, "miniaudio backend already initialized");
        return true;
    }

    ma_result result = ma_engine_init(nullptr, &m_Impl->engine);
    if (result != MA_SUCCESS) {
        ENJIN_LOG_ERROR(Audio, "Failed to initialize miniaudio engine (error %d)", result);
        return false;
    }

    m_Impl->initialized = true;

    m_CategoryVolumes[SoundType::SoundEffect] = 1.0f;
    m_CategoryVolumes[SoundType::Music] = 1.0f;
    m_CategoryVolumes[SoundType::Ambient] = 1.0f;
    m_CategoryVolumes[SoundType::Voice] = 1.0f;

    ENJIN_LOG_INFO(Audio, "miniaudio backend initialized");
    return true;
}

void MiniaudioBackend::Shutdown() {
    if (!m_Impl || !m_Impl->initialized) return;

    // Stop and free all playing channels
    {
        std::lock_guard<std::mutex> lock(m_ChannelMutex);
        for (auto& [handle, ptr] : m_Channels) {
            auto* sound = static_cast<ma_sound*>(ptr);
            ma_sound_stop(sound);
            ma_sound_uninit(sound);
            delete sound;
        }
        m_Channels.clear();
    }
    m_Sounds.clear();

    ma_engine_uninit(&m_Impl->engine);
    m_Impl->initialized = false;

    ENJIN_LOG_INFO(Audio, "miniaudio backend shut down");
}

void MiniaudioBackend::Update() {
    if (!m_Impl->initialized) return;

    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    // Clean up channels that have finished playing
    for (auto it = m_Channels.begin(); it != m_Channels.end();) {
        auto* sound = static_cast<ma_sound*>(it->second);
        if (ma_sound_at_end(sound)) {
            ma_sound_uninit(sound);
            delete sound;
            it = m_Channels.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Sound loading
// ============================================================================

SoundHandle MiniaudioBackend::LoadSound(const std::string& path, const SoundSettings& settings) {
    if (!m_Impl->initialized) return INVALID_SOUND;

    SoundHandle handle = m_NextSound++;
    m_Sounds[handle] = { path, settings };

    ENJIN_LOG_INFO(Audio, "miniaudio: loaded sound '%s' (handle %u)", path.c_str(), handle);
    return handle;
}

void MiniaudioBackend::UnloadSound(SoundHandle handle) {
    m_Sounds.erase(handle);
}

// ============================================================================
// Playback
// ============================================================================

ChannelHandle MiniaudioBackend::Play(SoundHandle sound, const Math::Vector3& position) {
    if (!m_Impl->initialized) return INVALID_CHANNEL;

    auto it = m_Sounds.find(sound);
    if (it == m_Sounds.end()) {
        ENJIN_LOG_WARN(Audio, "miniaudio: cannot play invalid sound %u", sound);
        return INVALID_CHANNEL;
    }

    const auto& info = it->second;

    // Heap-allocate a ma_sound for this channel
    auto* maSound = new ma_sound();

    ma_uint32 flags = 0;
    if (info.settings.loadMode == LoadMode::Streaming) {
        flags |= MA_SOUND_FLAG_STREAM;
    }
    if (!info.settings.is3D) {
        flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    }

    ma_result result = ma_sound_init_from_file(
        &m_Impl->engine,
        info.path.c_str(),
        flags,
        nullptr, // group
        nullptr, // fence
        maSound
    );

    if (result != MA_SUCCESS) {
        ENJIN_LOG_ERROR(Audio, "miniaudio: failed to init sound from '%s' (error %d)", info.path.c_str(), result);
        delete maSound;
        return INVALID_CHANNEL;
    }

    // Apply settings
    ma_sound_set_volume(maSound, info.settings.volume * m_MasterVolume);
    ma_sound_set_pitch(maSound, info.settings.pitch);
    ma_sound_set_looping(maSound, info.settings.loop ? MA_TRUE : MA_FALSE);

    if (info.settings.is3D) {
        ma_sound_set_position(maSound, position.x, position.y, position.z);
        ma_sound_set_min_distance(maSound, info.settings.minDistance);
        ma_sound_set_max_distance(maSound, info.settings.maxDistance);

        switch (info.settings.attenuation) {
            case AttenuationMode::None:
                ma_sound_set_attenuation_model(maSound, ma_attenuation_model_none);
                break;
            case AttenuationMode::Linear:
                ma_sound_set_attenuation_model(maSound, ma_attenuation_model_linear);
                break;
            case AttenuationMode::InverseDistance:
            case AttenuationMode::InverseDistanceClamped:
                ma_sound_set_attenuation_model(maSound, ma_attenuation_model_inverse);
                break;
            case AttenuationMode::Logarithmic:
                ma_sound_set_attenuation_model(maSound, ma_attenuation_model_exponential);
                break;
        }
    }

    ma_sound_start(maSound);

    ChannelHandle channel = m_NextChannel++;
    {
        std::lock_guard<std::mutex> lock(m_ChannelMutex);
        m_Channels[channel] = maSound;
    }

    return channel;
}

void MiniaudioBackend::Stop(ChannelHandle channel) {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return;

    auto* sound = static_cast<ma_sound*>(it->second);
    ma_sound_stop(sound);
    ma_sound_uninit(sound);
    delete sound;
    m_Channels.erase(it);
}

void MiniaudioBackend::Pause(ChannelHandle channel) {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return;

    auto* sound = static_cast<ma_sound*>(it->second);
    ma_sound_stop(sound); // miniaudio: stop without uninit = pause
}

void MiniaudioBackend::Resume(ChannelHandle channel) {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return;

    auto* sound = static_cast<ma_sound*>(it->second);
    ma_sound_start(sound);
}

// ============================================================================
// Channel properties
// ============================================================================

void MiniaudioBackend::SetChannelVolume(ChannelHandle channel, f32 volume) {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return;

    auto* sound = static_cast<ma_sound*>(it->second);
    ma_sound_set_volume(sound, std::clamp(volume, 0.0f, 1.0f));
}

void MiniaudioBackend::SetChannelPitch(ChannelHandle channel, f32 pitch) {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return;

    auto* sound = static_cast<ma_sound*>(it->second);
    ma_sound_set_pitch(sound, std::clamp(pitch, 0.1f, 4.0f));
}

void MiniaudioBackend::SetChannelPosition(ChannelHandle channel, const Math::Vector3& position) {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return;

    auto* sound = static_cast<ma_sound*>(it->second);
    ma_sound_set_position(sound, position.x, position.y, position.z);
}

void MiniaudioBackend::SetChannelLoop(ChannelHandle channel, bool loop) {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return;

    auto* sound = static_cast<ma_sound*>(it->second);
    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
}

bool MiniaudioBackend::IsChannelPlaying(ChannelHandle channel) const {
    std::lock_guard<std::mutex> lock(m_ChannelMutex);
    auto it = m_Channels.find(channel);
    if (it == m_Channels.end()) return false;

    auto* sound = static_cast<ma_sound*>(it->second);
    return ma_sound_is_playing(sound) != 0;
}

// ============================================================================
// Volume
// ============================================================================

void MiniaudioBackend::SetMasterVolume(f32 volume) {
    m_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
    if (m_Impl->initialized) {
        ma_engine_set_volume(&m_Impl->engine, m_MasterVolume);
    }
}

void MiniaudioBackend::SetCategoryVolume(SoundType type, f32 volume) {
    m_CategoryVolumes[type] = std::clamp(volume, 0.0f, 1.0f);
}

// ============================================================================
// Listener
// ============================================================================

void MiniaudioBackend::SetListener(const AudioListener& listener) {
    if (!m_Impl->initialized) return;

    ma_engine_listener_set_position(&m_Impl->engine, 0,
        listener.position.x, listener.position.y, listener.position.z);
    ma_engine_listener_set_direction(&m_Impl->engine, 0,
        listener.forward.x, listener.forward.y, listener.forward.z);
    ma_engine_listener_set_world_up(&m_Impl->engine, 0,
        listener.up.x, listener.up.y, listener.up.z);
}

} // namespace Audio
} // namespace Enjin
