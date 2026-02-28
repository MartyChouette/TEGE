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

private:
    ElementalPool m_Pool;
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
