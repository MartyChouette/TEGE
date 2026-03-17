#pragma once

#ifdef ENJIN_AUDIO_STEAM_AUDIO

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <phonon.h>

#include <unordered_map>
#include <vector>
#include <mutex>

namespace Enjin {
namespace Audio {

// Forward declaration — SoundHandle defined in SimpleAudio.h
#ifndef ENJIN_SOUND_HANDLE_DEFINED
#define ENJIN_SOUND_HANDLE_DEFINED
using SoundHandle = u32;
constexpr SoundHandle INVALID_SOUND = 0;
#endif

// Steam Audio HRTF binaural processor with occlusion & transmission.
// Processes mono 3D audio through IPLDirectEffect (occlusion/transmission)
// then IPLBinauralEffect (HRTF) for physics-based spatial audio.
// Thread safety: CreateSource/DestroySource are mutex-guarded.
// SetSourceDirection/UpdateOcclusion are main-thread only (benign race with audio thread).
// Process is called from the audio thread.
class ENJIN_API SteamAudioProcessor {
public:
    SteamAudioProcessor() = default;
    ~SteamAudioProcessor();

    bool Initialize(u32 sampleRate, u32 frameSize);
    void Shutdown();

    bool IsInitialized() const { return m_Initialized; }
    bool IsEnabled() const { return m_Enabled; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

    // Occlusion & transmission toggles
    void SetOcclusionEnabled(bool enabled) { m_OcclusionEnabled = enabled; }
    bool IsOcclusionEnabled() const { return m_OcclusionEnabled; }
    void SetTransmissionEnabled(bool enabled) { m_TransmissionEnabled = enabled; }
    bool IsTransmissionEnabled() const { return m_TransmissionEnabled; }

    // Scene geometry lifecycle (main thread, not concurrent with audio)
    bool BuildScene(const std::vector<Math::Vector3>& vertices,
                    const std::vector<i32>& indices,
                    const std::vector<IPLMaterial>& materials,
                    const std::vector<IPLint32>& materialIndices);
    void DestroyScene();
    bool HasScene() const { return m_Scene != nullptr; }

    // Per-source lifecycle (main thread, mutex-guarded)
    void CreateSource(SoundHandle handle);
    void DestroySource(SoundHandle handle);

    // Main thread: update direction (listener-relative unit vector)
    void SetSourceDirection(SoundHandle handle, const Math::Vector3& direction);

    // Main thread: update occlusion for a source (~10Hz)
    void UpdateOcclusion(SoundHandle handle,
                         const Math::Vector3& sourceWorldPos,
                         const Math::Vector3& listenerWorldPos);

    // Audio thread: process mono -> binaural stereo
    // Returns true if HRTF was applied, false = passthrough
    bool Process(SoundHandle handle, const f32* inMono, f32* outStereo, u32 frameCount);

    // Compute listener-relative direction from world positions
    static Math::Vector3 ComputeListenerRelativeDirection(
        const Math::Vector3& listenerPos, const Math::Vector3& listenerFwd,
        const Math::Vector3& listenerUp, const Math::Vector3& sourcePos);

    u32 GetFrameSize() const { return m_FrameSize; }

private:
    IPLContext m_Context = nullptr;
    IPLHRTF m_HRTF = nullptr;
    IPLScene m_Scene = nullptr;
    IPLStaticMesh m_StaticMesh = nullptr;
    IPLSimulator m_Simulator = nullptr;
    IPLAudioSettings m_AudioSettings{};
    bool m_Initialized = false;
    bool m_Enabled = true;
    bool m_OcclusionEnabled = true;
    bool m_TransmissionEnabled = true;
    u32 m_SampleRate = 0;
    u32 m_FrameSize = 0;

    struct BinauralSource {
        IPLBinauralEffect effect = nullptr;
        IPLDirectEffect directEffect = nullptr;
        IPLSource simulationSource = nullptr;  // For simulator-based occlusion queries
        f32* inData = nullptr;       // mono input buffer
        f32* outData = nullptr;      // interleaved stereo output buffer
        f32* directOutData = nullptr; // mono buffer for direct effect output
        Math::Vector3 direction;     // listener-relative
        f32 occlusion = 1.0f;        // 1.0 = fully audible, 0.0 = fully occluded
        f32 transmissionLow = 1.0f;
        f32 transmissionMid = 1.0f;
        f32 transmissionHigh = 1.0f;
        bool active = false;
    };

    std::unordered_map<SoundHandle, BinauralSource> m_Sources;
    std::mutex m_SourceMutex;  // guards create/destroy only
};

} // namespace Audio
} // namespace Enjin

#endif // ENJIN_AUDIO_STEAM_AUDIO
