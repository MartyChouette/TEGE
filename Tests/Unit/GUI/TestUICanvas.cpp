#include "EnjinTest.h"
#include "Enjin/GUI/UICanvas.h"

using namespace Enjin;
using namespace Enjin::GUI;

// ===========================================================================
// UICanvasComponent Defaults
// ===========================================================================

ENJIN_TEST(UICanvasDefaults, CanvasName) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_STR_EQ(canvas.canvasName, "Canvas");
}

ENJIN_TEST(UICanvasDefaults, Visible) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_TRUE(canvas.visible);
}

ENJIN_TEST(UICanvasDefaults, SortOrder) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_EQ(canvas.sortOrder, 0);
}

ENJIN_TEST(UICanvasDefaults, DesignResolution) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_FLOAT_EQ(canvas.designWidth, 1920.0f);
    ENJIN_EXPECT_FLOAT_EQ(canvas.designHeight, 1080.0f);
}

ENJIN_TEST(UICanvasDefaults, ScaleMode) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_EQ((int)canvas.scaleMode, (int)UIScaleMode::ScaleWithScreenSize);
}

ENJIN_TEST(UICanvasDefaults, EmptyElements) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_EQ(canvas.elements.size(), (size_t)0);
    ENJIN_EXPECT_EQ(canvas.nextElementId, 1u);
    ENJIN_EXPECT_EQ(canvas.focusedElementId, 0u);
}

// ===========================================================================
// Element Management
// ===========================================================================

ENJIN_TEST(UICanvasElements, AddElement) {
    UICanvasComponent canvas;
    u32 id = canvas.AddElement(UIWidgetType::Button, "MyButton");
    ENJIN_EXPECT_NE(id, 0u);
    ENJIN_EXPECT_EQ(canvas.elements.size(), (size_t)1);
}

ENJIN_TEST(UICanvasElements, AddMultiple) {
    UICanvasComponent canvas;
    u32 id1 = canvas.AddElement(UIWidgetType::Button, "Btn1");
    u32 id2 = canvas.AddElement(UIWidgetType::Label, "Lbl1");
    u32 id3 = canvas.AddElement(UIWidgetType::Panel, "Panel1");
    ENJIN_EXPECT_NE(id1, id2);
    ENJIN_EXPECT_NE(id2, id3);
    ENJIN_EXPECT_EQ(canvas.elements.size(), (size_t)3);
}

ENJIN_TEST(UICanvasElements, GetElement) {
    UICanvasComponent canvas;
    u32 id = canvas.AddElement(UIWidgetType::Slider, "Volume");
    UIElement* elem = canvas.GetElement(id);
    ENJIN_ASSERT_NOT_NULL(elem);
    ENJIN_EXPECT_STR_EQ(elem->name, "Volume");
    ENJIN_EXPECT_EQ((int)elem->type, (int)UIWidgetType::Slider);
}

ENJIN_TEST(UICanvasElements, GetNonexistentReturnsNull) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_NULL(canvas.GetElement(999));
}

ENJIN_TEST(UICanvasElements, RemoveElement) {
    UICanvasComponent canvas;
    u32 id = canvas.AddElement(UIWidgetType::Button, "ToRemove");
    canvas.RemoveElement(id);
    ENJIN_EXPECT_NULL(canvas.GetElement(id));
}

ENJIN_TEST(UICanvasElements, AddChildElement) {
    UICanvasComponent canvas;
    u32 parentId = canvas.AddElement(UIWidgetType::Panel, "Parent");
    u32 childId = canvas.AddElement(UIWidgetType::Label, "Child", parentId);
    UIElement* child = canvas.GetElement(childId);
    ENJIN_ASSERT_NOT_NULL(child);
    ENJIN_EXPECT_EQ(child->parentId, parentId);
}

