#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/GUI/DialogueTree.h"
#include "Enjin/Gameplay/StateRingBuffer.h"
#include "Enjin/Gameplay/RewindChannel.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <cctype>

namespace Enjin {
namespace ECS {

// Tag: runtime-spawned entity that must NEVER be serialized into a scene
// (generated tree colliders, pooled spawns, sim proxies). SceneSerializer
// skips tagged entities, so a save taken mid-play (MCP save_scene, script
// SaveScene) can't leak runtime junk into the authored scene file.
struct TransientComponent {
    u8 reserved = 0;   // tag component; no data
};

// ============================================================================
// HEALTH & DAMAGE SYSTEM
// ============================================================================

// Health component - tracks HP and handles damage/healing
struct HealthComponent {
    f32 maxHealth = 100.0f;
    f32 currentHealth = 100.0f;
    f32 regenRate = 0.0f;          // HP per second (0 = no regen)
    f32 regenDelay = 3.0f;         // Seconds after damage before regen starts
    f32 timeSinceLastDamage = 0.0f;

    bool isDead = false;
    bool isInvulnerable = false;
    f32 invulnerabilityTime = 0.0f;  // Seconds of invulnerability after hit
    f32 invulnerabilityTimer = 0.0f;

    // Shield (absorbs damage before health)
    f32 maxShield = 0.0f;
    f32 currentShield = 0.0f;
    f32 shieldRegenRate = 0.0f;
    f32 shieldRegenDelay = 5.0f;

    // Events (entity IDs to notify)
    Entity onDamageNotify = 0;   // INVALID_ENTITY
    Entity onDeathNotify = 0;
    Entity onHealNotify = 0;

    // Helper methods
    f32 GetHealthPercent() const { return maxHealth > 0 ? currentHealth / maxHealth : 0; }
    f32 GetShieldPercent() const { return maxShield > 0 ? currentShield / maxShield : 0; }
    bool IsFullHealth() const { return currentHealth >= maxHealth; }
};

// Damage component - apply to projectiles, hazards, etc.
struct DamageComponent {
    f32 damage = 10.0f;
    f32 knockbackForce = 0.0f;
    bool destroyOnHit = true;      // Destroy this entity after dealing damage
    bool damageOnce = true;        // Only damage once per entity
    f32 damageInterval = 0.0f;     // For continuous damage (lava, poison)

    // Damage type for resistances/weaknesses
    enum class DamageType : u8 {
        Physical,
        Fire,
        Ice,
        Electric,
        Poison,
        Magic
    };
    DamageType type = DamageType::Physical;

    // Tracking for damageOnce
    std::vector<Entity> damagedEntities;
};

// ============================================================================
// PHYSICS & COLLISION
// ============================================================================

// Rigidbody - physics simulation
struct RigidbodyComponent {
    f32 mass = 1.0f;
    f32 drag = 0.0f;               // Linear drag (air resistance)
    f32 angularDrag = 0.05f;       // Angular drag
    bool useGravity = true;
    f32 gravityScale = 1.0f;

    // Velocity
    Math::Vector3 velocity = Math::Vector3(0, 0, 0);
    Math::Vector3 angularVelocity = Math::Vector3(0, 0, 0);

    // Stability: max velocity magnitudes (prevents physics explosions)
    f32 maxVelocity = 100.0f;         // m/s — clamped each frame
    f32 maxAngularVelocity = 50.0f;   // rad/s — clamped each frame

    // Constraints
    bool freezePositionX = false;
    bool freezePositionY = false;
    bool freezePositionZ = false;
    bool freezeRotationX = false;
    bool freezeRotationY = false;
    bool freezeRotationZ = false;

    // Physics type
    enum class BodyType : u8 {
        Dynamic,    // Fully simulated
        Kinematic,  // Moved by code, affects dynamic bodies
        Static      // Never moves
    };
    BodyType bodyType = BodyType::Dynamic;

    // Collision detection mode
    enum class CollisionMode : u8 {
        Discrete,      // Standard collision detection
        Continuous,    // Prevents fast objects from passing through
        ContinuousSpeculative  // Fastest continuous mode
    };
    CollisionMode collisionMode = CollisionMode::Discrete;

    bool isGrounded = false;
    bool isSleeping = false;

    // Networking: when true, this body is driven by the network (a remote-owned entity). The
    // physics backend treats it as KINEMATIC and moves it to the ECS transform the snapshot
    // stream writes each step, and does NOT sim it back into the ECS — otherwise the local
    // simulation and the network overwrite fight each other. Set by NetworkSystem on non-owned
    // networked entities; false by default so single-player physics is completely unchanged.
    bool networkControlled = false;
};

// Box Collider
struct BoxColliderComponent {
    Math::Vector3 center = Math::Vector3(0, 0, 0);
    Math::Vector3 size = Math::Vector3(1, 1, 1);
    bool isTrigger = false;  // If true, doesn't block movement

    // Physics material
    f32 friction = 0.5f;
    f32 bounciness = 0.0f;

    // Collision filtering (bitmask system)
    u32 categoryBits = 1;            // Bit 0 = "Default" group
    u32 collisionMask = 0xFFFFFFFF;  // Collides with all groups by default
};

// Sphere Collider
struct SphereColliderComponent {
    Math::Vector3 center = Math::Vector3(0, 0, 0);
    f32 radius = 0.5f;
    bool isTrigger = false;

    f32 friction = 0.5f;
    f32 bounciness = 0.0f;

    u32 categoryBits = 1;
    u32 collisionMask = 0xFFFFFFFF;
};

// Capsule Collider (for characters)
struct CapsuleColliderComponent {
    Math::Vector3 center = Math::Vector3(0, 0, 0);
    f32 radius = 0.5f;
    f32 height = 2.0f;

    enum class Direction : u8 { X, Y, Z };
    Direction direction = Direction::Y;

    bool isTrigger = false;
    f32 friction = 0.5f;
    f32 bounciness = 0.0f;

    u32 categoryBits = 1;
    u32 collisionMask = 0xFFFFFFFF;
};

// Mesh Collider — generates collision shape from entity's mesh vertices.
// STATIC bodies always cook the exact triangle mesh (a convex hull of concave
// level geometry is a dome over every valley — characters floated on it).
struct MeshColliderComponent {
    bool convex = true;           // DYNAMIC bodies only: true = convex hull (Jolt requires convex for moving bodies)
    bool autoGenerate = true;     // auto-generate from MeshComponent on first use
    bool generated = false;       // internal: set after vertices are populated
    bool isTrigger = false;

    // Physics material
    f32 friction = 0.5f;
    f32 bounciness = 0.0f;

    // Collision filtering
    u32 categoryBits = 1;
    u32 collisionMask = 0xFFFFFFFF;

    // Cached collision geometry (populated from MeshComponent or manually)
    std::vector<Math::Vector3> vertices;
    std::vector<u32> indices;  // used for triangle mesh mode
};

// Trigger Zone - fires events when entities enter/exit
struct TriggerZoneComponent {
    enum class Shape : u8 { Box, Sphere };
    Shape shape = Shape::Box;

    // Box shape
    Math::Vector3 boxSize = Math::Vector3(2, 2, 2);

    // Sphere shape
    f32 sphereRadius = 1.0f;

    // Filtering
    u32 triggerMask = 0xFFFFFFFF;  // Which layers can trigger
    bool triggerOnce = false;      // Only trigger once ever
    bool hasTriggered = false;

    // Entities currently inside
    std::vector<Entity> entitiesInside;

    // Events
    Entity onEnterNotify = 0;
    Entity onExitNotify = 0;
    Entity onStayNotify = 0;
};

// ============================================================================
// AUDIO
// ============================================================================

// Audio channel — controls diegetic vs non-diegetic behavior and independent volume
// Matches Audio::AudioChannel enum values (kept here to avoid header dependency)
enum class AudioChannel : u8 {
    SFX = 0,     // Sound effects (diegetic — in-world, respects 3D spatialization)
    Music = 1,   // Background music / score (non-diegetic — always 2D, ignores listener)
    UI = 2,      // Menu / interface sounds (non-diegetic — always 2D, short one-shots)
    Voice = 3,   // Dialogue / voice (diegetic or non-diegetic depending on is3D)
    Count
};

// Audio Source - plays sounds
struct AudioSourceComponent {
    std::string clipPath;          // Path to audio file
    f32 volume = 1.0f;
    f32 pitch = 1.0f;
    f32 minDistance = 1.0f;        // Distance at full volume
    f32 maxDistance = 500.0f;      // Distance at zero volume (or min volume)

    bool playOnAwake = false;
    bool loop = false;
    bool is3D = true;              // Spatial audio vs 2D

    // Audio channel (SFX, Music, UI, Voice)
    // Music and UI channels force non-diegetic playback (2D, no spatialization)
    AudioChannel channel = AudioChannel::SFX;

    // Spatial blend (0 = 2D, 1 = 3D)
    f32 spatialBlend = 1.0f;

    // Rolloff mode
    enum class Rolloff : u8 {
        Logarithmic,
        Linear,
        Custom
    };
    Rolloff rolloff = Rolloff::Logarithmic;

    // State
    bool isPlaying = false;
    f32 playbackPosition = 0.0f;
    u32 soundHandle = 0;        // Handle from SimpleAudio (0 = invalid)
    bool awakeTriggered = false; // Whether playOnAwake has fired

    // Priority (lower = higher priority when too many sounds)
    i32 priority = 128;

    // Sound randomization — per-play variation for natural-sounding effects
    f32 pitchMin = 1.0f;               // Random pitch range minimum
    f32 pitchMax = 1.0f;               // Random pitch range maximum (1.0 = no variation)
    f32 volumeMin = 1.0f;              // Random volume range minimum
    f32 volumeMax = 1.0f;              // Random volume range maximum
    std::vector<std::string> clipVariations;  // Alternative clips chosen randomly
    bool noRepeat = true;              // Avoid repeating the same clip twice
    u32 lastPlayedIndex = 0;           // Runtime: tracks last clip for no-repeat

    // Sound pooling — pre-allocate voices for rapid fire SFX
    bool usePooling = false;
    u32 poolSize = 8;

    // Accessibility: description of this sound for audio-impaired users
    std::string audioDescription;
};

// Audio Listener - the "ears" of the scene
struct AudioListenerComponent {
    bool isActive = true;
    f32 volumeScale = 1.0f;
};

// ============================================================================
// ADVANCED AUDIO — Reverb zones, ambient layers, music zones, snapshots, lip sync
// ============================================================================

// Reverb zone — applies reverb effect when listener is inside the volume.
// Follows the same shape/priority/blend pattern as PostProcessVolumeComponent.
struct ReverbZoneComponent {
    enum class Shape : u8 { Box, Sphere };
    Shape shape = Shape::Box;
    Math::Vector3 halfExtents = Math::Vector3(5.0f);
    i32 priority = 0;
    bool isActive = true;
    bool isGlobal = false;           // Scene-wide default reverb
    f32 blendRadius = 2.0f;          // Fade-in distance outside the shape

    // Reverb parameters
    f32 roomSize = 0.5f;             // 0 = tiny, 1 = huge
    f32 damping = 0.5f;              // High-frequency absorption (0 = bright, 1 = muffled)
    f32 wetDryMix = 0.3f;            // 0 = dry, 1 = fully wet
    f32 decayTime = 1.5f;            // Seconds of reverb tail
    f32 preDelay = 0.02f;            // Gap before first reflection

    // Preset (overrides individual params when set)
    enum class Preset : u8 {
        Custom, SmallRoom, LargeRoom, Hall, Cathedral, Cave, Outdoors, Bathroom, UnderWater
    };
    Preset preset = Preset::Custom;
};

// Ambient sound layer — layered environmental audio with conditions.
struct AmbientSoundLayerComponent {
    struct Layer {
        std::string clipPath;
        f32 volume = 1.0f;
        f32 pitch = 1.0f;
        bool loop = true;
        std::string caption;         // Accessibility: "[Birds chirping]", "[Water flowing]"

        // Conditions (layer only plays when ALL conditions are met)
        f32 minTimeOfDay = 0.0f;     // 0-24 hours (0 = always)
        f32 maxTimeOfDay = 24.0f;
        f32 minWeatherIntensity = 0.0f;
        f32 maxWeatherIntensity = 1.0f;
    };

    std::vector<Layer> layers;

    // Zone shape
    Math::Vector3 halfExtents = Math::Vector3(10.0f);
    f32 blendRadius = 5.0f;
    bool isActive = true;

    // Runtime
    std::vector<u32> layerHandles;   // Sound handles per active layer
};

// Music zone — triggers music crossfade when listener enters.
struct MusicZoneComponent {
    std::string trackPath;
    f32 fadeInTime = 2.0f;
    f32 fadeOutTime = 2.0f;
    i32 priority = 0;
    Math::Vector3 halfExtents = Math::Vector3(10.0f);
    f32 blendRadius = 3.0f;
    bool isActive = true;
};

// Audio snapshot trigger — push/pop a snapshot when listener enters/exits.
struct AudioSnapshotTriggerComponent {
    std::string snapshotName;        // Name of the snapshot to push (e.g., "Dialogue", "Combat")
    Math::Vector3 halfExtents = Math::Vector3(5.0f);
    bool isActive = true;

    // Runtime
    bool listenerInside = false;     // Tracks enter/exit state
};

// Viseme enum — standard 15-viseme set (compatible with Oculus/Meta LipSync)
enum class Viseme : u8 {
    Silent = 0, PP, FF, TH, DD, KK, CH, SS, NN, RR, AA, EE, IH, OH, OU, Count
};

// Lip sync — drives mouth shapes from audio playback.
struct LipSyncComponent {
    // Viseme data (time → viseme mapping)
    struct VisemeKey {
        f32 time = 0.0f;
        Viseme viseme = Viseme::Silent;
        f32 weight = 1.0f;
    };
    std::vector<VisemeKey> visemeData;

    f32 blendSpeed = 10.0f;          // Interpolation speed between visemes
    bool autoFromAmplitude = true;   // Fallback: amplitude-based mouth open/close

    // Viseme → morph target mapping
    // Maps each viseme to one or more morph target names + weights.
    // E.g., Viseme::AA → "jawOpen" at 0.8 + "mouthOpen" at 0.6
    struct VisemeMorphMapping {
        std::string morphTargetName;
        f32 weight = 1.0f;
    };
    std::vector<VisemeMorphMapping> visemeMorphMap[static_cast<usize>(Viseme::Count)];

    // Auto-map: try to detect standard morph target names on first use
    bool autoMapVisemes = true;
    bool autoMapped = false;         // Runtime: has auto-mapping been attempted?

