#include "Enjin/Renderer/VectorTessellator.h"
#include "Enjin/Logging/Log.h"

// NANOSVG_IMPLEMENTATION lives in SVGLoader.cpp; this file only uses the API.
#include <nanosvg.h>

#include <cmath>
#include <cstring>
#include <fstream>
#include <algorithm>

namespace Enjin {
namespace Renderer {

namespace {

// nanosvg colors are packed ABGR (r in the low byte).
Math::Vector4 UnpackColor(unsigned int c, f32 opacity) {
    return Math::Vector4(
        static_cast<f32>((c)       & 0xFF) / 255.0f,
        static_cast<f32>((c >> 8)  & 0xFF) / 255.0f,
        static_cast<f32>((c >> 16) & 0xFF) / 255.0f,
        (static_cast<f32>((c >> 24) & 0xFF) / 255.0f) * opacity);
}

// Paint -> flat color. Solid uses its color; a gradient contributes its first
// stop (v1 - real gradient fills are a later pass).
bool PaintColor(const NSVGpaint& paint, f32 opacity, Math::Vector4& out) {
    if (paint.type == NSVG_PAINT_COLOR) {
        out = UnpackColor(paint.color, opacity);
        return true;
    }
    if ((paint.type == NSVG_PAINT_LINEAR_GRADIENT || paint.type == NSVG_PAINT_RADIAL_GRADIENT)
        && paint.gradient && paint.gradient->nstops > 0) {
        out = UnpackColor(paint.gradient->stops[0].color, opacity);
        return true;
    }
    return false;
}

// Adaptive cubic bezier flattening (de Casteljau split until flat enough).
void FlattenCubic(std::vector<Math::Vector2>& out,
                  f32 x1, f32 y1, f32 x2, f32 y2,
                  f32 x3, f32 y3, f32 x4, f32 y4,
                  f32 tolSq, int depth) {
    // Flatness: control points' distance from the chord.
    f32 dx = x4 - x1, dy = y4 - y1;
    f32 d2 = std::fabs((x2 - x4) * dy - (y2 - y4) * dx);
    f32 d3 = std::fabs((x3 - x4) * dy - (y3 - y4) * dx);
    f32 d23 = d2 + d3;
    if (depth > 10 || d23 * d23 < tolSq * (dx * dx + dy * dy)) {
        out.push_back(Math::Vector2(x4, y4));
        return;
    }
    f32 x12 = (x1 + x2) * 0.5f,   y12 = (y1 + y2) * 0.5f;
    f32 x23 = (x2 + x3) * 0.5f,   y23 = (y2 + y3) * 0.5f;
    f32 x34 = (x3 + x4) * 0.5f,   y34 = (y3 + y4) * 0.5f;
    f32 x123 = (x12 + x23) * 0.5f, y123 = (y12 + y23) * 0.5f;
    f32 x234 = (x23 + x34) * 0.5f, y234 = (y23 + y34) * 0.5f;
    f32 xm = (x123 + x234) * 0.5f, ym = (y123 + y234) * 0.5f;
    FlattenCubic(out, x1, y1, x12, y12, x123, y123, xm, ym, tolSq, depth + 1);
    FlattenCubic(out, xm, ym, x234, y234, x34, y34, x4, y4, tolSq, depth + 1);
}

// Flatten one NSVGpath (cubic bezier point list) to a polyline.
std::vector<Math::Vector2> FlattenPath(const NSVGpath* path, f32 tol) {
    std::vector<Math::Vector2> pts;
    if (!path || path->npts < 1) return pts;
    pts.push_back(Math::Vector2(path->pts[0], path->pts[1]));
    for (int i = 0; i + 3 < path->npts; i += 3) {
        const f32* p = &path->pts[i * 2];
        FlattenCubic(pts, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], tol * tol, 0);
    }
    // Drop consecutive duplicates (they break ear clipping).
    std::vector<Math::Vector2> clean;
    clean.reserve(pts.size());
    for (const auto& p : pts) {
        if (clean.empty() || (std::fabs(p.x - clean.back().x) > 1e-5f ||
                              std::fabs(p.y - clean.back().y) > 1e-5f)) {
            clean.push_back(p);
        }
    }
    // Closed path: remove a duplicated end point.
    if (clean.size() > 1 && std::fabs(clean.front().x - clean.back().x) < 1e-5f &&
        std::fabs(clean.front().y - clean.back().y) < 1e-5f) {
        clean.pop_back();
    }
    return clean;
}

f32 SignedArea(const std::vector<Math::Vector2>& poly) {
    f32 a = 0.0f;
    for (usize i = 0, n = poly.size(); i < n; ++i) {
        const auto& p = poly[i];
        const auto& q = poly[(i + 1) % n];
        a += p.x * q.y - q.x * p.y;
    }
    return a * 0.5f;
}

f32 Cross(const Math::Vector2& o, const Math::Vector2& a, const Math::Vector2& b) {
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

bool PointInTriangle(const Math::Vector2& p, const Math::Vector2& a,
                     const Math::Vector2& b, const Math::Vector2& c) {
    f32 c1 = Cross(a, b, p);
    f32 c2 = Cross(b, c, p);
    f32 c3 = Cross(c, a, p);
    bool hasNeg = (c1 < 0) || (c2 < 0) || (c3 < 0);
    bool hasPos = (c1 > 0) || (c2 > 0) || (c3 > 0);
    return !(hasNeg && hasPos);
}

// Ear clipping for a simple polygon (no holes, no self-intersection). Emits
// index triples into outIdx referring to poly's indices.
void EarClip(const std::vector<Math::Vector2>& poly, std::vector<u32>& outIdx) {
    const usize n = poly.size();
    if (n < 3) return;

    // Work on a CCW copy (in y-down doc space CCW = negative signed area, but
    // orientation just needs to be consistent for the ear test).
    std::vector<u32> idx(n);
    for (usize i = 0; i < n; ++i) idx[i] = static_cast<u32>(i);
    if (SignedArea(poly) < 0.0f) std::reverse(idx.begin(), idx.end());

    usize remaining = n;
    usize guard = 0;
    while (remaining > 3 && guard < n * n) {
        bool clipped = false;
        for (usize i = 0; i < remaining; ++i) {
            u32 i0 = idx[(i + remaining - 1) % remaining];
            u32 i1 = idx[i];
            u32 i2 = idx[(i + 1) % remaining];
            const auto& a = poly[i0];
            const auto& b = poly[i1];
            const auto& c = poly[i2];
            if (Cross(a, b, c) <= 1e-9f) continue;   // reflex or degenerate
            bool contains = false;
            for (usize j = 0; j < remaining; ++j) {
                u32 pj = idx[j];
                if (pj == i0 || pj == i1 || pj == i2) continue;
                if (PointInTriangle(poly[pj], a, b, c)) { contains = true; break; }
            }
            if (contains) continue;
            outIdx.push_back(i0); outIdx.push_back(i1); outIdx.push_back(i2);
            idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(i));
            --remaining;
            clipped = true;
            break;
        }
        ++guard;
        if (!clipped) break;   // no ear found (self-intersecting input) - bail with what we have
    }
    if (remaining == 3) {
        outIdx.push_back(idx[0]); outIdx.push_back(idx[1]); outIdx.push_back(idx[2]);
    }
}

void EmitFill(TessellatedGraphic& g, const std::vector<Math::Vector2>& poly,
              const Math::Vector4& color, u32 shapeIndex) {
    std::vector<u32> tris;
    EarClip(poly, tris);
    if (tris.empty()) return;
    u32 base = static_cast<u32>(g.vertices.size());
    for (const auto& p : poly) g.vertices.push_back({p, color, shapeIndex});
    for (u32 t : tris) g.indices.push_back(base + t);
}

void EmitStroke(TessellatedGraphic& g, const std::vector<Math::Vector2>& pts,
                bool closed, f32 width, const Math::Vector4& color, u32 shapeIndex) {
    if (pts.size() < 2 || width <= 0.0f) return;
    const f32 hw = width * 0.5f;
    const usize n = pts.size();
    const usize segs = closed ? n : n - 1;
    for (usize i = 0; i < segs; ++i) {
        const auto& a = pts[i];
        const auto& b = pts[(i + 1) % n];
        f32 dx = b.x - a.x, dy = b.y - a.y;
        f32 len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-6f) continue;
        f32 nx = -dy / len * hw, ny = dx / len * hw;
        u32 base = static_cast<u32>(g.vertices.size());
        g.vertices.push_back({Math::Vector2(a.x + nx, a.y + ny), color, shapeIndex});
        g.vertices.push_back({Math::Vector2(b.x + nx, b.y + ny), color, shapeIndex});
        g.vertices.push_back({Math::Vector2(b.x - nx, b.y - ny), color, shapeIndex});
        g.vertices.push_back({Math::Vector2(a.x - nx, a.y - ny), color, shapeIndex});
        g.indices.push_back(base + 0); g.indices.push_back(base + 1); g.indices.push_back(base + 2);
        g.indices.push_back(base + 0); g.indices.push_back(base + 2); g.indices.push_back(base + 3);
    }
}

TessellatedGraphic TessellateImage(NSVGimage* image, f32 tol) {
    TessellatedGraphic g;
    if (!image) return g;
    g.width = image->width;
    g.height = image->height;

    u32 shapeIndex = 0;
    for (NSVGshape* shape = image->shapes; shape != nullptr; shape = shape->next) {
        if (!(shape->flags & NSVG_FLAGS_VISIBLE)) continue;

        Math::Vector4 fillColor;
        const bool hasFill = PaintColor(shape->fill, shape->opacity, fillColor)
                             && fillColor.w > 0.0f;
        Math::Vector4 strokeColor;
        const bool hasStroke = PaintColor(shape->stroke, shape->opacity, strokeColor)
                               && strokeColor.w > 0.0f && shape->strokeWidth > 0.0f;
        if (!hasFill && !hasStroke) continue;

        for (NSVGpath* path = shape->paths; path != nullptr; path = path->next) {
            std::vector<Math::Vector2> poly = FlattenPath(path, tol);
            if (hasFill && poly.size() >= 3) EmitFill(g, poly, fillColor, shapeIndex);
            if (hasStroke) EmitStroke(g, poly, path->closed != 0, shape->strokeWidth,
                                      strokeColor, shapeIndex);
        }
        ++shapeIndex;
    }
    g.shapeCount = shapeIndex;
    g.valid = !g.indices.empty();
    return g;
}

} // namespace

TessellatedGraphic TessellateSVG(const std::string& path, f32 curveTolerance) {
    NSVGimage* image = nsvgParseFromFile(path.c_str(), "px", 96.0f);
    if (!image) {
        ENJIN_LOG_WARN(Renderer, "VectorTessellator: failed to parse SVG '%s'", path.c_str());
        return {};
    }
    TessellatedGraphic g = TessellateImage(image, std::max(curveTolerance, 0.01f));
    nsvgDelete(image);
    if (!g.valid) {
        ENJIN_LOG_WARN(Renderer, "VectorTessellator: '%s' produced no geometry", path.c_str());
    }
    return g;
}

TessellatedGraphic TessellateSVGFromString(const std::string& svgText, f32 curveTolerance) {
    // nsvgParse mutates its input buffer, so hand it a scratch copy.
    std::vector<char> buf(svgText.begin(), svgText.end());
    buf.push_back('\0');
    NSVGimage* image = nsvgParse(buf.data(), "px", 96.0f);
    if (!image) return {};
    TessellatedGraphic g = TessellateImage(image, std::max(curveTolerance, 0.01f));
    nsvgDelete(image);
    return g;
}

} // namespace Renderer
} // namespace Enjin
