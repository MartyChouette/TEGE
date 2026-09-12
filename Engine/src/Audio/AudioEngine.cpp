#include "Enjin/Platform/Platform.h"
#include "Enjin/Audio/AudioEngine.h"
#include "Enjin/Audio/AcousticScene.h"
#include "Enjin/Acoustics/ReverbDSP.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_set>

// miniaudio supports Emscripten/Web Audio out of the box (MA_ENABLE_WEBAUDIO).
// No special handling needed — miniaudio auto-detects the platform.
#include "miniaudio.h"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#ifdef ENJIN_AUDIO_STEAM_AUDIO
#include "Enjin/Audio/SteamAudioProcessor.h"
#include "Enjin/ECS/Components/Gameplay.h"

// Custom binaural processing node for Steam Audio HRTF
struct BinauralNode {
    ma_node_base base;
    Enjin::Audio::SteamAudioProcessor* processor;
    Enjin::Audio::SoundHandle soundHandle;
};

static void binaural_node_process(ma_node* pNode, const float** ppFramesIn,
                                   ma_uint32* pFrameCountIn, float** ppFramesOut,
                                   ma_uint32* pFrameCountOut)
{
    (void)pFrameCountIn;
    auto* node = reinterpret_cast<BinauralNode*>(pNode);
    ma_uint32 frameCount = *pFrameCountOut;

    if (node->processor && node->processor->IsEnabled() && node->processor->IsInitialized()) {
        if (node->processor->Process(node->soundHandle, ppFramesIn[0], ppFramesOut[0], frameCount)) {
            return;
        }
    }

    // Fallback: copy mono to both stereo channels
    for (ma_uint32 i = 0; i < frameCount; ++i) {
        ppFramesOut[0][i * 2]     = ppFramesIn[0][i];
        ppFramesOut[0][i * 2 + 1] = ppFramesIn[0][i];
    }
}

static ma_node_vtable g_binauralNodeVTable = {
    binaural_node_process,
    nullptr,  // onGetRequiredInputFrameCount
    1,        // inputBusCount
    1,        // outputBusCount
    0         // flags
};
#endif // ENJIN_AUDIO_STEAM_AUDIO

