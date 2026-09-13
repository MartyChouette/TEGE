#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Audio/AudioBus.h"
#include "Enjin/Acoustics/EarlyReflections.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <random>

#ifdef ENJIN_AUDIO_STEAM_AUDIO
#include "Enjin/Audio/SteamAudioProcessor.h"
#endif

namespace Enjin {
namespace Audio {

// Audio clip handle
using AudioClipHandle = u32;
constexpr AudioClipHandle INVALID_AUDIO_CLIP = 0;

// Audio channel — controls volume mixing and diegetic behavior
// SFX/Music are typically diegetic (in-world) or non-diegetic (score/UI) respectively
enum class AudioChannel : u8 {
    SFX = 0,     // Sound effects (diegetic — in-world sounds, respects 3D spatialization)
    Music = 1,   // Background music / score (non-diegetic — always 2D, ignores listener position)
    UI = 2,      // UI / menu sounds (non-diegetic — always 2D, typically short one-shots)
    Voice = 3,   // Dialogue / voice lines (can be diegetic or non-diegetic depending on is3D)
    Count
};

// Sound instance (playing sound)
struct SoundInstance {
    AudioClipHandle clip = INVALID_AUDIO_CLIP;
    f32 volume = 1.0f;
    f32 pitch = 1.0f;
    f32 pan = 0.0f;        // -1 = left, 0 = center, 1 = right
    bool loop = false;
    bool is3D = false;
    AudioChannel channel = AudioChannel::SFX;
    Math::Vector3 position;
    f32 minDistance = 1.0f;
    f32 maxDistance = 500.0f;

    // State
    bool isPlaying = false;
    f32 playbackPosition = 0.0f;

    // Opaque pointer to ma_sound (owned by this instance, heap-allocated)
    void* maSound = nullptr;

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    void* binauralNode = nullptr;  // Custom ma_node for HRTF processing
#endif
};

// SoundHandle may already be defined by AudioSystem.h — guard against redefinition
#ifndef ENJIN_SOUND_HANDLE_DEFINED
#define ENJIN_SOUND_HANDLE_DEFINED
using SoundHandle = u32;
constexpr SoundHandle INVALID_SOUND = 0;
#endif

// Simple audio manager — uses miniaudio for cross-platform audio playback
class ENJIN_API AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool Initialize();
    void Shutdown();

    void SetWorld(ECS::World* world) { m_World = world; }

    // Root for resolving relative clip paths (the project directory). The
    // process CWD is the exe dir, so project-relative paths like
    // "assets/sfx/x.wav" never resolve without this. Empty = CWD (legacy).
    void SetAssetRoot(const std::string& root) { m_AssetRoot = root; }

    // Set listener position (usually camera)
    // Environmental reverb (Freeverb bus). Spatialized sounds route through it;
    // 2D/UI/music stay dry. Zone systems set the target each frame - the DSP
    // reads smoothed values on the audio thread. wetDry 0 = bypass.
    void SetEnvironmentReverb(f32 wetDry, f32 roomSize, f32 damping, f32 decayTime, f32 preDelay);

    // Hand the bus a room that was MEASURED rather than authored.
    //
    // rt60 is per band in seconds, meanFreePath in metres, reflectedEnergy the
    // fraction that came back at all. While a measurement is set, the bus runs
    // a feedback delay network configured from it and the decay is the decay
    // that was measured. Without one it stays on Freeverb, so every scene
    // authored so far sounds exactly as it does today -- measurement wins where
    // there is one, authoring answers where there is not.
    //
    // wetDry still comes from SetEnvironmentReverb: how much room you hear is a
    // mixing decision, not a property of the room.
    void SetMeasuredRoom(const f32 rt60[3], f32 meanFreePath, f32 reflectedEnergy);

    // The discrete reflections off named surfaces, as opposed to the diffuse
    // tail behind them.
    //
    // This is what makes a room a PLACE rather than an amount of reverb. The
    // tail tells you how live the room is; the first handful of reflections
    // tell you a wall is a metre behind the thing making the sound, and which
    // wall, and what it is made of -- a tiled wall returns the top of the clap
    // almost intact, a curtain returns the bottom of it and nothing else.
    //
    // Without these the engine had one diffuse wash whose only variable was
    // decay time, which is why every room sounded like the same room turned up
    // or down. The tracing code and its tests existed for a while before
    // anything called them, so the tail was all anybody ever heard.
    void SetMeasuredReflections(const Acoustics::EarlyReflectionResult& reflections);

    // Give ONE sound its own reflection pattern, traced from where it actually
    // is rather than from the listener.
    //
    // The shared bus renders reflections traced with the source at the
    // listener's own position, which is exactly right for a clap at your feet
    // and increasingly wrong the further away a sound is: which walls answer,
    // and how long after the direct sound, are properties of the path from THAT
    // source to your ears. A handful of sounds get the real thing.
    //
    // Slots are limited (each holds a quarter-second stereo delay line) and are
    // claimed on a first-come basis; a sound that cannot get one keeps routing
    // through the shared pattern, which is a reasonable approximation and not a
    // failure. Returns whether this sound holds a slot.
    bool SetSourceReflections(SoundHandle sound,
                              const Acoustics::EarlyReflectionResult& reflections);