    // Runtime state
    Viseme currentViseme = Viseme::Silent;
    f32 currentWeight = 0.0f;
    Entity linkedAudioSource = 0;    // Entity with AudioSourceComponent to track
    f32 amplitudeSmoothed = 0.0f;    // Smoothed audio amplitude for auto mode
};

// Sound occlusion — muffle audio behind walls via low-pass filter.
struct AudioOcclusionComponent {
    bool enabled = true;
    f32 lowPassCutoff = 800.0f;      // Frequency cutoff when fully occluded
    f32 volumeReduction = 0.3f;      // Additional volume reduction when occluded
    f32 updateRate = 10.0f;          // Raycast check frequency (Hz)

    // Runtime
    f32 occlusionAmount = 0.0f;      // 0 = clear, 1 = fully occluded
    f32 updateTimer = 0.0f;
};

// ============================================================================
// AUDIO-REACTIVE SYSTEMS — Sound drives visuals, game params drive sound
// ============================================================================

// Target property that an audio signal or RTPC can drive
enum class AudioTargetProperty : u8 {
    LightIntensity,       // LightComponent::intensity
    LightColor,           // LightComponent::color (modulates RGB uniformly)
    EmissiveStrength,     // MaterialComponent::emissiveStrength
    Scale,                // TransformComponent::scale (uniform)
    Opacity,              // MaterialComponent push constant opacity
    ParticleRate,         // ParticleEmitterComponent::emissionRate
    CameraShake,          // Adds shake offset to camera position
    Custom                // Fires event with value for script handling
};

// AudioReactiveComponent — maps a bus VU level to an entity property.
// "When the Explosions bus is loud, make this light bright."
struct AudioReactiveComponent {
    std::string busName = "SFX";     // Which bus to read VU from
    AudioTargetProperty target = AudioTargetProperty::LightIntensity;

    f32 threshold = 0.1f;            // Minimum VU level before effect starts
    f32 multiplier = 2.0f;           // Scale factor applied to VU level
    f32 smoothing = 10.0f;           // Interpolation speed (higher = snappier)
    bool invert = false;             // Invert the effect (loud = dim instead of bright)

    f32 baseValue = 1.0f;            // Property value when silent
    f32 maxValue = 5.0f;             // Property value at full VU

    // Runtime
    f32 currentValue = 0.0f;         // Smoothed current output
    bool enabled = true;
};

// AudioThresholdTriggerComponent — fires a one-shot event when bus exceeds level.
// "When Explosions bus > 0.8, flicker lights for 0.5s."
struct AudioThresholdTriggerComponent {
    std::string busName = "SFX";
    f32 threshold = 0.7f;            // VU level that triggers
    f32 cooldown = 0.5f;             // Minimum seconds between triggers

    // What to do when triggered
    enum class Action : u8 {
        FlickerLights,               // Rapidly oscillate light intensity
        CameraShake,                 // Shake camera by shakeAmount
        ParticleBurst,               // Emit burstCount particles
        FireEvent                    // Broadcast named event via EntityEventBus
    };
    Action action = Action::FlickerLights;

    f32 effectDuration = 0.3f;       // How long the effect lasts
    f32 effectIntensity = 1.0f;      // Strength multiplier
    std::string eventName;           // For Action::FireEvent

    // Runtime
    f32 cooldownTimer = 0.0f;
    f32 effectTimer = 0.0f;
    bool triggered = false;
    bool enabled = true;
};

// RTPCComponent — Real-Time Parameter Control.
// Named game parameters continuously drive audio properties.
// "As player speed increases, engine pitch rises."
struct RTPCComponent {
    struct Mapping {
        std::string parameterName;   // Game parameter (e.g., "speed", "health", "danger")
        f32 paramMin = 0.0f;         // Parameter range minimum
        f32 paramMax = 1.0f;         // Parameter range maximum

        enum class AudioTarget : u8 {
            Volume, Pitch, LowPassCutoff, ReverbSend
        };
        AudioTarget audioTarget = AudioTarget::Volume;

        f32 outputMin = 0.0f;        // Audio output at paramMin
        f32 outputMax = 1.0f;        // Audio output at paramMax

        // Curve shape (0 = linear, <0 = ease in, >0 = ease out)
        f32 curve = 0.0f;
    };

    std::vector<Mapping> mappings;

    // Runtime parameter values (set by gameplay code or script)
    std::unordered_map<std::string, f32> parameters;

    void SetParameter(const std::string& name, f32 value) { parameters[name] = value; }
    f32 GetParameter(const std::string& name) const {
        auto it = parameters.find(name);
        return (it != parameters.end()) ? it->second : 0.0f;
    }

    bool enabled = true;
};

// BeatClockComponent — global BPM clock for rhythm-synced effects.
// Attach to a single manager entity. All BeatSyncComponents read from this.
struct BeatClockComponent {
    f32 bpm = 120.0f;                // Beats per minute
    i32 beatsPerBar = 4;             // Time signature numerator
    bool playing = true;

    // Runtime state
    f64 beatAccumulator = 0.0;       // Fractional beat position (0.0 to beatsPerBar)
    u32 currentBeat = 0;             // Current beat within bar (0 to beatsPerBar-1)
    u32 currentBar = 0;              // Current bar number
    u32 totalBeats = 0;              // Total beats since start
    bool beatThisFrame = false;      // True on the frame a beat occurs
    bool downbeatThisFrame = false;  // True on beat 0 of each bar

    f32 GetBeatPhase() const {       // 0.0-1.0 within current beat
        return static_cast<f32>(beatAccumulator - static_cast<f64>(currentBeat));
    }
};

// BeatSyncComponent — sync an entity's properties to the global beat.
// "This light pulses on every downbeat. These particles emit on every beat."
struct BeatSyncComponent {
    enum class SyncMode : u8 {
        EveryBeat,                   // Trigger on every beat
        EveryDownbeat,               // Trigger on beat 0 of each bar
        EveryNBeats,                 // Trigger every N beats
        Continuous                   // Smooth sine wave synced to beat phase
    };
    SyncMode mode = SyncMode::EveryBeat;
    u32 beatDivisor = 1;             // For EveryNBeats: trigger every N beats

    AudioTargetProperty target = AudioTargetProperty::LightIntensity;
    f32 baseValue = 1.0f;            // Value when not pulsing
    f32 pulseValue = 3.0f;           // Value on beat hit
    f32 decaySpeed = 8.0f;           // How fast pulse decays back to base (higher = snappier)

    // Runtime
    f32 currentValue = 0.0f;
    bool enabled = true;
};

// Per-bus parametric EQ — 3-band (low shelf, mid bell, high shelf)
struct AudioBusEQ {
    f32 lowFreq = 200.0f;            // Low shelf frequency (Hz)
    f32 lowGain = 0.0f;              // Low shelf gain (dB, -12 to +12)
    f32 midFreq = 1000.0f;           // Mid bell frequency (Hz)
    f32 midGain = 0.0f;              // Mid bell gain (dB)
    f32 midQ = 1.0f;                 // Mid bell Q factor (0.1 to 10)
    f32 highFreq = 5000.0f;          // High shelf frequency (Hz)
    f32 highGain = 0.0f;             // High shelf gain (dB)
    bool enabled = false;
};

// ============================================================================
// AUDIO FIDELITY — Sound chip emulation + aesthetic style matching
// ============================================================================

// AudioFidelityMode — matches visual art styles with audio processing
enum class AudioFidelityMode : u8 {
    Modern,         // Full fidelity — no processing
    LoFi,           // Vinyl crackle, slight distortion, warm low-pass
    Retro16Bit,     // 16-bit sample rate (22050 Hz), slight quantization
    Retro8Bit,      // 8-bit depth, 11025 Hz, aggressive quantization
    ChipTune,       // NES/Game Boy — square/triangle/noise channels only
    FMSynth,        // FM synthesis style (Sega Genesis / OPL)
    PSOne,          // PS1 era — 22050 Hz, reverb, compressed dynamic range
    Cassette        // Tape wobble, hiss, warm saturation, narrow stereo
};

// AudioFidelityComponent — attach to a game manager entity to apply
// global audio fidelity processing. Matches the visual ArtStyleComponent.
struct AudioFidelityComponent {
    AudioFidelityMode mode = AudioFidelityMode::Modern;
    f32 intensity = 1.0f;            // Blend between modern and styled (0=off, 1=full)
    bool autoMatchArtStyle = true;   // Auto-detect from ArtStyleComponent in scene

    // Per-mode parameters (adjusted by preset, editable by designer)
    f32 sampleRateReduction = 1.0f;  // 1.0 = native, 0.5 = half rate
    f32 bitDepthReduction = 1.0f;    // 1.0 = full, 0.5 = 8-bit style
    f32 lowPassCutoff = 20000.0f;    // Hz (lower = more muffled)
    f32 noiseFloor = 0.0f;           // Background noise/hiss level (0-0.1)
    f32 wobble = 0.0f;               // Pitch wobble amount (cassette/vinyl)
    f32 wobbleSpeed = 0.5f;          // Wobble speed in Hz
    f32 saturation = 0.0f;           // Warm distortion (0-1)
    f32 stereoWidth = 1.0f;          // 0 = mono, 1 = normal, <1 = narrow

    bool enabled = true;

    // Apply preset values for a mode
    void ApplyPreset(AudioFidelityMode m) {
        mode = m;
        switch (m) {
            case AudioFidelityMode::Modern:
                sampleRateReduction = 1.0f; bitDepthReduction = 1.0f;
                lowPassCutoff = 20000.0f; noiseFloor = 0.0f; wobble = 0.0f;
                saturation = 0.0f; stereoWidth = 1.0f;
                break;
            case AudioFidelityMode::LoFi:
                sampleRateReduction = 0.7f; bitDepthReduction = 0.8f;
                lowPassCutoff = 8000.0f; noiseFloor = 0.02f; wobble = 0.01f;
                wobbleSpeed = 0.3f; saturation = 0.3f; stereoWidth = 0.8f;
                break;
            case AudioFidelityMode::Retro16Bit:
                sampleRateReduction = 0.5f; bitDepthReduction = 0.7f;
                lowPassCutoff = 10000.0f; noiseFloor = 0.005f; wobble = 0.0f;
                saturation = 0.1f; stereoWidth = 1.0f;
                break;
            case AudioFidelityMode::Retro8Bit:
                sampleRateReduction = 0.25f; bitDepthReduction = 0.3f;
                lowPassCutoff = 5000.0f; noiseFloor = 0.01f; wobble = 0.0f;
                saturation = 0.2f; stereoWidth = 0.5f;
                break;
            case AudioFidelityMode::ChipTune:
                sampleRateReduction = 0.2f; bitDepthReduction = 0.15f;
                lowPassCutoff = 4000.0f; noiseFloor = 0.0f; wobble = 0.0f;
                saturation = 0.4f; stereoWidth = 0.0f; // Mono
                break;
            case AudioFidelityMode::FMSynth:
                sampleRateReduction = 0.4f; bitDepthReduction = 0.5f;
                lowPassCutoff = 8000.0f; noiseFloor = 0.005f; wobble = 0.0f;
                saturation = 0.5f; stereoWidth = 0.6f;
                break;
            case AudioFidelityMode::PSOne:
                sampleRateReduction = 0.5f; bitDepthReduction = 0.7f;
                lowPassCutoff = 9000.0f; noiseFloor = 0.008f; wobble = 0.005f;
                wobbleSpeed = 0.1f; saturation = 0.15f; stereoWidth = 0.7f;
                break;
            case AudioFidelityMode::Cassette:
                sampleRateReduction = 0.6f; bitDepthReduction = 0.6f;
                lowPassCutoff = 7000.0f; noiseFloor = 0.04f; wobble = 0.03f;
                wobbleSpeed = 0.5f; saturation = 0.4f; stereoWidth = 0.6f;
                break;
        }
    }
};

// ============================================================================
// MIDI BINDING — Map MIDI CC/notes to entity properties
// ============================================================================

// MIDIBindingComponent — turn a MIDI controller into a live control surface.
// Map knobs (CC) to light intensity, faders to bus volumes, pads to trigger
// actions, keys to bone rotations or morph weights.
struct MIDIBindingComponent {
    struct Binding {
        enum class Source : u8 { CC, NoteVelocity, PitchBend };
        Source source = Source::CC;
        u8 midiCC = 0;              // CC number (0-127) or note number
        u8 midiChannel = 0;         // MIDI channel (0-15, 0xFF = any)

        // Target property
        AudioTargetProperty target = AudioTargetProperty::LightIntensity;
        std::string customTarget;    // For Custom target: morph target name, bus name, etc.

        // Mapping range
        f32 outputMin = 0.0f;        // Value when MIDI = 0
        f32 outputMax = 1.0f;        // Value when MIDI = 127
        f32 smoothing = 10.0f;       // Interpolation speed

        // Runtime
        f32 currentValue = 0.0f;
        f32 rawValue = 0.0f;
    };

    std::vector<Binding> bindings;
    bool enabled = true;
};

// ============================================================================
// CONDUCTOR — AI-driven dynamic music that responds to gameplay
// ============================================================================

// ConductorComponent — attach to a game manager entity.
// Watches gameplay state (combat, exploration, stealth, cutscene) and
// dynamically layers/removes music stems, adjusts reverb, shifts EQ.
struct ConductorComponent {
    // Music stems — each stem is a separate audio loop that can be faded in/out
    struct Stem {
        std::string clipPath;        // Audio file for this stem
        std::string name;            // Display name ("Strings", "Percussion", "Bass")
        f32 volume = 1.0f;           // Current volume (0-1, managed by conductor)
        f32 targetVolume = 0.0f;     // Target volume (smoothly interpolated)
        f32 fadeSpeed = 2.0f;        // Fade speed (units/sec)
        u32 soundHandle = 0;         // Runtime handle

        // Which gameplay states activate this stem
        bool playDuringExplore = true;
        bool playDuringCombat = false;
        bool playDuringStealth = false;
        bool playDuringCutscene = false;
    };

    std::vector<Stem> stems;

    // Gameplay state detection
    enum class GameplayState : u8 { Explore, Combat, Stealth, Cutscene };
    GameplayState currentState = GameplayState::Explore;
    GameplayState previousState = GameplayState::Explore;

    // Auto-detect state from scene (if enabled)
    bool autoDetect = true;
    f32 combatRadius = 20.0f;        // Radius to check for enemies with HealthComponent
    f32 stealthThreshold = 0.3f;     // Player speed below this = stealth
    f32 stateChangeDelay = 2.0f;     // Seconds before confirming state change (prevents flicker)
    f32 stateTimer = 0.0f;

