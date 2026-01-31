#include "Enjin/GUI/ScreenTransition.h"

#include <cmath>
#include <cstdint>

namespace Enjin::GUI
{

// Simple hash for pixel dissolve pattern - produces a deterministic pseudo-random
// threshold in [0,1] for each grid cell so pixels appear/disappear in a scattered order.
static f32 CellHash(i32 x, i32 y)
{
    u32 h = static_cast<u32>(x) * 374761393u + static_cast<u32>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return static_cast<f32>(h & 0xFFFFu) / 65535.0f;
}

void ScreenTransition::Start(const TransitionConfig& config)
{
    m_Config = config;
    m_Phase = TransitionPhase::TransitionOut;
    m_Timer = 0.0f;
    m_Progress = 0.0f;
}

void ScreenTransition::Update(f32 dt)
{
    if (m_Phase == TransitionPhase::Idle)
        return;

    m_Timer += dt;

    switch (m_Phase)
    {
    case TransitionPhase::TransitionOut:
    {
        if (m_Config.outDuration <= 0.0f)
        {
            m_Progress = 1.0f;
            m_Phase = TransitionPhase::Hold;
            m_Timer = 0.0f;
        }
        else
        {
            m_Progress = m_Timer / m_Config.outDuration;
            if (m_Progress >= 1.0f)
            {
                m_Progress = 1.0f;
                m_Phase = TransitionPhase::Hold;
                m_Timer = 0.0f;
            }
        }
        break;
    }
    case TransitionPhase::Hold:
    {
        m_Progress = 1.0f;
        if (m_Timer >= m_Config.holdDuration)
        {
            m_Phase = TransitionPhase::TransitionIn;
            m_Timer = 0.0f;
        }
        break;
    }
    case TransitionPhase::TransitionIn:
    {
        if (m_Config.inDuration <= 0.0f)
        {
            m_Progress = 1.0f;
            m_Phase = TransitionPhase::Idle;
        }
        else
        {
            m_Progress = m_Timer / m_Config.inDuration;
            if (m_Progress >= 1.0f)
            {
                m_Progress = 1.0f;
                m_Phase = TransitionPhase::Idle;
            }
        }
        break;
    }
    default:
        break;
    }
}

void ScreenTransition::Render(f32 screenW, f32 screenH)
{
    if (m_Phase == TransitionPhase::Idle)
        return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl)
        return;

    bool closing = (m_Phase == TransitionPhase::TransitionOut || m_Phase == TransitionPhase::Hold);
    f32 progress = m_Progress;

    // During hold phase the screen is fully covered
    if (m_Phase == TransitionPhase::Hold)
        progress = 1.0f;

    switch (m_Config.type)
    {
    case TransitionType::FadeBlack:
    case TransitionType::FadeWhite:
    {
        f32 alpha;
        if (closing)
            alpha = progress; // 0 -> 1
        else
            alpha = 1.0f - progress; // 1 -> 0

        RenderFade(dl, screenW, screenH, alpha);
        break;
    }
    case TransitionType::CircleIris:
        RenderCircleIris(dl, screenW, screenH, progress, closing);
        break;

    case TransitionType::DiamondWipe:
        RenderDiamondWipe(dl, screenW, screenH, progress, closing);
        break;

    case TransitionType::PixelDissolve:
        RenderPixelDissolve(dl, screenW, screenH, progress, closing);
        break;

    case TransitionType::SlideLeft:
    case TransitionType::SlideRight:
    case TransitionType::SlideUp:
    case TransitionType::SlideDown:
        RenderSlide(dl, screenW, screenH, progress, closing, m_Config.type);
        break;
    }
}

bool ScreenTransition::IsActive() const
{
    return m_Phase != TransitionPhase::Idle;
}

bool ScreenTransition::IsAtMidpoint() const
{
    return m_Phase == TransitionPhase::Hold;
}

bool ScreenTransition::IsComplete() const
{
    return m_Phase == TransitionPhase::Idle && m_Timer > 0.0f;
}

TransitionPhase ScreenTransition::GetPhase() const
{
    return m_Phase;
}

// ---------------------------------------------------------------------------
// Private render helpers
// ---------------------------------------------------------------------------

void ScreenTransition::RenderFade(ImDrawList* dl, f32 w, f32 h, f32 alpha)
{
    Math::Vector3 c = m_Config.color;

    // FadeWhite overrides to white regardless of config color
    if (m_Config.type == TransitionType::FadeWhite)
        c = Math::Vector3(1.0f, 1.0f, 1.0f);

    u8 r = static_cast<u8>(c.x * 255.0f);
    u8 g = static_cast<u8>(c.y * 255.0f);
    u8 b = static_cast<u8>(c.z * 255.0f);
    u8 a = static_cast<u8>(alpha * 255.0f);

    ImU32 col = IM_COL32(r, g, b, a);
    dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(w, h), col);
}

