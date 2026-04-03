#include "Enjin/Audio/AudioBus.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>

namespace Enjin::Audio {

// ============================================================================
// AudioMixer
// ============================================================================

AudioMixer::AudioMixer() {
    m_MasterBus.name = "Master";
    m_MasterBus.volume = 1.0f;

    // Create default buses
    CreateBus("SFX", "Master");
    CreateBus("Music", "Master");
    CreateBus("UI", "Master");
    CreateBus("Voice", "Master");

    ENJIN_LOG_INFO(Audio, "AudioMixer initialized (Master + 4 default buses)");
}

void AudioMixer::Update(f32 deltaTime) {
    // Apply snapshot targets
    if (!m_ActiveSnapshots.empty()) {
        // Find highest-priority active snapshot per bus
        std::unordered_map<std::string, f32> targets;
        for (const auto& snap : m_ActiveSnapshots) {
            for (const auto& [busName, vol] : snap.busVolumes) {
                auto it = targets.find(busName);
                if (it == targets.end()) {
                    targets[busName] = vol;
                }
                // Higher priority snapshot wins (snapshots sorted by priority)
            }
        }

        // Set target volumes
        for (const auto& [busName, targetVol] : targets) {
            AudioBus* bus = GetBus(busName);
            if (bus) bus->targetVolume = targetVol;
        }
    }

    // Interpolate volumes toward targets
    for (AudioBus* bus : m_AllBuses) {
        if (bus->volume != bus->targetVolume) {
            f32 diff = bus->targetVolume - bus->volume;
            f32 step = bus->fadeSpeed * deltaTime;
            if (std::fabs(diff) <= step) {
                bus->volume = bus->targetVolume;
            } else {
                bus->volume += (diff > 0.0f ? step : -step);
            }
        }

        // Decay VU peak
        bus->vuPeak *= 0.95f;
        if (bus->vuLevel > bus->vuPeak) bus->vuPeak = bus->vuLevel;
    }
}

AudioBus* AudioMixer::GetBus(const std::string& name) {
    if (name == "Master") return &m_MasterBus;
    auto it = m_Buses.find(name);
    return (it != m_Buses.end()) ? &it->second : nullptr;
}

AudioBus* AudioMixer::CreateBus(const std::string& name, const std::string& parentName) {
    if (m_Buses.count(name)) return &m_Buses[name];

    AudioBus& bus = m_Buses[name];
    bus.name = name;
    bus.parent = GetBus(parentName);
    if (bus.parent) {
        bus.parent->children.push_back(&bus);
    }

    RebuildBusList();
    return &bus;
}

f32 AudioMixer::GetEffectiveVolume(const std::string& busName) const {
    if (busName == "Master") return m_MasterBus.GetEffectiveVolume();
    auto it = m_Buses.find(busName);
    return (it != m_Buses.end()) ? it->second.GetEffectiveVolume() : 1.0f;
}

void AudioMixer::PushSnapshot(const AudioSnapshot& snapshot) {
    // Remove existing snapshot with same name
    PopSnapshot(snapshot.name);
    m_ActiveSnapshots.push_back(snapshot);
    // Sort by priority (highest first)
    std::sort(m_ActiveSnapshots.begin(), m_ActiveSnapshots.end(),
        [](const AudioSnapshot& a, const AudioSnapshot& b) { return a.priority > b.priority; });
    ENJIN_LOG_INFO(Audio, "Audio snapshot pushed: %s (priority %d)", snapshot.name.c_str(), snapshot.priority);
}

void AudioMixer::PopSnapshot(const std::string& name) {
    m_ActiveSnapshots.erase(
        std::remove_if(m_ActiveSnapshots.begin(), m_ActiveSnapshots.end(),
            [&](const AudioSnapshot& s) { return s.name == name; }),
        m_ActiveSnapshots.end());

    // Reset targets to 1.0 for buses no longer in any snapshot
    for (AudioBus* bus : m_AllBuses) {
        bool inAnySnapshot = false;
        for (const auto& snap : m_ActiveSnapshots) {
            if (snap.busVolumes.count(bus->name)) { inAnySnapshot = true; break; }
        }
        if (!inAnySnapshot) bus->targetVolume = 1.0f;
    }
}

bool AudioMixer::IsSnapshotActive(const std::string& name) const {
    for (const auto& s : m_ActiveSnapshots) {
        if (s.name == name) return true;
    }
    return false;
}

void AudioMixer::UpdateVU(const std::string& busName, f32 rmsLevel, u32 soundCount) {
    AudioBus* bus = GetBus(busName);
    if (bus) {
        bus->vuLevel = rmsLevel;
        bus->activeSoundCount = soundCount;
        if (rmsLevel > bus->vuPeak) bus->vuPeak = rmsLevel;
    }
}

void AudioMixer::RebuildBusList() {
    m_AllBuses.clear();
    m_AllBuses.push_back(&m_MasterBus);
    for (auto& [name, bus] : m_Buses) {
        m_AllBuses.push_back(&bus);
    }
}

// ============================================================================
// MusicCrossfader
// ============================================================================

void MusicCrossfader::Play(const std::string& clipPath, f32 fadeInTime, f32 fadeOutTime,
                            TransitionMode mode) {
    m_TrackB = clipPath;
    m_Mode = mode;
    m_FadeDuration = (fadeInTime + fadeOutTime) * 0.5f;
    if (m_FadeDuration < 0.01f) m_FadeDuration = 0.01f;
    m_FadeTimer = 0.0f;

    if (m_State == State::Playing || m_State == State::FadingIn) {
        m_State = State::Crossfading;
    } else {
        m_TrackA = clipPath;
        m_TrackB.clear();
        m_VolumeA = 0.0f;
        m_State = State::FadingIn;
    }
}

void MusicCrossfader::Stop(f32 fadeOutTime) {
    m_FadeDuration = fadeOutTime;
    if (m_FadeDuration < 0.01f) m_FadeDuration = 0.01f;
    m_FadeTimer = 0.0f;
    m_State = State::FadingOut;
}

void MusicCrossfader::Update(f32 deltaTime) {
    if (m_State == State::Idle) return;

    m_FadeTimer += deltaTime;
    f32 t = Math::Clamp(m_FadeTimer / m_FadeDuration, 0.0f, 1.0f);

    switch (m_State) {
        case State::FadingIn:
            m_VolumeA = (m_Mode == TransitionMode::EqualPower) ? std::sqrt(t) : t;
            if (t >= 1.0f) m_State = State::Playing;
            break;

        case State::Playing:
            m_VolumeA = 1.0f;
            break;

        case State::Crossfading:
            if (m_Mode == TransitionMode::EqualPower) {
                m_VolumeA = std::sqrt(1.0f - t);
                m_VolumeB = std::sqrt(t);
            } else if (m_Mode == TransitionMode::FadeOutThenIn) {
                if (t < 0.5f) { m_VolumeA = 1.0f - t * 2.0f; m_VolumeB = 0.0f; }
                else { m_VolumeA = 0.0f; m_VolumeB = (t - 0.5f) * 2.0f; }
            } else {
                m_VolumeA = 1.0f - t;
                m_VolumeB = t;
            }
            if (t >= 1.0f) {
                m_TrackA = m_TrackB;
                m_TrackB.clear();
                m_VolumeA = 1.0f;
                m_VolumeB = 0.0f;
                m_HandleA = m_HandleB;
                m_HandleB = 0;
                m_State = State::Playing;
            }
            break;

        case State::FadingOut:
            m_VolumeA = 1.0f - t;
            if (t >= 1.0f) {
                m_TrackA.clear();
                m_VolumeA = 0.0f;
                m_HandleA = 0;
                m_State = State::Idle;
            }
            break;

        default: break;
    }
}

} // namespace Enjin::Audio
