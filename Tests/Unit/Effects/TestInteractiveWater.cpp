#include "EnjinTest.h"
#include "Enjin/Effects/InteractiveWater.h"

using namespace Enjin;
using namespace Enjin::Effects;

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