    // Master settings
    f32 masterVolume = 1.0f;
    f32 crossfadeTime = 3.0f;        // Time to crossfade between states
    bool enabled = true;
};

// ============================================================================
// AUDIO COLLISION — Physics impacts auto-generate sound
// ============================================================================

// Surface material enum — shared between collision audio and material interaction
enum class SurfaceMaterial : u8 {
    Default, Metal, Wood, Stone, Glass, Flesh, Water, Dirt, Grass, Ice, Count
};

inline const char* SurfaceMaterialName(SurfaceMaterial m) {
    static const char* names[] = {"Default","Metal","Wood","Stone","Glass","Flesh","Water","Dirt","Grass","Ice"};
    return (static_cast<u8>(m) < 10) ? names[static_cast<u8>(m)] : "Unknown";
}

// AudioCollisionComponent — TOTK-style physics audio.
// Every surface has a material type. Impacts auto-generate sound based on
// material combination, velocity, mass, and hollowness. Continuous contacts
// (scraping, rolling) play looping sounds that pitch-shift with velocity.
struct AudioCollisionComponent {
    SurfaceMaterial material = SurfaceMaterial::Default;

    // Sound clips per impact type
    std::string impactSoftClip;      // Low-velocity impact (tap, nudge)
    std::string impactHardClip;      // High-velocity impact (slam, crash)
    std::string scrapeClip;          // Continuous sliding contact
    std::string rollClip;            // Continuous rolling contact

    // Thresholds
    f32 softThreshold = 0.5f;        // Min velocity for soft impact
    f32 hardThreshold = 3.0f;        // Min velocity for hard impact
    f32 volumeScale = 1.0f;          // Multiply velocity-derived volume
    f32 pitchVariance = 0.15f;       // Random pitch variation per impact
    f32 cooldown = 0.05f;            // Min seconds between impacts (prevents spam)

    // TOTK-inspired extensions
    f32 hollowness = 0.0f;           // 0 = solid, 1 = hollow (affects resonance pitch)
    f32 mass = 1.0f;                 // Object mass (heavier = lower pitch, louder)

    // Continuous contact (scrape/roll)
    f32 scrapeMinVelocity = 0.3f;    // Min velocity for continuous scrape sound
    f32 scrapePitchScale = 0.5f;     // How much velocity affects scrape pitch (0-1)

    // Distance-based detail culling (TOTK optimization)
    f32 detailDistance = 30.0f;      // Beyond this, use simplified sounds
    f32 cullDistance = 80.0f;        // Beyond this, skip entirely

    // Runtime
    f32 cooldownTimer = 0.0f;
    u32 scrapeSoundHandle = 0;       // Handle for active continuous scrape
    u32 rollSoundHandle = 0;         // Handle for active continuous roll
    f32 contactVelocity = 0.0f;      // Current contact sliding velocity
    bool inContact = false;          // Currently in sliding/rolling contact
    bool enabled = true;
};

// MaterialInteractionTable — defines what sound plays when material A hits material B.
// Attach to a game manager entity. Shared by all AudioCollisionComponents.
// If no table exists, falls back to the per-entity clip paths.
struct MaterialInteractionTableComponent {
    // Key: materialA * SurfaceMaterial::Count + materialB (symmetric: A×B == B×A)
    // Value: clip path for that interaction
    struct Interaction {
        SurfaceMaterial a = SurfaceMaterial::Default;
        SurfaceMaterial b = SurfaceMaterial::Default;
        std::string softClip;        // Soft impact sound
        std::string hardClip;        // Hard impact sound
        std::string scrapeClip;      // Continuous scrape
        f32 pitchOffset = 0.0f;      // Pitch shift for this combination
        f32 volumeMultiplier = 1.0f; // Volume adjustment
    };

    std::vector<Interaction> interactions;

    // Find the interaction for two materials (symmetric lookup)
    const Interaction* Find(SurfaceMaterial a, SurfaceMaterial b) const {
        for (const auto& i : interactions) {
            if ((i.a == a && i.b == b) || (i.a == b && i.b == a)) return &i;
        }
        return nullptr;
    }
};

// ============================================================================
// SIDECHAIN COMPRESSION — Per-sample bus ducking
// ============================================================================

// SidechainComponent — attach to an entity to duck one bus when another is active.
// "When Voice bus has signal, duck Music bus by -6dB over 200ms."
struct SidechainComponent {
    std::string sourceBus = "Voice"; // Bus that triggers the ducking
    std::string targetBus = "Music"; // Bus that gets ducked
    f32 threshold = 0.1f;            // Source bus level that triggers ducking
    f32 ratio = 0.3f;                // Target volume when ducking (0.3 = -10dB)
    f32 attackTime = 0.1f;           // Seconds to reach full duck
    f32 releaseTime = 0.5f;          // Seconds to release duck
    f32 holdTime = 0.2f;             // Seconds to hold duck after source drops below threshold

    // Runtime
    f32 currentGain = 1.0f;          // Current ducking multiplier (1.0 = no duck)
    f32 holdTimer = 0.0f;
    bool ducking = false;
    bool enabled = true;
};

// ============================================================================
// INTERACTION & ITEMS
// ============================================================================

// Interactable - can be interacted with by player
struct InteractableComponent {
    std::string promptText = "Press E to interact";
    f32 interactionRange = 2.0f;
    bool requiresLookAt = true;    // Must be looking at object
    f32 lookAtAngle = 45.0f;       // Degrees

    bool isEnabled = true;
    bool singleUse = false;
    bool hasBeenUsed = false;

    // Highlight when in range
    bool highlightOnHover = true;
    Math::Vector3 highlightColor = Math::Vector3(1, 1, 0);

    // Events
    Entity onInteractNotify = 0;
};

// Pickup Item - collectible items
struct PickupComponent {
    enum class PickupType : u8 {
        Health,
        Ammo,
        Coin,
        Key,
        Powerup,
        Custom
    };
    PickupType type = PickupType::Coin;

    f32 value = 1.0f;              // Amount to give
    std::string customId;          // For custom items

    f32 pickupRange = 1.0f;
    bool destroyOnPickup = true;
    bool magnetToPlayer = false;   // Auto-move toward player
    f32 magnetRange = 3.0f;
    f32 magnetSpeed = 10.0f;

    // Respawn
    bool canRespawn = false;
    f32 respawnTime = 10.0f;
    f32 respawnTimer = 0.0f;
    bool isCollected = false;

    // Visual feedback
    f32 bobSpeed = 2.0f;           // Floating bob animation
    f32 bobHeight = 0.2f;
    f32 rotationSpeed = 90.0f;     // Degrees per second
};

// Simple Inventory
struct InventoryComponent {
    struct InventorySlot {
        std::string itemId;
        i32 quantity = 0;
        i32 maxStack = 99;
    };

    std::vector<InventorySlot> slots;
    usize maxSlots = 20;

    // Currency
    i32 coins = 0;
    i32 gems = 0;

    // Keys
    std::vector<std::string> keys;
};

// ============================================================================
// AI & NAVIGATION
// ============================================================================

// Simple AI Controller
struct AIControllerComponent {
    enum class AIState : u8 {
        Idle,
        Patrol,
        Chase,
        Attack,
        Flee,
        Dead
    };
    AIState currentState = AIState::Idle;

    // Target
    Entity targetEntity = 0;       // Who to chase/attack
    Math::Vector3 targetPosition;

    // Detection
    f32 detectionRange = 10.0f;
    f32 attackRange = 2.0f;
    f32 loseTargetRange = 15.0f;
    f32 fieldOfView = 120.0f;      // Degrees

    // Movement
    f32 moveSpeed = 3.0f;
    f32 turnSpeed = 180.0f;
    f32 stoppingDistance = 1.0f;

    // Attack
    f32 attackCooldown = 1.0f;
    f32 attackTimer = 0.0f;
    f32 attackDamage = 10.0f;

    // Patrol
    std::vector<Math::Vector3> patrolPoints;
    usize currentPatrolIndex = 0;
    f32 patrolWaitTime = 2.0f;
    f32 patrolWaitTimer = 0.0f;
    bool patrolLoop = true;            // Loop patrol or ping-pong

    // Navigation
    bool useNavmesh = true;            // Use navmesh pathfinding (A*) when available
    f32 repathInterval = 0.5f;         // How often to recalculate path during chase (seconds)
    f32 arrivalRadius = 0.5f;          // Distance threshold for reaching a waypoint
    f32 chaseSpeed = 5.0f;             // Speed when chasing target
    f32 fleeSpeed = 6.0f;             // Speed when fleeing
    f32 fleeDistance = 15.0f;          // How far to flee before returning to idle

    // 2D mode: move on XY plane (not XZ), flip scale.x for facing instead of Y-rotation
    bool is2D = false;

    // State timers
    f32 stateTimer = 0.0f;

    // Debug visualization
    bool debugDrawPath = false;        // Draw current A* path
    bool debugDrawDetection = false;   // Draw detection range circle
};

// Follow Target - makes entity follow another entity
struct FollowTargetComponent {
    Entity target = 0;             // Entity to follow
    f32 followDistance = 3.0f;     // Desired distance from target
    f32 minDistance = 1.0f;        // Stop if closer than this
    f32 maxDistance = 20.0f;       // Give up if farther than this
    f32 moveSpeed = 5.0f;
    f32 smoothTime = 0.3f;         // Smoothing for movement

    bool matchTargetRotation = false;
    f32 rotationSpeed = 360.0f;

    Math::Vector3 offset = Math::Vector3(0, 0, 0);  // Offset from target
    bool useLocalOffset = false;   // Offset relative to target's rotation

    // State
    Math::Vector3 currentVelocity;
};

// Look At Target - makes entity rotate to face target
struct LookAtTargetComponent {
    Entity target = 0;
    Math::Vector3 worldTarget;     // Alternative: world position
    bool useWorldTarget = false;

    f32 rotationSpeed = 180.0f;    // Degrees per second
    bool instant = false;          // Instantly snap to target

    // Constraints
    bool constrainX = false;       // Don't rotate on X axis
    bool constrainY = false;
    bool constrainZ = true;        // Usually constrain Z (no roll)

    // Limits (degrees)
    f32 minYaw = -180.0f;
    f32 maxYaw = 180.0f;
    f32 minPitch = -89.0f;
    f32 maxPitch = 89.0f;
};

// Waypoint - marker for AI paths
struct WaypointComponent {
    std::string waypointId;
    i32 index = 0;                 // Order in path
    Entity nextWaypoint = 0;       // For linked list style

    f32 waitTime = 0.0f;           // Time to wait at this point
    f32 radius = 0.5f;             // Arrival threshold
};

// ============================================================================
// SPAWNING
// ============================================================================

// Spawn Point - where entities spawn
struct SpawnPointComponent {
    std::string spawnId;           // Identifier for this spawn point
    std::string prefabToSpawn;     // What to spawn here

    // Spawn settings
    bool spawnOnStart = false;
    f32 spawnDelay = 0.0f;
    f32 respawnTime = 0.0f;        // 0 = no respawn
    i32 maxSpawns = -1;            // -1 = unlimited
    i32 currentSpawns = 0;

    // Spawn area
    f32 spawnRadius = 0.0f;        // Random position within radius
    bool randomRotation = false;

    // State
    f32 spawnTimer = 0.0f;
    std::vector<Entity> spawnedEntities;
};

// ============================================================================
// TIMERS & EVENTS
// ============================================================================

// Timer component - for delayed actions
struct TimerComponent {
    f32 duration = 1.0f;
    f32 elapsed = 0.0f;
    bool isRunning = false;
    bool loop = false;
    bool autoStart = false;
    i32 loopCount = 0;             // Number of times looped

    // Event
    Entity onCompleteNotify = 0;

    // Helper
    f32 GetProgress() const { return duration > 0 ? elapsed / duration : 0; }
    f32 GetRemaining() const { return duration - elapsed; }
    bool IsComplete() const { return elapsed >= duration; }
};

// ============================================================================
// VISUAL EFFECTS
// ============================================================================

// Billboard - always faces camera
struct BillboardComponent {
    bool faceCamera = true;
    bool lockY = true;             // Only rotate on Y axis (like trees)
    f32 rotationOffset = 0.0f;     // Additional rotation in degrees
};

// Single particle instance (runtime only)
struct Particle {
    Math::Vector3 position;
    Math::Vector3 velocity;
    f32 lifetime = 0.0f;
    f32 maxLifetime = 1.0f;
    f32 size = 0.5f;
    f32 alpha = 1.0f;
    f32 rotation = 0.0f;           // Radians
    f32 rotationSpeed = 0.0f;      // Radians per second
    Math::Vector3 color = Math::Vector3(1, 1, 1);
};

// Runtime particle pool (not serialized, managed by ParticleSystem)
struct ParticlePool {
    std::vector<Particle> particles;
    u32 activeCount = 0;
    u32 maxParticles = 1024;
    f32 spawnAccumulator = 0.0f;
    f32 burstTimer = 0.0f;
    f32 systemAge = 0.0f;
    bool initialized = false;
};

// Particle Emitter Settings
struct ParticleEmitterComponent {
    bool isPlaying = false;
    bool playOnAwake = true;
    bool loop = true;

    // Emission
    f32 emissionRate = 10.0f;      // Particles per second
    i32 burstCount = 0;            // Instant burst particles
    f32 burstInterval = 0.0f;

    // Particle settings
    f32 lifetime = 2.0f;
    f32 lifetimeVariance = 0.5f;
    f32 startSpeed = 5.0f;
    f32 speedVariance = 1.0f;
    f32 startSize = 0.5f;
    f32 endSize = 0.1f;
    Math::Vector3 startColor = Math::Vector3(1, 1, 1);
    Math::Vector3 endColor = Math::Vector3(1, 1, 1);
    f32 startAlpha = 1.0f;
    f32 endAlpha = 0.0f;

    // Size curve (piecewise linear: start -> mid -> end)
    f32 sizeMid = -1.0f;           // -1 = auto interpolate between start and end

    // Speed curve multipliers over lifetime
    f32 speedMultiplierMid = 1.0f;
    f32 speedMultiplierEnd = 0.5f;

    // Rotation
    f32 startRotation = 0.0f;      // Radians
    f32 rotationVariance = 0.0f;
    f32 rotationSpeed = 0.0f;      // Radians per second
    f32 rotationSpeedVariance = 0.0f;

    // Shape
    enum class EmitterShape : u8 {
        Point,
        Sphere,
        Hemisphere,
        Cone,
        Box
    };
    EmitterShape shape = EmitterShape::Cone;
    f32 shapeRadius = 0.1f;
    f32 coneAngle = 30.0f;

