#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/GUI/DialogueTree.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Enjin {
namespace ECS {

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
};

// Audio Listener - the "ears" of the scene
struct AudioListenerComponent {
    bool isActive = true;
    f32 volumeScale = 1.0f;
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

// Save data component (marks what to save about this entity)
struct SaveDataComponent {
    bool savePosition = true;
    bool saveRotation = true;
    bool saveScale = false;
    bool saveEnabled = true;

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

    // Global ragdoll properties
    f32 gravityScale = 1.0f;
    f32 linearDamping = 0.1f;
    f32 angularDamping = 0.3f;
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

} // namespace ECS
} // namespace Enjin
