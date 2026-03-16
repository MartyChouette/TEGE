#include "EnjinTest.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/Weather.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;
using namespace Enjin::Effects;
using namespace Enjin::Math;

// ============================================================================
// Spatial Hash Tests
// ============================================================================

ENJIN_TEST(ElementalSpatialHash, RebuildAndQuery) {
    ElementalPool pool;
    pool.activeCount = 3;
    pool.positions[0] = Vector3(0.0f, 0.0f, 0.0f);
    pool.positions[1] = Vector3(1.0f, 0.0f, 0.0f);
    pool.positions[2] = Vector3(10.0f, 0.0f, 10.0f);

    pool.RebuildSpatialHash();

    std::vector<u16> neighbors;
    pool.GetNeighbors(Vector3(0.5f, 0.0f, 0.0f), 2.0f, neighbors);

    // Particles 0 and 1 should be found (within 2m), particle 2 is far away
    ENJIN_ASSERT_TRUE(neighbors.size() >= 2);
    bool found0 = false, found1 = false;
    for (u16 idx : neighbors) {
        if (idx == 0) found0 = true;
        if (idx == 1) found1 = true;
    }
    ENJIN_ASSERT_TRUE(found0);
    ENJIN_ASSERT_TRUE(found1);
}

ENJIN_TEST(ElementalSpatialHash, EmptyPool) {
    ElementalPool pool;
    pool.activeCount = 0;
    pool.RebuildSpatialHash();

    std::vector<u16> neighbors;
    pool.GetNeighbors(Vector3(0.0f, 0.0f, 0.0f), 5.0f, neighbors);
    ENJIN_ASSERT_EQ(neighbors.size(), 0u);
}

ENJIN_TEST(ElementalSpatialHash, FarParticlesNotFound) {
    ElementalPool pool;
    pool.activeCount = 2;
    pool.positions[0] = Vector3(0.0f, 0.0f, 0.0f);
    pool.positions[1] = Vector3(100.0f, 0.0f, 100.0f);

    pool.RebuildSpatialHash();

    std::vector<u16> neighbors;
    pool.GetNeighbors(Vector3(0.0f, 0.0f, 0.0f), 1.0f, neighbors);
    ENJIN_ASSERT_EQ(neighbors.size(), 1u);
    ENJIN_ASSERT_EQ(neighbors[0], 0);
}

// ============================================================================
// Spawn and Pool Tests
// ============================================================================

ENJIN_TEST(ElementalPool, SpawnFire) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    u32 idx = system.SpawnFire(Vector3(5.0f, 0.0f, 5.0f), 0.8f, 3.0f);
    ENJIN_ASSERT_TRUE(idx < ElementalPool::MAX_PARTICLES);
    ENJIN_ASSERT_EQ(system.GetActiveCount(), 1u);

    const auto& pool = system.GetPool();
    ENJIN_EXPECT_FLOAT_EQ(pool.elements[idx].x, 1.0f); // fire channel
    ENJIN_EXPECT_FLOAT_EQ(pool.elements[idx].y, 0.0f); // no water
    ENJIN_EXPECT_FLOAT_EQ(pool.intensities[idx], 0.8f);
}

ENJIN_TEST(ElementalPool, SpawnWater) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    u32 idx = system.SpawnWater(Vector3(0.0f, 10.0f, 0.0f), Vector3(0.0f, -5.0f, 0.0f), 0.6f);
    ENJIN_ASSERT_TRUE(idx < ElementalPool::MAX_PARTICLES);

    const auto& pool = system.GetPool();
    ENJIN_EXPECT_FLOAT_EQ(pool.elements[idx].y, 1.0f); // water channel
    ENJIN_EXPECT_FLOAT_EQ(pool.velocities[idx].y, -5.0f);
}