namespace Enjin {
namespace Audio {

// ============================================================================
// Environmental reverb: classic Freeverb (8 parallel combs + 4 serial allpasses
// per channel, mono-summed input, stereo-spread tunings) as a miniaudio node.
// Spatialized sounds attach here instead of the endpoint; the node mixes
// wet/dry and forwards to the endpoint. Parameters are atomics written by the
// game thread and smoothed per block on the audio thread.
// ============================================================================
namespace {

constexpr int kCombs = 8;
constexpr int kAllpasses = 4;
constexpr int kCombTuning[kCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
constexpr int kAllpassTuning[kAllpasses] = { 556, 441, 341, 225 };
constexpr int kStereoSpread = 23;
constexpr float kFixedGain = 0.015f;   // classic Freeverb input gain

struct ReverbChannel {
    std::vector<float> comb[kCombs];
    int combPos[kCombs] = {};
    float combStore[kCombs] = {};
    std::vector<float> allpass[kAllpasses];
    int allpassPos[kAllpasses] = {};
};

struct ReverbNode {
    ma_node_base base;
    ReverbChannel ch[2];
    std::vector<float> preDelayBuf;   // stereo interleaved
    int preDelayPos = 0;
    // Targets (game thread) and smoothed working values (audio thread)
    std::atomic<float> tWet{0.0f}, tRoom{0.5f}, tDamp{0.5f}, tDecay{1.5f}, tPre{0.02f};
    float wet = 0.0f, room = 0.5f, damp = 0.5f, pre = 0.02f, feedback = 0.8f;
    ma_uint32 sampleRate = 48000;

    // The measured room, when there is one.
    //
    // Freeverb stays as the fallback rather than being removed. Its controls
    // are room size and damping, which relate to a decay time only by feel, so
    // it cannot be handed "1.24 seconds" -- but every scene authored so far
    // sounds the way it does because of it, and replacing it wholesale would
    // change how existing projects sound without anybody asking. Measurement
    // wins where there is one; authoring answers where there is not.
    Acoustics::FeedbackDelayNetwork fdn;
    std::atomic<float> tRT60Low{0.0f}, tRT60Mid{0.0f}, tRT60High{0.0f};
    std::atomic<float> tMeanFreePath{8.0f}, tReflected{1.0f};
    // Bumped by the game thread whenever the numbers above change. The audio
    // thread reconfigures when it sees a new value, which keeps the comparison
    // to one integer instead of five floats.
    std::atomic<unsigned> measuredGeneration{0};
    unsigned appliedGeneration = 0;
    bool measuredActive = false;
    float lateLevel = 1.0f;
};

static void reverb_node_process(ma_node* pNode, const float** ppFramesIn,
                                ma_uint32* pFrameCountIn, float** ppFramesOut,
                                ma_uint32* pFrameCountOut)
{
    (void)pFrameCountIn;
    auto* rn = reinterpret_cast<ReverbNode*>(pNode);
    const float* in = ppFramesIn[0];
    float* out = ppFramesOut[0];
    ma_uint32 frames = *pFrameCountOut;

    // Smooth toward targets once per block (fast enough at typical block sizes)
    const float a = 0.08f;
    rn->wet  += (rn->tWet.load(std::memory_order_relaxed)  - rn->wet)  * a;
    rn->room += (rn->tRoom.load(std::memory_order_relaxed) - rn->room) * a;
    rn->damp += (rn->tDamp.load(std::memory_order_relaxed) - rn->damp) * a;
    rn->pre  += (rn->tPre.load(std::memory_order_relaxed)  - rn->pre)  * a;
    float decay = rn->tDecay.load(std::memory_order_relaxed);
    float room01 = rn->room < 0.0f ? 0.0f : (rn->room > 1.0f ? 1.0f : rn->room);
    float decayBoost = decay / 5.0f; if (decayBoost > 1.0f) decayBoost = 1.0f;
    rn->feedback = 0.58f + 0.3f * room01 + 0.1f * decayBoost;
    if (rn->feedback > 0.985f) rn->feedback = 0.985f;
    float damp1 = rn->damp < 0.0f ? 0.0f : (rn->damp > 1.0f ? 1.0f : rn->damp);
    float wet = rn->wet < 0.0f ? 0.0f : (rn->wet > 1.0f ? 1.0f : rn->wet);

    if (wet < 0.005f) {   // bypass: straight copy
        for (ma_uint32 i = 0; i < frames * 2; ++i) out[i] = in[i];
        return;
    }

    // Pick up a new measurement, if the game thread left one.
    //
    // Reconfiguring is allocation-free and deliberately does not clear the
    // delay lines: this happens when a listener walks between rooms, and
    // zeroing the tail at that moment is an audible click at exactly the
    // instant somebody is listening for the room to change.
    const unsigned generation = rn->measuredGeneration.load(std::memory_order_acquire);
    if (generation != rn->appliedGeneration) {
        rn->appliedGeneration = generation;
        const float rt60[3] = {
            rn->tRT60Low.load(std::memory_order_relaxed),
            rn->tRT60Mid.load(std::memory_order_relaxed),
            rn->tRT60High.load(std::memory_order_relaxed),
        };
        if (rt60[1] > 0.0f) {
            rn->fdn.Configure(rt60, rn->tMeanFreePath.load(std::memory_order_relaxed));
            rn->lateLevel = rn->tReflected.load(std::memory_order_relaxed);
            if (rn->lateLevel > 1.0f) rn->lateLevel = 1.0f;
            if (rn->lateLevel < 0.0f) rn->lateLevel = 0.0f;
            rn->measuredActive = rn->fdn.IsConfigured();
        } else {
            rn->measuredActive = false;
        }
    }

    if (rn->measuredActive) {
        const int preLenM = static_cast<int>(rn->preDelayBuf.size() / 2);
        int preSamplesM = static_cast<int>(rn->pre * static_cast<float>(rn->sampleRate));
        if (preSamplesM >= preLenM) preSamplesM = preLenM - 1;
        if (preSamplesM < 0) preSamplesM = 0;

        for (ma_uint32 f = 0; f < frames; ++f) {
            const float dryL = in[f * 2], dryR = in[f * 2 + 1];

            rn->preDelayBuf[rn->preDelayPos * 2]     = dryL;
            rn->preDelayBuf[rn->preDelayPos * 2 + 1] = dryR;
            int rd = rn->preDelayPos - preSamplesM;
            if (rd < 0) rd += preLenM;
            const float feed = (rn->preDelayBuf[rd * 2] + rn->preDelayBuf[rd * 2 + 1]) * 0.5f;
            rn->preDelayPos = (rn->preDelayPos + 1) % preLenM;

            float wl = 0.0f, wr = 0.0f;
            rn->fdn.Process(feed, wl, wr);
            out[f * 2]     = dryL + wl * rn->lateLevel * wet;
            out[f * 2 + 1] = dryR + wr * rn->lateLevel * wet;
        }
        return;
    }

    const int preLen = static_cast<int>(rn->preDelayBuf.size() / 2);
    int preSamples = static_cast<int>(rn->pre * static_cast<float>(rn->sampleRate));
    if (preSamples >= preLen) preSamples = preLen - 1;
    if (preSamples < 0) preSamples = 0;

    for (ma_uint32 f = 0; f < frames; ++f) {
        float dryL = in[f * 2], dryR = in[f * 2 + 1];

        // Pre-delay the reverb feed
        rn->preDelayBuf[rn->preDelayPos * 2]     = dryL;
        rn->preDelayBuf[rn->preDelayPos * 2 + 1] = dryR;
        int rd = rn->preDelayPos - preSamples;
        if (rd < 0) rd += preLen;
        float feed = (rn->preDelayBuf[rd * 2] + rn->preDelayBuf[rd * 2 + 1]) * kFixedGain;
        rn->preDelayPos = (rn->preDelayPos + 1) % preLen;

        float wetOut[2] = { 0.0f, 0.0f };
        for (int c = 0; c < 2; ++c) {
            ReverbChannel& rc = rn->ch[c];
            float sum = 0.0f;
            for (int i = 0; i < kCombs; ++i) {
                std::vector<float>& buf = rc.comb[i];
                int& pos = rc.combPos[i];
                float output = buf[pos];
                rc.combStore[i] = output * (1.0f - damp1) + rc.combStore[i] * damp1;
                buf[pos] = feed + rc.combStore[i] * rn->feedback;
                pos = (pos + 1) % static_cast<int>(buf.size());
                sum += output;
            }
            for (int i = 0; i < kAllpasses; ++i) {
                std::vector<float>& buf = rc.allpass[i];
                int& pos = rc.allpassPos[i];
                float bufOut = buf[pos];
                buf[pos] = sum + bufOut * 0.5f;
                sum = bufOut - sum;
                pos = (pos + 1) % static_cast<int>(buf.size());
            }
            wetOut[c] = sum;
        }
        out[f * 2]     = dryL + wetOut[0] * wet * 3.0f;
        out[f * 2 + 1] = dryR + wetOut[1] * wet * 3.0f;
    }
}

static ma_node_vtable g_reverbNodeVTable = {
    reverb_node_process,
    nullptr,
    1,   // input buses
    1,   // output buses
    MA_NODE_FLAG_CONTINUOUS_PROCESSING   // tail keeps ringing after inputs stop
};

} // namespace

// pImpl holding the ma_engine
struct AudioEngine::Impl {
    ma_engine engine{};
    ReverbNode reverb{};
    bool reverbReady = false;
    bool initialized = false;
};

AudioEngine::AudioEngine()
    : m_Impl(std::make_unique<Impl>()) {
}

AudioEngine::~AudioEngine() {
    Shutdown();
}

bool AudioEngine::Initialize() {
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
#if defined(__EMSCRIPTEN__)
    // Every AudioContext a browser creates starts suspended and stays that way
    // until the page has seen a real user gesture. Chrome says so in the
    // console. Nothing plays until ResumeAfterUserGesture lifts this.
    m_WebAudioGated = true;
    ENJIN_LOG_INFO(Audio, "AudioEngine initialized (miniaudio/WebAudio) - held until the first user gesture");
#else
    ENJIN_LOG_INFO(Audio, "AudioEngine initialized (miniaudio backend)");
#endif

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    // Initialize Steam Audio HRTF processor
    if (m_HRTFEnabled) {
        m_SteamAudio = std::make_unique<SteamAudioProcessor>();
        // Environmental reverb bus: allocate Freeverb delay lines for the actual
        // sample rate and splice the node in front of the endpoint. Spatialized
        // sounds attach to this node; the rest stay directly on the endpoint.
        {
            ma_uint32 sr = ma_engine_get_sample_rate(&m_Impl->engine);
            m_Impl->reverb.sampleRate = sr;
    m_Impl->reverb.fdn.Prepare(sr);
            const float srScale = static_cast<float>(sr) / 44100.0f;
            for (int c = 0; c < 2; ++c) {
                for (int i = 0; i < kCombs; ++i) {
                    int len = static_cast<int>(static_cast<float>(kCombTuning[i] + c * kStereoSpread) * srScale);
                    m_Impl->reverb.ch[c].comb[i].assign(static_cast<usize>(len > 8 ? len : 8), 0.0f);
                }
                for (int i = 0; i < kAllpasses; ++i) {
                    int len = static_cast<int>(static_cast<float>(kAllpassTuning[i] + c * kStereoSpread) * srScale);
                    m_Impl->reverb.ch[c].allpass[i].assign(static_cast<usize>(len > 4 ? len : 4), 0.0f);
                }
            }
            m_Impl->reverb.preDelayBuf.assign(static_cast<usize>(sr / 4) * 2, 0.0f);  // up to 250 ms

            ma_node_config nodeCfg = ma_node_config_init();
            ma_uint32 chans = 2;
            nodeCfg.vtable = &g_reverbNodeVTable;
            nodeCfg.pInputChannels = &chans;
            nodeCfg.pOutputChannels = &chans;
            if (ma_node_init(ma_engine_get_node_graph(&m_Impl->engine), &nodeCfg, nullptr,
                             &m_Impl->reverb.base) == MA_SUCCESS) {
                ma_node_attach_output_bus(&m_Impl->reverb.base, 0,
                                          ma_engine_get_endpoint(&m_Impl->engine), 0);
                m_Impl->reverbReady = true;
            } else {
                ENJIN_LOG_WARN(Audio, "Environmental reverb node init failed - sounds stay dry");
            }
        }

        ma_uint32 sampleRate = ma_engine_get_sample_rate(&m_Impl->engine);
        // Use engine's period size for frame size (typically 480 for 48kHz)
        ma_uint32 periodSize = 0;
        ma_device* pDevice = ma_engine_get_device(&m_Impl->engine);
        if (pDevice) {
            periodSize = pDevice->playback.internalPeriodSizeInFrames;
        }
        if (periodSize == 0) periodSize = 480;  // safe default

        if (!m_SteamAudio->Initialize(sampleRate, periodSize)) {
            ENJIN_LOG_WARN(Audio, "Steam Audio HRTF initialization failed, HRTF disabled");
            m_SteamAudio.reset();
        }
    }
#endif

    return true;
}

void AudioEngine::Shutdown() {
    if (!m_Initialized) return;

    StopAll();
    m_Clips.clear();

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    // Shutdown Steam Audio before miniaudio engine
    if (m_SteamAudio) {
        m_SteamAudio->Shutdown();
        m_SteamAudio.reset();
    }
#endif

    if (m_Impl && m_Impl->initialized) {
        if (m_Impl->reverbReady) {
            ma_node_uninit(&m_Impl->reverb.base, nullptr);
            m_Impl->reverbReady = false;
        }
        ma_engine_uninit(&m_Impl->engine);
        m_Impl->initialized = false;
    }

    m_Initialized = false;
    ENJIN_LOG_INFO(Audio, "AudioEngine shutdown");
}

void AudioEngine::SetMeasuredRoom(const f32 rt60[3], f32 meanFreePath, f32 reflectedEnergy) {
    if (!m_Impl || !m_Impl->reverbReady) return;
    auto& rn = m_Impl->reverb;
    rn.tRT60Low.store(rt60[0], std::memory_order_relaxed);
    rn.tRT60Mid.store(rt60[1], std::memory_order_relaxed);
    rn.tRT60High.store(rt60[2], std::memory_order_relaxed);
    rn.tMeanFreePath.store(meanFreePath, std::memory_order_relaxed);
    rn.tReflected.store(reflectedEnergy, std::memory_order_relaxed);
    // Released last, so the audio thread never sees a new generation with half
    // the numbers still belonging to the previous room.
    rn.measuredGeneration.fetch_add(1, std::memory_order_release);
    m_HasMeasuredRoom = (rt60[1] > 0.0f);
}

void AudioEngine::ClearMeasuredRoom() {
    if (!m_Impl || !m_Impl->reverbReady) return;
    auto& rn = m_Impl->reverb;
    rn.tRT60Mid.store(0.0f, std::memory_order_relaxed);
    rn.measuredGeneration.fetch_add(1, std::memory_order_release);
    m_HasMeasuredRoom = false;
}

void AudioEngine::SetEnvironmentReverb(f32 wetDry, f32 roomSize, f32 damping, f32 decayTime, f32 preDelay) {
    if (!m_Impl || !m_Impl->reverbReady) return;
    m_Impl->reverb.tWet.store(wetDry, std::memory_order_relaxed);
    m_Impl->reverb.tRoom.store(roomSize, std::memory_order_relaxed);
    m_Impl->reverb.tDamp.store(damping, std::memory_order_relaxed);
    m_Impl->reverb.tDecay.store(decayTime, std::memory_order_relaxed);
    m_Impl->reverb.tPre.store(preDelay, std::memory_order_relaxed);
}

void AudioEngine::SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up) {
    m_ListenerPosition = position;
    m_ListenerForward = forward;
    m_ListenerUp = up;

    if (m_Impl && m_Impl->initialized) {
        ma_engine_listener_set_position(&m_Impl->engine, 0, position.x, position.y, position.z);
        ma_engine_listener_set_direction(&m_Impl->engine, 0, forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(&m_Impl->engine, 0, up.x, up.y, up.z);
    }

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    // Update all active binaural source directions (listener moved)
    if (m_SteamAudio && m_SteamAudio->IsInitialized()) {
        for (auto& [handle, sound] : m_Sounds) {
            if (sound.is3D && sound.binauralNode) {
                auto dir = SteamAudioProcessor::ComputeListenerRelativeDirection(
                    position, forward, up, sound.position);
                m_SteamAudio->SetSourceDirection(handle, dir);

                // Update distance attenuation (spatialization is off for HRTF sounds)
                if (sound.maSound) {
                    f32 distVol = Calculate3DVolume(sound.position, sound.minDistance, sound.maxDistance);
                    ma_sound_set_volume(static_cast<ma_sound*>(sound.maSound),
                        EffectiveVolume(sound.volume, sound.channel) * distVol);
                }
            }
        }
    }
#endif
}

bool AudioEngine::LoadWAV(const std::string& filepath, AudioClipData& clip) {
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

    ENJIN_LOG_INFO(Audio, "Registered audio file: %s (%lld bytes)", filepath.c_str(), static_cast<long long>(fileSize));
    return true;
}

AudioClipHandle AudioEngine::LoadClip(const std::string& filepath) {
    // Resolve relative paths against the asset root (project dir); the CWD is
    // the exe dir, so project-relative paths never resolve without this.
    std::string resolved = filepath;
    if (!m_AssetRoot.empty() && !std::filesystem::path(filepath).is_absolute()) {
        std::filesystem::path rooted = std::filesystem::path(m_AssetRoot) / filepath;
        std::error_code ec;
        if (std::filesystem::exists(rooted, ec)) {
            resolved = rooted.string();
        }
    }

    // Check if already loaded
    for (const auto& [handle, clip] : m_Clips) {
        if (clip.filepath == resolved) {
            return handle;
        }
    }

    AudioClipHandle handle = m_NextClipHandle++;
    if (m_NextClipHandle == 0) m_NextClipHandle = 1;
    AudioClipData clipData;
    clipData.filepath = resolved;

    // Verify file exists (miniaudio supports WAV, MP3, FLAC, and Vorbis natively)
    std::ifstream testFile(resolved, std::ios::binary);
    if (testFile.is_open()) {
        clipData.loaded = true;
        testFile.close();
    } else {
        ENJIN_LOG_WARN(Audio, "Audio file not found: %s", resolved.c_str());
    }

    m_Clips[handle] = std::move(clipData);
    ENJIN_LOG_INFO(Audio, "Registered audio clip: %s (handle: %u)", resolved.c_str(), handle);
    return handle;
}

void AudioEngine::UnloadClip(AudioClipHandle clip) {
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

void AudioEngine::CleanupUnusedClips() {
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

void AudioEngine::CleanupSound(SoundInstance& sound) {
#ifdef ENJIN_AUDIO_STEAM_AUDIO
    if (sound.binauralNode) {
        auto* bNode = static_cast<BinauralNode*>(sound.binauralNode);
        ma_node_detach_all_output_buses(&bNode->base);
        ma_node_uninit(&bNode->base, nullptr);
        if (m_SteamAudio) {
            m_SteamAudio->DestroySource(bNode->soundHandle);
        }
        delete bNode;
        sound.binauralNode = nullptr;
    }
#endif

    if (sound.maSound) {
        auto* maS = static_cast<ma_sound*>(sound.maSound);
        ma_sound_stop(maS);
        ma_sound_uninit(maS);
        delete maS;
        sound.maSound = nullptr;
    }
    sound.isPlaying = false;
}

SoundHandle AudioEngine::Play(AudioClipHandle clip, f32 volume, f32 pitch, bool loop,
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

SoundHandle AudioEngine::Play3D(AudioClipHandle clip, const Math::Vector3& position,
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
            // World sounds take on the environment: route through the reverb
            // bus (2D/UI/music stay attached to the endpoint = dry).
            if (m_Impl->reverbReady) {
                ma_node_attach_output_bus(maS, 0, &m_Impl->reverb.base, 0);
            }
            sound.maSound = maS;

#ifdef ENJIN_AUDIO_STEAM_AUDIO
            // Wire HRTF binaural processing if available
            if (m_SteamAudio && m_SteamAudio->IsInitialized() && m_HRTFEnabled) {
                // Disable miniaudio's built-in spatialization — HRTF replaces it
                ma_sound_set_spatialization_enabled(maS, MA_FALSE);

                // Apply distance attenuation manually
                f32 distVol = Calculate3DVolume(position, clampedMinDist, clampedMaxDist);
                ma_sound_set_volume(maS, EffectiveVolume(clampedVolume, channel) * distVol);

                // Create Steam Audio binaural source
                m_SteamAudio->CreateSource(handle);
                auto dir = SteamAudioProcessor::ComputeListenerRelativeDirection(
                    m_ListenerPosition, m_ListenerForward, m_ListenerUp, position);
                m_SteamAudio->SetSourceDirection(handle, dir);

                // Create and wire binaural node into the audio graph
                auto* bNode = new BinauralNode();
                bNode->processor = m_SteamAudio.get();
                bNode->soundHandle = handle;

                ma_uint32 inputChannels[1] = {1};   // mono from sound
                ma_uint32 outputChannels[1] = {2};  // stereo binaural output

                ma_node_config nodeConfig = ma_node_config_init();
                nodeConfig.vtable = &g_binauralNodeVTable;
                nodeConfig.pInputChannels = inputChannels;
                nodeConfig.pOutputChannels = outputChannels;
                nodeConfig.inputBusCount = 1;
                nodeConfig.outputBusCount = 1;

                ma_result nodeResult = ma_node_init(
                    ma_engine_get_node_graph(&m_Impl->engine),
                    &nodeConfig, nullptr, &bNode->base);

                if (nodeResult == MA_SUCCESS) {
                    // Detach sound from endpoint, wire through binaural node
                    ma_node_detach_output_bus(reinterpret_cast<ma_node*>(maS), 0);
                    ma_node_attach_output_bus(reinterpret_cast<ma_node*>(maS), 0, &bNode->base, 0);
                    ma_node_attach_output_bus(&bNode->base, 0,
                        ma_engine_get_endpoint(&m_Impl->engine), 0);
                    sound.binauralNode = bNode;
                } else {
                    ENJIN_LOG_WARN(Audio, "Failed to create binaural node (error %d), falling back to basic spatialization",
                                   nodeResult);
                    ma_sound_set_spatialization_enabled(maS, MA_TRUE);
                    ma_sound_set_volume(maS, EffectiveVolume(clampedVolume, channel));
                    m_SteamAudio->DestroySource(handle);
                    delete bNode;
                }
            }
#endif

            ma_sound_start(maS);
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

void AudioEngine::PlayOneShot(AudioClipHandle clip, f32 volume, AudioChannel channel) {
    Play(clip, volume, 1.0f, false, channel);
}

void AudioEngine::PlayOneShot3D(AudioClipHandle clip, const Math::Vector3& position, f32 volume) {
    Play3D(clip, position, volume);
}

void AudioEngine::Stop(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        CleanupSound(it->second);
        m_Sounds.erase(it);
    }
}

void AudioEngine::StopAll() {
    for (auto& [handle, sound] : m_Sounds) {
        CleanupSound(sound);
    }
    m_Sounds.clear();
}

void AudioEngine::Pause(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end() && it->second.maSound) {
        ma_sound_stop(static_cast<ma_sound*>(it->second.maSound));
        it->second.isPlaying = false;
    }
}

void AudioEngine::Resume(SoundHandle sound) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end() && it->second.maSound) {
        ma_sound_start(static_cast<ma_sound*>(it->second.maSound));
        it->second.isPlaying = true;
    }
}

void AudioEngine::SetVolume(SoundHandle sound, f32 volume) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.volume = Math::Clamp(volume, 0.0f, 1.0f);
        if (it->second.maSound) {
            f32 vol = EffectiveVolume(it->second.volume, it->second.channel);
#ifdef ENJIN_AUDIO_STEAM_AUDIO
            // HRTF sounds need manual distance attenuation
            if (it->second.binauralNode && it->second.is3D) {
                vol *= Calculate3DVolume(it->second.position, it->second.minDistance, it->second.maxDistance);
            }
#endif
            ma_sound_set_volume(static_cast<ma_sound*>(it->second.maSound), vol);
        }
    }
}

void AudioEngine::SetPitch(SoundHandle sound, f32 pitch) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.pitch = Math::Clamp(pitch, 0.1f, 3.0f);
        if (it->second.maSound) {
            ma_sound_set_pitch(static_cast<ma_sound*>(it->second.maSound), it->second.pitch);
        }
    }
}