void ScreenTransition::RenderCircleIris(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing)
{
    // Max radius is the distance from center to a corner so the circle covers
    // the entire screen when fully open.
    f32 cx = w * 0.5f;
    f32 cy = h * 0.5f;
    f32 maxRadius = std::sqrt(cx * cx + cy * cy);

    f32 radius;
    if (closing)
        radius = (1.0f - progress) * maxRadius; // shrinks to 0
    else
        radius = progress * maxRadius; // grows from 0

    Math::Vector3 c = m_Config.color;
    u8 cr = static_cast<u8>(c.x * 255.0f);
    u8 cg = static_cast<u8>(c.y * 255.0f);
    u8 cb = static_cast<u8>(c.z * 255.0f);
    ImU32 col = IM_COL32(cr, cg, cb, 255);

    if (radius <= 0.5f)
    {
        // Fully covered
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(w, h), col);
        return;
    }

    // Strategy: draw four rects covering everything outside the circle's bounding
    // box, then fill the corners with triangular fans to approximate the curved mask.
    // This avoids stencil buffers and works purely with ImDrawList.

    // Bounding box of the circle
    f32 left = cx - radius;
    f32 right = cx + radius;
    f32 top = cy - radius;
    f32 bottom = cy + radius;

    // Top strip
    if (top > 0.0f)
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(w, top), col);
    // Bottom strip
    if (bottom < h)
        dl->AddRectFilled(ImVec2(0.0f, bottom), ImVec2(w, h), col);
    // Left strip (between top and bottom)
    f32 stripTop = (top > 0.0f) ? top : 0.0f;
    f32 stripBottom = (bottom < h) ? bottom : h;
    if (left > 0.0f)
        dl->AddRectFilled(ImVec2(0.0f, stripTop), ImVec2(left, stripBottom), col);
    // Right strip
    if (right < w)
        dl->AddRectFilled(ImVec2(right, stripTop), ImVec2(w, stripBottom), col);

    // Fill the four corner arcs between the circle edge and the bounding box corners.
    // We generate triangle fans from the bounding-box corner inward along the arc.
    const i32 segments = 32;

    // Helper lambda: fill the area between a bounding-box corner and the circle arc.
    // quadrant: 0=top-left, 1=top-right, 2=bottom-right, 3=bottom-left
    auto fillCornerArc = [&](i32 quadrant)
    {
        // Angle range for each quadrant (going around the circle)
        f32 angleStart, angleEnd;
        ImVec2 corner;

        switch (quadrant)
        {
        case 0: // top-left
            angleStart = 3.14159265f;      // pi (left)
            angleEnd = 3.14159265f * 1.5f; // 3pi/2 (top)
            corner = ImVec2(left > 0.0f ? left : 0.0f, top > 0.0f ? top : 0.0f);
            break;
        case 1: // top-right
            angleStart = 3.14159265f * 1.5f; // 3pi/2 (top)
            angleEnd = 3.14159265f * 2.0f;   // 2pi (right)
            corner = ImVec2(right < w ? right : w, top > 0.0f ? top : 0.0f);
            break;
        case 2: // bottom-right
            angleStart = 0.0f;              // 0 (right)
            angleEnd = 3.14159265f * 0.5f;  // pi/2 (bottom)
            corner = ImVec2(right < w ? right : w, bottom < h ? bottom : h);
            break;
        case 3: // bottom-left
            angleStart = 3.14159265f * 0.5f; // pi/2 (bottom)
            angleEnd = 3.14159265f;           // pi (left)
            corner = ImVec2(left > 0.0f ? left : 0.0f, bottom < h ? bottom : h);
            break;
        default:
            return;
        }

        // Build a triangle fan: corner -> arc points
        for (i32 i = 0; i < segments; ++i)
        {
            f32 a0 = angleStart + (angleEnd - angleStart) * static_cast<f32>(i) / static_cast<f32>(segments);
            f32 a1 = angleStart + (angleEnd - angleStart) * static_cast<f32>(i + 1) / static_cast<f32>(segments);

            ImVec2 p0(cx + std::cos(a0) * radius, cy + std::sin(a0) * radius);
            ImVec2 p1(cx + std::cos(a1) * radius, cy + std::sin(a1) * radius);

            dl->AddTriangleFilled(corner, p0, p1, col);
        }
    };

    fillCornerArc(0);
    fillCornerArc(1);
    fillCornerArc(2);
    fillCornerArc(3);
}