    // Forces
    Math::Vector3 gravity = Math::Vector3(0, -9.8f, 0);
    f32 drag = 0.0f;

    // Wind: when on, the scene's wind (WindSystem / WeatherZone) pushes particles so
    // they drift and gust with the rest of the world (leaves, dust, snow, embers).
    // Assign a texture + emit across an area and this becomes a wind visualizer.
    bool useSceneWind = false;
    f32 windInfluence = 1.0f;      // how strongly the scene wind pushes these particles

    // Max particles
    u32 maxParticles = 1024;

    // Simulation space
    enum class SimulationSpace : u8 { World, Local };
    SimulationSpace simulationSpace = SimulationSpace::World;

    // Render mode
    enum class RenderMode : u8 { Billboard, VelocityStretch };
    RenderMode renderMode = RenderMode::Billboard;
    f32 velocityStretchScale = 0.0f;  // 0 = no stretch, 1 = normal, 2+ = exaggerated

    // Texture
    std::string texturePath;
    i32 textureSheetX = 1;         // Animation frames
    i32 textureSheetY = 1;

    // Runtime pool (not serialized)
    ParticlePool pool;
};

// Shared particle preset applicator — used by editor, scripting, and visual script
inline void ApplyParticlePreset(ParticleEmitterComponent& e, const std::string& preset) {
    // Reset pool so particles respawn with new settings
    e.pool.activeCount = 0;
    e.pool.spawnAccumulator = 0.0f;
    e.pool.burstTimer = 0.0f;
    e.pool.systemAge = 0.0f;
    e.isPlaying = true;
    e.loop = true;

    if (preset == "Fire") {
        e.emissionRate = 60.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 1.2f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 2.5f;
        e.speedVariance = 0.5f;
        e.startSize = 0.4f;
        e.sizeMid = 0.6f;
        e.endSize = 0.05f;
        e.startColor = Math::Vector3(1.0f, 0.6f, 0.1f);
        e.endColor = Math::Vector3(0.8f, 0.1f, 0.0f);
        e.startAlpha = 0.9f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.8f;
        e.speedMultiplierEnd = 0.3f;
        e.shape = ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.2f;
        e.coneAngle = 15.0f;
        e.gravity = Math::Vector3(0, 1.5f, 0);
        e.drag = 0.5f;
        e.rotationSpeed = 0.5f;
        e.rotationSpeedVariance = 1.0f;
        e.maxParticles = 512;
    } else if (preset == "Smoke") {
        e.emissionRate = 20.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 3.0f;
        e.lifetimeVariance = 0.5f;
        e.startSpeed = 1.0f;
        e.speedVariance = 0.3f;
        e.startSize = 0.3f;
        e.sizeMid = 0.8f;
        e.endSize = 1.5f;
        e.startColor = Math::Vector3(0.4f, 0.4f, 0.4f);
        e.endColor = Math::Vector3(0.2f, 0.2f, 0.2f);
        e.startAlpha = 0.6f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.7f;
        e.speedMultiplierEnd = 0.2f;
        e.shape = ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.3f;
        e.coneAngle = 20.0f;
        e.gravity = Math::Vector3(0, 0.5f, 0);
        e.drag = 0.8f;
        e.rotationSpeed = 0.3f;
        e.rotationSpeedVariance = 0.5f;
        e.maxParticles = 256;
    } else if (preset == "Sparks") {
        e.emissionRate = 5.0f;
        e.burstCount = 10;
        e.burstInterval = 0.5f;
        e.lifetime = 0.8f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 8.0f;
        e.speedVariance = 3.0f;
        e.startSize = 0.1f;
        e.sizeMid = -1.0f;
        e.endSize = 0.02f;
        e.startColor = Math::Vector3(1.0f, 0.9f, 0.3f);
        e.endColor = Math::Vector3(1.0f, 0.3f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.6f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ParticleEmitterComponent::EmitterShape::Point;
        e.shapeRadius = 0.05f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, -9.8f, 0);
        e.drag = 0.2f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 256;
    } else if (preset == "Snow") {
        e.emissionRate = 40.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 4.0f;
        e.lifetimeVariance = 1.0f;
        e.startSpeed = 0.5f;
        e.speedVariance = 0.2f;
        e.startSize = 0.15f;
        e.sizeMid = -1.0f;
        e.endSize = 0.1f;
        e.startColor = Math::Vector3(1.0f, 1.0f, 1.0f);
        e.endColor = Math::Vector3(0.9f, 0.95f, 1.0f);
        e.startAlpha = 0.8f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 1.0f;
        e.speedMultiplierEnd = 0.8f;
        e.shape = ParticleEmitterComponent::EmitterShape::Box;
        e.shapeRadius = 5.0f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0.3f, -1.5f, 0.1f);
        e.drag = 0.3f;
        e.rotationSpeed = 0.5f;
        e.rotationSpeedVariance = 1.0f;
        e.maxParticles = 512;
    } else if (preset == "Rain") {
        e.emissionRate = 80.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 1.5f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 12.0f;
        e.speedVariance = 2.0f;
        e.startSize = 0.05f;
        e.sizeMid = -1.0f;
        e.endSize = 0.03f;
        e.startColor = Math::Vector3(0.5f, 0.6f, 0.8f);
        e.endColor = Math::Vector3(0.4f, 0.5f, 0.7f);
        e.startAlpha = 0.6f;
        e.endAlpha = 0.1f;
        e.speedMultiplierMid = 1.0f;
        e.speedMultiplierEnd = 1.0f;
        e.shape = ParticleEmitterComponent::EmitterShape::Box;
        e.shapeRadius = 5.0f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0.5f, -15.0f, 0);
        e.drag = 0.0f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 1024;
    } else if (preset == "Magic") {
        e.emissionRate = 30.0f;
        e.burstCount = 5;
        e.burstInterval = 1.0f;
        e.lifetime = 2.0f;
        e.lifetimeVariance = 0.5f;
        e.startSpeed = 1.5f;
        e.speedVariance = 0.5f;
        e.startSize = 0.2f;
        e.sizeMid = 0.35f;
        e.endSize = 0.0f;
        e.startColor = Math::Vector3(0.3f, 0.5f, 1.0f);
        e.endColor = Math::Vector3(0.8f, 0.2f, 1.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 1.2f;
        e.speedMultiplierEnd = 0.4f;
        e.shape = ParticleEmitterComponent::EmitterShape::Sphere;
        e.shapeRadius = 0.5f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, 0.5f, 0);
        e.drag = 0.3f;
        e.rotationSpeed = 2.0f;
        e.rotationSpeedVariance = 1.5f;
        e.maxParticles = 512;
    } else if (preset == "Explosion") {
        e.emissionRate = 0.0f;
        e.burstCount = 80;
        e.burstInterval = 0.0f;
        e.lifetime = 1.0f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 10.0f;
        e.speedVariance = 4.0f;
        e.startSize = 0.5f;
        e.sizeMid = 0.8f;
        e.endSize = 0.1f;
        e.startColor = Math::Vector3(1.0f, 0.8f, 0.2f);
        e.endColor = Math::Vector3(0.3f, 0.1f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.4f;
        e.speedMultiplierEnd = 0.05f;
        e.shape = ParticleEmitterComponent::EmitterShape::Point;
        e.shapeRadius = 0.1f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, -3.0f, 0);
        e.drag = 1.5f;
        e.rotationSpeed = 3.0f;
        e.rotationSpeedVariance = 3.0f;
        e.loop = false;
        e.maxParticles = 256;
    } else if (preset == "Water Splash") {
        e.emissionRate = 0.0f;
        e.burstCount = 40;
        e.burstInterval = 0.0f;
        e.lifetime = 0.8f;
        e.lifetimeVariance = 0.2f;
        e.startSpeed = 8.0f;
        e.speedVariance = 3.0f;
        e.startSize = 0.15f;
        e.sizeMid = 0.2f;
        e.endSize = 0.05f;
        e.startColor = Math::Vector3(0.4f, 0.7f, 1.0f);
        e.endColor = Math::Vector3(0.2f, 0.5f, 0.9f);
        e.startAlpha = 0.9f;
        e.endAlpha = 0.1f;
        e.speedMultiplierMid = 0.6f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ParticleEmitterComponent::EmitterShape::Hemisphere;
        e.shapeRadius = 0.3f;
        e.coneAngle = 45.0f;
        e.gravity = Math::Vector3(0, -12.0f, 0);
        e.drag = 0.3f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.loop = false;
        e.maxParticles = 256;
        e.renderMode = ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 1.5f;
    } else if (preset == "Blood/Sap") {
        e.emissionRate = 15.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 1.5f;
        e.lifetimeVariance = 0.4f;
        e.startSpeed = 3.0f;
        e.speedVariance = 1.0f;
        e.startSize = 0.12f;
        e.sizeMid = 0.18f;
        e.endSize = 0.06f;
        e.startColor = Math::Vector3(0.6f, 0.05f, 0.05f);
        e.endColor = Math::Vector3(0.3f, 0.0f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.3f;
        e.speedMultiplierMid = 0.7f;
        e.speedMultiplierEnd = 0.2f;
        e.shape = ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.05f;
        e.coneAngle = 20.0f;
        e.gravity = Math::Vector3(0, -6.0f, 0);
        e.drag = 2.0f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 256;
        e.renderMode = ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 2.0f;
    } else if (preset == "Lava") {
        e.emissionRate = 8.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 3.0f;
        e.lifetimeVariance = 0.8f;
        e.startSpeed = 2.0f;
        e.speedVariance = 0.8f;
        e.startSize = 0.6f;
        e.sizeMid = 0.8f;
        e.endSize = 0.3f;
        e.startColor = Math::Vector3(1.0f, 0.5f, 0.0f);
        e.endColor = Math::Vector3(0.4f, 0.05f, 0.0f);
        e.startAlpha = 1.0f;
        e.endAlpha = 0.6f;
        e.speedMultiplierMid = 0.5f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ParticleEmitterComponent::EmitterShape::Hemisphere;
        e.shapeRadius = 0.5f;
        e.coneAngle = 30.0f;
        e.gravity = Math::Vector3(0, -2.0f, 0);
        e.drag = 1.0f;
        e.rotationSpeed = 0.2f;
        e.rotationSpeedVariance = 0.3f;
        e.maxParticles = 256;
        e.renderMode = ParticleEmitterComponent::RenderMode::Billboard;
        e.velocityStretchScale = 0.0f;
    } else if (preset == "Fountain") {
        e.emissionRate = 40.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 2.0f;
        e.lifetimeVariance = 0.3f;
        e.startSpeed = 10.0f;
        e.speedVariance = 2.0f;
        e.startSize = 0.1f;
        e.sizeMid = 0.15f;
        e.endSize = 0.05f;
        e.startColor = Math::Vector3(0.6f, 0.85f, 1.0f);
        e.endColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        e.startAlpha = 0.8f;
        e.endAlpha = 0.0f;
        e.speedMultiplierMid = 0.5f;
        e.speedMultiplierEnd = 0.1f;
        e.shape = ParticleEmitterComponent::EmitterShape::Cone;
        e.shapeRadius = 0.1f;
        e.coneAngle = 10.0f;
        e.gravity = Math::Vector3(0, -9.8f, 0);
        e.drag = 0.2f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 512;
        e.renderMode = ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 1.2f;
    } else if (preset == "Drip") {
        e.emissionRate = 2.0f;
        e.burstCount = 0;
        e.burstInterval = 0.0f;
        e.lifetime = 2.5f;
        e.lifetimeVariance = 0.5f;
        e.startSpeed = 0.5f;
        e.speedVariance = 0.2f;
        e.startSize = 0.1f;
        e.sizeMid = 0.15f;
        e.endSize = 0.08f;
        e.startColor = Math::Vector3(0.3f, 0.6f, 0.9f);
        e.endColor = Math::Vector3(0.2f, 0.4f, 0.7f);
        e.startAlpha = 0.9f;
        e.endAlpha = 0.2f;
        e.speedMultiplierMid = 1.5f;
        e.speedMultiplierEnd = 2.0f;
        e.shape = ParticleEmitterComponent::EmitterShape::Point;
        e.shapeRadius = 0.02f;
        e.coneAngle = 5.0f;
        e.gravity = Math::Vector3(0, -9.8f, 0);
        e.drag = 0.5f;
        e.rotationSpeed = 0.0f;
        e.rotationSpeedVariance = 0.0f;
        e.maxParticles = 64;
        e.renderMode = ParticleEmitterComponent::RenderMode::VelocityStretch;
        e.velocityStretchScale = 2.5f;
    }
}

// ============================================================================
// TAGS & LAYERS
// ============================================================================

// Tag component for filtering and identification
struct TagComponent {
    std::vector<std::string> tags;

    bool HasTag(const std::string& tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }

    void AddTag(const std::string& tag) {
        if (!HasTag(tag)) {
            tags.push_back(tag);
        }
    }

    void RemoveTag(const std::string& tag) {
        for (auto it = tags.begin(); it != tags.end(); ++it) {
            if (*it == tag) {
                tags.erase(it);
                return;
            }
        }
    }
};

// Layer component (alternative to tags, uses bitmask)
struct LayerComponent {
    u32 layer = 0;                 // Which layer this entity is on
    std::string layerName;         // Human-readable name
};

// ============================================================================
// 2D RENDERING COMPONENTS
// ============================================================================

// Sprite2D component for 2D games
struct Sprite2DComponent {
    // Texture reference (path for now, could be handle later)
    std::string texturePath;

    // Optional normal map for lit sprite mode (2.5D lighting)
    std::string normalMapPath;

    // Source rectangle in texture (for sprite sheets)
    f32 srcX = 0, srcY = 0;
    f32 srcWidth = 0, srcHeight = 0;  // 0 = use full texture

    // Display settings
    Math::Vector2 size = Math::Vector2(1.0f, 1.0f);   // World units
    Math::Vector2 pivot = Math::Vector2(0.5f, 0.5f);  // 0-1, center by default
    Math::Vector3 tint = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 alpha = 1.0f;

    // Rendering order
    i32 sortingLayer = 0;
    i32 orderInLayer = 0;

    // Flip
    bool flipX = false;
    bool flipY = false;

    // Visibility
    bool visible = true;

    // Dirty flag — triggers mesh regeneration in RenderSystem
    bool spriteDirty = true;

    // Texture pixel dimensions (set by RenderSystem on texture load for UV normalization)
    f32 texPixelWidth = 0, texPixelHeight = 0;

