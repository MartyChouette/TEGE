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

ENJIN_TEST_MAIN()
