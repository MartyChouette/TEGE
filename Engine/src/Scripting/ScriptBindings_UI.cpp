#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UIElement.h"
#include "Enjin/GUI/Localization.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

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

static void UI_SetTextColor(u64 entity, int elementId, float r, float g, float b) {
    auto* canvas = GetCanvas(entity);
    if (!canvas) return;
    auto* el = canvas->GetElement(static_cast<u32>(elementId));
    if (el) el->style.textColor = Math::Vector3(r, g, b);
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
        asFUNCTION(UI_SetCanvasVisible), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsCanvasVisible(uint64)",
        asFUNCTION(UI_IsCanvasVisible), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetCanvasSortOrder(uint64, int)",
        asFUNCTION(UI_SetCanvasSortOrder), asCALL_CDECL));

    // -- Text --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetText(uint64, int, const string &in)",
        asFUNCTION(UI_SetText), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string UI_GetText(uint64, int)",
        asFUNCTION(UI_GetText), asCALL_CDECL));

    // -- Visibility / Enabled --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetElementVisible(uint64, int, bool)",
        asFUNCTION(UI_SetElementVisible), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsElementVisible(uint64, int)",
        asFUNCTION(UI_IsElementVisible), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetElementEnabled(uint64, int, bool)",
        asFUNCTION(UI_SetElementEnabled), asCALL_CDECL));

    // -- Progress Bar --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetProgress(uint64, int, float)",
        asFUNCTION(UI_SetProgress), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float UI_GetProgress(uint64, int)",
        asFUNCTION(UI_GetProgress), asCALL_CDECL));

    // -- Slider --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetSliderValue(uint64, int, float)",
        asFUNCTION(UI_SetSliderValue), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float UI_GetSliderValue(uint64, int)",
        asFUNCTION(UI_GetSliderValue), asCALL_CDECL));

    // -- Checkbox / Toggle --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetChecked(uint64, int, bool)",
        asFUNCTION(UI_SetChecked), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsChecked(uint64, int)",
        asFUNCTION(UI_IsChecked), asCALL_CDECL));

    // -- Image --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetImagePath(uint64, int, const string &in)",
        asFUNCTION(UI_SetImagePath), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetImageAlpha(uint64, int, float)",
        asFUNCTION(UI_SetImageAlpha), asCALL_CDECL));

    // -- Style --
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetBgColor(uint64, int, float, float, float, float)",
        asFUNCTION(UI_SetBgColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void UI_SetTextColor(uint64, int, float, float, float)",
        asFUNCTION(UI_SetTextColor), asCALL_CDECL));

    // -- Interaction --
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsHovered(uint64, int)",
        asFUNCTION(UI_IsHovered), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool UI_IsPressed(uint64, int)",
        asFUNCTION(UI_IsPressed), asCALL_CDECL));

    // -- Localization --
    AS_CHECK(engine->RegisterGlobalFunction("string Loc_Get(const string &in)",
        asFUNCTION(Loc_Get), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Loc_GetWithFallback(const string &in, const string &in)",
        asFUNCTION(Loc_GetWithFallback), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Loc_SetLocale(const string &in)",
        asFUNCTION(Loc_SetLocale), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Loc_GetLocale()",
        asFUNCTION(Loc_GetLocale), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Loc_HasString(const string &in)",
        asFUNCTION(Loc_HasString), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