    // Drop shadow (fake depth grounding)
    bool dropShadow = false;
    Math::Vector2 shadowOffset = Math::Vector2(3.0f, -3.0f);  // pixels offset
    Math::Vector4 shadowColor = Math::Vector4(0.0f, 0.0f, 0.0f, 0.4f);  // RGBA
    f32 shadowScale = 1.0f;  // 1.0 = same size as sprite
};

// Animated sprite component (sprite sheet animation)
struct AnimatedSprite2DComponent {
    // Animation frames (each frame is srcX, srcY in the sprite sheet)
    struct Frame {
        f32 srcX, srcY;
        f32 duration = 0.1f;  // Seconds per frame
    };

    std::vector<Frame> frames;
    u32 currentFrame = 0;
    f32 frameTimer = 0.0f;

    bool playing = true;
    bool loop = true;
    f32 playbackSpeed = 1.0f;

    // Events
    bool frameChanged = false;
    bool animationComplete = false;
};

// Tilemap component (for retro-style tile-based games)
struct TilemapComponent {
    // Tile data (index into tileset, -1 = empty)
    std::vector<i32> tiles;
    u32 width = 0;   // Tiles
    u32 height = 0;  // Tiles

    // Tileset settings
    std::string tilesetPath;
    f32 tileWidth = 16.0f;   // Pixels
    f32 tileHeight = 16.0f;  // Pixels
    u32 tilesetColumns = 16; // Tiles per row in tileset

    // World scale
    f32 worldTileWidth = 1.0f;
    f32 worldTileHeight = 1.0f;

    // Collision layer
    bool hasCollision = false;
    std::vector<bool> collisionMask;  // Which tiles are solid

    // Helper to get tile at position
    i32 GetTile(u32 x, u32 y) const {
        if (x >= width || y >= height) return -1;
        return tiles[y * width + x];
    }

    void SetTile(u32 x, u32 y, i32 tileIndex) {
        if (x < width && y < height) {
            tiles[y * width + x] = tileIndex;
            meshDirty = true;
        }
    }

    // Dirty flag — triggers mesh regeneration in RenderSystem
    bool meshDirty = true;
};

// 2D Camera bounds (for 2D games)
struct Camera2DBoundsComponent {
    bool useBounds = false;
    Math::Vector2 minBounds;  // World position
    Math::Vector2 maxBounds;
    f32 boundsPadding = 0.0f;

    // Smooth follow target
    Entity followTarget = 0;
    f32 followSmoothing = 5.0f;  // Higher = faster
    Math::Vector2 followOffset;

    // Zoom limits and smoothing
    f32 minZoom = 0.5f;
    f32 maxZoom = 3.0f;
    f32 currentZoom = 1.0f;
    f32 targetZoom = 1.0f;
    f32 zoomSmoothing = 5.0f;
    f32 baseOrthoSize = 0.0f;  // S4: Per-entity base (0 = not yet captured)

    // Dead zone - camera doesn't move until target exits this region
    Math::Vector2 deadZoneSize;  // Width/height in world units

    // Look-ahead - camera leads in movement direction
    f32 lookAheadDistance = 0.0f;
    f32 lookAheadSmoothing = 3.0f;
    Math::Vector2 currentLookAhead;  // State: current look-ahead offset

    // Screen shake
    f32 shakeIntensity = 0.0f;   // Current shake strength (0 = none)
    f32 shakeFrequency = 15.0f;  // Oscillation speed
    f32 shakeDuration = 0.0f;    // Remaining shake time
    f32 shakeTimer = 0.0f;       // Internal timer

    // Multi-target framing
    std::vector<Entity> additionalTargets;
    f32 multiTargetPadding = 2.0f;
    bool autoZoomToFitTargets = false;

    // Helper to trigger screen shake
    void TriggerShake(f32 intensity, f32 duration) {
        shakeIntensity = intensity;
        shakeDuration = duration;
        shakeTimer = 0.0f;
    }
};

// ============================================================================
// STATE MACHINE (for game logic)
// ============================================================================

// Condition type for state machine transitions
enum class SMConditionType : u8 {
    BoolTrue = 0,    // parameter == true
    BoolFalse,       // parameter == false
    FloatGreater,    // parameter > threshold
    FloatLess,       // parameter < threshold
    IntEquals,       // parameter == intValue
    IntNotEquals,    // parameter != intValue
    Trigger,         // one-shot trigger (auto-reset after transition)
    COUNT
};

struct SMTransitionCondition {
    std::string paramName;
    SMConditionType type = SMConditionType::Trigger;
    f32 threshold = 0.0f;
    i32 intValue = 0;
};

struct SMTransition {
    std::string toState;
    std::vector<SMTransitionCondition> conditions;  // All must be true (AND)
};

struct SMState {
    std::string name;
    std::vector<SMTransition> transitions;
    Math::Vector2 editorPosition = Math::Vector2(0, 0);  // Node position in graph editor

    // Script callback function names (called on entity's TegeBehavior scripts)
    std::string onEnter;   // Called once when entering this state
    std::string onUpdate;  // Called each frame while in this state
    std::string onExit;    // Called once when leaving this state
};

// State machine component with defined states, transitions, and conditions
struct StateMachineComponent {
    std::vector<SMState> states;
    std::string currentState;
    std::string previousState;
    f32 stateTime = 0.0f;         // Time in current state

    // Parameters — shared across all states
    std::unordered_map<std::string, bool> boolParams;
    std::unordered_map<std::string, f32> floatParams;
    std::unordered_map<std::string, i32> intParams;
    std::vector<std::string> activeTriggers;  // Consumed on transition

    void SetState(const std::string& newState) {
        if (currentState != newState) {
            previousState = currentState;
            currentState = newState;
            stateTime = 0.0f;
        }
    }

    void SetFloat(const std::string& name, f32 value) {
        floatParams[name] = value;
    }

    f32 GetFloat(const std::string& name, f32 defaultValue = 0.0f) const {
        auto it = floatParams.find(name);
        return it != floatParams.end() ? it->second : defaultValue;
    }

    void SetInt(const std::string& name, i32 value) {
        intParams[name] = value;
    }

    i32 GetInt(const std::string& name, i32 defaultValue = 0) const {
        auto it = intParams.find(name);
        return it != intParams.end() ? it->second : defaultValue;
    }

    void SetBool(const std::string& name, bool value) {
        boolParams[name] = value;
    }

    bool GetBool(const std::string& name, bool defaultValue = false) const {
        auto it = boolParams.find(name);
        return it != boolParams.end() ? it->second : defaultValue;
    }

    void SendTrigger(const std::string& trigger) {
        activeTriggers.push_back(trigger);
    }

    bool HasState(const std::string& name) const {
        for (const auto& s : states) {
            if (s.name == name) return true;
        }
        return false;
    }
};

// ============================================================================
// DIALOGUE SYSTEM (for RPG/Adventure games)
// ============================================================================

// Dialogue component (retro RPG style text boxes)
struct DialogueComponent {
    // Current dialogue
    std::vector<std::string> dialogueLines;
    u32 currentLine = 0;
    u32 currentChar = 0;         // For typewriter effect
    f32 charTimer = 0.0f;
    f32 charDelay = 0.05f;       // Seconds between characters
    bool isTyping = false;
    bool waitingForInput = false;

    // Dialogue box settings
    std::string speakerName;
    std::string portraitPath;     // Optional character portrait

    // Typewriter sound (optional)
    std::string typeSound;
    bool playTypeSound = true;

    // Choices (for branching dialogue)
    struct Choice {
        std::string text;
        std::string nextDialogueId;  // ID of dialogue to jump to
    };
    std::vector<Choice> choices;
    i32 selectedChoice = 0;

    // --- Tree-based dialogue (node graph) ---
    GUI::DialogueTreeData dialogueTree;  // If nodes non-empty -> tree mode
    std::unordered_map<std::string, std::string> variables;  // Persisted dialogue variables

    // Runtime state for tree mode (not serialized — managed by DialogueSystem)
    bool treeActive = false;
    u32 currentNodeId = 0;
    std::string currentSpeaker;
    std::string currentText;
    Math::Vector3 currentSpeakerColor = Math::Vector3(1, 1, 1);
    std::vector<GUI::DialogueChoice> currentChoices;

    bool IsTreeMode() const { return !dialogueTree.nodes.empty(); }

    // Tree-mode visible text (typewriter on currentText)
    std::string GetTreeVisibleText() const {
        if (currentText.empty()) return "";
        u32 visibleChars = currentChar;
        if (visibleChars > static_cast<u32>(currentText.size()))
            visibleChars = static_cast<u32>(currentText.size());
        return currentText.substr(0, visibleChars);
    }

    // Helpers
    bool IsComplete() const {
        return currentLine >= dialogueLines.size();
    }

    void StartDialogue(const std::vector<std::string>& lines) {
        dialogueLines = lines;
        currentLine = 0;
        currentChar = 0;
        charTimer = 0.0f;
        isTyping = true;
        waitingForInput = false;
    }

    std::string GetVisibleText() const {
        if (currentLine >= dialogueLines.size()) return "";
        const std::string& line = dialogueLines[currentLine];
        return line.substr(0, currentChar);
    }
};

// ============================================================================
// DIALOGUE BOX UI (auto-builds UICanvas elements for dialogue display)
// ============================================================================

struct DialogueBoxComponent {
    // --- Configuration ---
    f32 boxHeight = 200.0f;             // Dialogue box height in design pixels
    f32 boxMargin = 20.0f;              // Margin from screen edges
    f32 boxPadding = 16.0f;             // Inner padding
    Math::Vector3 boxColor = Math::Vector3(0.05f, 0.05f, 0.08f);
    f32 boxAlpha = 0.92f;
    f32 boxBorderRadius = 8.0f;

    // Speaker name
    f32 speakerFontSize = 20.0f;
    Math::Vector3 defaultSpeakerColor = Math::Vector3(0.9f, 0.85f, 0.5f);

    // Dialogue text
    f32 textFontSize = 17.0f;
    Math::Vector3 textColor = Math::Vector3(0.9f, 0.9f, 0.9f);

    // Portrait
    bool showPortrait = true;
    f32 portraitSize = 96.0f;           // Width and height of portrait image

    // Choice buttons
    f32 choiceSpacing = 6.0f;
    Math::Vector3 choiceColor = Math::Vector3(0.15f, 0.15f, 0.2f);
    Math::Vector3 choiceTextColor = Math::Vector3(0.85f, 0.85f, 0.85f);

    // Continue indicator
    std::string continueText = ">>>";
    f32 continueBlinkSpeed = 2.0f;      // Blinks per second

    // --- Runtime state (not serialized) ---
    bool initialized = false;
    u32 panelElementId = 0;
    u32 speakerElementId = 0;
    u32 textElementId = 0;
    u32 portraitElementId = 0;
    u32 continueElementId = 0;
    static constexpr u32 MAX_CHOICES = 6;
    u32 choiceElementIds[MAX_CHOICES] = {};
    u32 choiceCount = 0;
    f32 blinkTimer = 0.0f;
};

// ============================================================================
// SAVE DATA (for game progress)
// ============================================================================

// How long data survives across game sessions
enum class PersistenceTier : u8 {
    SceneState = 0,     // Per-scene within a run (enemies dead, doors opened). Resets on new run.
    RunState,           // Per-run (player health, inventory, quest progress). Resets on new game.
    MetaProgression,    // Permanent (unlocks, achievements, meta-currencies). Survives across runs.
    COUNT
};

// Save data component (marks what to save about this entity)
struct SaveDataComponent {
    bool savePosition = true;
    bool saveRotation = true;
    bool saveScale = false;
    bool saveEnabled = true;
    PersistenceTier tier = PersistenceTier::RunState;
    std::vector<std::string> tags;   // Custom tags for filtering

    // Custom data to save
    std::vector<std::pair<std::string, std::string>> customData;

    void SetData(const std::string& key, const std::string& value) {
        for (auto& p : customData) {
            if (p.first == key) { p.second = value; return; }
        }
        customData.push_back({key, value});
    }

    std::string GetData(const std::string& key, const std::string& defaultValue = "") const {
        for (const auto& p : customData) {
            if (p.first == key) return p.second;
        }
        return defaultValue;
    }

    bool HasTag(const std::string& tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }
};

// In-game save/load menu overlay
struct SaveLoadMenuComponent {
    bool showOnPause = true;
    bool allowManualSave = true;
    bool allowManualLoad = true;
    bool allowDelete = true;
    bool showAutoSaves = true;
    i32 columnsPerRow = 4;
    std::string headerText = "Save / Load Game";

    // Runtime state (not serialized)
    bool isOpen = false;
    i32 selectedSlot = -1;
    bool confirmOverwrite = false;
    bool confirmDelete = false;
    enum class Mode : u8 { Save, Load } mode = Mode::Save;
};

// ============================================================================
// PUZZLE & INTERACTION COMPONENTS
// ============================================================================

// Lock component — requires a key to open (door, gate, chest, etc.)
struct LockComponent {
    std::string requiredKey;       // Key ID that unlocks this (matches InventoryComponent::keys)
    bool isLocked = true;
    bool consumeKey = false;       // Remove key from inventory on use
    bool autoOpen = false;         // Open automatically when player with key enters range
    f32 interactRange = 2.0f;      // Range for manual interact

    // Door behavior
    enum class OpenMode : u8 {
        Toggle,      // Open/close on interact
        OpenOnly,    // Once open, stays open
        Timed        // Opens for openDuration then closes
    };
    OpenMode openMode = OpenMode::Toggle;
    f32 openDuration = 5.0f;       // For Timed mode
    f32 openTimer = 0.0f;

    // Animation
    Math::Vector3 closedPosition = Math::Vector3(0, 0, 0);
    Math::Vector3 openPosition = Math::Vector3(0, 3, 0);   // Default: slide up
    Math::Vector3 closedRotation = Math::Vector3(0, 0, 0);
    Math::Vector3 openRotation = Math::Vector3(0, 0, 0);
    f32 openSpeed = 3.0f;          // Lerp speed
    f32 openProgress = 0.0f;       // 0 = closed, 1 = open

    // State
    bool isOpen = false;
    bool isAnimating = false;

    // Prompt
    std::string lockedPrompt = "Requires key";
    std::string unlockedPrompt = "Press E to open";
};

// Pushable component — entity can be pushed by the player or other forces
struct PushableComponent {
    f32 mass = 1.0f;               // Heavier = slower to push
    f32 pushSpeed = 3.0f;          // Movement speed when being pushed
    f32 friction = 0.9f;           // Velocity damping per frame

