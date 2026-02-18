#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UISystem.h"

using namespace Enjin;
using namespace Enjin::GUI;

// ============================================================================
// 1. UIElement creation and hierarchy
// ============================================================================

ENJIN_TEST(UICanvas, AddElementAssignsIncrementingIds) {
    UICanvasComponent canvas;
    u32 id1 = canvas.AddElement(UIWidgetType::Panel, "Panel1");
    u32 id2 = canvas.AddElement(UIWidgetType::Button, "Button1");
    u32 id3 = canvas.AddElement(UIWidgetType::Label, "Label1");

    ENJIN_EXPECT_EQ(id1, 1u);
    ENJIN_EXPECT_EQ(id2, 2u);
    ENJIN_EXPECT_EQ(id3, 3u);
    ENJIN_EXPECT_EQ(canvas.elements.size(), (usize)3);
}

ENJIN_TEST(UICanvas, AddChildRegistersParent) {
    UICanvasComponent canvas;
    u32 parentId = canvas.AddElement(UIWidgetType::Panel, "Parent");
    u32 childId  = canvas.AddElement(UIWidgetType::Button, "Child", parentId);

    // Child should record its parent
    UIElement* child = canvas.GetElement(childId);
    ENJIN_ASSERT_NOT_NULL(child);
    ENJIN_EXPECT_EQ(child->parentId, parentId);

    // Parent should list child
    UIElement* parent = canvas.GetElement(parentId);
    ENJIN_ASSERT_NOT_NULL(parent);
    ENJIN_EXPECT_EQ(parent->childIds.size(), (usize)1);
    ENJIN_EXPECT_EQ(parent->childIds[0], childId);
}

ENJIN_TEST(UICanvas, RemoveElementAndDescendants) {
    UICanvasComponent canvas;
    u32 rootId  = canvas.AddElement(UIWidgetType::Panel, "Root");
    u32 childId = canvas.AddElement(UIWidgetType::Panel, "Child", rootId);
    u32 grandchildId = canvas.AddElement(UIWidgetType::Label, "Grandchild", childId);

    ENJIN_EXPECT_EQ(canvas.elements.size(), (usize)3);

    // Remove the child -- should also remove the grandchild
    canvas.RemoveElement(childId);
    ENJIN_EXPECT_EQ(canvas.elements.size(), (usize)1);
    ENJIN_EXPECT_NULL(canvas.GetElement(childId));
    ENJIN_EXPECT_NULL(canvas.GetElement(grandchildId));

    // Root should have no children left
    UIElement* root = canvas.GetElement(rootId);
    ENJIN_ASSERT_NOT_NULL(root);
    ENJIN_EXPECT_EQ(root->childIds.size(), (usize)0);
}

// ============================================================================
// 2. UIAnchor layout calculations
// ============================================================================

ENJIN_TEST(UILayout, CenteredElementComputesCorrectRect) {
    UICanvasComponent canvas;
    canvas.designWidth  = 1920.0f;
    canvas.designHeight = 1080.0f;
    canvas.scaleMode = UIScaleMode::ConstantPixelSize;

    u32 id = canvas.AddElement(UIWidgetType::Panel, "Centered");
    UIElement* elem = canvas.GetElement(id);
    ENJIN_ASSERT_NOT_NULL(elem);

    // Anchor at center (0.5, 0.5), negative offsets trigger fixed-size path (w < 0)
    elem->anchor.anchorMin = Math::Vector2(0.5f, 0.5f);
    elem->anchor.anchorMax = Math::Vector2(0.5f, 0.5f);
    elem->anchor.pivot     = Math::Vector2(0.5f, 0.5f);
    elem->anchor.offsetLeft   = -100.0f;
    elem->anchor.offsetRight  = 100.0f;   // left < right triggers negative w path
    elem->anchor.offsetTop    = -50.0f;
    elem->anchor.offsetBottom = 50.0f;

    UISystem system;
    system.ComputeLayoutForCanvas(canvas, 1920.0f, 1080.0f);

    // resolvedLeft = 960 + (-100) = 860, resolvedRight = 960 + 100 = 1060
    // w = 1060 - 860 = 200, h = 590 - 490 = 100
    ENJIN_EXPECT_FLOAT_NEAR(elem->computedRect.w, 200.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(elem->computedRect.h, 100.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(elem->computedRect.x, 860.0f, 0.01f);
    ENJIN_EXPECT_FLOAT_NEAR(elem->computedRect.y, 490.0f, 0.01f);
}

ENJIN_TEST(UILayout, StretchedElementFillsParent) {
    UICanvasComponent canvas;
    canvas.designWidth  = 800.0f;
    canvas.designHeight = 600.0f;
    canvas.scaleMode = UIScaleMode::ConstantPixelSize;

    u32 id = canvas.AddElement(UIWidgetType::Panel, "Stretched");
    UIElement* elem = canvas.GetElement(id);
    ENJIN_ASSERT_NOT_NULL(elem);

    // Stretch from (0,0) to (1,1) with zero offsets -> fills entire viewport
    elem->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
    elem->anchor.anchorMax = Math::Vector2(1.0f, 1.0f);
    elem->anchor.offsetLeft   = 0.0f;
    elem->anchor.offsetRight  = 0.0f;
    elem->anchor.offsetTop    = 0.0f;
    elem->anchor.offsetBottom = 0.0f;

    UISystem system;
    system.ComputeLayoutForCanvas(canvas, 800.0f, 600.0f);

    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.x, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.y, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.w, 800.0f);
    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.h, 600.0f);
}

