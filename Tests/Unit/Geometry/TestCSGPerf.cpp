// What CSG costs, and where it stops being realtime.
//
// Cutting a hole in a wall is cheap enough to do every frame. Accumulating many
// cuts into ONE solid is not: every Subtract re-clips every face the solid has,
// and each cut adds faces, so the cost grows with the square of the cut count.
// Measured on this machine, a 20x20 room:
//
//     1 cut      0.017 ms      24 faces
//     4 cuts     0.142 ms     123 faces
//    16 cuts     1.910 ms     555 faces
//    64 cuts    27.526 ms    2283 faces
//
// So realtime gameplay cutting is a question of how the brushes are arranged,
// not whether the boolean is fast. One or two cuts per object is free. Sixty
// cuts into a single solid drops a frame.
//
// The thresholds below are deliberately loose. They exist to catch an
// algorithmic regression -- somebody making the inner loop O(n) worse -- not to
// police milliseconds on a shared CI machine.
#include "EnjinTest.h"
#include "Enjin/Geometry/CSG.h"

#include <chrono>
#include <functional>

using namespace Enjin;
using namespace Enjin::Geometry;
using namespace Enjin::Math;

namespace {

f64 TimeMs(const std::function<void()>& fn, int reps) {
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < reps; ++i) fn();
    const auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<f64, std::milli>(t1 - t0).count() / reps;
}

std::vector<BrushEntry> RoomWithCuts(int n) {
    std::vector<BrushEntry> b;
    b.push_back({ Brush::Box(Vector3(0.0f), Vector3(20.0f, 4.0f, 20.0f)), BrushOp::Add });
    for (int i = 0; i < n; ++i) {
        const f32 x = -18.0f + 0.55f * static_cast<f32>(i);
        const f32 z = -18.0f + 0.37f * static_cast<f32>(i);
        b.push_back({ Brush::Box(Vector3(x, 0.0f, z), Vector3(0.6f, 0.6f, 0.6f)), BrushOp::Subtract });
    }
    return b;
}

} // namespace

// The gameplay case: shoot a hole in a wall panel. One cut, one small solid.
// This is the number that decides whether realtime cutting is viable, and it has
// two orders of magnitude of headroom against a 60Hz frame.
ENJIN_TEST(CsgPerf, SingleCutIsFrameCheap) {
    const std::vector<BrushEntry> b = {
        { Brush::Box(Vector3(0.0f), Vector3(4.0f, 3.0f, 0.25f)), BrushOp::Add },
        { Brush::Box(Vector3(0.0f), Vector3(1.0f, 1.0f, 1.0f)),  BrushOp::Subtract },
    };

    const f64 ms = TimeMs([&] {
        const auto solid = BuildSolid(b);
        const auto mesh = ToMesh(solid);
        const auto col = BuildCollision(solid);
        (void)mesh; (void)col;
    }, 200);

    // Measured at 0.019 ms. Two milliseconds is a hundredfold allowance for a
    // loaded machine, and still fails loudly if the algorithm regresses.
    ENJIN_EXPECT_TRUE(ms < 2.0);
}

// A moderately carved room still rebuilds inside a frame. Past this the cost
// curve is what stops you, not the constant factor.
ENJIN_TEST(CsgPerf, SixteenCutsStillRebuildsWithinAFrame) {
    const auto b = RoomWithCuts(16);
    const f64 ms = TimeMs([&] {
        const auto solid = BuildSolid(b);
        const auto col = BuildCollision(solid);
        (void)col;
    }, 20);

    ENJIN_EXPECT_TRUE(ms < 16.0);   // measured 1.9 ms
}

// The growth is superlinear and that is a property of the design, not a bug:
// each Subtract re-clips every face the solid has, and each one adds faces.
// Asserted so nobody discovers it in a shipping game instead.
ENJIN_TEST(CsgPerf, FaceCountGrowsWithEachCut) {
    const usize f1 = BuildSolid(RoomWithCuts(1)).size();
    const usize f16 = BuildSolid(RoomWithCuts(16)).size();

    ENJIN_EXPECT_TRUE(f16 > f1 * 8);   // 24 -> 555 as measured

    // Nothing simplifies the solid between cuts, so a system that cuts
    // continuously has to bound the cut count per solid rather than rely on
    // this staying cheap.
    ENJIN_EXPECT_TRUE(f16 < 5000);
}

ENJIN_TEST_MAIN()
