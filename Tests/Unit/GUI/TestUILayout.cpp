#include "EnjinTest.h"
#include "Enjin/GUI/UISystem.h"
#include "Enjin/GUI/UICanvas.h"
#include <cmath>

using namespace Enjin;
using namespace Enjin::GUI;

// Unified display P3: container layout runs in ComputeLayout, so the arranged
// rects are what rendering AND input both see. These tests drive the public
// ComputeLayoutForCanvas with a 1:1 viewport (design == viewport => scale 1).

namespace {
UICanvasComponent MakeCanvas() {
    UICanvasComponent c;
    c.designWidth = 800.0f;
    c.designHeight = 600.0f;
    c.scaleMode = UIScaleMode::ConstantPixelSize;
    return c;
}

// A child sized via anchors: anchored to parent top-left, fixed w x h.
void SizeChild(UIElement* e, f32 w, f32 h) {
    e->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
    e->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
    e->anchor.offsetLeft = 0.0f;
    e->anchor.offsetRight = w;
    e->anchor.offsetTop = 0.0f;
    e->anchor.offsetBottom = h;
}

bool Near(f32 a, f32 b) { return std::fabs(a - b) < 0.01f; }
} // namespace

ENJIN_TEST(UILayout, VStackArrangesAndInputAgrees) {
    UICanvasComponent canvas = MakeCanvas();
    u32 box = canvas.AddElement(UIWidgetType::Panel, "box");
    UIElement* boxEl = canvas.GetElement(box);
    SizeChild(boxEl, 200.0f, 400.0f);
    boxEl->anchor.offsetLeft = 100.0f; boxEl->anchor.offsetRight = 300.0f;
    boxEl->anchor.offsetTop = 50.0f;   boxEl->anchor.offsetBottom = 450.0f;
    boxEl->layoutMode = UILayoutMode::VStack;
    boxEl->layoutSpacing = 10.0f;
    boxEl->layoutPaddingX = 8.0f;
    boxEl->layoutPaddingY = 12.0f;
    boxEl->layoutAlign = UILayoutAlign::Stretch;

    u32 a = canvas.AddElement(UIWidgetType::Button, "a", box);
    u32 b = canvas.AddElement(UIWidgetType::Button, "b", box);
    u32 cc = canvas.AddElement(UIWidgetType::Button, "c", box);
    SizeChild(canvas.GetElement(a), 50.0f, 40.0f);
    SizeChild(canvas.GetElement(b), 50.0f, 60.0f);
    SizeChild(canvas.GetElement(cc), 50.0f, 40.0f);

    UISystem ui;
    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);

    const UIRect& ra = canvas.GetElement(a)->computedRect;
    const UIRect& rb = canvas.GetElement(b)->computedRect;
    const UIRect& rc = canvas.GetElement(cc)->computedRect;

    // Stretch: children fill the inner width (200 - 2*8 = 184) at x=108.
    ENJIN_EXPECT_TRUE(Near(ra.x, 108.0f) && Near(ra.w, 184.0f));
    // Stacked: y = 50+12, then +40+10, then +60+10; heights preserved.
    ENJIN_EXPECT_TRUE(Near(ra.y, 62.0f)  && Near(ra.h, 40.0f));
    ENJIN_EXPECT_TRUE(Near(rb.y, 112.0f) && Near(rb.h, 60.0f));
    ENJIN_EXPECT_TRUE(Near(rc.y, 182.0f) && Near(rc.h, 40.0f));
    // The arranged rect IS the hit-test rect.
    ENJIN_EXPECT_TRUE(rb.Contains(150.0f, 140.0f));
}

ENJIN_TEST(UILayout, HStackCenterKeepsChildSizes) {
    UICanvasComponent canvas = MakeCanvas();
    u32 bar = canvas.AddElement(UIWidgetType::Panel, "bar");
    UIElement* barEl = canvas.GetElement(bar);
    SizeChild(barEl, 300.0f, 100.0f);
    barEl->layoutMode = UILayoutMode::HStack;
    barEl->layoutSpacing = 5.0f;
    barEl->layoutAlign = UILayoutAlign::Center;

    u32 a = canvas.AddElement(UIWidgetType::Button, "a", bar);
    u32 b = canvas.AddElement(UIWidgetType::Button, "b", bar);
    SizeChild(canvas.GetElement(a), 60.0f, 40.0f);
    SizeChild(canvas.GetElement(b), 80.0f, 20.0f);

    UISystem ui;
    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);

    const UIRect& ra = canvas.GetElement(a)->computedRect;
    const UIRect& rb = canvas.GetElement(b)->computedRect;
    ENJIN_EXPECT_TRUE(Near(ra.x, 0.0f)  && Near(ra.w, 60.0f));
    ENJIN_EXPECT_TRUE(Near(rb.x, 65.0f) && Near(rb.w, 80.0f));
    // Centered on the 100-high bar: 40-high at y=30, 20-high at y=40.
    ENJIN_EXPECT_TRUE(Near(ra.y, 30.0f) && Near(ra.h, 40.0f));
    ENJIN_EXPECT_TRUE(Near(rb.y, 40.0f) && Near(rb.h, 20.0f));
}

