#include "Enjin/Renderer/VectorRaster.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Renderer {

namespace {

struct Rgba {
    f32 r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
};

// Source-over, on straight (un-premultiplied) colours.
void Blend(Rgba& dst, const Math::Vector4& src) {
    const f32 sa = std::max(0.0f, std::min(1.0f, src.w));
    if (sa <= 0.0f) return;
    const f32 outA = sa + dst.a * (1.0f - sa);
    if (outA <= 0.0f) {
        dst = Rgba{};
        return;
    }
    dst.r = (src.x * sa + dst.r * dst.a * (1.0f - sa)) / outA;
    dst.g = (src.y * sa + dst.g * dst.a * (1.0f - sa)) / outA;
    dst.b = (src.z * sa + dst.b * dst.a * (1.0f - sa)) / outA;
    dst.a = outA;
}

u8 ToByte(f32 v) {
    return static_cast<u8>(std::max(0.0f, std::min(1.0f, v)) * 255.0f + 0.5f);
}

} // namespace

bool RasterizeTessellated(const TessellatedGraphic& graphic, u32 width, u32 height,
                          std::vector<u8>& outRGBA, const VectorRasterOptions& options) {
    outRGBA.clear();

    if (!graphic.valid || graphic.indices.empty() || graphic.vertices.empty()) {
        return false;
    }
    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        return false;
    }
    if (graphic.width <= 0.0f || graphic.height <= 0.0f) {
        return false;
    }

    const u32 ss = std::max(1u, std::min(4u, options.supersample));
    const u64 sw = static_cast<u64>(width) * ss;
    const u64 sh = static_cast<u64>(height) * ss;
    if (sw * sh > 268435456ull) {   // 256M samples, ~4 GB at 16 bytes each
        ENJIN_LOG_ERROR(Renderer, "Vector raster: %llux%llu samples is too large",
                        static_cast<unsigned long long>(sw),
                        static_cast<unsigned long long>(sh));
        return false;
    }

    Rgba background;
    background.r = options.backgroundR / 255.0f;
    background.g = options.backgroundG / 255.0f;
    background.b = options.backgroundB / 255.0f;
    background.a = options.backgroundA / 255.0f;

    std::vector<Rgba> samples(static_cast<usize>(sw * sh), background);

    // SVG document space to sample space. The tessellator keeps y DOWN with the
    // origin top-left, which is the same convention as an image, so no flip.
    const f32 scaleX = static_cast<f32>(sw) / graphic.width;
    const f32 scaleY = static_cast<f32>(sh) / graphic.height;

    // Triangles are drawn in index order. The tessellator emits them in paint
    // order (shapeIndex ascending), so later shapes land on top -- which is what
    // an SVG means by document order, and re-sorting here would reverse it.
    for (usize i = 0; i + 2 < graphic.indices.size(); i += 3) {
        const u32 ia = graphic.indices[i];
        const u32 ib = graphic.indices[i + 1];
        const u32 ic = graphic.indices[i + 2];
        if (ia >= graphic.vertices.size() || ib >= graphic.vertices.size() ||
            ic >= graphic.vertices.size()) {
            continue;
        }

        const auto& va = graphic.vertices[ia];
        const auto& vb = graphic.vertices[ib];
        const auto& vc = graphic.vertices[ic];

        const f32 ax = va.pos.x * scaleX, ay = va.pos.y * scaleY;
        const f32 bx = vb.pos.x * scaleX, by = vb.pos.y * scaleY;
        const f32 cx = vc.pos.x * scaleX, cy = vc.pos.y * scaleY;

        // Twice the signed area. Zero means degenerate, which the tessellator can
        // legitimately emit for a zero-length stroke segment.
        const f32 area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        if (std::fabs(area) < 1e-9f) continue;
        const f32 invArea = 1.0f / area;

        i64 minX = static_cast<i64>(std::floor(std::min({ ax, bx, cx })));
        i64 maxX = static_cast<i64>(std::ceil(std::max({ ax, bx, cx })));
        i64 minY = static_cast<i64>(std::floor(std::min({ ay, by, cy })));
        i64 maxY = static_cast<i64>(std::ceil(std::max({ ay, by, cy })));
        minX = std::max<i64>(0, minX);
        minY = std::max<i64>(0, minY);
        maxX = std::min<i64>(static_cast<i64>(sw) - 1, maxX);
        maxY = std::min<i64>(static_cast<i64>(sh) - 1, maxY);
        if (maxX < minX || maxY < minY) continue;

        for (i64 y = minY; y <= maxY; ++y) {
            const f32 py = static_cast<f32>(y) + 0.5f;
            for (i64 x = minX; x <= maxX; ++x) {
                const f32 px = static_cast<f32>(x) + 0.5f;

                // Barycentric coverage. Signs are compared against the triangle's
                // own winding, so both windings fill -- an SVG does not promise
                // consistent winding and dropping back-facing triangles would make
                // half of a drawing disappear.
                f32 w0 = ((bx - px) * (cy - py) - (by - py) * (cx - px)) * invArea;
                f32 w1 = ((cx - px) * (ay - py) - (cy - py) * (ax - px)) * invArea;
                f32 w2 = 1.0f - w0 - w1;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;

                const Math::Vector4 colour(
                    va.color.x * w0 + vb.color.x * w1 + vc.color.x * w2,
                    va.color.y * w0 + vb.color.y * w1 + vc.color.y * w2,
                    va.color.z * w0 + vb.color.z * w1 + vc.color.z * w2,
                    va.color.w * w0 + vb.color.w * w1 + vc.color.w * w2);

                Blend(samples[static_cast<usize>(y * static_cast<i64>(sw) + x)], colour);
            }
        }
    }

    // Resolve the supersampled buffer. Averaging in straight alpha would darken
    // edges against transparency, so the average is taken PREMULTIPLIED and
    // un-premultiplied once at the end.
    outRGBA.assign(static_cast<usize>(width) * height * 4, 0);
    const f32 inv = 1.0f / static_cast<f32>(ss * ss);

    for (u32 y = 0; y < height; ++y) {
        for (u32 x = 0; x < width; ++x) {
            f32 pr = 0.0f, pg = 0.0f, pb = 0.0f, pa = 0.0f;
            for (u32 sy = 0; sy < ss; ++sy) {
                for (u32 sx = 0; sx < ss; ++sx) {
                    const usize idx = static_cast<usize>(
                        (static_cast<u64>(y) * ss + sy) * sw + (static_cast<u64>(x) * ss + sx));
                    const Rgba& s = samples[idx];
                    pr += s.r * s.a;
                    pg += s.g * s.a;
                    pb += s.b * s.a;
                    pa += s.a;
                }
            }
            pr *= inv; pg *= inv; pb *= inv; pa *= inv;

            const usize o = (static_cast<usize>(y) * width + x) * 4;
            if (pa > 1e-6f) {
                outRGBA[o + 0] = ToByte(pr / pa);
                outRGBA[o + 1] = ToByte(pg / pa);
                outRGBA[o + 2] = ToByte(pb / pa);
            }
            outRGBA[o + 3] = ToByte(pa);
        }
    }
    return true;
}

} // namespace Renderer
} // namespace Enjin