void AudioEngine::SetPosition(SoundHandle sound, const Math::Vector3& position) {
    auto it = m_Sounds.find(sound);
    if (it != m_Sounds.end()) {
        it->second.position = position;
        if (it->second.maSound) {
#ifdef ENJIN_AUDIO_STEAM_AUDIO
            if (it->second.binauralNode && m_SteamAudio && m_SteamAudio->IsInitialized()) {
                // Update HRTF direction
                auto dir = SteamAudioProcessor::ComputeListenerRelativeDirection(
                    m_ListenerPosition, m_ListenerForward, m_ListenerUp, position);
                m_SteamAudio->SetSourceDirection(sound, dir);

                // Update distance attenuation (spatialization disabled for HRTF sounds)
                f32 distVol = Calculate3DVolume(position, it->second.minDistance, it->second.maxDistance);
                ma_sound_set_volume(static_cast<ma_sound*>(it->second.maSound),
                    EffectiveVolume(it->second.volume, it->second.channel) * distVol);
            } else
#endif
            {
                ma_sound_set_position(static_cast<ma_sound*>(it->second.maSound),
                    position.x, position.y, position.z);
            }
        }
    }
}

bool AudioEngine::IsPlaying(SoundHandle sound) const {
    auto it = m_Sounds.find(sound);
    if (it == m_Sounds.end()) return false;

    // Check miniaudio state if available
    if (it->second.maSound) {
        return ma_sound_is_playing(static_cast<ma_sound*>(it->second.maSound)) != 0;
    }
    return it->second.isPlaying;
}

