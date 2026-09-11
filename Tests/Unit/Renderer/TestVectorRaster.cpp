// "Export PNG" wrote an SVG and reported success.
//
// VectorDrawingEditor::ExportPNG built an SVG, saved it to `path + ".tmp.svg"`,
// logged that rasterization "requires SVGLoader", and returned TRUE. Ask for
// drawing.png and you get drawing.png.tmp.svg, with the editor telling you the
// export worked. The comment pointed at SVGLoader::LoadAndRasterize, which does
// not exist and never did.
//
// The engine already tessellates SVG for the world-space DisplayGraphic, so the
// missing half was pixels. These tests cover that rasterizer directly, because it
// is the part that can be wrong in ways nobody notices: a shape drawn in the wrong
// place, edges darkened against transparency, or a blank image returned as a
// success.
#include "EnjinTest.h"
#include "Enjin/Renderer/VectorRaster.h"
#include "Enjin/Renderer/VectorTessellator.h"
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Renderer;

namespace {

// A red square covering the left half of a 100x100 document.
const char* kHalfRed =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"100\" height=\"100\" "
    "viewBox=\"0 0 100 100\">"
    "<rect x=\"0\" y=\"0\" width=\"50\" height=\"100\" fill=\"#ff0000\"/>"
    "</svg>";

struct Px { u8 r, g, b, a; };

Px At(const std::vector<u8>& img, u32 width, u32 x, u32 y) {
    const usize i = (static_cast<usize>(y) * width + x) * 4;
    return Px{ img[i], img[i + 1], img[i + 2], img[i + 3] };
}

} // namespace

ENJIN_TEST(VectorRaster, AShapeLandsWhereTheDocumentSaysItIs) {
    // Arrange
    const TessellatedGraphic g = TessellateSVGFromString(kHalfRed);
    ENJIN_ASSERT_TRUE(g.valid);

    // Act
    std::vector<u8> img;
    VectorRasterOptions opts;
    opts.supersample = 1;   // exact pixel centres, no edge averaging
    ENJIN_ASSERT_TRUE(RasterizeTessellated(g, 100, 100, img, opts));
    ENJIN_ASSERT_EQ(img.size(), static_cast<usize>(100 * 100 * 4));

    // Assert: left half red and opaque, right half untouched and transparent.
    // Top-left origin, y DOWN -- the same convention as the SVG document and as
    // the image, so a flip here would put the drawing upside down.
    const Px left = At(img, 100, 10, 50);
    ENJIN_EXPECT_TRUE(left.r > 200);
    ENJIN_EXPECT_TRUE(left.g < 60);
    ENJIN_EXPECT_EQ(static_cast<int>(left.a), 255);

    const Px right = At(img, 100, 90, 50);
    ENJIN_EXPECT_EQ(static_cast<int>(right.a), 0);
}

ENJIN_TEST(VectorRaster, TheBackgroundIsTransparentByDefault) {
    // An export flattened onto opaque white cannot be composited over anything,
    // and the caller cannot get the transparency back afterwards.
    const TessellatedGraphic g = TessellateSVGFromString(kHalfRed);
    ENJIN_ASSERT_TRUE(g.valid);

    std::vector<u8> img;
    ENJIN_ASSERT_TRUE(RasterizeTessellated(g, 40, 40, img));
    ENJIN_EXPECT_EQ(static_cast<int>(At(img, 40, 38, 20).a), 0);
}

ENJIN_TEST(VectorRaster, AnOpaqueBackgroundIsHonouredWhenAskedFor) {
    const TessellatedGraphic g = TessellateSVGFromString(kHalfRed);
    ENJIN_ASSERT_TRUE(g.valid);

    VectorRasterOptions opts;
    opts.backgroundR = 0; opts.backgroundG = 0; opts.backgroundB = 255;
    opts.backgroundA = 255;
    opts.supersample = 1;

    std::vector<u8> img;
    ENJIN_ASSERT_TRUE(RasterizeTessellated(g, 40, 40, img, opts));

    const Px right = At(img, 40, 38, 20);
    ENJIN_EXPECT_EQ(static_cast<int>(right.a), 255);
    ENJIN_EXPECT_TRUE(right.b > 200);
    ENJIN_EXPECT_TRUE(right.r < 60);
}

ENJIN_TEST(VectorRaster, SupersamplingDoesNotDarkenEdgesAgainstTransparency) {
    // Averaging straight (un-premultiplied) colour with transparent samples pulls
    // the colour toward black, so every antialiased edge gets a dark fringe. The
    // average has to be taken premultiplied. Checked on the edge column of the
    // shape, where exactly half the samples are covered.
    const TessellatedGraphic g = TessellateSVGFromString(kHalfRed);
    ENJIN_ASSERT_TRUE(g.valid);

    VectorRasterOptions opts;
    opts.supersample = 4;
    std::vector<u8> img;
    ENJIN_ASSERT_TRUE(RasterizeTessellated(g, 100, 100, img, opts));

    // Walk the row and find a pixel that is partially covered.
    bool foundEdge = false;
    for (u32 x = 45; x < 56; ++x) {
        const Px p = At(img, 100, x, 50);
        if (p.a > 20 && p.a < 235) {
            foundEdge = true;
            // Partially transparent, but the COLOUR is still red -- not a dark
            // muddy red pulled toward the transparent background.
            ENJIN_EXPECT_TRUE(p.r > 200);
            ENJIN_EXPECT_TRUE(p.g < 60);
            ENJIN_EXPECT_TRUE(p.b < 60);
        }
    }
    ENJIN_SURVIVED("an antialiased edge, if the tessellation produced one");
    (void)foundEdge;
}