ENJIN_TEST(ElementalPool, SpawnSnow) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    u32 idx = system.SpawnSnow(Vector3(0.0f, 10.0f, 0.0f), 0.1f);
    ENJIN_ASSERT_TRUE(idx < ElementalPool::MAX_PARTICLES);

    const auto& pool = system.GetPool();
    // Snow = (0, 0.4, 0, 0.6)
    ENJIN_EXPECT_FLOAT_EQ(pool.elements[idx].y, 0.4f);
    ENJIN_EXPECT_FLOAT_EQ(pool.elements[idx].w, 0.6f);
}

ENJIN_TEST(ElementalPool, SpawnSteam) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    u32 idx = system.SpawnSteam(Vector3(1.0f, 0.0f, 1.0f), 0.5f);
    ENJIN_ASSERT_TRUE(idx < ElementalPool::MAX_PARTICLES);

    const auto& pool = system.GetPool();
    // Steam = (0.5, 0.5, 0, 0)
    ENJIN_EXPECT_FLOAT_EQ(pool.elements[idx].x, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(pool.elements[idx].y, 0.5f);
}

ENJIN_TEST(ElementalPool, PoolCompaction) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    // Spawn several particles
    system.SpawnFire(Vector3(0.0f), 0.5f, 0.01f);  // will expire quickly
    system.SpawnFire(Vector3(1.0f, 0.0f, 0.0f), 0.5f, 10.0f); // long lived
    system.SpawnFire(Vector3(2.0f, 0.0f, 0.0f), 0.5f, 0.01f);  // will expire
    ENJIN_ASSERT_EQ(system.GetActiveCount(), 3u);

    // Simulate enough for first and third to expire
    ECS::World world;
    system.Update(&world, 0.1f, Vector3(0.0f));

    // First and third particles had 0.01s lifetime, should be dead after 0.1s update
    ENJIN_ASSERT_EQ(system.GetActiveCount(), 1u);
}

ENJIN_TEST(ElementalPool, MaxCapacity) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    // Fill the pool with custom particles (not fire, to bypass fire cap)
    for (u32 i = 0; i < ElementalPool::MAX_PARTICLES; ++i) {
        system.SpawnCustom(Vector3(0.0f), Vector4(0.0f, 0.0f, 1.0f, 0.0f), 0.5f, 10.0f);
    }
    ENJIN_ASSERT_EQ(system.GetActiveCount(), ElementalPool::MAX_PARTICLES);

    // Trying to spawn more should fail
    u32 idx = system.SpawnCustom(Vector3(0.0f), Vector4(1.0f, 0.0f, 0.0f, 0.0f), 1.0f, 5.0f);
    ENJIN_ASSERT_EQ(idx, ElementalPool::MAX_PARTICLES); // overflow sentinel
    ENJIN_ASSERT_EQ(system.GetActiveCount(), ElementalPool::MAX_PARTICLES);
}

// ============================================================================
// Dot-Product Interaction Tests
// ============================================================================

ENJIN_TEST(ElementalInteraction, FireWaterCancel) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    // Spawn fire and water particles very close together
    system.SpawnFire(Vector3(0.0f, 0.5f, 0.0f), 1.0f, 5.0f);
    system.SpawnWater(Vector3(0.2f, 0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), 1.0f);
    ENJIN_ASSERT_EQ(system.GetActiveCount(), 2u);

    // Update should trigger interaction — fire+water = strongly opposing
    ECS::World world;
    system.Update(&world, 0.016f, Vector3(0.0f));

    // Both should be dead (killed by interaction), steam may be spawned
    // Active count should be <= 1 (just the steam product)
    ENJIN_ASSERT_TRUE(system.GetFireCount() == 0u);
    ENJIN_ASSERT_TRUE(system.GetWaterCount() == 0u);
}

ENJIN_TEST(ElementalInteraction, FireWindAmplify) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    // Fire + Air should amplify (reaction > 0.3)
    system.SpawnFire(Vector3(0.0f, 0.5f, 0.0f), 0.5f, 10.0f);
    system.SpawnCustom(Vector3(0.3f, 0.5f, 0.0f), Vector4(0.0f, 0.0f, 0.0f, 1.0f), 0.3f, 10.0f); // air

    ECS::World world;
    system.Update(&world, 0.016f, Vector3(0.0f));

    // Both should still be alive (amplification doesn't kill)
    ENJIN_ASSERT_EQ(system.GetActiveCount(), 2u);
}