bool AudioEngine::IsDeviceRunning() const {
    if (!m_Initialized) return false;
    // The browser's refusal is invisible from here, so it is tracked separately
    // rather than inferred from the device. See m_WebAudioGated.
    if (m_WebAudioGated) return false;
    ma_device* dev = ma_engine_get_device(const_cast<ma_engine*>(&m_Impl->engine));
    return dev != nullptr && ma_device_get_state(dev) == ma_device_state_started;
}

void AudioEngine::ResumeAfterUserGesture() {
    if (!m_Initialized || !m_WebAudioGated) return;

#if defined(__EMSCRIPTEN__)
    // Only a real activation lifts the gate. The caller watches for input, but
    // input is not the same fact as "the browser considers this page
    // activated", and lifting the gate early would put us back where we
    // started: sources starting into a context that is not running.
    const int activated = EM_ASM_INT({
        return (navigator.userActivation && navigator.userActivation.hasBeenActive) ? 1 : 0;
    });
    if (!activated) return;

    // ma_device_start is what calls resume() on the AudioContext, and the
    // browser only honours it once the page has been activated.
    ma_device* dev = ma_engine_get_device(&m_Impl->engine);
    if (dev) ma_device_start(dev);
#endif

    m_WebAudioGated = false;
    ENJIN_LOG_INFO(Audio, "Audio released: the page has been activated, sound can start now");
}