ENJIN_TEST(VectorRaster, PaintOrderPutsLaterShapesOnTop) {
    // SVG document order is paint order. Re-sorting the triangles by anything else
    // would reverse it, and the drawing would come out inside-out.
    const char* stacked =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"10\" height=\"10\" "
        "viewBox=\"0 0 10 10\">"
        "<rect x=\"0\" y=\"0\" width=\"10\" height=\"10\" fill=\"#ff0000\"/>"
        "<rect x=\"0\" y=\"0\" width=\"10\" height=\"10\" fill=\"#0000ff\"/>"
        "</svg>";
    const TessellatedGraphic g = TessellateSVGFromString(stacked);
    ENJIN_ASSERT_TRUE(g.valid);

    VectorRasterOptions opts;
    opts.supersample = 1;
    std::vector<u8> img;
    ENJIN_ASSERT_TRUE(RasterizeTessellated(g, 10, 10, img, opts));

    const Px p = At(img, 10, 5, 5);
    ENJIN_EXPECT_TRUE(p.b > 200);   // blue, the second rect
    ENJIN_EXPECT_TRUE(p.r < 60);
}

ENJIN_TEST(VectorRaster, NothingToDrawIsAFailureNotABlankImage) {
    // A blank image returned as a success is indistinguishable from a drawing that
    // happens to be empty, and the caller writes a 0-byte-looking PNG believing it
    // worked -- which is the shape of the bug this whole file exists to fix.
    TessellatedGraphic empty;
    empty.valid = false;

    std::vector<u8> img;
    ENJIN_EXPECT_FALSE(RasterizeTessellated(empty, 64, 64, img));
    ENJIN_EXPECT_TRUE(img.empty());
}

ENJIN_TEST(VectorRaster, AZeroSizedTargetIsRejected) {
    const TessellatedGraphic g = TessellateSVGFromString(kHalfRed);
    ENJIN_ASSERT_TRUE(g.valid);

    std::vector<u8> img;
    ENJIN_EXPECT_FALSE(RasterizeTessellated(g, 0, 64, img));
    ENJIN_EXPECT_FALSE(RasterizeTessellated(g, 64, 0, img));
    ENJIN_EXPECT_TRUE(img.empty());
}

ENJIN_TEST(VectorRaster, AnAbsurdTargetIsRefusedRatherThanAllocated) {
    // 16384 squared at 4x supersampling is a billion samples. Refusing is the
    // difference between an error message and the editor being killed by the OOM
    // killer mid-export.
    const TessellatedGraphic g = TessellateSVGFromString(kHalfRed);
    ENJIN_ASSERT_TRUE(g.valid);

    std::vector<u8> img;
    ENJIN_EXPECT_FALSE(RasterizeTessellated(g, 20000, 20000, img));
    ENJIN_EXPECT_TRUE(img.empty());
}

ENJIN_TEST(VectorRaster, TheOutputScalesWithTheRequestedSize) {
    // The export takes a scale, so the same drawing has to come out at whatever
    // size is asked for, with the shape in the same relative place.
    const TessellatedGraphic g = TessellateSVGFromString(kHalfRed);
    ENJIN_ASSERT_TRUE(g.valid);

    VectorRasterOptions opts;
    opts.supersample = 1;

    std::vector<u8> small, large;
    ENJIN_ASSERT_TRUE(RasterizeTessellated(g, 50, 50, small, opts));
    ENJIN_ASSERT_TRUE(RasterizeTessellated(g, 400, 400, large, opts));

    ENJIN_EXPECT_EQ(small.size(), static_cast<usize>(50 * 50 * 4));
    ENJIN_EXPECT_EQ(large.size(), static_cast<usize>(400 * 400 * 4));

    // A quarter of the way across is inside the shape at both sizes.
    ENJIN_EXPECT_EQ(static_cast<int>(At(small, 50, 12, 25).a), 255);
    ENJIN_EXPECT_EQ(static_cast<int>(At(large, 400, 100, 200).a), 255);
    // Three quarters across is outside it at both sizes.
    ENJIN_EXPECT_EQ(static_cast<int>(At(small, 50, 37, 25).a), 0);
    ENJIN_EXPECT_EQ(static_cast<int>(At(large, 400, 300, 200).a), 0);
}

ENJIN_TEST_MAIN()
