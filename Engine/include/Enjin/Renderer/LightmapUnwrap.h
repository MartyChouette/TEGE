#pragma once

// Lightmap UVs: giving every triangle its own patch of an atlas.
//
// A lightmap stores light per POSITION on a surface, so it needs a UV layout
// where no two points on the mesh land on the same texel. The mesh's own UVs
// almost never qualify -- they tile, they mirror, they overlap deliberately to
// save texture memory -- and lighting one texel with two different parts of a
// room is not a small artifact, it is light leaking between rooms.
//
// This produces a second, disposable UV set that has the property the first one
// lacks. Each triangle is projected onto its own plane, measured in world units,
// and given a rectangle of the atlas that nothing else uses.
//
// The cost of per-triangle islands is that vertices cannot be shared: a vertex
// on the seam between two triangles needs a different lightmap UV for each, so
// the caller gets back a triangle soup. That is the honest trade for needing no
// unwrapping library, and for static geometry -- which is all a lightmap can
// describe anyway -- it is paid once at bake time.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"

#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

struct LightmapUnwrapOptions {
    // Square, and a power of two. The bake writes three of these (one per
    // basis direction), so the memory cost is three times what this implies.
    u32 atlasSize = 512;

    // Lightmap resolution in world terms. Eight texels per unit over a 4-unit
    // wall is 32 texels across it -- soft, which is what baked light is for.
    // Raising it costs atlas area quadratically and buys detail a normal map
    // is usually better at providing.
    f32 texelsPerUnit = 8.0f;

    // Texels of empty space around every island. Not decoration: the atlas is
    // sampled with bilinear filtering, so without a margin a texel at the edge
    // of one triangle blends with its neighbour in the atlas -- a triangle it
    // is nowhere near in the world. That shows up as light bleeding across
    // seams, the classic lightmap artifact, and 2 is the smallest value that
    // stops it for bilinear.
    u32 padding = 2;
};

struct LightmapUnwrapResult {
    bool ok = false;
    u32 triangleCount = 0;
    // Atlas area actually covered by islands, as a fraction. Low numbers mean
    // the atlas is mostly padding, which is the signal to lower texelsPerUnit
    // rather than to raise the atlas size.
    f32 coverage = 0.0f;
    std::string error;
};

// Build lightmap UVs for a triangle list.
//
// `outUV` receives THREE UVs per triangle, in index order: the caller rebuilds
// its vertex buffer as a soup and assigns these to uv1. Positions and indices
// are read, nothing is written back to them.
LightmapUnwrapResult BuildLightmapUVs(const std::vector<Math::Vector3>& positions,
                                      const std::vector<u32>& indices,
                                      const LightmapUnwrapOptions& options,
                                      std::vector<Math::Vector2>& outUV);

} // namespace Renderer
} // namespace Enjin
