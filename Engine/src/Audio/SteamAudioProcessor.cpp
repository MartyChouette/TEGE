#ifdef ENJIN_AUDIO_STEAM_AUDIO

#include "Enjin/Audio/SteamAudioProcessor.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Math.h"

#include <cstring>

namespace Enjin {
namespace Audio {

SteamAudioProcessor::~SteamAudioProcessor() {
    Shutdown();
}

bool SteamAudioProcessor::Initialize(u32 sampleRate, u32 frameSize) {
    if (m_Initialized) return true;

    // Create Steam Audio context
    IPLContextSettings contextSettings{};
    contextSettings.version = STEAMAUDIO_VERSION;

    IPLerror err = iplContextCreate(&contextSettings, &m_Context);
    if (err != IPL_STATUS_SUCCESS) {
        ENJIN_LOG_ERROR(Audio, "Steam Audio: failed to create context (error %d)", static_cast<int>(err));
        return false;
    }

    // Create HRTF with default settings
    IPLAudioSettings audioSettings{};
    audioSettings.samplingRate = static_cast<IPLint32>(sampleRate);
    audioSettings.frameSize = static_cast<IPLint32>(frameSize);

    IPLHRTFSettings hrtfSettings{};
    hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;

    err = iplHRTFCreate(m_Context, &audioSettings, &hrtfSettings, &m_HRTF);
    if (err != IPL_STATUS_SUCCESS) {
        ENJIN_LOG_ERROR(Audio, "Steam Audio: failed to create HRTF (error %d)", static_cast<int>(err));
        iplContextRelease(&m_Context);
        m_Context = nullptr;
        return false;
    }

    m_SampleRate = sampleRate;
    m_FrameSize = frameSize;
    m_Initialized = true;

    ENJIN_LOG_INFO(Audio, "Steam Audio HRTF initialized (sample rate: %u, frame size: %u)", sampleRate, frameSize);
    return true;
}

void SteamAudioProcessor::Shutdown() {
    if (!m_Initialized) return;

    // Destroy all sources
    {
        std::lock_guard<std::mutex> lock(m_SourceMutex);
        for (auto& [handle, src] : m_Sources) {
            if (src.effect) {
                iplBinauralEffectRelease(&src.effect);
            }
            delete[] src.inData;
            delete[] src.outData;
        }
        m_Sources.clear();
    }

    if (m_HRTF) {
        iplHRTFRelease(&m_HRTF);
        m_HRTF = nullptr;
    }

    if (m_Context) {
        iplContextRelease(&m_Context);
        m_Context = nullptr;
    }

    m_Initialized = false;
    ENJIN_LOG_INFO(Audio, "Steam Audio HRTF shutdown");
}

void SteamAudioProcessor::CreateSource(SoundHandle handle) {
    if (!m_Initialized) return;

    std::lock_guard<std::mutex> lock(m_SourceMutex);

    if (m_Sources.count(handle)) return;

    IPLAudioSettings audioSettings{};
    audioSettings.samplingRate = static_cast<IPLint32>(m_SampleRate);
    audioSettings.frameSize = static_cast<IPLint32>(m_FrameSize);

    IPLBinauralEffectSettings effectSettings{};
    effectSettings.hrtf = m_HRTF;

    BinauralSource src{};

    IPLerror err = iplBinauralEffectCreate(m_Context, &audioSettings, &effectSettings, &src.effect);
    if (err != IPL_STATUS_SUCCESS) {
        ENJIN_LOG_WARN(Audio, "Steam Audio: failed to create binaural effect for sound %u (error %d)",
                       handle, static_cast<int>(err));
        return;
    }

    // Allocate buffers
    src.inData = new f32[m_FrameSize]();      // mono
    src.outData = new f32[m_FrameSize * 2]();  // interleaved stereo
    src.direction = Math::Vector3(0, 0, 1);    // default: ahead
    src.active = true;

    m_Sources[handle] = src;
}

void SteamAudioProcessor::DestroySource(SoundHandle handle) {
    std::lock_guard<std::mutex> lock(m_SourceMutex);

    auto it = m_Sources.find(handle);
    if (it == m_Sources.end()) return;

    auto& src = it->second;
    if (src.effect) {
        iplBinauralEffectRelease(&src.effect);
    }
    delete[] src.inData;
    delete[] src.outData;

    m_Sources.erase(it);
}

void SteamAudioProcessor::SetSourceDirection(SoundHandle handle, const Math::Vector3& direction) {
    // No lock — benign race. 1-frame stale direction is inaudible.
    auto it = m_Sources.find(handle);
    if (it != m_Sources.end() && it->second.active) {
        it->second.direction = direction;
    }
}

bool SteamAudioProcessor::Process(SoundHandle handle, const f32* inMono, f32* outStereo, u32 frameCount) {
    if (!m_Initialized || !m_Enabled) return false;

    // No lock — source lifetime is managed so it's alive during audio callback
    auto it = m_Sources.find(handle);
    if (it == m_Sources.end() || !it->second.active) return false;

    auto& src = it->second;

    // Copy input mono samples to IPL buffer
    u32 count = (frameCount < m_FrameSize) ? frameCount : m_FrameSize;
    std::memcpy(src.inData, inMono, count * sizeof(f32));
    if (count < m_FrameSize) {
        std::memset(src.inData + count, 0, (m_FrameSize - count) * sizeof(f32));
    }

    // Build IPL audio buffers (deinterleaved format)
    IPLAudioBuffer inBuffer{};
    inBuffer.numChannels = 1;
    inBuffer.numSamples = static_cast<IPLint32>(m_FrameSize);
    f32* inChannels[1] = { src.inData };
    inBuffer.data = inChannels;

    // Output: 2 channels, need deinterleaved temp buffers
    f32* outLeft = src.outData;                    // reuse first half
    f32* outRight = src.outData + m_FrameSize;     // second half (we have 2*frameSize allocated)

    IPLAudioBuffer outBuffer{};
    outBuffer.numChannels = 2;
    outBuffer.numSamples = static_cast<IPLint32>(m_FrameSize);
    f32* outChannels[2] = { outLeft, outRight };
    outBuffer.data = outChannels;

    // Convert direction to Steam Audio coordinate system (negate Z for LH -> RH)
    IPLVector3 dir;
    dir.x = src.direction.x;
    dir.y = src.direction.y;
    dir.z = -src.direction.z;  // LH to RH

    IPLBinauralEffectParams params{};
    params.direction = dir;
    params.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
    params.spatialBlend = 1.0f;
    params.hrtf = m_HRTF;
    params.peakDelays = nullptr;

    IPLAudioEffectState state = iplBinauralEffectApply(src.effect, &params, &inBuffer, &outBuffer);
    (void)state;

    // Interleave output: L0 R0 L1 R1 ...
    for (u32 i = 0; i < count; ++i) {
        outStereo[i * 2]     = outLeft[i];
        outStereo[i * 2 + 1] = outRight[i];
    }

    return true;
}

Math::Vector3 SteamAudioProcessor::ComputeListenerRelativeDirection(
    const Math::Vector3& listenerPos, const Math::Vector3& listenerFwd,
    const Math::Vector3& listenerUp, const Math::Vector3& sourcePos)
{
    Math::Vector3 toSource = sourcePos - listenerPos;
    f32 dist = Math::Sqrt(toSource.x * toSource.x + toSource.y * toSource.y + toSource.z * toSource.z);
    if (dist < 0.0001f) {
        return Math::Vector3(0, 0, 1);  // directly on top — default to forward
    }
    toSource = toSource * (1.0f / dist);

    // Build listener local axes
    Math::Vector3 fwd = listenerFwd;
    f32 fwdLen = Math::Sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (fwdLen < 0.0001f) fwd = Math::Vector3(0, 0, 1);
    else fwd = fwd * (1.0f / fwdLen);

    Math::Vector3 up = listenerUp;
    f32 upLen = Math::Sqrt(up.x * up.x + up.y * up.y + up.z * up.z);
    if (upLen < 0.0001f) up = Math::Vector3(0, 1, 0);
    else up = up * (1.0f / upLen);

    // Right = forward x up (left-handed cross product)
    Math::Vector3 right;
    right.x = fwd.y * up.z - fwd.z * up.y;
    right.y = fwd.z * up.x - fwd.x * up.z;
    right.z = fwd.x * up.y - fwd.y * up.x;

    // Project onto local axes
    Math::Vector3 localDir;
    localDir.x = toSource.x * right.x + toSource.y * right.y + toSource.z * right.z;
    localDir.y = toSource.x * up.x + toSource.y * up.y + toSource.z * up.z;
    localDir.z = toSource.x * fwd.x + toSource.y * fwd.y + toSource.z * fwd.z;

    return localDir;
}

} // namespace Audio
} // namespace Enjin

#endif // ENJIN_AUDIO_STEAM_AUDIO