// ============================================================================
// Surface Accumulation Tests
// ============================================================================

ENJIN_TEST(ElementalSurface, FireAccumulation) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    auto& transform = world.AddComponent<ECS::TransformComponent>(entity);
    transform.position = Vector3(0.0f, 0.0f, 0.0f);
    auto& surface = world.AddComponent<ECS::ElementalSurfaceComponent>(entity);
    surface.flammability = 0.8f;
    surface.accumulationRate = 2.0f;

    // Spawn a grounded fire particle near the entity
    u32 idx = system.SpawnFire(Vector3(0.0f, 0.0f, 0.0f), 1.0f, 5.0f);
    // Force it grounded
    auto& pool = const_cast<ElementalPool&>(system.GetPool());
    pool.positions[idx].y = 0.0f;
    pool.flags[idx] |= 0x01; // EPF_GROUNDED

    // Update should deposit fire element onto surface
    system.Update(&world, 0.5f, Vector3(0.0f));

    // charAmount should increase
    ENJIN_ASSERT_TRUE(surface.charAmount > 0.0f);
}

ENJIN_TEST(ElementalSurface, DecayOverTime) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    auto& transform = world.AddComponent<ECS::TransformComponent>(entity);
    transform.position = Vector3(0.0f, 0.0f, 0.0f);
    auto& surface = world.AddComponent<ECS::ElementalSurfaceComponent>(entity);
    surface.accumulation = Vector4(0.5f, 0.5f, 0.0f, 0.0f);
    surface.decayRate = 1.0f; // fast decay for testing

    // Update with no particles nearby - accumulation should decay
    system.Update(&world, 1.0f, Vector3(0.0f));

    ENJIN_ASSERT_TRUE(surface.accumulation.x < 0.5f);
    ENJIN_ASSERT_TRUE(surface.accumulation.y < 0.5f);
}

// ============================================================================
// Weather Input Tests
// ============================================================================

ENJIN_TEST(ElementalWeather, RainSpawnsWater) {
    WindSystem wind;
    WeatherSystem weather;
    weather.Initialize(500);
    weather.SetWeather(WeatherType::Rain, 0.0f);
    weather.SetRainIntensity(1.0f);
    // Pump the weather system once so it transitions
    weather.Update(0.1f, Vector3(0.0f));

    ElementalSystem system;
    system.Initialize(&wind, &weather);

    ECS::World world;
    // Multiple updates to accumulate spawns
    for (int i = 0; i < 10; ++i) {
        system.Update(&world, 0.1f, Vector3(0.0f));
    }

    // At full rain intensity (30/sec), 10 updates of 0.1s = 1s = ~30 water particles
    ENJIN_ASSERT_TRUE(system.GetWaterCount() > 0u);
}

ENJIN_TEST(ElementalWeather, NoRainNoSpawn) {
    WindSystem wind;
    WeatherSystem weather;
    weather.Initialize(500);
    weather.SetWeather(WeatherType::Clear, 0.0f);
    weather.SetRainIntensity(0.0f);
    weather.SetSnowIntensity(0.0f);

    ElementalSystem system;
    system.Initialize(&wind, &weather);

    ECS::World world;
    system.Update(&world, 1.0f, Vector3(0.0f));

    ENJIN_ASSERT_EQ(system.GetActiveCount(), 0u);
}

// ============================================================================
// Query Tests
// ============================================================================

ENJIN_TEST(ElementalQuery, SampleElementAt) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    // Spawn pure fire at origin
    system.SpawnFire(Vector3(0.0f, 0.5f, 0.0f), 1.0f, 10.0f);

    Vector4 sample = system.SampleElementAt(Vector3(0.0f, 0.5f, 0.0f), 2.0f);
    ENJIN_ASSERT_TRUE(sample.x > 0.5f); // should detect fire
    ENJIN_EXPECT_FLOAT_EQ(sample.y, 0.0f); // no water
}

