#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UIElement.h"
#include "Enjin/GUI/Localization.h"
#include <imgui.h>
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// UI Canvas helpers
// ============================================================================

static GUI::UICanvasComponent* GetCanvas(u64 entity) {
    if (!s_BindingsWorld) return nullptr;
    return s_BindingsWorld->GetComponent<GUI::UICanvasComponent>(entity);
}

// -- Canvas --

static void UI_SetCanvasVisible(u64 entity, bool visible) {
    auto* canvas = GetCanvas(entity);
    if (canvas) canvas->visible = visible;
}

static bool UI_IsCanvasVisible(u64 entity) {
    auto* canvas = GetCanvas(entity);
    return canvas ? canvas->visible : false;
}

static void UI_SetCanvasSortOrder(u64 entity, int order) {
    auto* canvas = GetCanvas(entity);
    if (canvas) canvas->sortOrder = order;
}

// -- Element Text --

static void UI_SetText(u64 entity, int elementId, const std::string& text) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->data.text = text;
}

static std::string UI_GetText(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return "";
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->data.text : std::string("");
}

// -- Element Visibility --

static void UI_SetElementVisible(u64 entity, int elementId, bool visible) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->visible = visible;
}

static bool UI_IsElementVisible(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return false;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->visible : false;
}

// -- Element Enabled --

static void UI_SetElementEnabled(u64 entity, int elementId, bool enabled) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->enabled = enabled;
}

// -- Progress Bar --

static void UI_SetProgress(u64 entity, int elementId, float value) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->data.progressValue = Math::Clamp(value, 0.0f, 1.0f);
}

static float UI_GetProgress(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return 0.0f;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->data.progressValue : 0.0f;
}

// -- Slider --

static void UI_SetSliderValue(u64 entity, int elementId, float value) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->data.sliderValue = Math::Clamp(value, el->data.sliderMin, el->data.sliderMax);
}

static float UI_GetSliderValue(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return 0.0f;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->data.sliderValue : 0.0f;
}

// -- Checkbox / Toggle --

static void UI_SetChecked(u64 entity, int elementId, bool checked) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->data.checked = checked;
}

static bool UI_IsChecked(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return false;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->data.checked : false;
}

// -- Image --

static void UI_SetImagePath(u64 entity, int elementId, const std::string& path) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->data.imagePath = path;
}

static void UI_SetImageAlpha(u64 entity, int elementId, float alpha) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->data.imageAlpha = Math::Clamp(alpha, 0.0f, 1.0f);
}

// -- Style --

static void UI_SetBgColor(u64 entity, int elementId, float r, float g, float b, float a) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (!el) return;
    el->style.bgColor = Math::Vector3(r, g, b);
    el->style.bgAlpha = a;
}

// -- Element lookup and creation --
//
// Every other UI_* function addresses an element by its integer id, and until now
// a script had no way to obtain one: ids are assigned by the canvas as elements
// are authored, are not shown anywhere in the inspector, and shift when an element
// is removed. So the entire UI surface was addressable only by hardcoding a
// number and hoping, which is why the HUD tutorial documented a
// `canvas.AddElement(...)` / `canvas.GetElement(...)` object API that has never
// existed -- the sample was reaching for the missing half of this.
//
// -1 for "no such element", never 0: 0 is the valid parent id meaning "canvas
// root", so a 0 return would be accepted as a real element by every function it
// was then passed to, and the wrong element would be written instead of none.
static int UI_FindElement(u64 entity, const std::string& name) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return -1;
    for (const auto& el : canvas->elements) {
        if (el.name == name) return static_cast<int>(el.id);
    }
    return -1;
}

static int UI_AddElement(u64 entity, int widgetType, const std::string& name, int parentId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) {
        ENJIN_LOG_WARN(Script, "UI_AddElement('%s'): entity has no UICanvasComponent",
                       name.c_str());
        return -1;
    }
    if (widgetType < 0 || widgetType >= static_cast<int>(GUI::UIWidgetType::Count)) {
        ENJIN_LOG_WARN(Script, "UI_AddElement('%s'): %d is not a UIWidgetType",
                       name.c_str(), widgetType);
        return -1;
    }
    // A parent that does not exist would silently produce a root element, which
    // lays out in the wrong place and looks like a broken anchor.
    if (parentId != 0 && canvas->GetElement(static_cast<u32>(parentId)) == nullptr) {
        ENJIN_LOG_WARN(Script, "UI_AddElement('%s'): parent id %d does not exist",
                       name.c_str(), parentId);
        return -1;
    }
    return static_cast<int>(canvas->AddElement(static_cast<GUI::UIWidgetType>(widgetType),
                                              name, static_cast<u32>(parentId)));
}

// The offsets above are insets from the anchored edges, so they only mean
// anything once the anchor itself is placed. Setting offsets with no way to set
// the anchor left every scripted element pinned to the parent's centre.
static void UI_SetElementAnchor(u64 entity, int elementId,
                                float minX, float minY, float maxX, float maxY) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (!el) return;
    el->anchor.anchorMin = Math::Vector2(minX, minY);
    el->anchor.anchorMax = Math::Vector2(maxX, maxY);
}