    // Grid-based pushing (for Sokoban-style puzzles)
    bool gridSnap = false;         // Snap to grid cells
    f32 gridCellSize = 1.0f;       // Grid cell size
    f32 gridMoveSpeed = 6.0f;      // Speed of lerp between cells
    bool gridMoving = false;       // Currently transitioning
    Math::Vector3 gridMoveStart;
    Math::Vector3 gridMoveTarget;
    f32 gridMoveProgress = 0.0f;

    // Constraints
    bool pushableX = true;
    bool pushableY = false;        // Usually no vertical pushing
    bool pushableZ = true;
    bool canBePushedOff = false;   // Can be pushed off ledges

    // State
    bool isBeingPushed = false;
    Math::Vector3 velocity;
    Entity pushedBy = 0;           // Entity pushing this
};

// Switch / Pressure Plate — activates when triggered (by weight, interaction, etc.)
struct SwitchComponent {
    enum class SwitchType : u8 {
        PressurePlate,   // Activated by weight (entity standing/placed on it)
        Toggle,          // Click/interact to toggle on/off
        OneShot,         // Interact once, stays on permanently
        Timed,           // Turns on for duration, then off
        Sequence         // Part of a sequence (must activate in order)
    };
    SwitchType type = SwitchType::PressurePlate;

    // Activation
    bool isActive = false;
    bool requireSpecificTag;       // Only entities with matching tag activate it
    std::string requiredTag;       // Tag required (e.g., "crate", "player")
    f32 activationWeight = 0.0f;   // Min mass needed for pressure plate (0 = any)

    // Timed mode
    f32 activeDuration = 5.0f;
    f32 activeTimer = 0.0f;

    // Sequence mode
    i32 sequenceIndex = 0;         // Position in sequence (0-based)
    i32 sequenceGroup = 0;         // Which sequence this belongs to

    // Linked entities (what this switch controls)
    std::vector<Entity> linkedEntities;

    // Visual
    Math::Vector3 offPosition;     // Position when inactive
    Math::Vector3 onPosition;      // Position when active (e.g., pressed down)
    f32 transitionSpeed = 8.0f;
    f32 transitionProgress = 0.0f;

    // Prompt
    std::string promptText = "Press E";
    bool showPrompt = true;

    // State
    Entity activatedBy = 0;        // Entity that activated this switch
    bool wasActive = false;        // Previous frame state (for edge detection)
};

// Goal Zone — marks a target area for puzzle completion (Sokoban goals, checkpoints, etc.)
struct GoalZoneComponent {
    enum class GoalType : u8 {
        PushTarget,    // A box must be pushed here (Sokoban)
        StandOn,       // Player must stand here
        ItemDeposit,   // Specific item must be placed here
        Checkpoint,    // Save progress point
        LevelExit      // Transition to next level/scene
    };
    GoalType type = GoalType::PushTarget;

    // Requirements
    std::string requiredTag;       // Entity tag that satisfies this goal (e.g., "crate")
    std::string requiredItem;      // Item ID for ItemDeposit type

    // State
    bool isSatisfied = false;      // Goal condition met
    Entity satisfiedBy = 0;        // Which entity satisfied this goal
    i32 goalGroup = 0;             // Group ID (all goals in group must be satisfied)

    // Visual feedback
    Math::Vector3 inactiveColor = Math::Vector3(0.3f, 0.3f, 0.3f);
    Math::Vector3 activeColor = Math::Vector3(0.2f, 0.8f, 0.2f);

    // Level transition (for LevelExit type)
    std::string nextScene;
};

// Conveyor Belt — moves entities along a direction
struct ConveyorComponent {
    Math::Vector3 direction = Math::Vector3(1, 0, 0); // Movement direction (normalized)
    f32 speed = 3.0f;
    bool affectsPlayer = true;
    bool affectsPushables = true;
    bool isActive = true;
};

// Teleporter — moves entities to a target position
struct TeleporterComponent {
    Math::Vector3 targetPosition;
    Math::Vector3 targetRotation;   // Euler angles after teleport
    Entity linkedTeleporter = 0;    // For bidirectional teleporters
    f32 cooldown = 1.0f;            // Prevent rapid re-teleport
    f32 cooldownTimer = 0.0f;
    bool preserveVelocity = false;
    std::string requiredTag;        // Empty = teleports anything
};

// Destructible — entity can be destroyed by damage or interaction
struct DestructibleComponent {
    f32 health = 1.0f;             // Hits to destroy (or HP)
    bool destroyOnHit = true;      // One-hit destroy
    bool spawnPickup = false;      // Drop item on destroy
    std::string pickupId;          // What to drop
    i32 pickupCount = 1;

    // Respawn
    bool canRespawn = false;
    f32 respawnTime = 10.0f;
    f32 respawnTimer = 0.0f;
    bool isDestroyed = false;

    // Visual
    f32 shakeOnHit = 0.1f;        // Screen/entity shake amount

    // Event callbacks (set by gameplay code, called on destruction)
    std::function<void(Entity)> onDamaged;      // Called each time damage is applied
    std::function<void(Entity)> onDestroyed;    // Called when health reaches 0
    std::function<void(Entity)> onRespawned;    // Called when entity respawns

    // Visual damage feedback
    bool showDamageOverlay = true;              // Enable crack/damage visual
    f32 damageOverlayIntensity = 0.0f;          // Runtime: 0=pristine, 1=about to break (computed from health ratio)
    std::string crackTexturePath;               // Optional crack normal map overlay
    Math::Vector3 damageTint = Math::Vector3(0.3f, 0.25f, 0.2f); // Darkening tint at low health

    // Damage overlay internal state
    f32 maxHealth = 0.0f;                       // Initial health (auto-captured on first frame, 0 = not yet set)
    Math::Vector3 originalBaseColor;            // Cached for damage overlay restoration
    bool originalBaseColorCached = false;       // Whether we've captured the original color
};

// Moving Platform — entity moves between waypoints
struct MovingPlatformComponent {
    std::vector<Math::Vector3> waypoints;
    f32 speed = 2.0f;
    f32 waitTime = 1.0f;           // Pause at each waypoint
    f32 waitTimer = 0.0f;

    enum class PlatformMode : u8 {
        Loop,       // A → B → C → A → B → ...
        PingPong,   // A → B → C → B → A → ...
        OneWay,     // A → B → C (stops)
        Triggered   // Only moves when activated (via SwitchComponent)
    };
    PlatformMode mode = PlatformMode::PingPong;

    // State
    i32 currentWaypoint = 0;
    i32 direction = 1;             // +1 forward, -1 backward (for PingPong)
    f32 moveProgress = 0.0f;       // 0-1 between current and next waypoint
    bool isMoving = true;
    bool isWaiting = false;

    // Carries entities standing on it
    bool carryEntities = true;
};

// ============================================================================
// DAMAGE RESISTANCE SYSTEM
// ============================================================================

struct DamageResistanceComponent {
    // Multipliers per damage type (1.0 = normal, 0.0 = immune, 2.0 = weakness)
    f32 physicalMult = 1.0f;
    f32 fireMult = 1.0f;
    f32 iceMult = 1.0f;
    f32 electricMult = 1.0f;
    f32 poisonMult = 1.0f;
    f32 magicMult = 1.0f;

    f32 GetMultiplier(DamageComponent::DamageType type) const {
        switch (type) {
            case DamageComponent::DamageType::Physical: return physicalMult;
            case DamageComponent::DamageType::Fire:     return fireMult;
            case DamageComponent::DamageType::Ice:      return iceMult;
            case DamageComponent::DamageType::Electric:  return electricMult;
            case DamageComponent::DamageType::Poison:    return poisonMult;
            case DamageComponent::DamageType::Magic:     return magicMult;
            default: return 1.0f;
        }
    }
};

// ============================================================================
// STAMINA / RESOURCE SYSTEM
// ============================================================================

struct ResourceComponent {
    std::string resourceName = "Stamina";
    f32 maxValue = 100.0f;
    f32 currentValue = 100.0f;
    f32 regenRate = 10.0f;        // Per second
    f32 regenDelay = 1.0f;        // Seconds after use before regen starts
    f32 timeSinceLastUse = 0.0f;
    bool depleted = false;         // True when hits 0, stays true until threshold
    f32 depletedThreshold = 20.0f; // Must regen to this before un-depleted

    // Costs for common actions
    f32 sprintCostPerSec = 15.0f;
    f32 jumpCost = 20.0f;
    f32 dashCost = 25.0f;
    f32 attackCost = 0.0f;

    f32 GetPercent() const { return maxValue > 0.0f ? currentValue / maxValue : 0.0f; }

    bool TryConsume(f32 amount) {
        if (currentValue >= amount) {
            currentValue -= amount;
            timeSinceLastUse = 0.0f;
            if (currentValue <= 0.0f) { currentValue = 0.0f; depleted = true; }
            return true;
        }
        return false;
    }

    void Regenerate(f32 deltaTime) {
        timeSinceLastUse += deltaTime;
        if (timeSinceLastUse >= regenDelay && currentValue < maxValue) {
            currentValue = std::min(currentValue + regenRate * deltaTime, maxValue);
            if (depleted && currentValue >= depletedThreshold) depleted = false;
        }
    }
};

// ============================================================================
// FOOTSTEP SYSTEM
// ============================================================================

struct FootstepComponent {
    struct SurfaceSound {
        std::string surfaceTag;   // "grass", "stone", "wood", "metal", "water"
        std::string walkSound;     // Audio clip path
        std::string runSound;
        f32 volumeScale = 1.0f;
    };

    std::vector<SurfaceSound> surfaceSounds;
    std::string defaultWalkSound;
    std::string defaultRunSound;

    f32 walkStepInterval = 0.5f;   // Seconds between steps when walking
    f32 runStepInterval = 0.3f;
    f32 stepTimer = 0.0f;
    f32 volume = 0.8f;
    f32 pitchVariance = 0.1f;      // Random pitch variation
    // Speed above which the character counts as moving. Was a literal written
    // into the system twice, once per controller, so a character who walks
    // slower than it made no footstep sound at all and there was nothing to
    // change.
    f32 movementThreshold = 0.5f;  // world units/sec

    bool isMoving = false;
    bool isRunning = false;
    std::string currentSurface = "default";
};

// ============================================================================
// OBJECT POOLING
// ============================================================================

struct PoolableComponent {
    std::string poolId;
    bool isActive = false;
    f32 lifetime = 0.0f;      // 0 = infinite (manually returned)
    f32 activeTime = 0.0f;
    Entity spawnedBy = 0;
};

// ============================================================================
// QUEST / OBJECTIVE SYSTEM
// ============================================================================

struct QuestStateComponent {
    std::string questId;
    enum class Status : u8 { NotStarted, Active, Completed, Failed };
    Status status = Status::NotStarted;
    i32 currentObjective = 0;
    std::vector<std::pair<std::string, bool>> objectiveFlags;
    f32 timeElapsed = 0.0f;
};

// ============================================================================
// HUD / UI SYSTEM
// ============================================================================

struct HUDWidgetComponent {
    enum class WidgetType : u8 { HealthBar, ResourceBar, Label, ObjectiveMarker, Crosshair, Minimap };
    WidgetType type = WidgetType::HealthBar;
    bool visible = true;
    bool screenSpace = true; // true = fixed screen position, false = world-space billboard

    // Screen position (normalized 0-1)
    f32 anchorX = 0.05f;
    f32 anchorY = 0.05f;
    f32 width = 0.2f;
    f32 height = 0.03f;

    // Visual properties
    Math::Vector3 fillColor = Math::Vector3(0.2f, 0.8f, 0.2f);
    Math::Vector3 bgColor = Math::Vector3(0.2f, 0.2f, 0.2f);
    Math::Vector3 textColor = Math::Vector3(1.0f, 1.0f, 1.0f);
    f32 fontSize = 16.0f;
    std::string text;

    // Data binding
    Entity sourceEntity = 0; // Entity to read data from (0 = self)
    std::string bindField; // "health", "stamina", "custom"
    f32 currentValue = 1.0f;
    f32 maxValue = 1.0f;

    // World-space settings
    Math::Vector3 worldOffset = Math::Vector3(0, 2, 0);
    f32 maxRenderDistance = 50.0f;
};

// ============================================================================
// CINEMATIC CAMERA SYSTEM
// ============================================================================

struct CinematicCameraComponent {
    struct Waypoint {
        Math::Vector3 position;
        Math::Vector3 lookAt;
        f32 fov = 60.0f;
        f32 duration = 2.0f;         // Time to reach this waypoint
        f32 holdTime = 0.0f;         // Pause at waypoint
        enum class Easing : u8 { Linear, EaseIn, EaseOut, EaseInOut, SmashCut };
        Easing easing = Easing::EaseInOut;
    };

    std::vector<Waypoint> waypoints;
    bool isPlaying = false;
    bool loop = false;
    bool autoPlay = false;
    bool hideHUD = true;
    bool disableInput = true;
    f32 currentTime = 0.0f;
    i32 currentSegment = 0;
    f32 segmentProgress = 0.0f;
    bool isComplete = false;

    // Callbacks
    Entity onCompleteNotify = 0;
    Entity onWaypointReachNotify = 0;
};

// ============================================================================
// JOINT / CONSTRAINT COMPONENTS
// ============================================================================

// Joint type enumeration shared by all joint components
enum class JointType : u8 {
    Distance,
    Hinge,
    BallSocket,
    Spring,
    Fixed,
    Slider
};

// Distance Joint - maintains a fixed distance between two anchor points
struct DistanceJointComponent {
    Entity entityA = 0;
    Entity entityB = 0;
    Math::Vector3 anchorA = Math::Vector3(0, 0, 0);  // Local-space anchor on A
    Math::Vector3 anchorB = Math::Vector3(0, 0, 0);  // Local-space anchor on B

    f32 restDistance = 1.0f;         // Target distance between anchors
    f32 tolerance = 0.0f;           // Allowed deviation before constraint kicks in
    f32 stiffness = 1.0f;           // 0-1, how rigidly the constraint is enforced

    bool breakable = false;
    f32 breakForce = 1000.0f;
    f32 currentStress = 0.0f;
};

// Hinge Joint - allows rotation around a single axis
struct HingeJointComponent {
    Entity entityA = 0;
    Entity entityB = 0;
    Math::Vector3 anchorA = Math::Vector3(0, 0, 0);
    Math::Vector3 anchorB = Math::Vector3(0, 0, 0);

    Math::Vector3 axis = Math::Vector3(0, 1, 0);  // Hinge axis (local-space of A)