// ============================================================================
// 3. Widget types (Panel, Button, Label, Slider, Checkbox)
// ============================================================================

ENJIN_TEST(UICanvas, WidgetTypeAssignment) {
    UICanvasComponent canvas;

    u32 panelId    = canvas.AddElement(UIWidgetType::Panel, "P");
    u32 buttonId   = canvas.AddElement(UIWidgetType::Button, "B");
    u32 labelId    = canvas.AddElement(UIWidgetType::Label, "L");
    u32 sliderId   = canvas.AddElement(UIWidgetType::Slider, "S");
    u32 checkboxId = canvas.AddElement(UIWidgetType::Checkbox, "C");

    ENJIN_EXPECT_EQ((int)canvas.GetElement(panelId)->type, (int)UIWidgetType::Panel);
    ENJIN_EXPECT_EQ((int)canvas.GetElement(buttonId)->type, (int)UIWidgetType::Button);
    ENJIN_EXPECT_EQ((int)canvas.GetElement(labelId)->type, (int)UIWidgetType::Label);
    ENJIN_EXPECT_EQ((int)canvas.GetElement(sliderId)->type, (int)UIWidgetType::Slider);
    ENJIN_EXPECT_EQ((int)canvas.GetElement(checkboxId)->type, (int)UIWidgetType::Checkbox);

    // Panel is not focusable by default type, Button/Slider/Checkbox are
    ENJIN_EXPECT_FALSE(IsFocusableType(UIWidgetType::Panel));
    ENJIN_EXPECT_TRUE(IsFocusableType(UIWidgetType::Button));
    ENJIN_EXPECT_TRUE(IsFocusableType(UIWidgetType::Slider));
    ENJIN_EXPECT_TRUE(IsFocusableType(UIWidgetType::Checkbox));
    ENJIN_EXPECT_FALSE(IsFocusableType(UIWidgetType::Label));
}

// ============================================================================
// 4. Style override application
// ============================================================================

ENJIN_TEST(UIStyleOverride, DetectsActiveOverrides) {
    UIStyleOverride style;

    // All fields should default to "not set" (-1)
    ENJIN_EXPECT_FALSE(style.HasBgColor());
    ENJIN_EXPECT_FALSE(style.HasTextColor());
    ENJIN_EXPECT_FALSE(style.HasBorderColor());
    ENJIN_EXPECT_FALSE(style.HasBgAlpha());
    ENJIN_EXPECT_FALSE(style.HasBorderRadius());
    ENJIN_EXPECT_FALSE(style.HasBorderWidth());
    ENJIN_EXPECT_FALSE(style.HasFontSize());
    ENJIN_EXPECT_FALSE(style.HasFocusColor());

    // Set overrides
    style.bgColor    = Math::Vector3(0.2f, 0.3f, 0.4f);
    style.textColor  = Math::Vector3(1.0f, 1.0f, 1.0f);
    style.bgAlpha    = 0.8f;
    style.fontSize   = 20.0f;

    ENJIN_EXPECT_TRUE(style.HasBgColor());
    ENJIN_EXPECT_TRUE(style.HasTextColor());
    ENJIN_EXPECT_TRUE(style.HasBgAlpha());
    ENJIN_EXPECT_TRUE(style.HasFontSize());

    // Unset fields remain inactive
    ENJIN_EXPECT_FALSE(style.HasBorderColor());
    ENJIN_EXPECT_FALSE(style.HasBorderRadius());
    ENJIN_EXPECT_FALSE(style.HasBorderWidth());
    ENJIN_EXPECT_FALSE(style.HasFocusColor());
}

