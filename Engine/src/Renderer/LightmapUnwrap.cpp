#include "Enjin/Renderer/LightmapUnwrap.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

namespace {

// A shelf packer: fill a row left to right, start a new row when the next
// island will not fit, and give up when a new row would run off the bottom.
//
// Chosen over anything cleverer because it is deterministic and its failure is
// legible -- islands are placed in triangle order, so a bake that runs out of
// room always runs out at the same triangle, and the fix (fewer texels per
// unit, or a bigger atlas) is obvious from the coverage number.
struct ShelfPacker {
    u32 atlasSize = 0;
    u32 cursorX = 0;
    u32 shelfY = 0;
    u32 shelfHeight = 0;

    bool Place(u32 w, u32 h, u32& outX, u32& outY) {
        if (w > atlasSize || h > atlasSize) return false;
        if (cursorX + w > atlasSize) {          // next shelf
            shelfY += shelfHeight;
            cursorX = 0;
            shelfHeight = 0;
        }
        if (shelfY + h > atlasSize) return false;
        outX = cursorX;
        outY = shelfY;
        cursorX += w;
        shelfHeight = std::max(shelfHeight, h);
        return true;
    }
};

} // namespace

LightmapUnwrapResult BuildLightmapUVs(const std::vector<Math::Vector3>& positions,
                                      const std::vector<u32>& indices,
                                      const LightmapUnwrapOptions& options,
                                      std::vector<Math::Vector2>& outUV) {
    LightmapUnwrapResult result;
    outUV.clear();

    if (indices.empty() || indices.size() % 3 != 0) {
        result.error = "index count is not a whole number of triangles";
        return result;
    }
    if (options.atlasSize == 0 || !(options.texelsPerUnit > 0.0f)) {
        result.error = "atlas size and texel density must both be positive";
        return result;
    }
    for (u32 i : indices) {
        if (i >= positions.size()) {
            result.error = "an index points past the end of the vertex list";
            return result;
        }
    }

    const u32 triCount = static_cast<u32>(indices.size() / 3);
    outUV.resize(indices.size());

    ShelfPacker packer;
    packer.atlasSize = options.atlasSize;
    const f32 invAtlas = 1.0f / static_cast<f32>(options.atlasSize);
    const u32 pad = options.padding;
    u64 usedTexels = 0;

    for (u32 t = 0; t < triCount; ++t) {
        const Math::Vector3& p0 = positions[indices[t * 3 + 0]];
        const Math::Vector3& p1 = positions[indices[t * 3 + 1]];
        const Math::Vector3& p2 = positions[indices[t * 3 + 2]];

        // The triangle's own 2D frame: one edge as the U axis, and the part of
        // the other edge perpendicular to it as V. Projecting in the triangle's
        // own plane is what keeps texel density even -- a fixed world axis
        // would squash every surface that did not happen to face it.
        const Math::Vector3 e1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
        const Math::Vector3 e2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);

        const f32 e1Len = std::sqrt(e1.x * e1.x + e1.y * e1.y + e1.z * e1.z);
        if (!(e1Len > 1e-8f)) {
            // A degenerate triangle has no area to light. It still needs a UV,
            // because the vertex buffer has to stay the same shape; parking it
            // at the origin costs one texel and never shows.
            outUV[t * 3 + 0] = Math::Vector2(0.0f, 0.0f);
            outUV[t * 3 + 1] = Math::Vector2(0.0f, 0.0f);
            outUV[t * 3 + 2] = Math::Vector2(0.0f, 0.0f);
            continue;
        }
        const Math::Vector3 axisU(e1.x / e1Len, e1.y / e1Len, e1.z / e1Len);
        const f32 e2DotU = e2.x * axisU.x + e2.y * axisU.y + e2.z * axisU.z;
        const Math::Vector3 perp(e2.x - axisU.x * e2DotU,
                                 e2.y - axisU.y * e2DotU,
                                 e2.z - axisU.z * e2DotU);
        const f32 perpLen = std::sqrt(perp.x * perp.x + perp.y * perp.y + perp.z * perp.z);
        if (!(perpLen > 1e-8f)) {
            outUV[t * 3 + 0] = Math::Vector2(0.0f, 0.0f);
            outUV[t * 3 + 1] = Math::Vector2(0.0f, 0.0f);
            outUV[t * 3 + 2] = Math::Vector2(0.0f, 0.0f);
            continue;
        }

        // In that frame the triangle is (0,0), (e1Len,0), (e2DotU, perpLen).
        const f32 minX = std::min(0.0f, std::min(e1Len, e2DotU));
        const f32 maxX = std::max(0.0f, std::max(e1Len, e2DotU));
        const f32 spanX = maxX - minX;
        const f32 spanY = perpLen;

        // At least one texel each way, plus the padding margin. Rounding up is
        // deliberate: a triangle that rounded DOWN to zero texels would take no
        // atlas space and receive no light at all.
        const u32 wTex = static_cast<u32>(std::ceil(spanX * options.texelsPerUnit)) + 1 + pad * 2;
        const u32 hTex = static_cast<u32>(std::ceil(spanY * options.texelsPerUnit)) + 1 + pad * 2;

        u32 originX = 0, originY = 0;
        if (!packer.Place(wTex, hTex, originX, originY)) {
            result.error = "the atlas ran out of room at triangle " + std::to_string(t) +
                           " of " + std::to_string(triCount) +
                           " -- lower texelsPerUnit or raise atlasSize";
            outUV.clear();
            return result;
        }
        usedTexels += static_cast<u64>(wTex) * hTex;

        // Map the triangle inside its rectangle, inset by the padding so the
        // margin really is empty.
        const f32 boxX = static_cast<f32>(originX + pad);
        const f32 boxY = static_cast<f32>(originY + pad);
        auto toUV = [&](f32 x, f32 y) {
            const f32 tx = boxX + (x - minX) * options.texelsPerUnit;
            const f32 ty = boxY + y * options.texelsPerUnit;
            // +0.5 puts the coordinate at a texel CENTRE, which is where a
            // sampler reads. Landing on an edge would blend with the padding.
            return Math::Vector2((tx + 0.5f) * invAtlas, (ty + 0.5f) * invAtlas);
        };

        outUV[t * 3 + 0] = toUV(0.0f, 0.0f);
        outUV[t * 3 + 1] = toUV(e1Len, 0.0f);
        outUV[t * 3 + 2] = toUV(e2DotU, perpLen);
    }

    result.ok = true;
    result.triangleCount = triCount;
    const f64 total = static_cast<f64>(options.atlasSize) * options.atlasSize;
    result.coverage = static_cast<f32>(static_cast<f64>(usedTexels) / total);
    return result;
}

} // namespace Renderer
} // namespace Enjin