void ScreenTransition::RenderDiamondWipe(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing)
{
    f32 cx = w * 0.5f;
    f32 cy = h * 0.5f;
    // Max half-size so the diamond covers the full screen
    f32 maxHalf = cx + cy;

    f32 half;
    if (closing)
        half = (1.0f - progress) * maxHalf; // shrinks to 0
    else
        half = progress * maxHalf; // grows from 0

    Math::Vector3 c = m_Config.color;
    u8 cr = static_cast<u8>(c.x * 255.0f);
    u8 cg = static_cast<u8>(c.y * 255.0f);
    u8 cb = static_cast<u8>(c.z * 255.0f);
    ImU32 col = IM_COL32(cr, cg, cb, 255);

    if (half <= 0.5f)
    {
        // Fully covered
        dl->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(w, h), col);
        return;
    }

    // Diamond vertices (rotated square centered on screen)
    ImVec2 top(cx, cy - half);
    ImVec2 right(cx + half, cy);
    ImVec2 bottom(cx, cy + half);
    ImVec2 left(cx - half, cy);

    // Screen corners
    ImVec2 tl(0.0f, 0.0f);
    ImVec2 tr(w, 0.0f);
    ImVec2 br(w, h);
    ImVec2 bl(0.0f, h);

    // Fill the area outside the diamond with four triangles/quads per edge.
    // Top region (above the diamond top edge: tl -- top -- tr)
    dl->AddTriangleFilled(tl, top, tr, col);
    // Right region (right of diamond: tr -- right -- br)
    dl->AddTriangleFilled(tr, right, br, col);
    // Bottom region
    dl->AddTriangleFilled(br, bottom, bl, col);
    // Left region
    dl->AddTriangleFilled(bl, left, tl, col);

    // The four triangles above may leave small uncovered strips between them and
    // the diamond edges (depending on aspect ratio). Fill the four quadrilateral
    // gaps between adjacent screen-corner triangles.
    // top-right gap: tr, top, right
    dl->AddTriangleFilled(tr, top, right, col);
    // bottom-right gap: br, right, bottom
    dl->AddTriangleFilled(br, right, bottom, col);
    // bottom-left gap: bl, bottom, left
    dl->AddTriangleFilled(bl, bottom, left, col);
    // top-left gap: tl, left, top
    dl->AddTriangleFilled(tl, left, top, col);
}

void ScreenTransition::RenderPixelDissolve(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing)
{
    Math::Vector3 c = m_Config.color;
    u8 cr = static_cast<u8>(c.x * 255.0f);
    u8 cg = static_cast<u8>(c.y * 255.0f);
    u8 cb = static_cast<u8>(c.z * 255.0f);
    ImU32 col = IM_COL32(cr, cg, cb, 255);

    f32 ps = static_cast<f32>(m_Config.pixelSize > 0 ? m_Config.pixelSize : 8);
    i32 cols = static_cast<i32>(std::ceil(w / ps));
    i32 rows = static_cast<i32>(std::ceil(h / ps));

    // When closing, cells with hash < progress get filled (more cells appear).
    // When opening (TransitionIn), cells with hash < (1-progress) stay filled
    // (cells disappear as progress increases).
    f32 threshold;
    if (closing)
        threshold = progress;
    else
        threshold = 1.0f - progress;

    for (i32 gy = 0; gy < rows; ++gy)
    {
        for (i32 gx = 0; gx < cols; ++gx)
        {
            f32 cellVal = CellHash(gx, gy);
            if (cellVal < threshold)
            {
                f32 x0 = static_cast<f32>(gx) * ps;
                f32 y0 = static_cast<f32>(gy) * ps;
                f32 x1 = x0 + ps;
                f32 y1 = y0 + ps;
                if (x1 > w) x1 = w;
                if (y1 > h) y1 = h;
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), col);
            }
        }
    }
}

void ScreenTransition::RenderSlide(ImDrawList* dl, f32 w, f32 h, f32 progress, bool closing, TransitionType slideDir)
{
    Math::Vector3 c = m_Config.color;
    u8 cr = static_cast<u8>(c.x * 255.0f);
    u8 cg = static_cast<u8>(c.y * 255.0f);
    u8 cb = static_cast<u8>(c.z * 255.0f);
    ImU32 col = IM_COL32(cr, cg, cb, 255);

    // The solid rect slides in when closing and slides out when opening.
    // "closing" means the rect moves to cover the screen; "opening" means it
    // moves away to reveal it.
    f32 offset;
    ImVec2 p0, p1;

    switch (slideDir)
    {
    case TransitionType::SlideLeft:
        // Slides in from the right edge to cover screen, slides out to the left.
        if (closing)
            offset = w * (1.0f - progress); // rect left edge: w -> 0
        else
            offset = -w * progress; // rect left edge: 0 -> -w
        p0 = ImVec2(offset, 0.0f);
        p1 = ImVec2(offset + w, h);
        break;

    case TransitionType::SlideRight:
        // Slides in from the left edge, slides out to the right.
        if (closing)
            offset = -w * (1.0f - progress); // rect left edge: -w -> 0
        else
            offset = w * progress; // rect left edge: 0 -> w
        p0 = ImVec2(offset, 0.0f);
        p1 = ImVec2(offset + w, h);
        break;

    case TransitionType::SlideUp:
        // Slides in from the bottom, slides out upward.
        if (closing)
            offset = h * (1.0f - progress); // rect top edge: h -> 0
        else
            offset = -h * progress; // rect top edge: 0 -> -h
        p0 = ImVec2(0.0f, offset);
        p1 = ImVec2(w, offset + h);
        break;

    case TransitionType::SlideDown:
        // Slides in from the top, slides out downward.
        if (closing)
            offset = -h * (1.0f - progress); // rect top edge: -h -> 0
        else
            offset = h * progress; // rect top edge: 0 -> h
        p0 = ImVec2(0.0f, offset);
        p1 = ImVec2(w, offset + h);
        break;

    default:
        return;
    }

    dl->AddRectFilled(p0, p1, col);
}

} // namespace Enjin::GUI