    // Hand a slot back. Called when a sound stops, and when it is no longer
    // close enough to be worth one.
    void ReleaseSourceReflections(SoundHandle sound);

    // How many sounds currently hold their own pattern, and how many slots
    // exist. Reported because "my footsteps sound wrong across the room" and
    // "that source never got a slot" are the same symptom otherwise.
    u32 SourceReflectionSlotsInUse() const;
    u32 SourceReflectionSlotCount() const;
    void ClearMeasuredRoom();
    bool HasMeasuredRoom() const { return m_HasMeasuredRoom; }

    // Whether the environmental reverb bus actually exists in this build.
    //
    // Worth asking out loud because for the whole life of the feature the
    // answer was no, and nothing said so: the bus was created inside an
    // OFF-by-default CMake option, so every call that fed it returned at its
    // first line and every scene played dry. A capability that can be compiled
    // out needs a way to be asked about, or "the reverb sounds wrong" and
    // "there is no reverb" stay indistinguishable from the outside.
    bool HasReverbBus() const;

    void SetListenerPosition(const Math::Vector3& position, const Math::Vector3& forward, const Math::Vector3& up);

    // What one play of a source sounds like: which of its clips, and the pitch
    // and volume after the authored random ranges are applied.
    struct PlayVariation {
        std::string clipPath;
        f32 pitch = 1.0f;
        f32 volume = 1.0f;
    };
    // Advances the source's no-repeat state, hence the non-const reference.
    PlayVariation ChooseVariation(ECS::AudioSourceComponent& src);

    // Load audio clip from file
    AudioClipHandle LoadClip(const std::string& filepath);

    // Unload audio clip
    void UnloadClip(AudioClipHandle clip);

    // S-L1: Remove clips that have no active sound instances referencing them
    void CleanupUnusedClips();

    // Play a 2D sound (no spatialization)
    SoundHandle Play(AudioClipHandle clip, f32 volume = 1.0f, f32 pitch = 1.0f, bool loop = false,
                     AudioChannel channel = AudioChannel::SFX);

    // Play 3D sound at position (diegetic — attenuates with distance)
    SoundHandle Play3D(AudioClipHandle clip, const Math::Vector3& position,
                       f32 volume = 1.0f, f32 minDist = 1.0f, f32 maxDist = 500.0f,
                       AudioChannel channel = AudioChannel::SFX);

    // Play one-shot (fire and forget)
    void PlayOneShot(AudioClipHandle clip, f32 volume = 1.0f, AudioChannel channel = AudioChannel::SFX);
    void PlayOneShot3D(AudioClipHandle clip, const Math::Vector3& position, f32 volume = 1.0f);

    // Control playing sounds
    void Stop(SoundHandle sound);
    void StopAll();
    void Pause(SoundHandle sound);
    void Resume(SoundHandle sound);
    void SetVolume(SoundHandle sound, f32 volume);
    void SetPitch(SoundHandle sound, f32 pitch);
    void SetPosition(SoundHandle sound, const Math::Vector3& position);

    // Query
    bool IsPlaying(SoundHandle sound) const;

    // Is the output device actually running?
    //
    // On desktop this is true from Initialize onward. In a BROWSER it is false
    // until the page has seen a real user gesture: every AudioContext starts
    // suspended, and Chrome says so in the console --  "The AudioContext was
    // not allowed to start. It must be resumed (or created) after a user
    // gesture on the page." Measured with tools/web_audio_probe.mjs against an
    // exported build: state=suspended and currentTime frozen at 0 before a
    // click, running and advancing after one.
    //
    // Anything played while this is false is played into a stopped device.
    bool IsDeviceRunning() const;

    // Call from the first real user input on web. Starts the device, which is
    // what resumes the AudioContext, and releases any play-on-awake sources
    // that were held back waiting for it. A no-op everywhere else, and a no-op
    // once the device is already running, so it is safe to call on every input.
    void ResumeAfterUserGesture();

    // Where a playing sound is, in seconds, and how long it is. Both return -1
    // when the sound is unknown or the backend cannot answer -- a real 0.0 is
    // "at the start", which a caller has to be able to tell apart from "no
    // idea". Seek returns whether it happened.
    //
    // Asked for by Tune_In, which is a game about listening to radio
    // broadcasts: with no way to ask where the audio is, timed subtitles have
    // to dead-reckon from the frame Play was called and hardcode the clip
    // length, and any drift or engine-side loop restart desyncs the captions
    // permanently with nothing able to detect it. IsPlaying was the only
    // observability a script had.
    f32 GetPlaybackTime(SoundHandle sound) const;
    f32 GetLength(SoundHandle sound) const;
    bool Seek(SoundHandle sound, f32 seconds);

