#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Effects {

// Forward declarations
class WindSystem;
class WeatherSystem;
class SeasonalWeatherSystem;

// Unified elemental particle pool with SoA layout for cache-friendly iteration.
// All elemental effects (fire, water, earth, air) live in one contiguous pool
// and interact via dot-product math against a 4x4 interaction matrix.
struct ElementalPool {
    static constexpr u32 MAX_PARTICLES = 8192;

    // SoA arrays
    Math::Vector3 positions[MAX_PARTICLES];
    Math::Vector3 velocities[MAX_PARTICLES];
    Math::Vector4 elements[MAX_PARTICLES];    // element signature (fire, water, earth, air)
    f32 intensities[MAX_PARTICLES];
    f32 lifetimes[MAX_PARTICLES];
    f32 maxLifetimes[MAX_PARTICLES];
    f32 sizes[MAX_PARTICLES];
    u8  flags[MAX_PARTICLES];                  // bit 0: grounded, bit 1: on-surface, bit 2: decaying
    u32 activeCount = 0;

    // Spatial hash (cell size = 2m)
    // Key = packed cell coordinates, value = particle indices in that cell
    std::unordered_map<u32, std::vector<u16>> spatialHash;

    void RebuildSpatialHash();
    void GetNeighbors(const Math::Vector3& pos, f32 radius, std::vector<u16>& out) const;
};

// Particle flags
enum ElementalParticleFlags : u8 {
    EPF_GROUNDED   = 1 << 0,
    EPF_ON_SURFACE = 1 << 1,
    EPF_DECAYING   = 1 << 2
};

// A representative point light approximating a cluster of fire particles.
// Produced by ElementalSystem::BuildFireLights() for the renderer to inject
// into its transient-light set, which lights both surfaces (PBR point lights)
// and participating media (clustered lighting -> volumetric fog froxels) from
// one source. Decoupled from any renderer type on purpose: the engine renderer
// consumes a generic light list and never depends on this gameplay system.
struct FireLight {
    Math::Vector3 position;
    f32 range = 0.0f;
    Math::Vector3 color;
    f32 intensity = 0.0f;
};

class ENJIN_API ElementalSystem {
public:
    ElementalSystem() = default;
    ~ElementalSystem() = default;

    void Initialize(WindSystem* wind, WeatherSystem* weather, SeasonalWeatherSystem* seasonal = nullptr);
    void Update(ECS::World* world, f32 deltaTime, const Math::Vector3& cameraPos);

    // Spawn API
    u32 SpawnFire(const Math::Vector3& pos, f32 intensity, f32 lifetime);
    u32 SpawnWater(const Math::Vector3& pos, const Math::Vector3& velocity, f32 intensity);
    u32 SpawnEarth(const Math::Vector3& pos, const Math::Vector3& velocity, f32 size);
    u32 SpawnSnow(const Math::Vector3& pos, f32 size);
    u32 SpawnSteam(const Math::Vector3& pos, f32 intensity);
    u32 SpawnCustom(const Math::Vector3& pos, const Math::Vector4& element, f32 intensity, f32 lifetime);

    // Bulk spawners
    void SpawnRainBurst(const Math::Vector3& center, f32 radius, u32 count);
    void SpawnDebrisBurst(const Math::Vector3& center, const Math::Vector3& force, u32 count);

    // Query
    Math::Vector4 SampleElementAt(const Math::Vector3& pos, f32 radius) const;
    f32 GetFireIntensityAt(const Math::Vector3& pos, f32 radius) const;
    f32 GetMoistureAt(const Math::Vector3& pos, f32 radius) const;

    // Stats
    u32 GetActiveCount() const { return m_Pool.activeCount; }
    u32 GetFireCount() const;
    u32 GetWaterCount() const;

    // Access pool for rendering
    const ElementalPool& GetPool() const { return m_Pool; }

    // Maximum number of representative fire lights produced per frame.
    static constexpr u32 MAX_FIRE_LIGHTS = 8;

    // Cluster active fire particles into a small set of flickering point lights.
    // Fills `out` (cleared first) with at most MAX_FIRE_LIGHTS entries, one per
    // dominant fire cell. `time` drives per-cluster flicker (decorrelated so
    // separate fires do not pulse in unison).
    //
    // Single-thread only; call once per frame after Update(). Performs no heap
    // allocations beyond growing `out` once to its capacity — reserve `out` to
    // MAX_FIRE_LIGHTS at the call site to keep the hot path allocation-free.
    //
    // Example:
    //   std::vector<Effects::FireLight> fireLights;
    //   fireLights.reserve(Effects::ElementalSystem::MAX_FIRE_LIGHTS);
    //   elemental.BuildFireLights(worldTime, fireLights);
    //   for (const auto& fl : fireLights)
    //       renderer.AddTransientPointLight(fl.position, fl.range, fl.color, fl.intensity);
    void BuildFireLights(f32 time, std::vector<FireLight>& out);

private:
    ElementalPool m_Pool;

    // Pre-allocated scratch buckets for BuildFireLights — reused each frame so
    // the clustering pass performs no per-frame heap allocation.
    static constexpr u32 MAX_FIRE_BUCKETS = 64;
    struct FireBucket {
        i32 cx, cz;                 // coarse XZ cell coordinates
        Math::Vector3 weightedPos;  // sum of position * fire weight
        f32 mass;                   // sum of fire weight
        f32 maxY;                   // top of the flame column in this cell
    };
    FireBucket m_FireBuckets[MAX_FIRE_BUCKETS];
    u32 m_FireBucketCount = 0;

    WindSystem* m_Wind = nullptr;
    WeatherSystem* m_Weather = nullptr;
    SeasonalWeatherSystem* m_Seasonal = nullptr;

    // 4x4 element interaction matrix (row=source, col=target)
    f32 m_InteractionMatrix[4][4] = {
        //  Fire   Water  Earth  Air
        {  0.3f, -1.0f, -0.2f,  0.5f },  // Fire
        { -1.0f,  0.2f,  0.3f, -0.1f },  // Water
        { -0.2f,  0.3f,  0.0f, -0.3f },  // Earth
        {  0.5f, -0.1f, -0.3f,  0.0f }   // Air
    };

    u32 m_MaxInteractionsPerFrame = 64;
    u32 m_MaxFireParticles = 512;

    // Spawn accumulator for weather input
    f32 m_RainSpawnAccumulator = 0.0f;
    f32 m_SnowSpawnAccumulator = 0.0f;

    // Internal update steps
    void UpdateEmitters(ECS::World* world, f32 dt);
    void UpdateVolumes(ECS::World* world, f32 dt);
    void UpdatePhysics(f32 dt);
    void UpdateInteractions(f32 dt);
    void UpdateSurfaceAccumulation(ECS::World* world, f32 dt);
    void UpdateWeatherInput(f32 dt, const Math::Vector3& cameraPos);
    void UpdateFireSpread(f32 dt);
    void KillExpired();

    // Helpers
    u32 AllocateParticle();
    void FreeParticle(u32 index);
    f32 ComputeReaction(const Math::Vector4& a, const Math::Vector4& b) const;
    void SpawnInteractionProduct(const Math::Vector3& pos, const Math::Vector4& elemA, const Math::Vector4& elemB);
    u32 CountElementParticles(u32 channel) const;

    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