ENJIN_TEST(UICanvasElements, GetRootElementIds) {
    UICanvasComponent canvas;
    u32 root1 = canvas.AddElement(UIWidgetType::Panel, "Root1");
    u32 root2 = canvas.AddElement(UIWidgetType::Panel, "Root2");
    canvas.AddElement(UIWidgetType::Label, "Child", root1);
    auto roots = canvas.GetRootElementIds();
    ENJIN_EXPECT_EQ(roots.size(), (size_t)2);
}

ENJIN_TEST(UICanvasElements, GetChildIds) {
    UICanvasComponent canvas;
    u32 parentId = canvas.AddElement(UIWidgetType::Panel, "Parent");
    canvas.AddElement(UIWidgetType::Button, "Child1", parentId);
    canvas.AddElement(UIWidgetType::Label, "Child2", parentId);
    auto children = canvas.GetChildIds(parentId);
    ENJIN_EXPECT_EQ(children.size(), (size_t)2);
}

ENJIN_TEST(UICanvasElements, DuplicateElement) {
    UICanvasComponent canvas;
    u32 origId = canvas.AddElement(UIWidgetType::Button, "Orig");
    UIElement* orig = canvas.GetElement(origId);
    orig->data.text = "Click Me";
    u32 dupId = canvas.DuplicateElement(origId);
    ENJIN_EXPECT_NE(dupId, origId);
    UIElement* dup = canvas.GetElement(dupId);
    ENJIN_ASSERT_NOT_NULL(dup);
    ENJIN_EXPECT_STR_EQ(dup->data.text, "Click Me");
}

// ===========================================================================
// UIElement Defaults
// ===========================================================================

ENJIN_TEST(UIElement, Defaults) {
    UIElement elem;
    ENJIN_EXPECT_TRUE(elem.visible);
    ENJIN_EXPECT_TRUE(elem.enabled);
    ENJIN_EXPECT_TRUE(elem.focusable);
    ENJIN_EXPECT_EQ(elem.tabOrder, 0);
    ENJIN_EXPECT_EQ(elem.parentId, 0u);
}

// ===========================================================================
// UIRect
// ===========================================================================

ENJIN_TEST(UIRect, Contains) {
    UIRect rect;
    rect.x = 10; rect.y = 20; rect.w = 100; rect.h = 50;
    ENJIN_EXPECT_TRUE(rect.Contains(50.0f, 40.0f));
    ENJIN_EXPECT_FALSE(rect.Contains(5.0f, 40.0f));
    ENJIN_EXPECT_FALSE(rect.Contains(50.0f, 80.0f));
}

// ===========================================================================
// IsFocusableType
// ===========================================================================

ENJIN_TEST(UIFocusable, ButtonIsFocusable) {
    ENJIN_EXPECT_TRUE(IsFocusableType(UIWidgetType::Button));
}

ENJIN_TEST(UIFocusable, PanelNotFocusable) {
    ENJIN_EXPECT_FALSE(IsFocusableType(UIWidgetType::Panel));
}

ENJIN_TEST(UIFocusable, TextInputIsFocusable) {
    ENJIN_EXPECT_TRUE(IsFocusableType(UIWidgetType::TextInput));
}

// ===========================================================================
// Anchor-aware design-rect API (the editor's positioning backbone)
// ===========================================================================

