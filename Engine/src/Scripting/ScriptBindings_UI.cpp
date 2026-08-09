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
