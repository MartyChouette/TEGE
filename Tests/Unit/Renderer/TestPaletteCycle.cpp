// Palette cycling: the colours SWAP places, nothing blends.
//
// The whole effect depends on entries moving as whole steps. Interpolating
// between them turns a crisp travelling band into a soft gradient and the
// technique disappears, so that is the first thing pinned here.
//
// Pure and GPU-free, which is why a person can trust it without opening a game.
#include "EnjinTest.h"
#include "Enjin/Renderer/PaletteCycle.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/World.h"

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {

// A palette whose entries are trivially identifiable by index.
Palette Countable(u32 count) {
    Palette p;
    p.count = count;
    for (u32 i = 0; i < count; ++i) {
        p.colors[i] = PaletteColor{ static_cast<u8>(i), 0, 0, 255 };
    }
    return p;
}

} // namespace

ENJIN_TEST(PaletteCycle, AtTimeZeroNothingHasMoved) {
    const Palette base = Countable(16);
    std::vector<PaletteCycleRange> r{ { 1, 15, 6.0f, true } };

    Palette out;
    ApplyPaletteCycles(base, r, 0.0f, out);

    for (u32 i = 0; i < 16; ++i) ENJIN_EXPECT_TRUE(out.colors[i] == base.colors[i]);
}

ENJIN_TEST(PaletteCycle, EntriesRotateByWholeStepsAndWrapInsideTheRun) {
    const Palette base = Countable(8);
    // One entry per second across indices 1..4.
    std::vector<PaletteCycleRange> r{ { 1, 4, 1.0f, true } };

    Palette out;
    ApplyPaletteCycles(base, r, 1.0f, out);

    // Each slot takes the colour of the one after it, and the last wraps.
    ENJIN_EXPECT_EQ(out.colors[1].r, 2);
    ENJIN_EXPECT_EQ(out.colors[2].r, 3);
    ENJIN_EXPECT_EQ(out.colors[3].r, 4);
    ENJIN_EXPECT_EQ(out.colors[4].r, 1);
}

ENJIN_TEST(PaletteCycle, ColoursOutsideARunAreNeverTouched) {
    const Palette base = Countable(8);
    std::vector<PaletteCycleRange> r{ { 2, 3, 5.0f, true } };

    Palette out;
    ApplyPaletteCycles(base, r, 3.3f, out);

    // Index 0 in particular: art relies on one index that never moves.
    ENJIN_EXPECT_EQ(out.colors[0].r, 0);
    ENJIN_EXPECT_EQ(out.colors[1].r, 1);
    ENJIN_EXPECT_EQ(out.colors[5].r, 5);
    ENJIN_EXPECT_EQ(out.colors[6].r, 6);
    ENJIN_EXPECT_EQ(out.colors[7].r, 7);
}

ENJIN_TEST(PaletteCycle, ARunReturnsToItsStartAfterAFullLap) {
    const Palette base = Countable(8);
    std::vector<PaletteCycleRange> r{ { 1, 4, 1.0f, true } };

    Palette out;
    ApplyPaletteCycles(base, r, 4.0f, out);   // four entries, four seconds

    for (u32 i = 0; i < 8; ++i) ENJIN_EXPECT_TRUE(out.colors[i] == base.colors[i]);
}

// C++ modulo keeps the sign of the dividend, so a negative step would index
// backwards off the front of the run. Reverse cycling is what makes fire climb.
ENJIN_TEST(PaletteCycle, NegativeSpeedRunsBackwardsWithoutReadingOutOfBounds) {
    const Palette base = Countable(8);
    std::vector<PaletteCycleRange> r{ { 1, 4, -1.0f, true } };

    Palette out;
    ApplyPaletteCycles(base, r, 1.0f, out);

    ENJIN_EXPECT_EQ(out.colors[1].r, 4);
    ENJIN_EXPECT_EQ(out.colors[2].r, 1);
    ENJIN_EXPECT_EQ(out.colors[3].r, 2);
    ENJIN_EXPECT_EQ(out.colors[4].r, 3);
}

ENJIN_TEST(PaletteCycle, ADisabledOrSingleEntryRunDoesNothing) {
    const Palette base = Countable(8);

    Palette out;
    ApplyPaletteCycles(base, { { 1, 4, 9.0f, false } }, 2.0f, out);
    for (u32 i = 0; i < 8; ++i) ENJIN_EXPECT_TRUE(out.colors[i] == base.colors[i]);

    // A run of one has nothing to rotate into.
    ApplyPaletteCycles(base, { { 3, 1, 9.0f, true } }, 2.0f, out);
    for (u32 i = 0; i < 8; ++i) ENJIN_EXPECT_TRUE(out.colors[i] == base.colors[i]);
}

