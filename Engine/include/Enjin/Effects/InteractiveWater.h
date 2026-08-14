#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <vector>
#include <cmath>

/**
 * @file InteractiveWater.h
 * @brief Grid-based height-field water simulation inspired by Wave Race 64
 *
 * Distinct from the existing Water3D (Gerstner waves). This system provides
 * gameplay-focused interactive water with object-water collision response,
 * buoyancy, wakes, and splashes via a spring-damper height field on a
 * regular grid.
 */

namespace Enjin {
namespace Effects {

// ============================================================================
// INTERACTIVE WATER COMPONENT (ECS)
// ============================================================================

struct InteractiveWaterComponent {
    // --- Grid settings ---
    i32 gridResolution = 64;          // Vertices per axis (clamped 16-256)
    f32 gridSize = 20.0f;             // World-space extent
    f32 baseHeight = 0.0f;            // Rest water level (Y)

    // --- Wave simulation ---
    f32 waveSpeed = 2.0f;             // Wave propagation speed
    f32 damping = 0.98f;              // Per-step damping (0.95-0.999)
    f32 tension = 0.5f;               // Spring tension between neighbors

    // --- Appearance ---
    Math::Vector3 shallowColor = {0.2f, 0.6f, 0.9f};
    Math::Vector3 deepColor = {0.05f, 0.15f, 0.4f};
    f32 depthColorThreshold = 2.0f;   // Depth at which color fully transitions
    f32 foamThreshold = 0.3f;         // Wave height delta for foam
    Math::Vector3 foamColor = {0.9f, 0.95f, 1.0f};
    f32 opacity = 0.85f;
    f32 uvScrollSpeed = 0.02f;        // Animated surface texture scroll rate
    Math::Vector2 uvScrollDir = {1.0f, 0.5f};
    f32 uvTiling = 1.0f;             // UV tiling factor

    // --- Interaction ---
    f32 interactionRadius = 1.0f;     // Object splash radius in grid cells
    f32 interactionStrength = 0.5f;   // Splash force multiplier
    bool enableBuoyancy = true;       // Apply upward force to objects in water
    f32 buoyancyForce = 9.81f;        // Buoyancy force per unit depth
    f32 waterDrag = 0.5f;             // Drag on submerged objects

    // --- Shoreline ---
    bool enableShoreline = true;
    f32 shorelineDistance = 1.0f;     // Distance from edge for foam band

    // --- Boundary mode ---
    enum class BoundaryMode : u8 {
        Absorbing,   // Edge cells clamped to baseHeight (waves absorbed)
        Reflecting   // Edge cells reflect waves back
    };
    BoundaryMode boundaryMode = BoundaryMode::Absorbing;

    // --- Runtime data (not serialized) ---
    std::vector<f32> heightField;     // Current heights [gridRes * gridRes]
    std::vector<f32> previousField;   // Previous frame heights
    std::vector<f32> velocityField;   // Vertical velocities
    f32 accumulatedScrollTime = 0.0f; // UV scroll accumulator
    bool initialized = false;
};

// ============================================================================
// WATER INTERACTOR TAG COMPONENT
// ============================================================================

struct WaterInteractorComponent {
    f32 splashMultiplier = 1.0f;   // Per-entity splash strength scale
    f32 wakeWidth = 0.5f;          // Width of V-wake in grid cells
    bool generateWake = true;       // Generate continuous wake when moving
    bool applyBuoyancy = true;      // Whether this entity receives buoyancy