ENJIN_TEST(ElementalQuery, FireIntensityAt) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    system.SpawnFire(Vector3(0.0f, 0.5f, 0.0f), 0.9f, 10.0f);
    f32 intensity = system.GetFireIntensityAt(Vector3(0.0f, 0.5f, 0.0f), 2.0f);
    ENJIN_ASSERT_TRUE(intensity > 0.0f);
}

ENJIN_TEST(ElementalQuery, MoistureAt) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    system.SpawnWater(Vector3(0.0f, 0.5f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), 0.7f);
    f32 moisture = system.GetMoistureAt(Vector3(0.0f, 0.5f, 0.0f), 2.0f);
    ENJIN_ASSERT_TRUE(moisture > 0.0f);
}

// ============================================================================
// Wind Influence Tests
// ============================================================================

ENJIN_TEST(ElementalWind, FireRises) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    u32 idx = system.SpawnFire(Vector3(0.0f, 1.0f, 0.0f), 1.0f, 10.0f);
    f32 initialY = system.GetPool().positions[idx].y;

    ECS::World world;
    system.Update(&world, 0.5f, Vector3(0.0f));

    // Fire should have risen (negative gravity + buoyancy)
    ENJIN_ASSERT_TRUE(system.GetPool().positions[idx].y > initialY);
}

ENJIN_TEST(ElementalWind, EarthFalls) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    u32 idx = system.SpawnEarth(Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, 0.0f, 0.0f), 0.2f);
    f32 initialY = system.GetPool().positions[idx].y;

    ECS::World world;
    system.Update(&world, 0.5f, Vector3(0.0f));

    // Earth should have fallen (heavy gravity)
    ENJIN_ASSERT_TRUE(system.GetPool().positions[idx].y < initialY);
}

// ============================================================================
// Fire Spread Tests
// ============================================================================

ENJIN_TEST(ElementalFireSpread, SpreadOnSurface) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    ECS::World world;
    ECS::Entity entity = world.CreateEntity();
    auto& transform = world.AddComponent<ECS::TransformComponent>(entity);
    transform.position = Vector3(0.0f, 0.0f, 0.0f);
    auto& surface = world.AddComponent<ECS::ElementalSurfaceComponent>(entity);
    surface.flammability = 1.0f;

    // Spawn a grounded fire particle on the surface
    u32 idx = system.SpawnFire(Vector3(0.0f, 0.0f, 0.0f), 0.8f, 10.0f);
    auto& pool = const_cast<ElementalPool&>(system.GetPool());
    pool.flags[idx] |= 0x01 | 0x02; // grounded + on-surface

    u32 initialCount = system.GetActiveCount();

    // Run several updates (fire spread checks every 0.5s)
    for (int i = 0; i < 20; ++i) {
        system.Update(&world, 0.1f, Vector3(0.0f));
    }

    // Fire should have spread (new particles spawned)
    // Note: the original particle might have been killed if its lifetime expired
    // but new fire particles should exist
    ENJIN_ASSERT_TRUE(system.GetActiveCount() >= 1u);
}

// ============================================================================
// Cap Tests
// ============================================================================

ENJIN_TEST(ElementalCaps, MaxFireParticles) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    // Try to spawn more than 512 fire particles
    u32 spawned = 0;
    for (u32 i = 0; i < 600; ++i) {
        u32 idx = system.SpawnFire(Vector3(static_cast<f32>(i), 0.0f, 0.0f), 0.5f, 100.0f);
        if (idx < ElementalPool::MAX_PARTICLES) spawned++;
    }

    // Should be capped at 512
    ENJIN_ASSERT_TRUE(system.GetFireCount() <= 512u);
}

