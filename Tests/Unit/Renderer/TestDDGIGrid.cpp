// The DDGI probe grid sizes two GPU images, so the values that reach it are the
// ones that can turn a slider or a hand-edited scene file into a zero-sized
// image or a multi-gigabyte allocation.
//
// Background: before 2026-09-08 the editor's Probe Grid sliders wrote straight
// into the live config through a const_cast and nothing reallocated, so the
// config claimed a grid the atlas had never been built for. The shader took
// probe counts from the config and the atlas width from a cached value, so the
// readout lied and the lookup addressed texels that were never allocated. The
// fix is a deferred rebuild, and this clamp is the guard on its input.
#include "EnjinTest.h"
#include "Enjin/Renderer/DDGIProbeSystem.h"

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {

// Mirrors CreateProbeAtlas: probes are packed into a square-ish atlas of
// ceil(sqrt(total)) probes per row, each oct x oct texels.
u32 AtlasSideFor(i32 x, i32 y, i32 z, u32 oct) {
    const u32 total = static_cast<u32>(x) * static_cast<u32>(y) * static_cast<u32>(z);
    u32 perRow = 1;
    while (perRow * perRow < total) ++perRow;
    return perRow * oct;
}

} // namespace

ENJIN_TEST(DDGIGrid, AZeroGridBecomesTheSmallestUsableOne) {
    // Arrange: what an empty or truncated scene file deserializes to.
    i32 x = 0, y = 0, z = 0, vox = 0;
    u32 oct = 0;

    // Act
    ClampDDGIGridShape(x, y, z, vox, oct);

    // Assert: nothing that would produce a zero-extent image.
    ENJIN_EXPECT_TRUE(x >= 2);
    ENJIN_EXPECT_TRUE(y >= 2);
    ENJIN_EXPECT_TRUE(z >= 2);
    ENJIN_EXPECT_TRUE(vox >= 16);
    ENJIN_EXPECT_TRUE(oct >= 4);
}

ENJIN_TEST(DDGIGrid, NegativeCountsFromAHandEditedFileAreAbsorbed) {
    i32 x = -8, y = -1, z = -1000, vox = -64;
    u32 oct = 0;

    ClampDDGIGridShape(x, y, z, vox, oct);

    ENJIN_EXPECT_EQ(x, 2);
    ENJIN_EXPECT_EQ(y, 2);
    ENJIN_EXPECT_EQ(z, 2);
    ENJIN_EXPECT_EQ(vox, 16);
    ENJIN_EXPECT_EQ(oct, 4u);
}

ENJIN_TEST(DDGIGrid, AbsurdCountsAreCappedRatherThanAllocated) {
    // A voxel grid is voxelRes^3 R16F: 256 is 32 MB and 4096 would be terabytes.
    i32 x = 100000, y = 100000, z = 100000, vox = 4096;
    u32 oct = 4096;

    ClampDDGIGridShape(x, y, z, vox, oct);

    ENJIN_EXPECT_EQ(x, 32);
    ENJIN_EXPECT_EQ(y, 16);
    ENJIN_EXPECT_EQ(z, 32);
    ENJIN_EXPECT_EQ(vox, 256);
    ENJIN_EXPECT_EQ(oct, 16u);
}

// The reason the ceilings are where they are: the packed atlas has to stay a
// sane image on every target, including the 2048-limited ones.
ENJIN_TEST(DDGIGrid, TheLargestAllowedGridStillFitsAReasonableAtlas) {
    i32 x = 999, y = 999, z = 999, vox = 999;
    u32 oct = 999;
    ClampDDGIGridShape(x, y, z, vox, oct);

    const u32 side = AtlasSideFor(x, y, z, oct);
    ENJIN_EXPECT_TRUE(side <= 2048);
}

ENJIN_TEST(DDGIGrid, AValidGridIsLeftExactlyAlone) {
    // The engine's own defaults must survive untouched, or every scene that
    // never mentions DDGI would be silently reshaped on load.
    i32 x = 8, y = 4, z = 8, vox = 64;
    u32 oct = 8;

    ClampDDGIGridShape(x, y, z, vox, oct);

    ENJIN_EXPECT_EQ(x, 8);
    ENJIN_EXPECT_EQ(y, 4);
    ENJIN_EXPECT_EQ(z, 8);
    ENJIN_EXPECT_EQ(vox, 64);
    ENJIN_EXPECT_EQ(oct, 8u);
}

ENJIN_TEST(DDGIGrid, ClampingIsIdempotent) {
    // RequestGridRebuild compares a clamped request against the running shape to
    // decide whether to rebuild at all. If clamping moved a value a second time,
    // an out-of-range request would rebuild forever.
    i32 x = 5000, y = -3, z = 17, vox = 300;
    u32 oct = 1;
    ClampDDGIGridShape(x, y, z, vox, oct);

    i32 x2 = x, y2 = y, z2 = z, vox2 = vox;
    u32 oct2 = oct;
    ClampDDGIGridShape(x2, y2, z2, vox2, oct2);

    ENJIN_EXPECT_EQ(x2, x);
    ENJIN_EXPECT_EQ(y2, y);
    ENJIN_EXPECT_EQ(z2, z);
    ENJIN_EXPECT_EQ(vox2, vox);
    ENJIN_EXPECT_EQ(oct2, oct);
}

ENJIN_TEST_MAIN()
