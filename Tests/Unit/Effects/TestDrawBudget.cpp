// Sharing one instance budget between many things that want to draw.
//
// Both rules here shipped wrong in two renderers at once: the budget was taken
// first-come (so a saturated source starved every later one to nothing) and
// going over it truncated a scan rather than thinning it (so a z-outermost 3D
// scan lost a flat slab of its subject instead of losing density).
#include "EnjinTest.h"
#include "Enjin/Effects/DrawBudget.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::Effects;

ENJIN_TEST(DrawBudget, test_share_splits_the_budget_evenly_between_consumers) {
    ENJIN_EXPECT_EQ(DrawBudgetShare(65536, 1), static_cast<usize>(65536));
    ENJIN_EXPECT_EQ(DrawBudgetShare(65536, 4), static_cast<usize>(16384));
    ENJIN_EXPECT_EQ(DrawBudgetShare(65536, 8), static_cast<usize>(8192));
}

ENJIN_TEST(DrawBudget, test_share_never_returns_zero_for_a_crowded_scene) {
    // Two hundred campfires against a budget of a hundred: each must draw
    // SOMETHING rather than the first few taking everything.
    ENJIN_EXPECT_TRUE(DrawBudgetShare(100, 200) >= 1);
}

ENJIN_TEST(DrawBudget, test_share_with_no_consumers_returns_the_whole_budget) {
    ENJIN_EXPECT_EQ(DrawBudgetShare(65536, 0), static_cast<usize>(65536));
}

ENJIN_TEST(DrawBudget, test_stride_is_one_when_everything_already_fits) {
    ENJIN_EXPECT_EQ(DrawStride(100, 100), static_cast<usize>(1));
    ENJIN_EXPECT_EQ(DrawStride(1, 100), static_cast<usize>(1));
    ENJIN_EXPECT_EQ(DrawStride(0, 100), static_cast<usize>(1));
}

ENJIN_TEST(DrawBudget, test_stride_always_rounds_up_so_the_result_fits) {
    // 4096 items into 1000: a stride of 4 gives 1024, which does NOT fit, so
    // the answer has to be 5. Rounding down here is how an off-by-one becomes
    // a buffer overrun.
    const usize stride = DrawStride(4096, 1000);
    ENJIN_EXPECT_EQ(stride, static_cast<usize>(5));
    ENJIN_EXPECT_TRUE((4096 + stride - 1) / stride <= 1000);
}

ENJIN_TEST(DrawBudget, test_stride_result_fits_across_a_wide_sweep) {
    for (usize wanted = 1; wanted <= 5000; wanted += 7) {
        for (usize budget = 1; budget <= 500; budget += 13) {
            const usize stride = DrawStride(wanted, budget);
            ENJIN_EXPECT_TRUE(stride >= 1);
            // Number of items actually taken by stepping `stride` through
            // `wanted`, which must never exceed the budget.
            ENJIN_EXPECT_TRUE((wanted + stride - 1) / stride <= budget);
        }
    }
}

ENJIN_TEST(DrawBudget, test_stride_with_a_zero_budget_does_not_divide_by_zero) {
    ENJIN_EXPECT_EQ(DrawStride(100, 0), static_cast<usize>(1));
}

ENJIN_TEST(PoolDemandScale, test_scale_is_one_when_the_scene_fits_the_pool) {
    // Two emitters at 200/sec with 4 second lifetimes: 1600 live against 65536.
    ENJIN_EXPECT_FLOAT_NEAR(PoolDemandScale(1600.0, 65536), 1.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(PoolDemandScale(65536.0, 65536), 1.0f, 0.0001f);
}

ENJIN_TEST(PoolDemandScale, test_scale_cuts_everyone_by_the_same_fraction) {
    // Twice the pool asked for means everyone runs at half rate.
    ENJIN_EXPECT_FLOAT_NEAR(PoolDemandScale(131072.0, 65536), 0.5f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(PoolDemandScale(262144.0, 65536), 0.25f, 0.0001f);
}

ENJIN_TEST(PoolDemandScale, test_a_greedy_emitter_cannot_starve_a_quiet_one) {
    // THE regression. One emitter wants 200,000 live particles, another wants
    // 600. Scaled, the quiet one keeps its share of the ring in proportion
    // rather than having its plume overwritten mid-life.
    const f64 greedy = 200000.0, quiet = 600.0;
    const f32 scale = PoolDemandScale(greedy + quiet, 65536);

    // Both are cut by the same factor, and the total now fits.
    ENJIN_EXPECT_TRUE(scale < 1.0f);
    ENJIN_EXPECT_TRUE((greedy + quiet) * scale <= 65536.0 + 1.0);
    // The quiet emitter keeps the same SHARE of the pool it asked for.
    const f64 quietShareBefore = quiet / (greedy + quiet);
    const f64 quietShareAfter  = (quiet * scale) / ((greedy + quiet) * scale);
    ENJIN_EXPECT_TRUE(std::fabs(quietShareBefore - quietShareAfter) < 1e-6);
}

ENJIN_TEST(PoolDemandScale, test_zero_demand_and_zero_capacity_are_safe) {
    ENJIN_EXPECT_FLOAT_NEAR(PoolDemandScale(0.0, 65536), 1.0f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(PoolDemandScale(1000.0, 0), 0.0f, 0.0001f);
}

ENJIN_TEST_MAIN()
