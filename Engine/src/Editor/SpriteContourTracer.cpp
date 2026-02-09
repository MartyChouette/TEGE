#include "Enjin/Editor/SpriteContourTracer.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Editor {

// Helper: is pixel at (x,y) opaque (above threshold)?
static bool IsOpaque(const u8* pixels, u32 w, u32 h, i32 x, i32 y, u8 threshold) {
    if (x < 0 || y < 0 || x >= (i32)w || y >= (i32)h) return false;
    return pixels[((u32)y * w + (u32)x) * 4 + 3] > threshold;
}

void SpriteContourTracer::MarchingSquares(
    const u8* pixels, u32 w, u32 h, u8 alphaThreshold,
    std::vector<Math::Vector2>& outContour) {

    // Find a starting edge point: scan from top-left for first opaque pixel
    i32 startX = -1, startY = -1;
    for (u32 y = 0; y < h && startX < 0; y++) {
        for (u32 x = 0; x < w; x++) {
            if (IsOpaque(pixels, w, h, (i32)x, (i32)y, alphaThreshold)) {
                startX = (i32)x;
                startY = (i32)y;
                break;
            }
        }
    }
    if (startX < 0) return; // Fully transparent

    // Marching squares: walk the boundary
    // Cell (cx, cy) has corners: (cx,cy), (cx+1,cy), (cx+1,cy+1), (cx,cy+1)
    // We sample the alpha grid offset by -1 so the marching square grid is (w+1)x(h+1)
    i32 cx = startX;
    i32 cy = startY;
    // Adjust to find the first cell boundary (top-left of first opaque pixel)
    // We want the cell just above-left of the opaque pixel so that the top-right corner is opaque
    cx = startX - 1;
    cy = startY - 1;

    i32 initialCx = cx, initialCy = cy;
    i32 prevDx = 0, prevDy = 0;
    bool firstStep = true;

    constexpr u32 MAX_STEPS = 1000000;
    for (u32 step = 0; step < MAX_STEPS; step++) {
        // Sample 4 corners of cell (cx, cy)
        // Top-left = (cx, cy), Top-right = (cx+1, cy)
        // Bottom-left = (cx, cy+1), Bottom-right = (cx+1, cy+1)
        bool tl = IsOpaque(pixels, w, h, cx, cy, alphaThreshold);
        bool tr = IsOpaque(pixels, w, h, cx + 1, cy, alphaThreshold);
        bool bl = IsOpaque(pixels, w, h, cx, cy + 1, alphaThreshold);
        bool br = IsOpaque(pixels, w, h, cx + 1, cy + 1, alphaThreshold);

        u8 cellCase = (tl ? 8 : 0) | (tr ? 4 : 0) | (br ? 2 : 0) | (bl ? 1 : 0);

        // Direction to step: (dx, dy)
        i32 dx = 0, dy = 0;
        switch (cellCase) {
            case 1:  dx =  0; dy =  1; break; //   bl
            case 2:  dx =  1; dy =  0; break; //      br
            case 3:  dx =  1; dy =  0; break; //   bl br
            case 4:  dx =  0; dy = -1; break; // tr
            case 5:  dx =  0; dy =  1; break; // tr bl (saddle: use previous direction)
                     if (prevDx == 0 && prevDy == 1) { dx = 0; dy = 1; }
                     else { dx = 0; dy = -1; }
                     break;
            case 6:  dx =  1; dy =  0; break; // tr    br
            case 7:  dx =  1; dy =  0; break; // tr bl br
            case 8:  dx = -1; dy =  0; break; // tl
            case 9:  dx =  0; dy =  1; break; // tl    bl
            case 10: dx = -1; dy =  0; break; // tl       br (saddle: use previous direction)
                     if (prevDx == -1 && prevDy == 0) { dx = -1; dy = 0; }
                     else { dx = 1; dy = 0; }
                     break;
            case 11: dx =  1; dy =  0; break; // tl bl br
            case 12: dx = -1; dy =  0; break; // tl tr
            case 13: dx =  0; dy =  1; break; // tl tr bl
            case 14: dx = -1; dy =  0; break; // tl tr    br
            case 0:  // Empty
            case 15: // Full (shouldn't be on boundary)
                return;
        }

        // Edge midpoint: emit a contour vertex at the cell boundary
        f32 vx = static_cast<f32>(cx + 1); // Center of cell
        f32 vy = static_cast<f32>(cy + 1);
        // Shift based on direction for better placement
        if (dx == 1)       { vx = static_cast<f32>(cx + 2); }
        else if (dx == -1) { vx = static_cast<f32>(cx); }
        if (dy == 1)       { vy = static_cast<f32>(cy + 2); }
        else if (dy == -1) { vy = static_cast<f32>(cy); }

        outContour.push_back(Math::Vector2(vx * 0.5f, vy * 0.5f));

        cx += dx;
        cy += dy;
        prevDx = dx;
        prevDy = dy;

        if (!firstStep && cx == initialCx && cy == initialCy) {
            break; // Loop completed
        }
        firstStep = false;
    }
}

f32 SpriteContourTracer::PointLineDistance(const Math::Vector2& p,
                                          const Math::Vector2& a,
                                          const Math::Vector2& b) {
    f32 abx = b.x - a.x;
    f32 aby = b.y - a.y;
    f32 abLen2 = abx * abx + aby * aby;

    if (abLen2 < 1e-12f) {
        f32 dx = p.x - a.x;
        f32 dy = p.y - a.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    f32 t = ((p.x - a.x) * abx + (p.y - a.y) * aby) / abLen2;
    t = std::clamp(t, 0.0f, 1.0f);

    f32 projX = a.x + t * abx;
    f32 projY = a.y + t * aby;
    f32 dx = p.x - projX;
    f32 dy = p.y - projY;
    return std::sqrt(dx * dx + dy * dy);
}

void SpriteContourTracer::DouglasPeucker(
    const std::vector<Math::Vector2>& points,
    usize start, usize end, f32 epsilon,
    std::vector<bool>& keep) {

    if (end <= start + 1) return;

    f32 maxDist = 0.0f;
    usize maxIdx = start;

    for (usize i = start + 1; i < end; i++) {
        f32 d = PointLineDistance(points[i], points[start], points[end]);
        if (d > maxDist) {
            maxDist = d;
            maxIdx = i;
        }
    }

    if (maxDist > epsilon) {
        keep[maxIdx] = true;
        DouglasPeucker(points, start, maxIdx, epsilon, keep);
        DouglasPeucker(points, maxIdx, end, epsilon, keep);
    }
}

std::vector<Math::Vector2> SpriteContourTracer::TraceContour(
    const u8* pixels, u32 w, u32 h, u8 alphaThreshold) {

    std::vector<Math::Vector2> contour;
    if (!pixels || w == 0 || h == 0) return contour;

    MarchingSquares(pixels, w, h, alphaThreshold, contour);
    return contour;
}

std::vector<Math::Vector2> SpriteContourTracer::Simplify(
    const std::vector<Math::Vector2>& points, f32 epsilon) {

    if (points.size() < 3) return points;

    std::vector<bool> keep(points.size(), false);
    keep[0] = true;
    keep[points.size() - 1] = true;

    DouglasPeucker(points, 0, points.size() - 1, epsilon, keep);

    std::vector<Math::Vector2> result;
    result.reserve(points.size());
    for (usize i = 0; i < points.size(); i++) {
        if (keep[i]) result.push_back(points[i]);
    }

    return result;
}

} // namespace Editor
} // namespace Enjin
