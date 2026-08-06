#include "Enjin/Effects/VegetationTemplates.h"

#include <cmath>

namespace Enjin {
namespace Effects {
namespace VegTemplates {

// Art direction: docs/art/PROCEDURAL_EFFECTS_DIRECTION.md. These are the
// stylized-nature default silhouettes shared by raster and ray tracing.

void BuildGrassBlade(std::vector<VegVertex>& outVerts, std::vector<u32>& outIndices) {
    // 9-vert tapered blade with a baked forward arc (verts lean toward +Z near
    // the tip) so wind reads as a living curve rather than a rigid pendulum.
    outVerts = {
        { -0.50f, 0.00f, 0.00f,  0.00f, 0.00f },  // 0 base left
        {  0.50f, 0.00f, 0.00f,  1.00f, 0.00f },  // 1 base right
        { -0.35f, 0.33f, 0.00f,  0.15f, 0.33f },  // 2 mid-low left
        {  0.35f, 0.33f, 0.00f,  0.85f, 0.33f },  // 3 mid-low right
        { -0.20f, 0.60f, 0.04f,  0.30f, 0.60f },  // 4 upper-mid left (arc begins)
        {  0.20f, 0.60f, 0.04f,  0.70f, 0.60f },  // 5 upper-mid right
        { -0.10f, 0.80f, 0.08f,  0.38f, 0.80f },  // 6 upper left
        {  0.10f, 0.80f, 0.08f,  0.62f, 0.80f },  // 7 upper right
        {  0.02f, 1.00f, 0.10f,  0.50f, 1.00f },  // 8 tip
    };
    outIndices = { 0,1,2, 2,1,3, 2,3,4, 4,3,5, 4,5,6, 6,5,7, 6,7,8 };
}

void BuildShrub(std::vector<VegVertex>& outVerts, std::vector<u32>& outIndices) {
    // 3 crossed tapered dome quads (star at 0/60/120 deg). Each quad narrows to
    // a point at the top so the intersection reads as a lumpy crown from any
    // angle rather than a flat-topped slab. 21 verts, 15 tris.
    outVerts.clear();
    outIndices.clear();
    outVerts.reserve(21);
    outIndices.reserve(45);

    // Half-width by height fraction: wide base tapering to a collapsed tip
    struct Row { f32 y; f32 w; f32 u; f32 v; };
    const Row rows[7] = {
        { 0.00f, 0.50f, 0.0f, 0.00f },
        { 0.00f, 0.50f, 1.0f, 0.00f },  // base pair
        { 0.35f, 0.42f, 0.0f, 0.35f },
        { 0.35f, 0.42f, 1.0f, 0.35f },  // lower-mid pair
        { 0.65f, 0.28f, 0.0f, 0.65f },
        { 0.65f, 0.28f, 1.0f, 0.65f },  // upper-mid pair
        { 1.00f, 0.00f, 0.5f, 1.00f },  // collapsed tip
    };

    for (u32 q = 0; q < 3; ++q) {
        f32 angle = static_cast<f32>(q) * 3.14159265f / 3.0f;
        f32 cosA = std::cos(angle);
        f32 sinA = std::sin(angle);
        u32 base = static_cast<u32>(outVerts.size());
        for (u32 r = 0; r < 7; ++r) {
            f32 side = (r == 6) ? 0.0f : ((r % 2 == 0) ? -1.0f : 1.0f);
            f32 x = side * rows[r].w;
            outVerts.push_back({ x * cosA, rows[r].y, x * sinA, rows[r].u, rows[r].v });
        }
        // Tris: 0-1-2, 2-1-3, 2-3-4, 4-3-5, 4-5-6
        outIndices.insert(outIndices.end(), {
            base+0, base+1, base+2,  base+2, base+1, base+3,
            base+2, base+3, base+4,  base+4, base+3, base+5,
            base+4, base+5, base+6
        });
    }
}

void BuildTree(std::vector<VegVertex>& outVerts, std::vector<u32>& outIndices) {
    // Trunk: 2 crossed tapered quads (uv.y < 0.5). Good as-is.
    // Canopy: 3 crossed diamond quads (uv.y >= 0.5), widest at mid, tapering at
    // both ends, so the crown reads as a rounded broadleaf/conifer form rather
    // than a hexagonal slab. 8 trunk + 24 canopy = 32 verts.
    outVerts.clear();
    outIndices.clear();
    outVerts.reserve(32);
    outIndices.reserve(60);

    const f32 trunkBaseW = 0.5f;
    const f32 trunkTopW = 0.3f;
    for (u32 q = 0; q < 2; ++q) {
        f32 angle = static_cast<f32>(q) * 3.14159265f / 2.0f;
        f32 cosA = std::cos(angle);
        f32 sinA = std::sin(angle);
        u32 vi = static_cast<u32>(outVerts.size());
        outVerts.push_back({ -trunkBaseW * cosA, 0.0f, -trunkBaseW * sinA,  0.0f, 0.0f });
        outVerts.push_back({  trunkBaseW * cosA, 0.0f,  trunkBaseW * sinA,  1.0f, 0.0f });
        outVerts.push_back({  trunkTopW * cosA,  1.0f,  trunkTopW * sinA,   1.0f, 0.4f });
        outVerts.push_back({ -trunkTopW * cosA,  1.0f, -trunkTopW * sinA,   0.0f, 0.4f });
        outIndices.insert(outIndices.end(), { vi+0, vi+1, vi+2, vi+2, vi+3, vi+0 });
    }

    // Canopy diamond profile in template space: y 0.5 (base) .. 1.5 (top),
    // widest at ~35% up. tree.vert / the RT bake scale this by canopyRadius.
    struct CRow { f32 y; f32 halfW; };
    const CRow crows[6] = {
        { 0.50f, 0.00f },  // bottom point
        { 0.75f, 0.35f },  // lower
        { 0.85f, 0.50f },  // widest (~35% of the 0.5..1.5 span)
        { 1.15f, 0.42f },  // upper-wide
        { 1.35f, 0.20f },  // upper
        { 1.50f, 0.00f },  // top point
    };
    for (u32 q = 0; q < 3; ++q) {
        f32 angle = static_cast<f32>(q) * 3.14159265f / 3.0f;
        f32 cosA = std::cos(angle);
        f32 sinA = std::sin(angle);
        u32 base = static_cast<u32>(outVerts.size());
        for (u32 r = 0; r < 6; ++r) {
            // Center points (r 0,5) sit on the axis; the others form L/R pairs.
            // Emit a left and right vert per row except the collapsed ends.
            f32 vcoord = 0.5f + (crows[r].y - 0.5f) * 0.5f;  // V 0.5..1.0 (canopy band)
            if (crows[r].halfW <= 0.0f) {
                outVerts.push_back({ 0.0f, crows[r].y, 0.0f, 0.5f, vcoord });
            } else {
                outVerts.push_back({ -crows[r].halfW * cosA, crows[r].y, -crows[r].halfW * sinA, 0.0f, vcoord });
                outVerts.push_back({  crows[r].halfW * cosA, crows[r].y,  crows[r].halfW * sinA, 1.0f, vcoord });
            }
        }
        // Vert layout within this quad: 0=bottom, 1L 2R, 3L 4R, 5L 6R, 7L 8R, 9=top
        u32 b = base;
        outIndices.insert(outIndices.end(), {
            b+0, b+1, b+2,               // bottom fan
            b+1, b+3, b+4,  b+1, b+4, b+2,   // lower band
            b+3, b+5, b+6,  b+3, b+6, b+4,   // mid band
            b+5, b+7, b+8,  b+5, b+8, b+6,   // upper band
            b+7, b+9, b+8                 // top fan
        });
    }
}

} // namespace VegTemplates
} // namespace Effects
} // namespace Enjin