static void UI_SetElementOffsets(u64 entity, int elementId, float l, float r, float t, float b) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (!el) return;
    el->anchor.offsetLeft = l;
    el->anchor.offsetRight = r;
    el->anchor.offsetTop = t;
    el->anchor.offsetBottom = b;
}

static void UI_SetTextColor(u64 entity, int elementId, float r, float g, float b) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->style.textColor = Math::Vector3(r, g, b);
}

static void UI_SetFontSize(u64 entity, int elementId, float size) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->style.fontSize = size; // < 0 restores the theme default
}

static float UI_GetFontSize(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return -1.0f;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->style.fontSize : -1.0f;
}

// -- Per-character text colors --

static void UI_SetCharColor(u64 entity, int elementId, int charIndex, float r, float g, float b) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (!el || charIndex < 0) return;
    usize idx = static_cast<usize>(charIndex);
    if (el->data.charColors.size() <= idx)
        el->data.charColors.resize(idx + 1, IM_COL32(255, 255, 255, 255));
    el->data.charColors[idx] = IM_COL32(
        static_cast<u8>(r * 255.0f),
        static_cast<u8>(g * 255.0f),
        static_cast<u8>(b * 255.0f), 255);
}

static void UI_SetCharColorRange(u64 entity, int elementId, int startIdx, int endIdx, float r, float g, float b) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (!el || startIdx < 0 || endIdx < startIdx) return;
    usize end = static_cast<usize>(endIdx);
    if (el->data.charColors.size() <= end)
        el->data.charColors.resize(end + 1, IM_COL32(255, 255, 255, 255));
    ImU32 color = IM_COL32(
        static_cast<u8>(r * 255.0f),
        static_cast<u8>(g * 255.0f),
        static_cast<u8>(b * 255.0f), 255);
    for (usize i = static_cast<usize>(startIdx); i <= end; ++i)
        el->data.charColors[i] = color;
}

static void UI_ClearCharColors(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->data.charColors.clear();
}

// -- Interaction queries --

static bool UI_IsHovered(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return false;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->interaction.hovered : false;
}

static bool UI_IsPressed(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return false;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    return el ? el->interaction.pressed : false;
}

// ============================================================================
// Focus Management
// ============================================================================

static void UI_SetFocus(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    if (canvas) canvas->focusedElementId = static_cast<u32>(elementId);
}

static void UI_ClearFocus(u64 entity) {
    auto* canvas = GetCanvas(entity);
    if (canvas) canvas->focusedElementId = 0;
}

static int UI_GetFocusedElement(u64 entity) {
    auto* canvas = GetCanvas(entity);
    return canvas ? static_cast<int>(canvas->focusedElementId) : 0;
}

static bool UI_IsFocused(u64 entity, int elementId) {
    auto* canvas = GetCanvas(entity);
    return canvas ? (canvas->focusedElementId == static_cast<u32>(elementId)) : false;
}

static void UI_SetTabOrder(u64 entity, int elementId, int order) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->tabOrder = order;
}

static void UI_SetFocusable(u64 entity, int elementId, bool focusable) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->focusable = focusable;
}

// ============================================================================
// Localization
// ============================================================================

static std::string Loc_Get(const std::string& key) {
    return GUI::LocalizationManager::Get().GetString(key);
}

static std::string Loc_GetWithFallback(const std::string& key, const std::string& fallback) {
    return GUI::LocalizationManager::Get().GetString(key, fallback);
}

static void Loc_SetLocale(const std::string& locale) {
    GUI::LocalizationManager::Get().SetLocale(locale);
}

static std::string Loc_GetLocale() {
    return GUI::LocalizationManager::Get().GetCurrentLocale();
}

