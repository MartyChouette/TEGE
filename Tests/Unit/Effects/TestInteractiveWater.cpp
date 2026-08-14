#include "EnjinTest.h"
#include "Enjin/Effects/InteractiveWater.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

using namespace Enjin;
using namespace Enjin::Effects;

// Build a water body at the origin with its surface at y=0, plus one interactor with a
// dynamic rigidbody submerged 1 unit below the surface and the given density. Returns the
// interactor entity so the caller can inspect its velocity after sys.Update().
static ECS::Entity MakeSubmergedInteractor(ECS::World& world, f32 density, f32 waterlogRate = 0.0f) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent t;
    t.position = Math::Vector3(0.0f, -1.0f, 0.0f);   // below the y=0 surface -> submerged
    t.scale = Math::Vector3(1, 1, 1);
    world.AddComponent<ECS::TransformComponent>(e, t);

    ECS::RigidbodyComponent rb;
    rb.bodyType = ECS::RigidbodyComponent::BodyType::Dynamic;
    rb.velocity = Math::Vector3(0, 0, 0);
    world.AddComponent<ECS::RigidbodyComponent>(e, rb);

    WaterInteractorComponent wi;
    wi.density = density;
    wi.waterlogRate = waterlogRate;
    wi.generateWake = false;
    world.AddComponent<WaterInteractorComponent>(e, wi);
    return e;
}

// ===========================================================================
// InteractiveWaterComponent Defaults
// ===========================================================================

ENJIN_TEST(Defaults, GridSettings) {
    InteractiveWaterComponent water;
    ENJIN_EXPECT_EQ(water.gridResolution, 64);
    ENJIN_EXPECT_FLOAT_EQ(water.gridSize, 20.0f);
    ENJIN_EXPECT_FLOAT_EQ(water.baseHeight, 0.0f);
}