    bool useLimits = false;
    f32 lowerLimit = -180.0f;       // Degrees
    f32 upperLimit = 180.0f;        // Degrees
    f32 currentAngle = 0.0f;        // Current rotation around axis

    bool useMotor = false;
    f32 motorSpeed = 0.0f;          // Degrees per second
    f32 motorMaxForce = 100.0f;

    bool breakable = false;
    f32 breakForce = 1000.0f;
    f32 currentStress = 0.0f;
};

// Ball-Socket Joint - allows 3 DOF rotation, keeps anchors coincident
struct BallSocketJointComponent {
    Entity entityA = 0;
    Entity entityB = 0;
    Math::Vector3 anchorA = Math::Vector3(0, 0, 0);
    Math::Vector3 anchorB = Math::Vector3(0, 0, 0);

    // Cone limit (limits the angle between bodies)
    bool useConeLimit = false;
    f32 coneAngleLimit = 45.0f;     // Degrees, max angle from rest axis

    // Twist limit (limits rotation around the connecting axis)
    bool useTwistLimit = false;
    f32 twistLowerLimit = -180.0f;  // Degrees
    f32 twistUpperLimit = 180.0f;   // Degrees

    bool breakable = false;
    f32 breakForce = 1000.0f;
    f32 currentStress = 0.0f;
};

// Spring Joint - applies Hooke's law force based on distance
struct SpringJointComponent {
    Entity entityA = 0;
    Entity entityB = 0;
    Math::Vector3 anchorA = Math::Vector3(0, 0, 0);
    Math::Vector3 anchorB = Math::Vector3(0, 0, 0);

    f32 restLength = 1.0f;          // Natural length of the spring
    f32 springConstant = 50.0f;     // Hooke's law k (higher = stiffer)
    f32 dampingCoefficient = 5.0f;  // Velocity damping (reduces oscillation)

    f32 minDistance = 0.0f;         // Minimum allowed distance (0 = no limit)
    f32 maxDistance = 0.0f;         // Maximum allowed distance (0 = no limit)

    bool breakable = false;
    f32 breakForce = 1000.0f;
    f32 currentStress = 0.0f;
};

// Fixed Joint - keeps relative transform constant (like gluing two bodies)
struct FixedJointComponent {
    Entity entityA = 0;
    Entity entityB = 0;
    Math::Vector3 anchorA = Math::Vector3(0, 0, 0);
    Math::Vector3 anchorB = Math::Vector3(0, 0, 0);

    // Stored relative transform at creation time
    Math::Vector3 relativePosition = Math::Vector3(0, 0, 0);
    Math::Vector3 relativeRotation = Math::Vector3(0, 0, 0);  // Euler angles
    bool initialized = false;       // Set true once relative transform is captured

    bool breakable = false;
    f32 breakForce = 500.0f;        // Lower default; fixed joints are rigid
    f32 currentStress = 0.0f;
};

// Slider Joint - constrains movement to a single axis
struct SliderJointComponent {
    Entity entityA = 0;
    Entity entityB = 0;
    Math::Vector3 anchorA = Math::Vector3(0, 0, 0);
    Math::Vector3 anchorB = Math::Vector3(0, 0, 0);

    Math::Vector3 slideAxis = Math::Vector3(1, 0, 0);  // Axis of permitted motion (local-space of A)

    bool useLimits = false;
    f32 lowerLimit = -1.0f;         // Min displacement along axis
    f32 upperLimit = 1.0f;          // Max displacement along axis
    f32 currentDisplacement = 0.0f; // Current position along axis

    bool useMotor = false;
    f32 motorSpeed = 0.0f;          // Units per second along axis
    f32 motorMaxForce = 100.0f;

    bool breakable = false;
    f32 breakForce = 1000.0f;
    f32 currentStress = 0.0f;
};

// ============================================================================
// RAGDOLL SYSTEM
// ============================================================================

// Ragdoll component - maps physics joints to skeleton bones for ragdoll simulation
struct RagdollComponent {
    struct BoneJoint {
        std::string boneName;           // Skeleton bone name
        i32 boneIndex = -1;            // Index into SkeletonComponent bones
        JointType jointType = JointType::BallSocket;
        Entity jointEntity = 0;        // Entity holding the joint component

        // Per-bone physics properties
        f32 mass = 1.0f;
        f32 colliderRadius = 0.1f;     // Capsule/sphere radius for bone collision
        Math::Vector3 colliderSize = Math::Vector3(0.1f, 0.2f, 0.1f); // Capsule dims (radius, halfHeight, 0)

        // Joint limits for this bone connection
        f32 coneAngleLimit = 45.0f;    // Max angle from parent bone direction
        f32 twistLimit = 30.0f;        // Max twist around bone axis
    };

    std::vector<BoneJoint> boneJoints;

    bool enabled = false;               // Whether ragdoll simulation is active
    bool autoDisableAfterSettle = true;  // Disable after bodies come to rest
    f32 settleThreshold = 0.01f;        // Velocity magnitude below which considered settled
    f32 settleTimer = 0.0f;            // Accumulates time at rest
    f32 settleTime = 1.0f;             // Seconds of rest before auto-disable

    // Blend between animation and ragdoll (0 = full animation, 1 = full ragdoll)
    f32 blendWeight = 1.0f;
    f32 blendSpeed = 5.0f;            // How fast to transition between modes

    // Transition timing
    f32 blendTime = 0.3f;             // Duration of animation-to-ragdoll blend (seconds)
    f32 blendProgress = 0.0f;         // Runtime: current blend progress (0 = start, 1 = done)

    // Auto-activation on death
    bool autoActivateOnDeath = true;  // Activate ragdoll when HealthComponent reaches 0

    // Global ragdoll properties
    f32 gravityScale = 1.0f;
    f32 linearDamping = 0.1f;
    f32 angularDamping = 0.3f;

    // Runtime state (not serialized)
    bool bodiesCreated = false;        // Whether physics bodies have been spawned for bones
    bool wasAnimating = false;         // Was the animator playing before ragdoll activated
};

// ============================================================================
// PER-FRAME COLLIDER (animation hitboxes)
// ============================================================================

struct PerFrameColliderComponent {
    struct FrameCollider {
        Math::Vector2 offset;    // Collider center offset from sprite pivot
        Math::Vector2 size;      // Box extents (width, height)
        bool enabled = true;
    };
    std::vector<FrameCollider> frameColliders; // Indexed by animation frame
    bool autoApply = true;  // Update BoxCollider on frame change
};

// ============================================================================
// POLYGON COLLIDER 2D (sprite silhouette)
// ============================================================================

struct PolygonCollider2DComponent {
    std::vector<Math::Vector2> vertices; // CCW winding, local space
    bool isTrigger = false;
    f32 friction = 0.5f;
    f32 bounciness = 0.0f;
    u32 categoryBits = 1;
    u32 collisionMask = 0xFFFFFFFF;
};

// ============================================================================
// NETWORKING COMPONENTS
// ============================================================================

// Marks an entity as networked with ownership tracking
struct NetworkIdentityComponent {
    u32 networkId = 0;           // Assigned by NetworkSystem
    u8 ownerId = 0xFF;          // PlayerId of owner (0xFF = unowned)
    bool isLocallyOwned = false; // Runtime flag — true if we own this entity
    bool syncTransform = true;   // Auto-sync position/rotation
    f32 syncInterval = 0.05f;    // Seconds between syncs (20 Hz)
    f32 syncTimer = 0.0f;        // Runtime accumulator
};

// Stores network synchronization state for interpolation and prediction
struct NetworkTransformComponent {
    Math::Vector3 lastSyncedPosition;
    Math::Quaternion lastSyncedRotation = Math::Quaternion(0, 0, 0, 1);
    Math::Vector3 lastSyncedScale = Math::Vector3(1.0f, 1.0f, 1.0f);
    Math::Vector3 networkVelocity;
    Math::Vector3 interpStartPosition;
    Math::Quaternion interpStartRotation = Math::Quaternion(0, 0, 0, 1);
    f32 interpProgress = 0.0f;
    f32 interpDuration = 0.05f;
    Math::Vector3 predictionError;
    f32 correctionBlend = 0.0f;
};

// ============================================================================
// CURL NOISE FLOW FIELD
// ============================================================================

struct CurlNoiseFieldComponent {
    // Noise parameters
    i32 octaves = 4;
    f32 frequency = 1.0f;
    f32 amplitude = 1.0f;
    f32 lacunarity = 2.0f;
    f32 persistence = 0.5f;
    u32 seed = 0;
    f32 timeScale = 0.5f;          // How fast the field evolves over time

    // Volume bounds (AABB centered on entity position)
    Math::Vector3 halfExtents = Math::Vector3(5.0f, 5.0f, 5.0f);

    // Edge falloff
    enum class Falloff : u8 { None, Linear, Smooth };
    Falloff falloff = Falloff::Smooth;

    // What to affect
    bool affectParticles = true;
    bool affectMeshVertices = false;

    // Debug visualization
    bool showDebugArrows = false;
    u32 debugArrowResolution = 4;  // Arrows per axis (4 = 4x4x4 = 64 arrows)

    // Runtime accumulated time (not serialized)
    f32 accumulatedTime = 0.0f;
};

// ============================================================================
// FRACTURE CONFIGURATION (for Voronoi persistent physics fragments)
// ============================================================================

struct FractureConfigComponent {
    // Voronoi fracture parameters
    u32 fragmentCount = 8;               // Number of Voronoi cells
    f32 explosionForce = 5.0f;           // Outward impulse on fragments
    bool persistentFragments = true;     // Fragments become physics entities
    bool allowRefracture = true;         // Fragments can be fractured again
    u32 maxRefractureDepth = 2;          // Max recursion depth
    u32 currentDepth = 0;               // Current recursion depth

    // Fragment entity management
    u32 maxFragmentEntities = 256;       // Global cap on fragment entities
    bool autoCleanup = false;            // Timer-based fragment destruction
    f32 cleanupDelay = 10.0f;            // Seconds before auto-destroy

    // Pre-fracture mode (compute fracture at init, hold with joints)
    bool preFracture = false;
    bool preFractureInitialized = false; // Runtime flag (not serialized)
    f32 jointBreakForce = 100.0f;        // Force to break pre-fracture joints

    // Physics properties for fragments
    f32 fragmentDensity = 1.0f;          // Mass = volume * density
    f32 fragmentFriction = 0.6f;
    f32 fragmentBounciness = 0.1f;

    // Impact bias (fragments cluster around impact point)
    f32 impactBias = 0.5f;              // 0 = uniform, 1 = fully biased to impact
};

// ============================================================================
// GAME OVER
// ============================================================================

// Attach to an entity (typically a singleton "GameManager" entity) to define
// game over / victory behaviour. The GameplayLoop checks for player death
// and enemy elimination to trigger the appropriate state.
struct GameOverComponent {
    bool triggered = false;             // Set when game over is activated
    bool won = false;                   // true = victory, false = defeat

    f32 delay = 1.0f;                   // Seconds before showing game over screen
    f32 delayTimer = 0.0f;             // Runtime countdown (not serialized)
    bool screenVisible = false;        // Runtime: true once delay has elapsed

    std::string victoryMessage = "You Win!";
    std::string defeatMessage = "Game Over";

    bool allowRestart = true;          // Show "Restart" button
    bool returnToMenu = true;          // Show "Main Menu" button

    // Victory condition: all enemies defeated
    // An "enemy" is any entity with both DamageComponent and HealthComponent
    bool victoryOnAllEnemiesDefeated = true;

    // Victory condition: trigger zone reached (set entity ID, 0 = disabled)
    Entity victoryTriggerEntity = 0;   // INVALID_ENTITY

    // Gate the trigger-zone victory on collecting every coin first (a collectathon
    // exit). When true, reaching victoryTriggerEntity only wins once no uncollected
    // PickupType::Coin entities remain. Makes coins matter to the objective.
    bool victoryRequiresAllCoins = false;

    // Runtime: the UICanvas game-over screen has been spawned (not serialized).
    // GameplayLoop::UpdateGameOverState creates it once when the delay elapses.
    bool uiSpawned = false;
};

// ============================================================================
// ANIMATION RECORDING
// ============================================================================

// Records bone transforms over time to create new SkeletalAnimations from gameplay
// or manual posing. Captured keyframes are built into a playable animation when stopped.
struct AnimationRecorderComponent {
    bool recording = false;             // Whether recording is active
    f32 recordInterval = 1.0f / 30.0f;  // Sample rate (seconds between keyframes)
    std::string recordedAnimName = "Recorded"; // Name for the generated animation

    // Runtime state (not serialized — transient recording data)
    f32 timeSinceLastSample = 0.0f;
    f32 totalRecordedTime = 0.0f;

    // Per-bone captured keyframes: boneName -> list of (time, position, rotation, scale)
    struct RecordedKeyframe {
        f32 time = 0.0f;
        Math::Vector3 position;
        Math::Quaternion rotation;
        Math::Vector3 scale = Math::Vector3(1, 1, 1);
    };
    struct RecordedBoneTrack {
        std::string boneName;
        i32 boneIndex = -1;
        std::vector<RecordedKeyframe> keyframes;
    };
    std::vector<RecordedBoneTrack> tracks;

    // Number of recordings completed (for unique naming)
    i32 recordCount = 0;
};

// ============================================================================
// RecordRewindComponent — Braid / Prince of Persia-style time rewind mechanic.
// Attach to the player entity to enable rewinding gameplay. Records transform,
// velocity, and health state at regular intervals. When the player activates
// rewind (key hold), the world scrubs backward through recorded frames.
//
// Usage:
//   auto& rr = world->AddComponent<RecordRewindComponent>(player);
//   rr.maxDuration = 5.0f;        // 5 seconds of rewind buffer
//   rr.rewindKey = KeyCode::R;    // Hold R to rewind
//   rr.rewindSpeed = 2.0f;        // 2x speed rewind
//   rr.cooldown = 3.0f;           // 3 second cooldown after rewind
// ============================================================================
struct RecordRewindComponent {
    // Configuration
    f32 maxDuration = 5.0f;          // Max seconds of rewind buffer
    f32 recordInterval = 1.0f / 20.0f; // 20 frames per second
    f32 rewindSpeed = 2.0f;          // Playback speed multiplier when rewinding
    f32 cooldown = 3.0f;             // Seconds before rewind can be used again after release
    i32 rewindKey = 82;              // KeyCode to hold for rewind (default: R = 82)
    u32 channels = static_cast<u32>(Gameplay::RewindChannelFlags::Default);
    bool enabled = true;