    // Master volume (affects all channels)
    void SetMasterVolume(f32 volume);
    f32 GetMasterVolume() const { return m_MasterVolume; }

    // Per-channel volume (multiplied with master volume)
    void SetChannelVolume(AudioChannel channel, f32 volume);
    f32 GetChannelVolume(AudioChannel channel) const;

    // Stop all sounds on a specific channel
    void StopChannel(AudioChannel channel);

    // Update (call every frame to update 3D audio, fade-outs, etc.)
    void Update(f32 deltaTime);

    // Update ECS audio sources
    void UpdateAudioSources(f32 deltaTime);

    // Callback for accessibility audio visual indicators (Task #38)
    // Called whenever a sound is played, with the clip filepath as label
    using SoundPlayedCallback = std::function<void(const std::string& soundName)>;
    void SetOnSoundPlayed(SoundPlayedCallback cb) { m_OnSoundPlayed = std::move(cb); }
    const SoundPlayedCallback& GetOnSoundPlayed() const { return m_OnSoundPlayed; }

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    // HRTF binaural audio (requires Steam Audio SDK)
    void SetHRTFEnabled(bool enabled);
    bool IsHRTFEnabled() const;
    bool IsHRTFAvailable() const;

    // Phase 2: Occlusion & Transmission
    void SetOcclusionEnabled(bool enabled);
    bool IsOcclusionEnabled() const;
    void SetTransmissionEnabled(bool enabled);
    bool IsTransmissionEnabled() const;
    void BuildSteamAudioScene();   // Build scene geometry from ECS colliders
    void RebuildAudioScene();      // Force rebuild (e.g., after scene change)
#endif

private:
    f32 Calculate3DVolume(const Math::Vector3& soundPos, f32 minDist, f32 maxDist) const;
    f32 EffectiveVolume(f32 instanceVolume, AudioChannel channel) const;
    void CleanupSound(SoundInstance& sound);

    // pImpl for miniaudio engine
    struct Impl;
    std::unique_ptr<Impl> m_Impl;

    ECS::World* m_World = nullptr;
    std::string m_AssetRoot;
    bool m_HasMeasuredRoom = false;

    // Listener (camera) state
    Math::Vector3 m_ListenerPosition;

    // Web only: true from Initialize until a user gesture has let the browser
    // run the AudioContext. It exists because miniaudio cannot answer the
    // question -- ma_device_start returns MA_SUCCESS and the device reports
    // itself STARTED while the browser has quietly refused to resume the
    // context, so the device state says "running" and nothing is audible.
    // Measured: with the check on device state alone, a play-on-awake source
    // still loaded and started its clip before any click.
    bool m_WebAudioGated = false;

    // One generator for clip/pitch/volume variation. Seeded once: reseeding per
    // play from the clock gives runs of identical picks at frame rate.
    std::mt19937 m_Rng{std::random_device{}()};
    Math::Vector3 m_ListenerForward = Math::Vector3(0, 0, -1);
    Math::Vector3 m_ListenerUp = Math::Vector3(0, 1, 0);

    f32 m_MasterVolume = 1.0f;
    f32 m_ChannelVolumes[static_cast<usize>(AudioChannel::Count)] = {1.0f, 1.0f, 1.0f, 1.0f};

    // Loaded audio clip data
    struct AudioClipData {
        std::string filepath;
        std::vector<u8> pcmData;       // Raw PCM samples
        u32 sampleRate = 44100;
        u16 channels = 1;
        u16 bitsPerSample = 16;
        f32 duration = 0.0f;           // Duration in seconds
        bool loaded = false;           // True if PCM data is loaded
    };

    bool LoadWAV(const std::string& filepath, AudioClipData& clip);

    std::unordered_map<AudioClipHandle, AudioClipData> m_Clips;
    AudioClipHandle m_NextClipHandle = 1;

    // Playing sounds
    std::unordered_map<SoundHandle, SoundInstance> m_Sounds;
    SoundHandle m_NextSoundHandle = 1;

    bool m_Initialized = false;

    // Accessibility callback
    SoundPlayedCallback m_OnSoundPlayed;

public:
    // Audio bus mixer (hierarchical volume routing)
    AudioMixer& GetMixer() { return m_Mixer; }
    const AudioMixer& GetMixer() const { return m_Mixer; }

    // Music crossfader
    MusicCrossfader& GetCrossfader() { return m_Crossfader; }

private:
    AudioMixer m_Mixer;
    MusicCrossfader m_Crossfader;

#ifdef ENJIN_AUDIO_STEAM_AUDIO
    std::unique_ptr<SteamAudioProcessor> m_SteamAudio;
    bool m_HRTFEnabled = true;
    bool m_OcclusionEnabled = true;
    bool m_TransmissionEnabled = true;
    f32 m_OcclusionTimer = 0.0f;
#endif
};

} // namespace Audio
} // namespace Enjin
