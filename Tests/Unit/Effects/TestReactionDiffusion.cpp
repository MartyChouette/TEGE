#include "EnjinTest.h"
#include "Enjin/Effects/ReactionDiffusion.h"

#include <cmath>
#include <vector>

using namespace Enjin;
using namespace Enjin::Effects;

// ===========================================================================
// RDConfig Defaults
// ===========================================================================

ENJIN_TEST(Config, Defaults) {
    RDConfig cfg;
    ENJIN_EXPECT_EQ(cfg.width, 256u);
    ENJIN_EXPECT_EQ(cfg.height, 256u);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.feedRate, 0.055f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.killRate, 0.062f, 0.001f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.diffusionU, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(cfg.diffusionV, 0.5f, 0.01f);
    ENJIN_EXPECT_FLOAT_EQ(cfg.deltaTime, 1.0f);
    ENJIN_EXPECT_EQ(cfg.stepsPerFrame, 10u);
    ENJIN_EXPECT_TRUE(cfg.wrapEdges);
}

// ===========================================================================
// Preset Configs
// ===========================================================================

ENJIN_TEST(Presets, AllPresetsHaveNames) {
    for (int i = 0; i <= (int)RDPreset::Custom; ++i) {
        const char* name = ReactionDiffusion::GetPresetName((RDPreset)i);
        ENJIN_EXPECT_NOT_NULL(name);
        ENJIN_EXPECT_GT(strlen(name), (size_t)0);
    }
}

ENJIN_TEST(Presets, ConfigsHavePositiveRates) {
    RDPreset presets[] = {
        RDPreset::MitosisSpots, RDPreset::CoralGrowth, RDPreset::Fingerprints,
        RDPreset::Leopard, RDPreset::Labyrinth, RDPreset::WormHoles,
        RDPreset::BubblePacking, RDPreset::Spirals
    };
    for (auto p : presets) {
        RDConfig cfg = ReactionDiffusion::GetPresetConfig(p);
        ENJIN_EXPECT_GT(cfg.feedRate, 0.0f);
        ENJIN_EXPECT_GT(cfg.killRate, 0.0f);
    }
}

// ===========================================================================
// Initialize & State
// ===========================================================================

ENJIN_TEST(Init, AllocatesGrid) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 32;
    cfg.height = 32;
    rd.Initialize(cfg);

    const RDState& state = rd.GetState();
    ENJIN_EXPECT_EQ(state.width, 32u);
    ENJIN_EXPECT_EQ(state.height, 32u);
    ENJIN_EXPECT_EQ(state.chemU.size(), (size_t)(32 * 32));
    ENJIN_EXPECT_EQ(state.chemV.size(), (size_t)(32 * 32));
    ENJIN_EXPECT_EQ(state.stepCount, 0u);
}

ENJIN_TEST(Init, ChemUStartsAtOne) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 16;
    cfg.height = 16;
    rd.Initialize(cfg);

    // U should start at ~1.0 (uniform substrate)
    const RDState& state = rd.GetState();
    ENJIN_EXPECT_FLOAT_NEAR(state.chemU[0], 1.0f, 0.01f);
}

ENJIN_TEST(Init, ChemVStartsAtZeroForCustom) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 16;
    cfg.height = 16;
    cfg.preset = RDPreset::Custom;  // Custom preset does no auto-seeding
    rd.Initialize(cfg);

    // V should start at 0.0 (no activator) when no preset seeds the grid
    const RDState& state = rd.GetState();
    ENJIN_EXPECT_FLOAT_NEAR(state.chemV[0], 0.0f, 0.01f);
}

// ===========================================================================
// Seeding
// ===========================================================================

ENJIN_TEST(Seed, CircleAddsActivator) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 64;
    cfg.height = 64;
    rd.Initialize(cfg);

    // SeedCircle uses pixel coordinates, not normalized
    rd.SeedCircle(32.0f, 32.0f, 6.0f);

    // Center of grid should now have V > 0
    const RDState& state = rd.GetState();
    u32 cx = 32, cy = 32;
    f32 centerV = state.chemV[cy * 64 + cx];
    ENJIN_EXPECT_GT(centerV, 0.0f);
}

ENJIN_TEST(Seed, RandomAddsMultipleSpots) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 64;
    cfg.height = 64;
    cfg.seed = 42;
    rd.Initialize(cfg);

    rd.SeedRandom(10, 2.0f);

    // At least some cells should have V > 0
    const RDState& state = rd.GetState();
    i32 nonZero = 0;
    for (auto v : state.chemV) {
        if (v > 0.01f) nonZero++;
    }
    ENJIN_EXPECT_GT(nonZero, 0);
}

// ===========================================================================
// Simulation Stepping
// ===========================================================================

ENJIN_TEST(Step, StepCountIncrements) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 16;
    cfg.height = 16;
    rd.Initialize(cfg);

    rd.Step();
    ENJIN_EXPECT_EQ(rd.GetState().stepCount, 1u);
    rd.Step();
    ENJIN_EXPECT_EQ(rd.GetState().stepCount, 2u);
}

ENJIN_TEST(Step, UpdateRunsMultipleSteps) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 16;
    cfg.height = 16;
    cfg.stepsPerFrame = 5;
    rd.Initialize(cfg);

    rd.Update(0.016f);
    ENJIN_EXPECT_EQ(rd.GetState().stepCount, 5u);
}

ENJIN_TEST(Step, SimulationModifiesGrid) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 32;
    cfg.height = 32;
    cfg.stepsPerFrame = 1;
    cfg.preset = RDPreset::Custom;
    rd.Initialize(cfg);
    rd.SeedCircle(16.0f, 16.0f, 4.0f);

    // Snapshot entire V grid before stepping
    std::vector<f32> vBefore = rd.GetState().chemV;

    for (int i = 0; i < 50; ++i) rd.Step();

    // After 50 steps, at least some cells should have changed
    const auto& vAfter = rd.GetState().chemV;
    i32 changed = 0;
    for (size_t i = 0; i < vBefore.size(); ++i) {
        if (std::abs(vAfter[i] - vBefore[i]) > 1e-8f) changed++;
    }
    ENJIN_EXPECT_GT(changed, 0);
}

// ===========================================================================
// Baking
// ===========================================================================

ENJIN_TEST(Bake, RGBA8CorrectSize) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 16;
    cfg.height = 16;
    rd.Initialize(cfg);

    auto rgba = rd.BakeToRGBA8();
    ENJIN_EXPECT_EQ(rgba.size(), (size_t)(16 * 16 * 4));
}

ENJIN_TEST(Bake, HeightmapCorrectSize) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 16;
    cfg.height = 16;
    rd.Initialize(cfg);

    auto hmap = rd.BakeToHeightmap();
    ENJIN_EXPECT_EQ(hmap.size(), (size_t)(16 * 16));
}

// ===========================================================================
// Config Get/Set
// ===========================================================================

ENJIN_TEST(ConfigAccess, GetSetConfig) {
    ReactionDiffusion rd;
    RDConfig cfg;
    cfg.width = 32;
    cfg.height = 32;
    cfg.preset = RDPreset::Custom;  // Custom prevents Initialize overriding rates
    cfg.feedRate = 0.04f;
    rd.Initialize(cfg);

    ENJIN_EXPECT_FLOAT_NEAR(rd.GetConfig().feedRate, 0.04f, 0.001f);

    rd.GetConfig().feedRate = 0.08f;
    ENJIN_EXPECT_FLOAT_NEAR(rd.GetConfig().feedRate, 0.08f, 0.001f);
}

ENJIN_TEST_MAIN()