// ============================================================================
// 5. Focus navigation (next/previous element)
// ============================================================================

ENJIN_TEST(UIFocus, SetAndClearFocus) {
    UICanvasComponent canvas;
    u32 btnId = canvas.AddElement(UIWidgetType::Button, "Btn");

    UISystem system;
    ENJIN_EXPECT_EQ(system.GetFocusedElementId(canvas), 0u);

    system.SetFocus(canvas, btnId);
    ENJIN_EXPECT_EQ(system.GetFocusedElementId(canvas), btnId);

    system.ClearFocus(canvas);
    ENJIN_EXPECT_EQ(system.GetFocusedElementId(canvas), 0u);
}

ENJIN_TEST(UIFocus, SetFocusSameIdIsIdempotent) {
    UICanvasComponent canvas;
    u32 btnId = canvas.AddElement(UIWidgetType::Button, "Btn");

    // Track announcements to verify no duplicate announcements
    int announcementCount = 0;
    UISystem system;
    system.SetAnnouncerCallback([&](const std::string&) { announcementCount++; });

    system.SetFocus(canvas, btnId);
    ENJIN_EXPECT_EQ(announcementCount, 1);

    // Setting same focus again should be idempotent (early return)
    system.SetFocus(canvas, btnId);
    ENJIN_EXPECT_EQ(announcementCount, 1);
}

// ============================================================================
// 6. Element visibility toggling
// ============================================================================

ENJIN_TEST(UIElement, VisibilityToggle) {
    UICanvasComponent canvas;
    u32 id = canvas.AddElement(UIWidgetType::Panel, "Panel");
    UIElement* elem = canvas.GetElement(id);
    ENJIN_ASSERT_NOT_NULL(elem);

    // Visible by default
    ENJIN_EXPECT_TRUE(elem->visible);

    elem->visible = false;
    ENJIN_EXPECT_FALSE(elem->visible);

    elem->visible = true;
    ENJIN_EXPECT_TRUE(elem->visible);
}

// ============================================================================
// 7. Nine-slice config validation
// ============================================================================

ENJIN_TEST(NineSlice, IsActiveRequiresPathAndBorders) {
    NineSliceConfig config;

    // Empty path => inactive
    ENJIN_EXPECT_FALSE(config.IsActive());

    // Path set but no borders => inactive
    config.texturePath = "sprites/panel.png";
    ENJIN_EXPECT_FALSE(config.IsActive());

    // Path + at least one border => active
    config.borderLeft = 8.0f;
    ENJIN_EXPECT_TRUE(config.IsActive());

    // All borders set
    config.borderRight  = 8.0f;
    config.borderTop    = 8.0f;
    config.borderBottom = 8.0f;
    ENJIN_EXPECT_TRUE(config.IsActive());

    // Clear path => inactive again
    config.texturePath.clear();
    ENJIN_EXPECT_FALSE(config.IsActive());
}

// ============================================================================
// 8. Font scale multiplication
// ============================================================================

ENJIN_TEST(UISystem, FontScaleGetSet) {
    UISystem system;

    // Default should be 1.0
    ENJIN_EXPECT_FLOAT_EQ(system.GetFontScale(), 1.0f);

    system.SetFontScale(2.0f);
    ENJIN_EXPECT_FLOAT_EQ(system.GetFontScale(), 2.0f);

    system.SetFontScale(0.75f);
    ENJIN_EXPECT_FLOAT_EQ(system.GetFontScale(), 0.75f);

    // Values at or below 0.1 should clamp to 1.0
    system.SetFontScale(0.05f);
    ENJIN_EXPECT_FLOAT_EQ(system.GetFontScale(), 1.0f);

    system.SetFontScale(-1.0f);
    ENJIN_EXPECT_FLOAT_EQ(system.GetFontScale(), 1.0f);
}

