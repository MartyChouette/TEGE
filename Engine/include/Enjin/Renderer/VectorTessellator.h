#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

// P2 of the unified display system (docs/DESIGN_UNIFIED_DISPLAY.md): turn an
// SVG document (VectorDrawing exports SVG; SWF shapes convert to it) into a
// flat triangle list ONCE, then let both mounts consume the same result - the
// world mount as a MeshComponent, the canvas mount through the ImGui draw
// list. Real geometry, so vector art is crisp at any scale, like the SDF text.
//
// v1 scope: solid fills (a gradient contributes its first stop's color),
// even-odd holes are NOT supported (each subpath fills independently),
// strokes are miterless quad-strip polylines. Coordinates stay in SVG
// document space (y-DOWN, origin top-left); consumers map to their target
// space.
struct ENJIN_API TessellatedGraphic {
    struct Vtx {
        Math::Vector2 pos;      // SVG document space, y-down
        Math::Vector4 color;    // straight (un-premultiplied) RGBA 0..1
        u32 shapeIndex = 0;     // paint order: shapes with higher index draw on top
    };
    std::vector<Vtx> vertices;
    std::vector<u32> indices;   // triangle list
    f32 width = 0.0f;           // document size in SVG units
    f32 height = 0.0f;
    u32 shapeCount = 0;
    bool valid = false;
};

// Parse + tessellate an SVG file. curveTolerance = max flattening error in
// SVG units (smaller = more triangles). Returns valid=false on parse failure.
ENJIN_API TessellatedGraphic TessellateSVG(const std::string& path, f32 curveTolerance = 0.25f);

// Same, from SVG source text already in memory (the parser mutates its input,
// so this takes a copy by value internally).
ENJIN_API TessellatedGraphic TessellateSVGFromString(const std::string& svgText, f32 curveTolerance = 0.25f);

} // namespace Renderer
} // namespace Enjin