f32 AudioEngine::GetPlaybackTime(SoundHandle sound) const {
    auto it = m_Sounds.find(sound);
    if (it == m_Sounds.end() || !it->second.maSound) return -1.0f;

    f32 cursor = 0.0f;
    if (ma_sound_get_cursor_in_seconds(
            static_cast<ma_sound*>(it->second.maSound), &cursor) != MA_SUCCESS) {
        return -1.0f;
    }
    return cursor;
}

f32 AudioEngine::GetLength(SoundHandle sound) const {
    auto it = m_Sounds.find(sound);
    if (it == m_Sounds.end() || !it->second.maSound) return -1.0f;

    f32 length = 0.0f;
    if (ma_sound_get_length_in_seconds(
            static_cast<ma_sound*>(it->second.maSound), &length) != MA_SUCCESS) {
        return -1.0f;
    }
    return length;
}

bool AudioEngine::Seek(SoundHandle sound, f32 seconds) {
    auto it = m_Sounds.find(sound);
    if (it == m_Sounds.end() || !it->second.maSound) return false;
    if (seconds < 0.0f) seconds = 0.0f;

    auto* ma = static_cast<ma_sound*>(it->second.maSound);
    // Seeking is in FRAMES, so it needs the engine's sample rate rather than
    // the clip's: ma_sound rides the engine's output rate.
    const u32 rate = ma_engine_get_sample_rate(&m_Impl->engine);
    if (rate == 0) return false;
    const ma_uint64 frame = static_cast<ma_uint64>(seconds * static_cast<f32>(rate));
    return ma_sound_seek_to_pcm_frame(ma, frame) == MA_SUCCESS;
}

