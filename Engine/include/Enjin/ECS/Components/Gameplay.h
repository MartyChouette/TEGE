#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>
#include <functional>

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

    // Layer for collision filtering
    u32 layer = 0;
    u32 collisionMask = 0xFFFFFFFF;  // Collides with all layers by default
};

// Sphere Collider
struct SphereColliderComponent {
    Math::Vector3 center = Math::Vector3(0, 0, 0);
    f32 radius = 0.5f;
    bool isTrigger = false;

    f32 friction = 0.5f;
    f32 bounciness = 0.0f;

    u32 layer = 0;
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

    u32 layer = 0;
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

    // State timers
    f32 stateTimer = 0.0f;
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

// Particle Emitter Settings (simplified)
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

    // Texture
    std::string texturePath;
    i32 textureSheetX = 1;         // Animation frames
    i32 textureSheetY = 1;
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
        }
    }
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

    // Zoom limits
    f32 minZoom = 0.5f;
    f32 maxZoom = 3.0f;
    f32 currentZoom = 1.0f;
};

// ============================================================================
// STATE MACHINE (for game logic)
// ============================================================================

// Simple state machine component (great for AI, animations, game states)
struct StateMachineComponent {
    std::string currentState = "idle";
    std::string previousState;
    f32 stateTimer = 0.0f;         // Time in current state
    bool stateJustChanged = false;  // True on first frame of new state

    // State data (can store any state-specific values)
    std::vector<std::pair<std::string, f32>> floatParams;
    std::vector<std::pair<std::string, i32>> intParams;
    std::vector<std::pair<std::string, bool>> boolParams;

    void SetState(const std::string& newState) {
        if (currentState != newState) {
            previousState = currentState;
            currentState = newState;
            stateTimer = 0.0f;
            stateJustChanged = true;
        }
    }

    void SetFloat(const std::string& name, f32 value) {
        for (auto& p : floatParams) {
            if (p.first == name) { p.second = value; return; }
        }
        floatParams.push_back({name, value});
    }

    f32 GetFloat(const std::string& name, f32 defaultValue = 0.0f) const {
        for (const auto& p : floatParams) {
            if (p.first == name) return p.second;
        }
        return defaultValue;
    }

    void SetBool(const std::string& name, bool value) {
        for (auto& p : boolParams) {
            if (p.first == name) { p.second = value; return; }
        }
        boolParams.push_back({name, value});
    }

    bool GetBool(const std::string& name, bool defaultValue = false) const {
        for (const auto& p : boolParams) {
            if (p.first == name) return p.second;
        }
        return defaultValue;
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

} // namespace ECS
} // namespace Enjin