ENJIN_TEST(Defaults, WaveParams) {
    InteractiveWaterComponent water;
    ENJIN_EXPECT_FLOAT_EQ(water.waveSpeed, 2.0f);
    ENJIN_EXPECT_FLOAT_NEAR(water.damping, 0.98f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(water.tension, 0.5f, 0.01f);
}

ENJIN_TEST(Defaults, Interaction) {
    InteractiveWaterComponent water;
    ENJIN_EXPECT_FLOAT_EQ(water.interactionRadius, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(water.interactionStrength, 0.5f, 0.01f);
    ENJIN_EXPECT_TRUE(water.enableBuoyancy);
    ENJIN_EXPECT_FLOAT_NEAR(water.buoyancyForce, 9.81f, 0.01f);
}

ENJIN_TEST(Defaults, Appearance) {
    InteractiveWaterComponent water;
    ENJIN_EXPECT_FLOAT_NEAR(water.opacity, 0.85f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(water.foamThreshold, 0.3f, 0.01f);
}

ENJIN_TEST(Defaults, BoundaryMode) {
    InteractiveWaterComponent water;
    ENJIN_EXPECT_EQ((int)water.boundaryMode,
                    (int)InteractiveWaterComponent::BoundaryMode::Absorbing);
}

ENJIN_TEST(Defaults, RuntimeNotInitialized) {
    InteractiveWaterComponent water;
    ENJIN_EXPECT_FALSE(water.initialized);
    ENJIN_EXPECT_EQ(water.heightField.size(), (size_t)0);
    ENJIN_EXPECT_EQ(water.previousField.size(), (size_t)0);
    ENJIN_EXPECT_EQ(water.velocityField.size(), (size_t)0);
}

// ===========================================================================
// Initialize
// ===========================================================================

ENJIN_TEST(Init, AllocatesFields) {
    InteractiveWaterComponent water;
    water.gridResolution = 32;
    InteractiveWaterSystem sys;
    sys.Initialize(water);

    ENJIN_EXPECT_TRUE(water.initialized);
    ENJIN_EXPECT_EQ(water.heightField.size(), (size_t)(32 * 32));
    ENJIN_EXPECT_EQ(water.previousField.size(), (size_t)(32 * 32));
    ENJIN_EXPECT_EQ(water.velocityField.size(), (size_t)(32 * 32));
}

ENJIN_TEST(Init, FieldsStartAtBaseHeight) {
    InteractiveWaterComponent water;
    water.gridResolution = 16;
    water.baseHeight = 5.0f;
    InteractiveWaterSystem sys;
    sys.Initialize(water);

    for (auto h : water.heightField) {
        ENJIN_EXPECT_FLOAT_EQ(h, 5.0f);
    }
}

// ===========================================================================
// WaterInteractorComponent Defaults
// ===========================================================================

ENJIN_TEST(Interactor, Defaults) {
    WaterInteractorComponent interactor;
    ENJIN_EXPECT_FLOAT_EQ(interactor.splashMultiplier, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(interactor.wakeWidth, 0.5f, 0.01f);
    ENJIN_EXPECT_TRUE(interactor.generateWake);
    ENJIN_EXPECT_TRUE(interactor.applyBuoyancy);
}

ENJIN_TEST(Interactor, DensityDefaults) {
    WaterInteractorComponent interactor;
    ENJIN_EXPECT_FLOAT_NEAR(interactor.density, 0.5f, 0.001f);
    ENJIN_EXPECT_FLOAT_EQ(interactor.volume, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(interactor.waterlogRate, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(interactor.currentWaterlog, 0.0f);
}

// ===========================================================================
// Per-object buoyancy density (Gobliny spec)
// ===========================================================================

ENJIN_TEST(Buoyancy, LightObjectRisesDenseObjectSinks) {
    // A light object (density < 1) must get an UPWARD velocity in water; a dense
    // object (density > 1) must get a DOWNWARD velocity, from the same submersion.
    // This is the per-object variety the density model exists for.
    ECS::World world;
    InteractiveWaterSystem sys;

    ECS::Entity waterE = world.CreateEntity();
    ECS::TransformComponent wt; wt.position = Math::Vector3(0, 0, 0); wt.scale = Math::Vector3(1, 1, 1);
    world.AddComponent<ECS::TransformComponent>(waterE, wt);
    InteractiveWaterComponent water; water.gridResolution = 16; water.baseHeight = 0.0f;
    world.AddComponent<InteractiveWaterComponent>(waterE, water);

    ECS::Entity light = MakeSubmergedInteractor(world, 0.1f);   // foam noodle -> floats
    ECS::Entity heavy = MakeSubmergedInteractor(world, 3.5f);   // grill -> sinks

    sys.Update(&world, 0.016f);

    ENJIN_ASSERT_TRUE(world.GetComponent<ECS::RigidbodyComponent>(light)->velocity.y > 0.0f);
    ENJIN_ASSERT_TRUE(world.GetComponent<ECS::RigidbodyComponent>(heavy)->velocity.y < 0.0f);
}

ENJIN_TEST(Buoyancy, WaterlogRaisesEffectiveDensityOverTime) {
    // A buoyant but absorbent object waterlogs while submerged: currentWaterlog climbs
    // toward waterlogMaxDensity, so its upward force weakens step over step.
    ECS::World world;
    InteractiveWaterSystem sys;

    ECS::Entity waterE = world.CreateEntity();
    ECS::TransformComponent wt; wt.position = Math::Vector3(0, 0, 0); wt.scale = Math::Vector3(1, 1, 1);
    world.AddComponent<ECS::TransformComponent>(waterE, wt);
    InteractiveWaterComponent water; water.gridResolution = 16; water.baseHeight = 0.0f;
    world.AddComponent<InteractiveWaterComponent>(waterE, water);

    ECS::Entity cardboard = MakeSubmergedInteractor(world, 0.4f, /*waterlogRate=*/2.0f);

    // Run several steps; the interactor stays submerged (buoyancy only nudges velocity,
    // it does not integrate position here), so waterlog keeps accumulating.
    for (int i = 0; i < 30; ++i) sys.Update(&world, 0.016f);

    auto* wi = world.GetComponent<WaterInteractorComponent>(cardboard);
    ENJIN_ASSERT_TRUE(wi->currentWaterlog > 0.0f);
    // Effective density (0.4 + waterlog) must not exceed the configured cap.
    ENJIN_ASSERT_TRUE(0.4f + wi->currentWaterlog <= wi->waterlogMaxDensity + 0.001f);
}

// ===========================================================================
// GetWaterHeight
// ===========================================================================

ENJIN_TEST(Height, ReturnsBaseHeightWhenFlat) {
    InteractiveWaterComponent water;
    water.gridResolution = 16;
    water.baseHeight = 3.0f;
    InteractiveWaterSystem sys;
    sys.Initialize(water);

    ECS::TransformComponent transform;
    transform.position = Math::Vector3(0, 0, 0);
    transform.scale = Math::Vector3(1, 1, 1);

    f32 h = sys.GetWaterHeight(water, transform, 5.0f, 5.0f);
    ENJIN_EXPECT_FLOAT_NEAR(h, 3.0f, 0.1f);
}

// ===========================================================================
// CreateSplash
// ===========================================================================

ENJIN_TEST(Splash, ModifiesVelocityField) {
    InteractiveWaterComponent water;
    water.gridResolution = 32;
    water.baseHeight = 0.0f;
    InteractiveWaterSystem sys;
    sys.Initialize(water);

    ECS::TransformComponent transform;
    transform.position = Math::Vector3(0, 0, 0);
    transform.scale = Math::Vector3(1, 1, 1);

    // Splash at grid center (water centered at origin, gridSize=20 → valid range ~(-10,10))
    sys.CreateSplash(water, transform, 0.0f, 0.0f, 5.0f);

    // CreateSplash adds impulse to velocityField (heights change after Update)
    bool anyVelocity = false;
    for (auto v : water.velocityField) {
        if (std::abs(v) > 0.01f) { anyVelocity = true; break; }
    }
    ENJIN_EXPECT_TRUE(anyVelocity);
}

ENJIN_TEST_MAIN()