void AudioEngine::SetMasterVolume(f32 volume) {
    m_MasterVolume = Math::Clamp(volume, 0.0f, 1.0f);
    // Update all active sounds with new effective volume
    for (auto& [handle, sound] : m_Sounds) {
        if (sound.maSound) {
            f32 vol = EffectiveVolume(sound.volume, sound.channel);
#ifdef ENJIN_AUDIO_STEAM_AUDIO
            if (sound.binauralNode && sound.is3D) {
                vol *= Calculate3DVolume(sound.position, sound.minDistance, sound.maxDistance);
            }
#endif
            ma_sound_set_volume(static_cast<ma_sound*>(sound.maSound), vol);
        }
    }
}

void AudioEngine::SetChannelVolume(AudioChannel channel, f32 volume) {
    auto idx = static_cast<usize>(channel);
    if (idx >= static_cast<usize>(AudioChannel::Count)) return;
    m_ChannelVolumes[idx] = Math::Clamp(volume, 0.0f, 1.0f);
    // Update all active sounds on this channel
    for (auto& [handle, sound] : m_Sounds) {
        if (sound.channel == channel && sound.maSound) {
            f32 vol = EffectiveVolume(sound.volume, channel);
#ifdef ENJIN_AUDIO_STEAM_AUDIO
            if (sound.binauralNode && sound.is3D) {
                vol *= Calculate3DVolume(sound.position, sound.minDistance, sound.maxDistance);
            }
#endif
            ma_sound_set_volume(static_cast<ma_sound*>(sound.maSound), vol);
        }
    }
}

f32 AudioEngine::GetChannelVolume(AudioChannel channel) const {
    auto idx = static_cast<usize>(channel);
    if (idx >= static_cast<usize>(AudioChannel::Count)) return 1.0f;
    return m_ChannelVolumes[idx];
}