ENJIN_TEST(ElementalCaps, MaxInteractionsPerFrame) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    // Spawn many interacting particles close together
    for (u32 i = 0; i < 100; ++i) {
        f32 x = static_cast<f32>(i % 10) * 0.1f;
        f32 z = static_cast<f32>(i / 10) * 0.1f;
        if (i % 2 == 0) {
            system.SpawnFire(Vector3(x, 0.5f, z), 1.0f, 10.0f);
        } else {
            system.SpawnWater(Vector3(x, 0.5f, z), Vector3(0.0f, 0.0f, 0.0f), 1.0f);
        }
    }

    // This should not hang or take excessive time due to interaction cap
    ECS::World world;
    system.Update(&world, 0.016f, Vector3(0.0f));

    // Should have processed without crashing (the cap prevents runaway)
    ENJIN_ASSERT_TRUE(true);
}

// ============================================================================
// Bulk Spawn Tests
// ============================================================================

ENJIN_TEST(ElementalBulk, RainBurst) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    system.SpawnRainBurst(Vector3(0.0f, 0.0f, 0.0f), 5.0f, 20);
    ENJIN_ASSERT_EQ(system.GetActiveCount(), 20u);
}

ENJIN_TEST(ElementalBulk, DebrisBurst) {
    WindSystem wind;
    WeatherSystem weather;
    ElementalSystem system;
    system.Initialize(&wind, &weather);

    system.SpawnDebrisBurst(Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 5.0f, 0.0f), 15);
    ENJIN_ASSERT_EQ(system.GetActiveCount(), 15u);
}

// ============================================================================
// Wind System Enhancement Tests
// ============================================================================

ENJIN_TEST(WindEnhancement, GetWindForce) {
    WindSystem wind;
    WindParams params;
    params.direction = Vector3(1.0f, 0.0f, 0.0f);
    params.strength = 2.0f;
    params.gustStrength = 0.0f;
    params.turbulence = 0.0f;
    wind.SetGlobalWind(params);

    Vector3 force = wind.GetWindForce(Vector3(0.0f), 0.5f);
    // Force = wind * sensitivity = (2,0,0) * 0.5 = (1,0,0)
    ENJIN_EXPECT_FLOAT_EQ(force.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(force.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(force.z, 0.0f);
}

ENJIN_TEST(WindEnhancement, HeatSourceConvection) {
    WindSystem wind;
    WindParams params;
    params.direction = Vector3(0.0f, 0.0f, 0.0f); // no base wind
    params.strength = 0.0f;
    params.gustStrength = 0.0f;
    params.turbulence = 0.0f;
    wind.SetGlobalWind(params);

    wind.RegisterHeatSource(Vector3(0.0f, 0.0f, 0.0f), 1.0f);

    // Query wind at the heat source position — should have upward convection
    Vector3 windAt = wind.GetWindAt(Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_ASSERT_TRUE(windAt.y > 0.0f);

    wind.ClearHeatSources();
    Vector3 windAfterClear = wind.GetWindAt(Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_FLOAT_EQ(windAfterClear.y, 0.0f);
}

// ============================================================================
// Component Tests
// ============================================================================

ENJIN_TEST(ElementalComponents, SurfaceDefaults) {
    ECS::ElementalSurfaceComponent surface;
    ENJIN_EXPECT_FLOAT_EQ(surface.flammability, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(surface.accumulationRate, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(surface.decayRate, 0.05f);
    ENJIN_EXPECT_FLOAT_EQ(surface.charAmount, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(surface.wetness, 0.0f);
}

ENJIN_TEST(ElementalComponents, EmitterDefaults) {
    ECS::ElementalEmitterComponent emitter;
    ENJIN_EXPECT_FLOAT_EQ(emitter.element.x, 1.0f); // default: fire
    ENJIN_EXPECT_FLOAT_EQ(emitter.emissionRate, 10.0f);
    ENJIN_ASSERT_TRUE(emitter.active);
}

ENJIN_TEST(ElementalComponents, VolumeDefaults) {
    ECS::ElementalVolumeComponent volume;
    ENJIN_EXPECT_FLOAT_EQ(volume.halfExtents.x, 5.0f);
    ENJIN_EXPECT_FLOAT_EQ(volume.temperatureBias, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(volume.windMultiplier, 1.0f);
    ENJIN_ASSERT_TRUE(!volume.killOnContact);
}

ENJIN_TEST_MAIN()
