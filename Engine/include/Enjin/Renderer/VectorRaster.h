#pragma once

// Turn tessellated vector art into pixels, on the CPU.
//
// Written because "Export PNG" in the vector drawing editor wrote an SVG. Not a
// PNG with the wrong contents -- an SVG, to `<whatever>.png.tmp.svg`, and then
// returned true. Ask for drawing.png, get drawing.png.tmp.svg, and be told it
// worked. The comment in place said "the actual PNG export would use
// SVGLoader::LoadAndRasterize + stbi_write_png", and neither of those exists.
//
// On the CPU rather than through the renderer on purpose: an export must work
// with no window, no swapchain and no device -- in a headless build, in a test,
// and on a machine whose GPU is busy. It is also the only version that can be
// tested for what it actually produces.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/VectorTessellator.h"

#include <vector>

namespace Enjin {
namespace Renderer {

struct VectorRasterOptions {
    // Supersampling factor per axis. 2 means each output pixel is the average of a
    // 2x2 block, which is what stops every diagonal edge in the drawing from
    // looking like a staircase. 1 disables it.
    u32 supersample = 2;

    // Background, straight RGBA 0..255. Default is fully transparent: a drawing
    // exported onto opaque white cannot be composited over anything, and the
    // caller cannot get the transparency back.
    u8 backgroundR = 0, backgroundG = 0, backgroundB = 0, backgroundA = 0;
};

// Rasterize into `outRGBA` (width * height * 4, straight alpha, top-left origin
// to match SVG document space).
//
// Returns false when there is nothing to draw or the size is unusable, and
// leaves outRGBA empty -- rather than handing back a correct-looking blank image,
// which is indistinguishable from a drawing that happens to be empty.
ENJIN_API bool RasterizeTessellated(const TessellatedGraphic& graphic,
                                    u32 width, u32 height,
                                    std::vector<u8>& outRGBA,
                                    const VectorRasterOptions& options = {});

} // namespace Renderer
} // namespace Enjin