static bool Loc_HasString(const std::string& key) {
    return GUI::LocalizationManager::Get().HasString(key);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterUIBindings(asIScriptEngine* engine) {
    // -- Canvas --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetCanvasVisible(uint64, bool)",
        ENJIN_AS_FN(UI_SetCanvasVisible), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsCanvasVisible(uint64)",
        ENJIN_AS_FN(UI_IsCanvasVisible), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetCanvasSortOrder(uint64, int)",
        ENJIN_AS_FN(UI_SetCanvasSortOrder), ENJIN_AS_CALL_CDECL));

    // -- Text --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetText(uint64, int, const string &in)",
        ENJIN_AS_FN(UI_SetText), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string UI_GetText(uint64, int)",
        ENJIN_AS_FN(UI_GetText), ENJIN_AS_CALL_CDECL));

    // -- Visibility / Enabled --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetElementVisible(uint64, int, bool)",
        ENJIN_AS_FN(UI_SetElementVisible), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsElementVisible(uint64, int)",
        ENJIN_AS_FN(UI_IsElementVisible), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetElementEnabled(uint64, int, bool)",
        ENJIN_AS_FN(UI_SetElementEnabled), ENJIN_AS_CALL_CDECL));

    // -- Progress Bar --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetProgress(uint64, int, float)",
        ENJIN_AS_FN(UI_SetProgress), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float UI_GetProgress(uint64, int)",
        ENJIN_AS_FN(UI_GetProgress), ENJIN_AS_CALL_CDECL));

    // -- Slider --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetSliderValue(uint64, int, float)",
        ENJIN_AS_FN(UI_SetSliderValue), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float UI_GetSliderValue(uint64, int)",
        ENJIN_AS_FN(UI_GetSliderValue), ENJIN_AS_CALL_CDECL));

    // -- Checkbox / Toggle --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetChecked(uint64, int, bool)",
        ENJIN_AS_FN(UI_SetChecked), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsChecked(uint64, int)",
        ENJIN_AS_FN(UI_IsChecked), ENJIN_AS_CALL_CDECL));

    // -- Image --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetImagePath(uint64, int, const string &in)",
        ENJIN_AS_FN(UI_SetImagePath), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetImageAlpha(uint64, int, float)",
        ENJIN_AS_FN(UI_SetImageAlpha), ENJIN_AS_CALL_CDECL));

    // -- Style --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetBgColor(uint64, int, float, float, float, float)",
        ENJIN_AS_FN(UI_SetBgColor), ENJIN_AS_CALL_CDECL));
    // Animate/reposition elements from scripts (slide-in menus etc.) — offsets
    // are pixel insets on top of the element's anchors
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetElementOffsets(uint64, int, float, float, float, float)",
        ENJIN_AS_FN(UI_SetElementOffsets), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetTextColor(uint64, int, float, float, float)",
        ENJIN_AS_FN(UI_SetTextColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetFontSize(uint64, int, float)",
        ENJIN_AS_FN(UI_SetFontSize), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float UI_GetFontSize(uint64, int)",
        ENJIN_AS_FN(UI_GetFontSize), ENJIN_AS_CALL_CDECL));

    // -- Per-character text colors --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetCharColor(uint64, int, int, float, float, float)",
        ENJIN_AS_FN(UI_SetCharColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetCharColorRange(uint64, int, int, int, float, float, float)",
        ENJIN_AS_FN(UI_SetCharColorRange), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_ClearCharColors(uint64, int)",
        ENJIN_AS_FN(UI_ClearCharColors), ENJIN_AS_CALL_CDECL));

    // -- Interaction --
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsHovered(uint64, int)",
        ENJIN_AS_FN(UI_IsHovered), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsPressed(uint64, int)",
        ENJIN_AS_FN(UI_IsPressed), ENJIN_AS_CALL_CDECL));

    // -- Focus --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetFocus(uint64, int)",
        ENJIN_AS_FN(UI_SetFocus), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_ClearFocus(uint64)",
        ENJIN_AS_FN(UI_ClearFocus), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int UI_GetFocusedElement(uint64)",
        ENJIN_AS_FN(UI_GetFocusedElement), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsFocused(uint64, int)",
        ENJIN_AS_FN(UI_IsFocused), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetTabOrder(uint64, int, int)",
        ENJIN_AS_FN(UI_SetTabOrder), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetFocusable(uint64, int, bool)",
        ENJIN_AS_FN(UI_SetFocusable), ENJIN_AS_CALL_CDECL));

    // -- Element lookup and creation --
    AS_CHECK(engine->RegisterEnum("UIWidget"));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_PANEL", 0));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_BUTTON", 1));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_LABEL", 2));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_IMAGE", 3));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_PROGRESSBAR", 4));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_SLIDER", 5));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_CHECKBOX", 6));
    AS_CHECK(engine->RegisterEnumValue("UIWidget", "UI_TOGGLE", 7));
    AS_CHECK(engine->RegisterGlobalFunction("int UI_FindElement(uint64, const string &in)",
        ENJIN_AS_FN(UI_FindElement), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int UI_AddElement(uint64, int, const string &in, int parentId = 0)",
        ENJIN_AS_FN(UI_AddElement), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetElementAnchor(uint64, int, float, float, float, float)",
        ENJIN_AS_FN(UI_SetElementAnchor), ENJIN_AS_CALL_CDECL));

    // -- Localization --
    AS_CHECK(engine->RegisterGlobalFunction("string Loc_Get(const string &in)",
        ENJIN_AS_FN(Loc_Get), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Loc_GetWithFallback(const string &in, const string &in)",
        ENJIN_AS_FN(Loc_GetWithFallback), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Loc_SetLocale(const string &in)",
        ENJIN_AS_FN(Loc_SetLocale), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Loc_GetLocale()",
        ENJIN_AS_FN(Loc_GetLocale), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Loc_HasString(const string &in)",
        ENJIN_AS_FN(Loc_HasString), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
