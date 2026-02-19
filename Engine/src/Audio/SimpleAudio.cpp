#include "Enjin/Audio/SimpleAudio.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cstring>
#include <fstream>
#include <unordered_set>

#include "miniaudio.h"

#ifdef ENJIN_AUDIO_STEAM_AUDIO
#include "Enjin/Audio/SteamAudioProcessor.h"

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

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    // Initialize Steam Audio HRTF processor
    if (m_HRTFEnabled) {
        m_SteamAudio = std::make_unique<SteamAudioProcessor>();
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

void SimpleAudio::Shutdown() {
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

void SimpleAudio::SetChannelVolume(AudioChannel channel, f32 volume) {
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

#ifdef ENJIN_AUDIO_STEAM_AUDIO
void SimpleAudio::SetHRTFEnabled(bool enabled) {
    m_HRTFEnabled = enabled;
    if (m_SteamAudio) {
        m_SteamAudio->SetEnabled(enabled);
    }
}

bool SimpleAudio::IsHRTFEnabled() const {
    return m_HRTFEnabled && m_SteamAudio && m_SteamAudio->IsInitialized();
}

bool SimpleAudio::IsHRTFAvailable() const {
    return m_SteamAudio && m_SteamAudio->IsInitialized();
}
#endif

} // namespace Audio
} // namespace Enjin