    // Visual feedback
    f32 rewindVignetteStrength = 0.3f;  // Screen edge darkening during rewind
    Math::Vector3 rewindTint = Math::Vector3(0.4f, 0.6f, 1.0f); // Blue tint during rewind

    // State (runtime) — ring buffer replaces std::vector for O(1) eviction
    Gameplay::StateRingBuffer<Gameplay::EntitySnapshot> history;
    f32 recordTimer = 0.0f;
    f32 cooldownTimer = 0.0f;
    bool rewinding = false;
    f32 rewindPlayhead = 0.0f;       // Current position in rewind buffer (seconds from end)
    f32 currentRecordedTime = 0.0f;  // Total elapsed recording time
};

// ============================================================================
// SceneRewindComponent — Prince of Persia: Sands of Time-style scene rewind.
// Attach to a game manager entity to enable whole-scene time rewind.
// Records ALL entity transforms + velocities + health at regular intervals.
// When activated, the entire world scrubs backward simultaneously.
//
// Unlike RecordRewindComponent (per-entity, Braid-style), this rewinds
// everything: enemies, projectiles, pickups, physics objects, particles.
//
// Usage:
//   auto& sr = world->AddComponent<SceneRewindComponent>(gameManager);
//   sr.maxDuration = 10.0f;      // 10 seconds of rewind buffer
//   sr.rewindKey = KeyCode::T;   // Hold T to rewind
//   sr.rewindSpeed = 1.5f;       // 1.5x speed rewind
//   sr.charges = 3;              // Limited uses (0 = unlimited)
// ============================================================================
struct SceneRewindComponent {
    // Configuration
    f32 maxDuration = 10.0f;          // Max seconds of rewind buffer
    f32 recordInterval = 1.0f / 15.0f; // 15 snapshots per second
    f32 rewindSpeed = 1.5f;           // Playback speed when rewinding
    f32 cooldown = 5.0f;              // Seconds before rewind can be used again
    i32 charges = 0;                  // Max uses per level (0 = unlimited)
    i32 rewindKey = 84;               // KeyCode to hold for rewind (default: T = 84)
    u32 channels = static_cast<u32>(Gameplay::RewindChannelFlags::Default);
    u32 keyframeInterval = 30;        // Store full keyframe every N delta frames
    bool enabled = true;
    bool deltaCompression = true;     // Enable delta-only frames between keyframes

    // Visual feedback
    f32 rewindVignetteStrength = 0.5f;
    Math::Vector3 rewindTint = Math::Vector3(0.8f, 0.6f, 0.2f); // Gold tint (Sands of Time)

    // State (runtime) — ring buffer with delta compression
    Gameplay::StateRingBuffer<Gameplay::DeltaFrame> history;
    f32 recordTimer = 0.0f;
    f32 cooldownTimer = 0.0f;
    i32 chargesUsed = 0;
    bool rewinding = false;
    f32 rewindPlayhead = 0.0f;
    f32 currentRecordedTime = 0.0f;
    u32 framesSinceKeyframe = 0;

    // Previous-frame cache for delta detection
    std::unordered_map<Entity, Gameplay::EntitySnapshot> prevFrameCache;
};

// ============================================================================
// FACE CARDS / SPRITE SWAP PER EXPRESSION
// ============================================================================

// FaceCardComponent — sprite swap per expression for dialogue/visual novel.
// Attach to a character entity with a Sprite2DComponent or MeshComponent.
// When the expression changes, the system swaps the texture/sprite to match.
//
// Usage:
//   auto& fc = world->AddComponent<FaceCardComponent>(character);
//   fc.expressions["happy"] = "portraits/hero_happy.png";
//   fc.expressions["sad"] = "portraits/hero_sad.png";
//   fc.expressions["angry"] = "portraits/hero_angry.png";
//   fc.currentExpression = "happy";
struct FaceCardComponent {
    std::unordered_map<std::string, std::string> expressions;  // name -> texture path
    std::string currentExpression;      // Active expression key
    std::string previousExpression;     // Last expression (for change detection)
    f32 transitionDuration = 0.0f;      // Crossfade time (0 = instant swap)
    f32 transitionTimer = 0.0f;         // Runtime: current crossfade progress
    bool flipX = false;                 // Mirror the portrait horizontally
    bool enabled = true;
};

// ============================================================================
// SAVE SYSTEM COMPONENT
// ============================================================================

// SaveSystemComponent — unified save/load configuration for a game.
// Attach to a game manager entity. Configures auto-save, manual save,
// in-world save points, meta-progression, and cloud sync — all in one place.
//
// The underlying TieredSaveSystem handles the actual persistence.
// This component exposes the configuration to the editor inspector,
// serialization, and scripting.
//
// Usage:
//   auto& save = world->AddComponent<SaveSystemComponent>(gameManager);
//   save.maxManualSlots = 10;
//   save.autoSave.enabled = true;
//   save.autoSave.intervalSeconds = 120.0f;
//   save.allowInWorldSavePoints = true;
//
struct SaveSystemComponent {
    // --- Manual Save Slots ---
    u32 maxManualSlots = 17;              // Number of player-accessible save slots
    bool allowManualSave = true;          // Can the player save manually?
    bool allowManualLoad = true;          // Can the player load manually?
    bool allowDeleteSlot = true;          // Can the player delete saves?
    bool showSaveMenuOnPause = true;      // Show save/load UI in pause menu

    // --- Auto-Save ---
    bool autoSaveEnabled = false;         // Enable automatic saving
    bool autoSaveOnSceneTransition = true; // Save when changing scenes
    bool autoSaveOnCheckpoint = true;     // Save when hitting a checkpoint entity
    bool autoSaveOnInterval = false;      // Save on a timer
    f32 autoSaveIntervalSeconds = 300.0f; // Timer interval (default 5 minutes)
    u32 autoSaveSlotCount = 3;            // Rotating auto-save slots

    // --- In-World Save Points ---
    bool allowInWorldSavePoints = true;   // Enable save point entities in the world
    f32 savePointRadius = 2.0f;           // Interaction radius for save point entities
    bool savePointRequiresInput = true;   // Player must press a key (vs auto-save on enter)
    i32 savePointKey = 69;                // KeyCode for save point interaction (default: E = 69)
    bool savePointShowPrompt = true;      // Show "Press E to save" prompt near save points

    // --- Meta-Progression ---
    bool enableMetaProgression = true;    // Persistent data across save slots (unlocks, achievements)
    bool autoSaveMeta = true;             // Auto-save meta on changes

    // --- Cloud Sync ---
    bool enableCloudSync = false;         // Sync saves to cloud backend
    bool syncOnSave = true;              // Upload after each save
    bool syncOnLoad = true;              // Download before each load

    // --- Visual Feedback ---
    bool showSaveIndicator = true;        // Show "Saving..." icon during save
    f32 saveIndicatorDuration = 2.0f;     // How long the save indicator shows
    bool showAutoSaveWarning = true;      // "Don't turn off" message during auto-save

    // --- State (runtime, not serialized) ---
    f32 autoSaveTimer = 0.0f;
    f32 saveIndicatorTimer = 0.0f;
    bool isSaving = false;
    u32 autoSaveRotation = 0;            // Current auto-save slot rotation index
};

// SavePointComponent — attach to an in-world entity (glowing crystal, save
// station, campfire) to create a save trigger. When the player enters the
// radius, they can save the game.
//
// Usage:
//   auto& sp = world->AddComponent<SavePointComponent>(crystal);
//   sp.slotTarget = -1;  // -1 = next available slot
//   sp.saveOnEnter = false;  // Require key press
//
struct SavePointComponent {
    i32 slotTarget = -1;               // Save to specific slot (-1 = next available)
    bool saveOnEnter = false;          // Auto-save when player enters radius (vs key press)
    bool oneTimeUse = false;           // Disable after first use
    bool used = false;                 // Runtime: has this save point been used?
    bool playerInRange = false;        // Runtime: is a player within range?
    f32 radius = 2.0f;                 // Override radius (0 = use SaveSystemComponent default)
    std::string saveMessage = "Game Saved"; // Message shown after saving
};

// ============================================================================
// POSE LIBRARY — Save and recall named bone poses for face rigs, hand gestures, etc.
// ============================================================================
//
// Attach to any entity with an AnimatorComponent + Skeleton.
// Stores named poses (e.g., "smile", "fist", "point") as per-bone local rotation
// overrides. Designed for accessible face/hand rig editing — large buttons, grouped
// by body region, keyboard navigable.
//
// Usage:
//   auto& pl = world->AddComponent<PoseLibraryComponent>(character);
//   pl.AddPose("smile", {{"jaw", q1}, {"lip_upper", q2}, {"lip_lower", q3}});
//   pl.ApplyPose("smile", animator);  // Applies rotations to skeleton
//
struct PoseLibraryComponent {
    // A single bone override within a pose
    struct BoneOverride {
        std::string boneName;
        Math::Quaternion rotation;    // Local rotation
        f32 weight = 1.0f;           // Blend weight (0-1) for partial application
    };

    // A named pose (e.g., "smile", "fist", "relax")
    struct NamedPose {
        std::string name;
        std::string category;         // "Face", "Left Hand", "Right Hand", "Body"
        std::vector<BoneOverride> overrides;
    };

    std::vector<NamedPose> poses;

    // Runtime state
    std::string activePose;           // Currently applied pose name (empty = none)
    f32 blendWeight = 1.0f;          // Master blend weight for pose application
    f32 blendSpeed = 5.0f;           // Interpolation speed (units/sec)

    // Helper: find pose by name
    NamedPose* FindPose(const std::string& name) {
        for (auto& p : poses) { if (p.name == name) return &p; }
        return nullptr;
    }
    const NamedPose* FindPose(const std::string& name) const {
        for (const auto& p : poses) { if (p.name == name) return &p; }
        return nullptr;
    }
};

// ============================================================================
// BONE REGION — Auto-detected body region for accessible bone navigation
// ============================================================================

enum class BoneRegion : u8 {
    Unknown,
    Head,       // head, neck, skull
    Face,       // jaw, eye, lip, brow, cheek, nose, tongue, eyelid
    Spine,      // spine, chest, hips, pelvis
    LeftArm,    // left shoulder, upper arm, forearm
    RightArm,   // right shoulder, upper arm, forearm
    LeftHand,   // left hand, thumb, index, middle, ring, pinky
    RightHand,  // right hand, thumb, index, middle, ring, pinky
    LeftLeg,    // left thigh, calf, shin
    RightLeg,   // right thigh, calf, shin
    LeftFoot,   // left foot, toe
    RightFoot,  // right foot, toe
    Tail,       // tail
    Other
};

// Auto-detect bone region from bone name (case-insensitive substring matching)
inline BoneRegion ClassifyBoneRegion(const std::string& boneName) {
    std::string lower = boneName;
    for (auto& c : lower) c = static_cast<char>(std::tolower(c));

    // Face (check before head — face bones are also in the head)
    if (lower.find("jaw") != std::string::npos || lower.find("eye") != std::string::npos ||
        lower.find("lip") != std::string::npos || lower.find("brow") != std::string::npos ||
        lower.find("cheek") != std::string::npos || lower.find("nose") != std::string::npos ||
        lower.find("tongue") != std::string::npos || lower.find("eyelid") != std::string::npos ||
        lower.find("mouth") != std::string::npos)
        return BoneRegion::Face;

    // Head
    if (lower.find("head") != std::string::npos || lower.find("neck") != std::string::npos ||
        lower.find("skull") != std::string::npos)
        return BoneRegion::Head;

    // Hands (check before arms — hand bones contain "left"/"right" too)
    bool isLeft = lower.find("left") != std::string::npos || lower.find("_l") != std::string::npos ||
                  lower.find(".l") != std::string::npos || lower.find("_l_") != std::string::npos;
    bool isRight = lower.find("right") != std::string::npos || lower.find("_r") != std::string::npos ||
                   lower.find(".r") != std::string::npos || lower.find("_r_") != std::string::npos;

    if (lower.find("thumb") != std::string::npos || lower.find("index") != std::string::npos ||
        lower.find("middle") != std::string::npos || lower.find("ring") != std::string::npos ||
        lower.find("pinky") != std::string::npos || lower.find("finger") != std::string::npos) {
        return isRight ? BoneRegion::RightHand : BoneRegion::LeftHand;
    }
    if (lower.find("hand") != std::string::npos) {
        return isRight ? BoneRegion::RightHand : BoneRegion::LeftHand;
    }

    // Feet
    if (lower.find("foot") != std::string::npos || lower.find("toe") != std::string::npos ||
        lower.find("ball") != std::string::npos) {
        return isRight ? BoneRegion::RightFoot : BoneRegion::LeftFoot;
    }

    // Legs
    if (lower.find("thigh") != std::string::npos || lower.find("calf") != std::string::npos ||
        lower.find("shin") != std::string::npos || lower.find("knee") != std::string::npos ||
        lower.find("upleg") != std::string::npos || lower.find("leg") != std::string::npos) {
        return isRight ? BoneRegion::RightLeg : BoneRegion::LeftLeg;
    }

    // Arms
    if (lower.find("shoulder") != std::string::npos || lower.find("arm") != std::string::npos ||
        lower.find("elbow") != std::string::npos || lower.find("forearm") != std::string::npos ||
        lower.find("clavicle") != std::string::npos) {
        return isRight ? BoneRegion::RightArm : BoneRegion::LeftArm;
    }

    // Spine
    if (lower.find("spine") != std::string::npos || lower.find("chest") != std::string::npos ||
        lower.find("hip") != std::string::npos || lower.find("pelvis") != std::string::npos ||
        lower.find("torso") != std::string::npos)
        return BoneRegion::Spine;

    // Tail
    if (lower.find("tail") != std::string::npos)
        return BoneRegion::Tail;

    return BoneRegion::Other;
}

inline const char* BoneRegionName(BoneRegion region) {
    switch (region) {
        case BoneRegion::Head:      return "Head";
        case BoneRegion::Face:      return "Face";
        case BoneRegion::Spine:     return "Spine";
        case BoneRegion::LeftArm:   return "Left Arm";
        case BoneRegion::RightArm:  return "Right Arm";
        case BoneRegion::LeftHand:  return "Left Hand";
        case BoneRegion::RightHand: return "Right Hand";
        case BoneRegion::LeftLeg:   return "Left Leg";
        case BoneRegion::RightLeg:  return "Right Leg";
        case BoneRegion::LeftFoot:  return "Left Foot";
        case BoneRegion::RightFoot: return "Right Foot";
        case BoneRegion::Tail:      return "Tail";
        case BoneRegion::Other:     return "Other";
        default:                    return "Unknown";
    }
}

} // namespace ECS
} // namespace Enjin