// ============================================================================
// 9. Element find by ID
// ============================================================================

ENJIN_TEST(UICanvas, GetElementByIdAndNotFound) {
    UICanvasComponent canvas;
    u32 id1 = canvas.AddElement(UIWidgetType::Panel, "Alpha");
    u32 id2 = canvas.AddElement(UIWidgetType::Button, "Beta");

    // Find existing elements
    UIElement* elem1 = canvas.GetElement(id1);
    ENJIN_ASSERT_NOT_NULL(elem1);
    ENJIN_EXPECT_STR_EQ(elem1->name, "Alpha");
    ENJIN_EXPECT_EQ(elem1->id, id1);

    UIElement* elem2 = canvas.GetElement(id2);
    ENJIN_ASSERT_NOT_NULL(elem2);
    ENJIN_EXPECT_STR_EQ(elem2->name, "Beta");

    // Non-existent ID returns nullptr
    ENJIN_EXPECT_NULL(canvas.GetElement(9999));
    ENJIN_EXPECT_NULL(canvas.GetElement(0));
}

ENJIN_TEST(UICanvas, FindElementByNameViaLinearSearch) {
    UICanvasComponent canvas;
    canvas.AddElement(UIWidgetType::Panel, "Header");
    canvas.AddElement(UIWidgetType::Button, "SubmitBtn");
    canvas.AddElement(UIWidgetType::Label, "StatusLabel");

    // Linear search for element by name (no built-in API, but the pattern
    // users would follow with the elements vector)
    UIElement* found = nullptr;
    for (auto& elem : canvas.elements) {
        if (elem.name == "SubmitBtn") {
            found = &elem;
            break;
        }
    }
    ENJIN_ASSERT_NOT_NULL(found);
    ENJIN_EXPECT_EQ((int)found->type, (int)UIWidgetType::Button);

    // Name that does not exist
    UIElement* missing = nullptr;
    for (auto& elem : canvas.elements) {
        if (elem.name == "NonExistent") {
            missing = &elem;
            break;
        }
    }
    ENJIN_EXPECT_NULL(missing);
}

// ============================================================================
// 10. Canvas design resolution and scale modes
// ============================================================================

ENJIN_TEST(UICanvas, DesignResolutionDefaults) {
    UICanvasComponent canvas;
    ENJIN_EXPECT_FLOAT_EQ(canvas.designWidth, 1920.0f);
    ENJIN_EXPECT_FLOAT_EQ(canvas.designHeight, 1080.0f);
    ENJIN_EXPECT_EQ((int)canvas.scaleMode, (int)UIScaleMode::ScaleWithScreenSize);
}

ENJIN_TEST(UICanvas, ScaleModeAffectsLayout) {
    // Create canvas with a fixed-size element
    UICanvasComponent canvas;
    canvas.designWidth  = 1920.0f;
    canvas.designHeight = 1080.0f;

    u32 id = canvas.AddElement(UIWidgetType::Panel, "Box");
    UIElement* elem = canvas.GetElement(id);
    ENJIN_ASSERT_NOT_NULL(elem);

    // Fixed-size element anchored at top-left corner: 200x100 pixels
    elem->anchor.anchorMin = Math::Vector2(0.0f, 0.0f);
    elem->anchor.anchorMax = Math::Vector2(0.0f, 0.0f);
    elem->anchor.pivot     = Math::Vector2(0.0f, 0.0f);
    elem->anchor.offsetLeft   = 0.0f;
    elem->anchor.offsetRight  = 200.0f;
    elem->anchor.offsetTop    = 0.0f;
    elem->anchor.offsetBottom = 100.0f;

    UISystem system;

    // ConstantPixelSize: scale factor = 1.0 regardless of viewport
    canvas.scaleMode = UIScaleMode::ConstantPixelSize;
    system.ComputeLayoutForCanvas(canvas, 960.0f, 540.0f);
    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.w, 200.0f);
    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.h, 100.0f);

    // ScaleWithScreenSize: viewport is half design => scale factor = 0.5
    canvas.scaleMode = UIScaleMode::ScaleWithScreenSize;
    system.ComputeLayoutForCanvas(canvas, 960.0f, 540.0f);
    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.w, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(elem->computedRect.h, 50.0f);
}

