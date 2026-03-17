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

    m_AudioSettings = audioSettings;
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
            if (src.simulationSource) {
                if (m_Simulator) {
                    iplSourceRemove(src.simulationSource, m_Simulator);
                }
                iplSourceRelease(&src.simulationSource);
            }
            if (src.directEffect) {
                iplDirectEffectRelease(&src.directEffect);
            }
            if (src.effect) {
                iplBinauralEffectRelease(&src.effect);
            }
            delete[] src.inData;
            delete[] src.outData;
            delete[] src.directOutData;
        }
        m_Sources.clear();
    }

    // Destroy scene geometry before context
    DestroyScene();

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

bool SteamAudioProcessor::BuildScene(const std::vector<Math::Vector3>& vertices,
                                      const std::vector<i32>& indices,
                                      const std::vector<IPLMaterial>& materials,
                                      const std::vector<IPLint32>& materialIndices) {
    if (!m_Initialized || !m_Context) return false;
    if (vertices.empty() || indices.empty()) return false;
    if (indices.size() % 3 != 0) return false;

    usize numTris = indices.size() / 3;
    if (materialIndices.size() != numTris) {
        ENJIN_LOG_ERROR(Audio, "Steam Audio: materialIndices size (%zu) != triangle count (%zu)",
                        materialIndices.size(), numTris);
        return false;
    }

    // Destroy existing scene if rebuilding
    DestroyScene();

    // Create scene
    IPLSceneSettings sceneSettings{};
    sceneSettings.type = IPL_SCENETYPE_DEFAULT;

    IPLerror err = iplSceneCreate(m_Context, &sceneSettings, &m_Scene);
    if (err != IPL_STATUS_SUCCESS) {
        ENJIN_LOG_ERROR(Audio, "Steam Audio: failed to create scene (error %d)", static_cast<int>(err));
        return false;
    }

    // Convert vertices to IPL format (negate Z for LH -> RH)
    std::vector<IPLVector3> iplVertices(vertices.size());
    for (usize i = 0; i < vertices.size(); ++i) {
        iplVertices[i].x = vertices[i].x;
        iplVertices[i].y = vertices[i].y;
        iplVertices[i].z = -vertices[i].z;  // LH to RH
    }

    // Convert index triples to IPLTriangle
    usize numTriangles = indices.size() / 3;
    std::vector<IPLTriangle> iplTriangles(numTriangles);
    for (usize i = 0; i < numTriangles; ++i) {
        iplTriangles[i].indices[0] = indices[i * 3 + 0];
        iplTriangles[i].indices[1] = indices[i * 3 + 1];
        iplTriangles[i].indices[2] = indices[i * 3 + 2];
    }

    // Create static mesh
    IPLStaticMeshSettings meshSettings{};
    meshSettings.numVertices = static_cast<IPLint32>(iplVertices.size());
    meshSettings.numTriangles = static_cast<IPLint32>(numTriangles);
    meshSettings.numMaterials = static_cast<IPLint32>(materials.size());
    meshSettings.vertices = iplVertices.data();
    meshSettings.triangles = iplTriangles.data();
    meshSettings.materialIndices = materialIndices.data();
    meshSettings.materials = materials.data();

    err = iplStaticMeshCreate(m_Scene, &meshSettings, &m_StaticMesh);
    if (err != IPL_STATUS_SUCCESS) {
        ENJIN_LOG_ERROR(Audio, "Steam Audio: failed to create static mesh (error %d)", static_cast<int>(err));
        iplSceneRelease(&m_Scene);
        m_Scene = nullptr;
        return false;
    }

    iplStaticMeshAdd(m_StaticMesh, m_Scene);
    iplSceneCommit(m_Scene);

    // Create simulator for direct sound path computation (occlusion/transmission)
    IPLSimulationSettings simSettings{};
    simSettings.flags = IPL_SIMULATIONFLAGS_DIRECT;
    simSettings.sceneType = IPL_SCENETYPE_DEFAULT;

    err = iplSimulatorCreate(m_Context, &simSettings, &m_Simulator);
    if (err != IPL_STATUS_SUCCESS) {
        ENJIN_LOG_WARN(Audio, "Steam Audio: failed to create simulator (error %d), occlusion unavailable",
                       static_cast<int>(err));
        // Non-fatal — HRTF still works, just no occlusion
    } else {
        iplSimulatorSetScene(m_Simulator, m_Scene);
        iplSimulatorCommit(m_Simulator);
    }

    ENJIN_LOG_INFO(Audio, "Steam Audio: scene built (%zu vertices, %zu triangles)",
                   vertices.size(), numTriangles);
    return true;
}

