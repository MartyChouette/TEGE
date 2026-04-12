#pragma once

#include "Enjin/Audio/AudioSystem.h"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Enjin {
namespace Audio {

class ENJIN_API MiniaudioBackend : public IAudioBackend {
public:
    MiniaudioBackend();
    ~MiniaudioBackend();

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

    const char* GetBackendName() const override { return "miniaudio"; }

private:
    // pImpl to keep miniaudio.h (90k lines) out of this header
    struct Impl;
    std::unique_ptr<Impl> m_Impl;

    struct LoadedSound {
        std::string path;
        SoundSettings settings;
    };

    std::unordered_map<SoundHandle, LoadedSound> m_Sounds;
    std::unordered_map<ChannelHandle, void*> m_Channels; // ma_sound* stored as void*
    mutable std::mutex m_ChannelMutex;
    SoundHandle m_NextSound = 1;
    ChannelHandle m_NextChannel = 1;
    f32 m_MasterVolume = 1.0f;
    std::unordered_map<SoundType, f32> m_CategoryVolumes;
};

} // namespace Audio
} // namespace Enjin