    // --- Per-object density (Gobliny buoyancy spec) ---
    // density is relative to water: 1.0 = neutral, < 1.0 floats, > 1.0 sinks. This is
    // what gives buoyancy VARIETY — a flamingo floaty and a grill in the same pool
    // behave differently. Examples: flamingo 0.05, foam noodle 0.1, foam cup 0.2,
    // wooden table 0.6, human 0.95, grill 3.5.
    f32 density = 0.5f;
    // Displaced-volume multiplier on the buoyant force (approximate the object's size;
    // bigger objects get more lift for the same submersion). 1.0 = default.
    f32 volume = 1.0f;
    // Waterlogging: absorbent objects gain effective density while submerged and ride
    // lower over time (cardboard floats, then sinks). 0.0 = never waterlogs.
    f32 waterlogRate = 0.0f;           // effective density gained per second submerged
    f32 waterlogMaxDensity = 1.5f;     // cap the waterlogged effective density
    f32 currentWaterlog = 0.0f;        // runtime accumulator (not authored/serialized)
};

// ============================================================================
// INTERACTIVE WATER SYSTEM
// ============================================================================

class ENJIN_API InteractiveWaterSystem {
public:
    InteractiveWaterSystem() = default;
    ~InteractiveWaterSystem() = default;

    /**
     * @brief Initialize the height field arrays for a water component
     * @param water The component to initialize
     */
    void Initialize(InteractiveWaterComponent& water);

    /**
     * @brief Update all interactive water entities in the world
     *
     * Runs wave propagation, object interaction, buoyancy, and boundary
     * processing for each InteractiveWaterComponent.
     *
     * @param world  ECS world containing entities
     * @param dt     Delta time in seconds
     */
    void Update(ECS::World* world, f32 dt);

    /**
     * @brief Generate mesh data from the current water state
     *
     * Produces a grid mesh with positions from the height field, computed
     * normals, UV coordinates with animated scroll, and vertex colors
     * blended between shallow/deep/foam.
     *
     * @param water      The water component
     * @param transform  World transform of the water entity
     * @return Populated MeshComponent ready for rendering
     */
    ECS::MeshComponent GenerateMesh(const InteractiveWaterComponent& water,
                                     const ECS::TransformComponent& transform) const;

    /**
     * @brief Sample the water height at a world XZ position via bilinear interpolation
     * @param water     The water component
     * @param transform World transform of the water entity
     * @param worldX    X position in world space
     * @param worldZ    Z position in world space
     * @return Interpolated water height in world Y, or baseHeight if out of bounds
     */
    f32 GetWaterHeight(const InteractiveWaterComponent& water,
                       const ECS::TransformComponent& transform,
                       f32 worldX, f32 worldZ) const;

    /**
     * @brief Apply an instantaneous splash impulse at a world position
     * @param water    The water component (modified)
     * @param transform World transform of the water entity
     * @param worldX   X position
     * @param worldZ   Z position
     * @param strength Splash strength (positive = push down)
     */
    void CreateSplash(InteractiveWaterComponent& water,
                      const ECS::TransformComponent& transform,
                      f32 worldX, f32 worldZ, f32 strength);

    /**
     * @brief Apply a continuous V-shaped wake pattern behind a moving object
     * @param water     The water component (modified)
     * @param transform World transform of the water entity
     * @param worldX    Current X position
     * @param worldZ    Current Z position
     * @param velocityX X velocity of the moving object
     * @param velocityZ Z velocity of the moving object
     * @param wakeWidth Width of the wake pattern
     */
    void CreateWake(InteractiveWaterComponent& water,
                    const ECS::TransformComponent& transform,
                    f32 worldX, f32 worldZ,
                    f32 velocityX, f32 velocityZ,
                    f32 wakeWidth);

private:
    /**
     * @brief Run the spring-damper wave propagation step
     */
    void PropagateWaves(InteractiveWaterComponent& water, f32 dt);

    /**
     * @brief Apply boundary conditions (absorbing or reflecting)
     */
    void ApplyBoundary(InteractiveWaterComponent& water);

    /**
     * @brief Convert world XZ to grid coordinates
     * @return true if the position is within the grid bounds
     */
    bool WorldToGrid(const InteractiveWaterComponent& water,
                     const ECS::TransformComponent& transform,
                     f32 worldX, f32 worldZ,
                     f32& gridX, f32& gridZ) const;

    /**
     * @brief Get the cell size for a water component
     */
    f32 GetCellSize(const InteractiveWaterComponent& water) const;

    // Reusable entity list to avoid per-frame allocation
    std::vector<ECS::Entity> m_InteractorCache;
};

} // namespace Effects
} // namespace Enjin