ENJIN_TEST(UIAnchorAPI, SetDesignRectPreservesAnchors) {
    UICanvasComponent c;
    u32 id = c.AddElement(UIWidgetType::Panel, "p");
    UIElement* el = c.GetElement(id);
    // Anchor to bottom-right, then position via the design-rect API
    el->anchor.anchorMin = Math::Vector2(1.0f, 1.0f);
    el->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
    c.SetDesignRect(id, 1600.0f, 980.0f, 200.0f, 80.0f);
    // Anchors must be untouched (the old editor path force-reset to top-left)
    ENJIN_EXPECT_FLOAT_EQ(el->anchor.anchorMin.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(el->anchor.anchorMax.y, 1.0f);
    // And the resolved rect must be exactly what was asked for
    UIRect r = c.GetDesignRect(id);
    ENJIN_EXPECT_FLOAT_EQ(r.x, 1600.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.y, 980.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.w, 200.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.h, 80.0f);
}

ENJIN_TEST(UIAnchorAPI, ApplyAnchorPresetKeepsElementInPlace) {
    UICanvasComponent c;
    u32 id = c.AddElement(UIWidgetType::Button, "b");
    c.SetDesignRect(id, 100.0f, 50.0f, 300.0f, 120.0f);
    UIRect before = c.GetDesignRect(id);
    // Re-anchor to bottom-right point; rect must not move at design res
    c.ApplyAnchorPreset(id, 2, 2);
    UIElement* el = c.GetElement(id);
    ENJIN_EXPECT_FLOAT_EQ(el->anchor.anchorMin.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(el->anchor.anchorMin.y, 1.0f);
    UIRect after = c.GetDesignRect(id);
    ENJIN_EXPECT_FLOAT_EQ(after.x, before.x);
    ENJIN_EXPECT_FLOAT_EQ(after.y, before.y);
    ENJIN_EXPECT_FLOAT_EQ(after.w, before.w);
    ENJIN_EXPECT_FLOAT_EQ(after.h, before.h);
}

ENJIN_TEST(UIAnchorAPI, StretchPresetTracksParentSize) {
    UICanvasComponent c;
    u32 id = c.AddElement(UIWidgetType::Panel, "bar");
    c.SetDesignRect(id, 0.0f, 0.0f, c.designWidth, 60.0f);
    c.ApplyAnchorPreset(id, 3, 0);  // stretch X, anchored top
    UIElement* el = c.GetElement(id);
    ENJIN_EXPECT_FLOAT_EQ(el->anchor.anchorMin.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(el->anchor.anchorMax.x, 1.0f);
    // Rect unchanged at design resolution
    UIRect r = c.GetDesignRect(id);
    ENJIN_EXPECT_FLOAT_EQ(r.w, c.designWidth);
    ENJIN_EXPECT_FLOAT_EQ(r.h, 60.0f);
}

ENJIN_TEST(UIAnchorAPI, InvertedOffsetsResolveToIntendedRect) {
    // The classic authoring mistake: +100/-100 instead of -100/+100 for a
    // centered 200-wide element. The edge-swap semantics must yield the
    // intended rect instead of the old off-screen legacy fallback.
    UICanvasComponent c;
    u32 id = c.AddElement(UIWidgetType::Label, "l");
    UIElement* el = c.GetElement(id);
    el->anchor.anchorMin = Math::Vector2(0.5f, 0.5f);
    el->anchor.anchorMax = Math::Vector2(0.5f, 0.5f);
    el->anchor.offsetLeft = 100.0f;   // inverted on purpose
    el->anchor.offsetRight = -100.0f;
    el->anchor.offsetTop = -40.0f;    // correct order
    el->anchor.offsetBottom = 40.0f;
    UIRect r = c.GetDesignRect(id);
    ENJIN_EXPECT_FLOAT_EQ(r.w, 200.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.h, 80.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.x, c.designWidth * 0.5f - 100.0f);
}

ENJIN_TEST(UIAnchorAPI, ChildDesignRectResolvesThroughParent) {
    UICanvasComponent c;
    u32 parent = c.AddElement(UIWidgetType::Panel, "parent");
    c.SetDesignRect(parent, 100.0f, 100.0f, 400.0f, 200.0f);
    u32 child = c.AddElement(UIWidgetType::Label, "child", parent);
    UIElement* el = c.GetElement(child);
    // Child stretches over the parent
    el->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
    el->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
    el->anchor.offsetLeft = el->anchor.offsetRight = 0.0f;
    el->anchor.offsetTop = el->anchor.offsetBottom = 0.0f;
    UIRect r = c.GetDesignRect(child);
    ENJIN_EXPECT_FLOAT_EQ(r.x, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.y, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.w, 400.0f);
    ENJIN_EXPECT_FLOAT_EQ(r.h, 200.0f);
}

ENJIN_TEST_MAIN()