// ============================================================================
// Additional coverage: UIRect hit testing, accessibility, widget data
// ============================================================================

ENJIN_TEST(UIRect, ContainsPointHitTest) {
    UIRect rect;
    rect.x = 100.0f;
    rect.y = 200.0f;
    rect.w = 50.0f;
    rect.h = 30.0f;

    // Inside
    ENJIN_EXPECT_TRUE(rect.Contains(125.0f, 215.0f));
    // Edges (inclusive)
    ENJIN_EXPECT_TRUE(rect.Contains(100.0f, 200.0f));
    ENJIN_EXPECT_TRUE(rect.Contains(150.0f, 230.0f));
    // Outside
    ENJIN_EXPECT_FALSE(rect.Contains(99.0f, 215.0f));
    ENJIN_EXPECT_FALSE(rect.Contains(151.0f, 215.0f));
    ENJIN_EXPECT_FALSE(rect.Contains(125.0f, 199.0f));
    ENJIN_EXPECT_FALSE(rect.Contains(125.0f, 231.0f));
}

ENJIN_TEST(UICanvas, DuplicateElementCreatesIndependentCopy) {
    UICanvasComponent canvas;
    u32 parentId = canvas.AddElement(UIWidgetType::Panel, "Parent");
    u32 childId  = canvas.AddElement(UIWidgetType::Button, "OriginalBtn", parentId);

    UIElement* original = canvas.GetElement(childId);
    ENJIN_ASSERT_NOT_NULL(original);
    original->data.text = "Click Me";

    u32 dupId = canvas.DuplicateElement(childId);
    ENJIN_EXPECT_NE(dupId, 0u);
    ENJIN_EXPECT_NE(dupId, childId);

    UIElement* dup = canvas.GetElement(dupId);
    ENJIN_ASSERT_NOT_NULL(dup);
    ENJIN_EXPECT_STR_EQ(dup->name, "OriginalBtn Copy");
    ENJIN_EXPECT_STR_EQ(dup->data.text, "Click Me");
    ENJIN_EXPECT_EQ(dup->parentId, parentId);
    ENJIN_EXPECT_EQ((int)dup->type, (int)UIWidgetType::Button);

    // Parent should now have two children
    UIElement* parent = canvas.GetElement(parentId);
    ENJIN_ASSERT_NOT_NULL(parent);
    ENJIN_EXPECT_EQ(parent->childIds.size(), (usize)2);
}

ENJIN_TEST(UISystem, AccessibilitySettingsRoundTrip) {
    UISystem system;

    // Reduced motion
    ENJIN_EXPECT_FALSE(system.GetReducedMotion());
    system.SetReducedMotion(true);
    ENJIN_EXPECT_TRUE(system.GetReducedMotion());

    // Switch access
    ENJIN_EXPECT_FALSE(system.GetSwitchAccessEnabled());
    system.SetSwitchAccessEnabled(true, 2.0f);
    ENJIN_EXPECT_TRUE(system.GetSwitchAccessEnabled());

    // Dwell click
    ENJIN_EXPECT_FALSE(system.GetDwellClickEnabled());
    system.SetDwellClickEnabled(true, 1.5f);
    ENJIN_EXPECT_TRUE(system.GetDwellClickEnabled());
    ENJIN_EXPECT_FLOAT_EQ(system.GetDwellClickTime(), 1.5f);

    // Dwell click time clamps to 1.0 for very small values
    system.SetDwellClickEnabled(true, 0.05f);
    ENJIN_EXPECT_FLOAT_EQ(system.GetDwellClickTime(), 1.0f);

    // Sticky drag
    ENJIN_EXPECT_FALSE(system.GetStickyDragEnabled());
    system.SetStickyDragEnabled(true);
    ENJIN_EXPECT_TRUE(system.GetStickyDragEnabled());
}

ENJIN_TEST_MAIN()