ENJIN_TEST(PaletteCycle, ARunReachingPastTheEndIsClampedNotReadOutOfBounds) {
    PaletteCycleRange r{ 6, 99, 1.0f, true };
    ClampCycleRange(r, 8);

    ENJIN_EXPECT_TRUE(r.first < 8);
    ENJIN_EXPECT_TRUE(r.first + r.count <= 8);

    // And it must survive being applied.
    const Palette base = Countable(8);
    Palette out;
    ApplyPaletteCycles(base, { { 6, 99, 1.0f, true } }, 1.0f, out);
    ENJIN_EXPECT_EQ(out.count, 8u);
}

ENJIN_TEST(PaletteCycle, ClampingIsIdempotent) {
    PaletteCycleRange a{ 300, 500, 9999.0f, true };
    ClampCycleRange(a, 16);
    PaletteCycleRange b = a;
    ClampCycleRange(b, 16);

    ENJIN_EXPECT_EQ(b.first, a.first);
    ENJIN_EXPECT_EQ(b.count, a.count);
    ENJIN_EXPECT_TRUE(b.speed == a.speed);
}

ENJIN_TEST(PaletteCycle, CyclingIsDeterministic) {
    // A replay and a live session must agree, so the same time must always give
    // the same palette.
    const Palette base = Countable(16);
    std::vector<PaletteCycleRange> r{ { 1, 15, 6.0f, true } };

    Palette a, b;
    ApplyPaletteCycles(base, r, 2.75f, a);
    ApplyPaletteCycles(base, r, 2.75f, b);

    for (u32 i = 0; i < 16; ++i) ENJIN_EXPECT_TRUE(a.colors[i] == b.colors[i]);
}

ENJIN_TEST(PaletteCycle, EveryPresetActuallyAnimates) {
    // A preset that comes out static is indistinguishable from the feature being
    // switched off, which is the failure mode that hides in a demo.
    for (u32 i = 0; i < static_cast<u32>(PalettePreset::Count); ++i) {
        Palette p;
        std::vector<PaletteCycleRange> ranges;
        MakePalettePreset(static_cast<PalettePreset>(i), p, ranges);

        ENJIN_ASSERT_TRUE(p.count >= 2);
        ENJIN_ASSERT_TRUE(!ranges.empty());

        Palette t0, t;
        ApplyPaletteCycles(p, ranges, 0.0f, t0);

        // Swept rather than sampled at one time: a run that has completed a
        // whole lap is legitimately identical to its start, so a single sample
        // can report "static" for a preset that animates perfectly well. Rain
        // cycles 7 entries at 14/sec and is back where it began at exactly 1.0s.
        bool moved = false;
        for (int step = 1; step <= 20 && !moved; ++step) {
            ApplyPaletteCycles(p, ranges, static_cast<f32>(step) * 0.05f, t);
            for (u32 c = 0; c < p.count; ++c) {
                if (t0.colors[c] != t.colors[c]) { moved = true; break; }
            }
            // Index 0 stays put in every preset, at every time, by design.
            ENJIN_EXPECT_TRUE(t0.colors[0] == t.colors[0]);
        }
        ENJIN_EXPECT_TRUE(moved);
    }
}

// --- The clock, which is where this feature actually failed.
//
// ApplyPaletteCycles was correct from the first day and every test above
// passed while the palette on screen sat perfectly still, because the time it
// was being asked about never moved: the clock advanced in
// RenderSystem::Update, and the EDITOR never calls RenderSystem::Update. A
// pure test of the cycling maths cannot see that, so the clock itself is
// pinned here.

ENJIN_TEST(PaletteClock, ATickAdvancesTheClock) {
    // Arrange: a render system with no backend. TickPaletteTime touches two
    // members and nothing else, which is what makes this GPU-free.
    ECS::World world;
    ECS::RenderSystem rs(&world, nullptr);

    // Act
    rs.TickPaletteTime(0.25f);

    // Assert
    ENJIN_EXPECT_TRUE(rs.GetPaletteTime() > 0.24f && rs.GetPaletteTime() < 0.26f);
}

ENJIN_TEST(PaletteClock, ASecondTickInTheSameFrameIsIgnored) {
    // In editor play mode both EditorLayer::Update and RenderSystem::Update are
    // live and would each deposit the same frame's dt, cycling the palette at
    // double the authored speed. That is a subtler wrong than not cycling, so
    // the second deposit has to be dropped rather than merely discouraged.
    ECS::World world;
    ECS::RenderSystem rs(&world, nullptr);

    rs.TickPaletteTime(0.25f);
    rs.TickPaletteTime(0.25f);

    ENJIN_EXPECT_TRUE(rs.GetPaletteTime() < 0.26f);
}

ENJIN_TEST_MAIN()