void SteamAudioProcessor::DestroyScene() {
    if (m_Simulator) {
        iplSimulatorRelease(&m_Simulator);
        m_Simulator = nullptr;
    }
    if (m_StaticMesh) {
        iplStaticMeshRelease(&m_StaticMesh);
        m_StaticMesh = nullptr;
    }
    if (m_Scene) {
        iplSceneRelease(&m_Scene);
        m_Scene = nullptr;
    }
}

void SteamAudioProcessor::UpdateOcclusion(SoundHandle handle,
                                           const Math::Vector3& sourceWorldPos,
                                           const Math::Vector3& listenerWorldPos) {
    if (!m_Scene || !m_Simulator || !m_OcclusionEnabled) return;

    auto it = m_Sources.find(handle);
    if (it == m_Sources.end() || !it->second.active || !it->second.simulationSource) return;

    auto& src = it->second;

    // Set listener position on the simulator (negate Z for LH -> RH)
    IPLSimulationSharedInputs sharedInputs{};
    sharedInputs.listener.origin.x = listenerWorldPos.x;
    sharedInputs.listener.origin.y = listenerWorldPos.y;
    sharedInputs.listener.origin.z = -listenerWorldPos.z;
    sharedInputs.listener.ahead.x = 0.0f;
    sharedInputs.listener.ahead.y = 0.0f;
    sharedInputs.listener.ahead.z = -1.0f;  // default forward in RH
    sharedInputs.listener.up.x = 0.0f;
    sharedInputs.listener.up.y = 1.0f;
    sharedInputs.listener.up.z = 0.0f;
    sharedInputs.listener.right.x = 1.0f;
    sharedInputs.listener.right.y = 0.0f;
    sharedInputs.listener.right.z = 0.0f;

    iplSimulatorSetSharedInputs(m_Simulator, IPL_SIMULATIONFLAGS_DIRECT, &sharedInputs);

    // Set source position (negate Z for LH -> RH)
    IPLSimulationInputs sourceInputs{};
    sourceInputs.flags = IPL_SIMULATIONFLAGS_DIRECT;
    sourceInputs.directFlags = static_cast<IPLDirectSimulationFlags>(
        IPL_DIRECTSIMULATIONFLAGS_OCCLUSION |
        (m_TransmissionEnabled ? IPL_DIRECTSIMULATIONFLAGS_TRANSMISSION : 0));
    sourceInputs.source.origin.x = sourceWorldPos.x;
    sourceInputs.source.origin.y = sourceWorldPos.y;
    sourceInputs.source.origin.z = -sourceWorldPos.z;
    sourceInputs.source.ahead.x = 0.0f;
    sourceInputs.source.ahead.y = 0.0f;
    sourceInputs.source.ahead.z = -1.0f;
    sourceInputs.source.up.x = 0.0f;
    sourceInputs.source.up.y = 1.0f;
    sourceInputs.source.up.z = 0.0f;
    sourceInputs.source.right.x = 1.0f;
    sourceInputs.source.right.y = 0.0f;
    sourceInputs.source.right.z = 0.0f;
    sourceInputs.occlusionType = IPL_OCCLUSIONTYPE_RAYCAST;
    sourceInputs.occlusionRadius = 1.0f;
    sourceInputs.numOcclusionSamples = 1;

    iplSourceSetInputs(src.simulationSource, IPL_SIMULATIONFLAGS_DIRECT, &sourceInputs);

    // Run direct simulation
    iplSimulatorRunDirect(m_Simulator);

    // Read back results
    IPLSimulationOutputs outputs{};
    iplSourceGetOutputs(src.simulationSource, IPL_SIMULATIONFLAGS_DIRECT, &outputs);

    src.occlusion = outputs.direct.occlusion;

    if (m_TransmissionEnabled) {
        src.transmissionLow = outputs.direct.transmission[0];
        src.transmissionMid = outputs.direct.transmission[1];
        src.transmissionHigh = outputs.direct.transmission[2];
    } else {
        src.transmissionLow = 1.0f;
        src.transmissionMid = 1.0f;
        src.transmissionHigh = 1.0f;
    }
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

    // Create direct effect for occlusion/transmission if scene exists
    if (m_Scene) {
        IPLDirectEffectSettings directSettings{};
        directSettings.numChannels = 1;

        err = iplDirectEffectCreate(m_Context, &audioSettings, &directSettings, &src.directEffect);
        if (err != IPL_STATUS_SUCCESS) {
            ENJIN_LOG_WARN(Audio, "Steam Audio: failed to create direct effect for sound %u (error %d)",
                           handle, static_cast<int>(err));
            // Non-fatal — source will work without occlusion
        }

        // Create simulation source for occlusion queries
        if (m_Simulator) {
            IPLSourceSettings srcSettings{};
            srcSettings.flags = IPL_SIMULATIONFLAGS_DIRECT;

            err = iplSourceCreate(m_Simulator, &srcSettings, &src.simulationSource);
            if (err == IPL_STATUS_SUCCESS) {
                iplSourceAdd(src.simulationSource, m_Simulator);
                iplSimulatorCommit(m_Simulator);
            } else {
                ENJIN_LOG_WARN(Audio, "Steam Audio: failed to create simulation source for sound %u (error %d)",
                               handle, static_cast<int>(err));
            }
        }
    }

    // Allocate buffers
    src.inData = new f32[m_FrameSize]();      // mono
    src.outData = new f32[m_FrameSize * 2]();  // interleaved stereo
    src.directOutData = new f32[m_FrameSize](); // mono buffer for direct effect output
    src.direction = Math::Vector3(0, 0, 1);    // default: ahead
    src.active = true;

    m_Sources[handle] = src;
}