void AudioEngine::StopChannel(AudioChannel channel) {
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

f32 AudioEngine::EffectiveVolume(f32 instanceVolume, AudioChannel channel) const {
    // Cached bus pointers — avoid per-call string-based map lookup.
    // These are resolved once and reused (buses are never removed at runtime).
    static const Audio::AudioBus* cachedBuses[4] = {nullptr, nullptr, nullptr, nullptr};
    static bool busesResolved = false;
    if (!busesResolved) {
        cachedBuses[0] = m_Mixer.GetBus("SFX");
        cachedBuses[1] = m_Mixer.GetBus("Music");
        cachedBuses[2] = m_Mixer.GetBus("UI");
        cachedBuses[3] = m_Mixer.GetBus("Voice");
        busesResolved = true;
    }

    auto idx = static_cast<usize>(channel);
    f32 busVol = (idx < 4 && cachedBuses[idx]) ? cachedBuses[idx]->GetEffectiveVolume() : 1.0f;

    // Also apply legacy channel volume for backward compatibility
    f32 legacyVol = (idx < static_cast<usize>(AudioChannel::Count)) ? m_ChannelVolumes[idx] : 1.0f;

    return instanceVolume * busVol * legacyVol * m_MasterVolume;
}

void AudioEngine::Update(f32 deltaTime) {
    // Update bus mixer (volume fades, snapshot transitions)
    m_Mixer.Update(deltaTime);
    m_Crossfader.Update(deltaTime);

    // Compute per-bus VU levels from active sounds
    u32 busSoundCounts[4] = {0, 0, 0, 0};
    f32 busRmsLevels[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    for (const auto& [handle, sound] : m_Sounds) {
        if (!sound.isPlaying) continue;
        auto chIdx = static_cast<usize>(sound.channel);
        if (chIdx < 4) {
            busSoundCounts[chIdx]++;
            busRmsLevels[chIdx] += sound.volume * 0.5f;  // Approximate RMS from volume
        }
    }
    static const char* busNames[] = {"SFX", "Music", "UI", "Voice"};
    for (int i = 0; i < 4; ++i) {
        f32 rms = (busSoundCounts[i] > 0) ? busRmsLevels[i] / busSoundCounts[i] : 0.0f;
        m_Mixer.UpdateVU(busNames[i], rms, busSoundCounts[i]);
    }

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

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    // Throttled occlusion updates (~10Hz)
    if (m_SteamAudio && m_SteamAudio->IsInitialized() && m_SteamAudio->HasScene() && m_OcclusionEnabled) {
        m_OcclusionTimer += deltaTime;
        if (m_OcclusionTimer >= 0.1f) {
            m_OcclusionTimer = 0.0f;

            for (auto& [handle, sound] : m_Sounds) {
                if (sound.is3D && sound.binauralNode && sound.isPlaying) {
                    m_SteamAudio->UpdateOcclusion(handle, sound.position, m_ListenerPosition);
                }
            }
        }
    }
#endif
}

// Which clip this play uses, and at what pitch and volume.
//
// The component has carried pitchMin/pitchMax, volumeMin/volumeMax,
// clipVariations and noRepeat since the inspector grew a "Randomization"
// section, and NOTHING read any of them: they were authored, serialized,
// checked by the asset validator, and then ignored at play time. Every
// footstep came out identical, which is the exact thing the section exists to
// prevent. Implemented here rather than at each call site so a script-driven
// play and a play-on-awake sound the same.
AudioEngine::PlayVariation AudioEngine::ChooseVariation(ECS::AudioSourceComponent& src) {
    PlayVariation v;
    v.clipPath = src.clipPath;
    v.pitch = src.pitch;
    v.volume = src.volume;

    auto jitter = [this](f32 lo, f32 hi) {
        if (hi <= lo) return lo;
        std::uniform_real_distribution<f32> d(lo, hi);
        return d(m_Rng);
    };

    // A variation list is the SOURCE's clip plus its alternates: picking only
    // among the alternates would make the authored clip the one you never hear.
    if (!src.clipVariations.empty()) {
        const usize count = src.clipVariations.size() + 1;
        usize pick;
        if (src.noRepeat && count > 1) {
            // Choose from the others, then map back. Rerolling until it differs
            // has no bound; this picks in one go.
            std::uniform_int_distribution<usize> d(0, count - 2);
            pick = d(m_Rng);
            if (pick >= src.lastPlayedIndex) ++pick;
        } else {
            std::uniform_int_distribution<usize> d(0, count - 1);
            pick = d(m_Rng);
        }
        src.lastPlayedIndex = static_cast<u32>(pick);
        if (pick > 0) v.clipPath = src.clipVariations[pick - 1];
    }

    // 1.0/1.0 is the default and means "no variation", so the jitter only
    // applies where an author actually widened the range.
    v.pitch = src.pitch * jitter(src.pitchMin, src.pitchMax);
    v.volume = src.volume * jitter(src.volumeMin, src.volumeMax);
    return v;
}

void AudioEngine::UpdateAudioSources(f32 deltaTime) {
    if (!m_World) return;

    for (ECS::Entity entity : m_World->GetEntitiesWithComponent<ECS::AudioSourceComponent>()) {
        if (!m_World->IsValid(entity)) continue;

        auto* audio = m_World->GetComponent<ECS::AudioSourceComponent>(entity);
        if (!audio) continue;
        auto* transform = m_World->GetComponent<ECS::TransformComponent>(entity);

        Math::Vector3 position = transform ? transform->position : Math::Vector3(0, 0, 0);

        // Handle playOnAwake.
        //
        // Held until the device is actually RUNNING. In a browser that is not
        // until the player's first gesture, and starting a clip into a
        // suspended context does not queue it -- the context's clock is
        // stopped, so when it finally resumes the sound is already several
        // seconds "in" and starts from the middle, or is simply gone. A title
        // track would begin partway through the first time anyone clicked.
        // `awakeTriggered` is deliberately NOT set while we wait, so this is a
        // hold and not a skip.
        if (audio->playOnAwake && !audio->isPlaying && !audio->awakeTriggered &&
            !IsDeviceRunning()) {
            continue;
        }
        if (audio->playOnAwake && !audio->isPlaying && !audio->awakeTriggered) {
            if (!audio->clipPath.empty()) {
                PlayVariation v = ChooseVariation(*audio);
                AudioClipHandle clip = LoadClip(v.clipPath);
                // Map ECS::AudioChannel to Audio::AudioChannel (same enum values)
                auto ch = static_cast<AudioChannel>(static_cast<u8>(audio->channel));
                // Music and UI channels force non-diegetic (2D) playback
                bool diegetic3D = audio->is3D &&
                    ch != AudioChannel::Music && ch != AudioChannel::UI;
                SoundHandle snd;
                if (diegetic3D) {
                    snd = Play3D(clip, position, v.volume, audio->minDistance, audio->maxDistance, ch);
                } else {
                    snd = Play(clip, v.volume, v.pitch, audio->loop, ch);
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

f32 AudioEngine::Calculate3DVolume(const Math::Vector3& soundPos, f32 minDist, f32 maxDist) const {
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

#ifdef ENJIN_AUDIO_STEAM_AUDIO
void AudioEngine::SetHRTFEnabled(bool enabled) {
    m_HRTFEnabled = enabled;
    if (m_SteamAudio) {
        m_SteamAudio->SetEnabled(enabled);
    }
}

bool AudioEngine::IsHRTFEnabled() const {
    return m_HRTFEnabled && m_SteamAudio && m_SteamAudio->IsInitialized();
}

bool AudioEngine::IsHRTFAvailable() const {
    return m_SteamAudio && m_SteamAudio->IsInitialized();
}

void AudioEngine::SetOcclusionEnabled(bool enabled) {
    m_OcclusionEnabled = enabled;
    if (m_SteamAudio) {
        m_SteamAudio->SetOcclusionEnabled(enabled);
    }
}

bool AudioEngine::IsOcclusionEnabled() const {
    return m_OcclusionEnabled && m_SteamAudio && m_SteamAudio->IsInitialized();
}

void AudioEngine::SetTransmissionEnabled(bool enabled) {
    m_TransmissionEnabled = enabled;
    if (m_SteamAudio) {
        m_SteamAudio->SetTransmissionEnabled(enabled);
    }
}

bool AudioEngine::IsTransmissionEnabled() const {
    return m_TransmissionEnabled && m_SteamAudio && m_SteamAudio->IsInitialized();
}

// Hand the room to Steam Audio.
//
// UNVERIFIED ON THIS MACHINE. Everything in this function is inside
// ENJIN_AUDIO_STEAM_AUDIO, the SDK is not in third_party/steamaudio, and the
// option defaults to OFF -- so this does not compile here and no test can reach
// it. It is written to be as thin as possible for exactly that reason: all the
// judgement lives in BuildAcousticScene, which is plain engine code with
// twelve tests on it, and what is left here is a type conversion.
//
// What changed: this used to gather the geometry itself and hand every triangle
// ONE material, described in a comment as "roughly concrete/wood". A carpeted
// basement and a tiled kitchen reflected identically because nothing about them
// differed. It also collected box and sphere colliders only, so brush solids
// and voxel caves -- which produce mesh colliders and nothing else -- were
// acoustically invisible.
void AudioEngine::BuildSteamAudioScene() {
    if (!m_SteamAudio || !m_SteamAudio->IsInitialized() || !m_World) return;

    const Audio::AcousticScene scene = Audio::BuildAcousticScene(m_World);
    if (scene.Empty()) {
        ENJIN_LOG_INFO(Audio, "Steam Audio: no colliders found for audio scene");
        return;
    }

    // The one conversion. AcousticProperties is deliberately the same shape as
    // IPLMaterial -- three absorption bands, scattering, three transmission
    // bands -- so this is a copy and not a translation with opinions in it.
    std::vector<IPLMaterial> materials;
    materials.reserve(scene.materials.Count());
    for (usize i = 0; i < scene.materials.Count(); ++i) {
        const Audio::AcousticProperties& p = scene.materials.At(i);
        IPLMaterial m{};
        m.absorption[0] = p.absorption[0];
        m.absorption[1] = p.absorption[1];
        m.absorption[2] = p.absorption[2];
        m.scattering = p.scattering;
        m.transmission[0] = p.transmission[0];
        m.transmission[1] = p.transmission[1];
        m.transmission[2] = p.transmission[2];
        materials.push_back(m);
    }

    std::vector<IPLint32> materialIndices(scene.materialIndices.begin(),
                                          scene.materialIndices.end());

    if (m_SteamAudio->BuildScene(scene.vertices, scene.indices, materials, materialIndices)) {
        ENJIN_LOG_INFO(Audio,
                       "Steam Audio: audio scene built (%zu verts, %zu tris, %zu materials)",
                       scene.vertices.size(), scene.TriangleCount(), materials.size());
        for (usize i = 0; i < scene.materials.Count(); ++i) {
            ENJIN_LOG_INFO(Audio, "Steam Audio:   material %zu = %s", i,
                           ECS::SurfaceMaterialName(scene.materials.SurfaceAt(i)));
        }
    }
}

void AudioEngine::RebuildAudioScene() {
    if (m_SteamAudio) {
        m_SteamAudio->DestroyScene();
    }
    BuildSteamAudioScene();
}
#endif

} // namespace Audio
} // namespace Enjin