ENJIN_TEST(UILayout, GridCellsAndRows) {
    UICanvasComponent canvas = MakeCanvas();
    u32 grid = canvas.AddElement(UIWidgetType::Panel, "grid");
    UIElement* gridEl = canvas.GetElement(grid);
    SizeChild(gridEl, 230.0f, 300.0f);
    gridEl->layoutMode = UILayoutMode::Grid;
    gridEl->layoutSpacing = 10.0f;
    gridEl->data.gridColumns = 2;

    u32 ids[3];
    for (int i = 0; i < 3; ++i) {
        ids[i] = canvas.AddElement(UIWidgetType::Button, "cell", grid);
        SizeChild(canvas.GetElement(ids[i]), 50.0f, 50.0f);
    }

    UISystem ui;
    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);

    // Cell width = (230 - 10) / 2 = 110.
    const UIRect& r0 = canvas.GetElement(ids[0])->computedRect;
    const UIRect& r1 = canvas.GetElement(ids[1])->computedRect;
    const UIRect& r2 = canvas.GetElement(ids[2])->computedRect;
    ENJIN_EXPECT_TRUE(Near(r0.w, 110.0f) && Near(r0.x, 0.0f)   && Near(r0.y, 0.0f));
    ENJIN_EXPECT_TRUE(Near(r1.x, 120.0f) && Near(r1.y, 0.0f));
    ENJIN_EXPECT_TRUE(Near(r2.x, 0.0f)   && Near(r2.y, 60.0f));   // second row: 50 + spacing
}

ENJIN_TEST(UILayout, NestedContainersRelayout) {
    // HStack containing a VStack: the VStack is moved by the HStack, and its
    // own children must be arranged inside the MOVED rect.
    UICanvasComponent canvas = MakeCanvas();
    u32 row = canvas.AddElement(UIWidgetType::Panel, "row");
    UIElement* rowEl = canvas.GetElement(row);
    SizeChild(rowEl, 400.0f, 200.0f);
    rowEl->layoutMode = UILayoutMode::HStack;
    rowEl->layoutSpacing = 0.0f;
    rowEl->layoutAlign = UILayoutAlign::Stretch;

    u32 spacer = canvas.AddElement(UIWidgetType::Panel, "spacer", row);
    SizeChild(canvas.GetElement(spacer), 150.0f, 10.0f);

    u32 col = canvas.AddElement(UIWidgetType::Panel, "col", row);
    UIElement* colEl = canvas.GetElement(col);
    SizeChild(colEl, 100.0f, 10.0f);
    colEl->layoutMode = UILayoutMode::VStack;
    colEl->layoutSpacing = 0.0f;
    colEl->layoutAlign = UILayoutAlign::Stretch;

    u32 leaf = canvas.AddElement(UIWidgetType::Button, "leaf", col);
    SizeChild(canvas.GetElement(leaf), 10.0f, 30.0f);

    UISystem ui;
    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);

    // col sits after the 150-wide spacer, stretched to the row's height.
    const UIRect& rcol = canvas.GetElement(col)->computedRect;
    ENJIN_EXPECT_TRUE(Near(rcol.x, 150.0f) && Near(rcol.h, 200.0f));
    // leaf was re-laid-out inside the MOVED col: x follows col, stretched wide.
    const UIRect& rleaf = canvas.GetElement(leaf)->computedRect;
    ENJIN_EXPECT_TRUE(Near(rleaf.x, 150.0f) && Near(rleaf.w, 100.0f) && Near(rleaf.h, 30.0f));
}

ENJIN_TEST(UILayout, NoneModeLeavesAnchorsAlone) {
    UICanvasComponent canvas = MakeCanvas();
    u32 panel = canvas.AddElement(UIWidgetType::Panel, "panel");
    SizeChild(canvas.GetElement(panel), 200.0f, 200.0f);
    u32 child = canvas.AddElement(UIWidgetType::Button, "child", panel);
    UIElement* childEl = canvas.GetElement(child);
    SizeChild(childEl, 50.0f, 50.0f);
    childEl->anchor.offsetLeft = 25.0f; childEl->anchor.offsetRight = 75.0f;
    childEl->anchor.offsetTop = 30.0f;  childEl->anchor.offsetBottom = 80.0f;

    UISystem ui;
    ui.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);
    const UIRect& rc = canvas.GetElement(child)->computedRect;
    ENJIN_EXPECT_TRUE(Near(rc.x, 25.0f) && Near(rc.y, 30.0f) && Near(rc.w, 50.0f) && Near(rc.h, 50.0f));
}

ENJIN_TEST_MAIN()