void SteamAudioProcessor::DestroySource(SoundHandle handle) {
    std::lock_guard<std::mutex> lock(m_SourceMutex);

    auto it = m_Sources.find(handle);
    if (it == m_Sources.end()) return;

    auto& src = it->second;
    if (src.simulationSource) {
        if (m_Simulator) {
            iplSourceRemove(src.simulationSource, m_Simulator);
            iplSimulatorCommit(m_Simulator);
        }
        iplSourceRelease(&src.simulationSource);
    }
    if (src.directEffect) {
        iplDirectEffectRelease(&src.directEffect);
    }
    if (src.effect) {
        iplBinauralEffectRelease(&src.effect);
    }
    delete[] src.inData;
    delete[] src.outData;
    delete[] src.directOutData;

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
    if (!inMono || !outStereo || frameCount == 0) return false;

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

    // Determine which mono buffer to feed into HRTF
    // If occlusion is active, run direct effect first: mono -> filtered mono
    f32* hrtfInputData = src.inData;

    if (m_OcclusionEnabled && src.directEffect) {
        IPLAudioBuffer directOutBuffer{};
        directOutBuffer.numChannels = 1;
        directOutBuffer.numSamples = static_cast<IPLint32>(m_FrameSize);
        f32* directOutChannels[1] = { src.directOutData };
        directOutBuffer.data = directOutChannels;

        IPLDirectEffectParams directParams{};
        directParams.flags = static_cast<IPLDirectEffectFlags>(
            IPL_DIRECTEFFECTFLAGS_APPLYDISTANCEATTENUATION |
            IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION |
            (m_TransmissionEnabled ? IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION : 0));
        directParams.distanceAttenuation = 1.0f;  // distance handled by miniaudio
        directParams.occlusion = src.occlusion;
        directParams.transmissionType = IPL_TRANSMISSIONTYPE_FREQINDEPENDENT;
        // Use average of 3-band transmission as frequency-independent value
        directParams.transmission = (src.transmissionLow + src.transmissionMid + src.transmissionHigh) / 3.0f;

        iplDirectEffectApply(src.directEffect, &directParams, &inBuffer, &directOutBuffer);
        hrtfInputData = src.directOutData;
    }

    // Build HRTF input buffer from (possibly filtered) mono data
    IPLAudioBuffer hrtfInBuffer{};
    hrtfInBuffer.numChannels = 1;
    hrtfInBuffer.numSamples = static_cast<IPLint32>(m_FrameSize);
    f32* hrtfInChannels[1] = { hrtfInputData };
    hrtfInBuffer.data = hrtfInChannels;

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

    IPLAudioEffectState state = iplBinauralEffectApply(src.effect, &params, &hrtfInBuffer, &outBuffer);
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
