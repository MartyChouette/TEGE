#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ScriptComponentAccess.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/GUI/UICanvas.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// HUD_* API — now backed by UICanvasComponent.
// HUDSystem is RETIRED (UI unification: UICanvas is the one authorable UI
// system on every platform). Legacy hudWidget scene data is migrated to
// per-entity canvases on load, so scripts written against HUD widget entities
// keep working unchanged: text ops hit the canvas's first Label, value ops
// its first ProgressBar, layout ops its root elements.
// ============================================================================

static GUI::UICanvasComponent* HudCanvas(u64 entity) {
    if (!s_BindingsWorld) return nullptr;
    return s_BindingsWorld->GetComponent<GUI::UICanvasComponent>(static_cast<ECS::Entity>(entity));
}

static GUI::UIElement* FirstOfType(GUI::UICanvasComponent* c, GUI::UIWidgetType t) {
    if (!c) return nullptr;
    for (auto& e : c->elements) {
        if (e.type == t) return &e;
    }
    return nullptr;
}

static void HUD_SetVisible(u64 entity, bool visible) {
    if (auto* c = HudCanvas(entity)) c->visible = visible;
}

static bool HUD_IsVisible(u64 entity) {
    auto* c = HudCanvas(entity);
    return c ? c->visible : false;
}

static void HUD_SetText(u64 entity, const std::string& text) {
    // Warn rather than return in silence. An entity carrying a uiCanvas
    // instead of a hudWidget took this call, did nothing, and reported
    // nothing -- which is exactly what a broken interact prompt looks like.
    if (auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::Label)) {
        el->data.text = text;
    } else {
        ::Enjin::Scripting::WarnMissingScriptComponent(entity, "hudWidget Label", __func__);
    }
}

static std::string HUD_GetText(u64 entity) {
    auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::Label);
    return el ? el->data.text : std::string("");
}

static void HUD_SetValue(u64 entity, float current, float max) {
    if (auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::ProgressBar)) {
        el->data.bindMaxValue = max;
        el->data.progressValue = (max > 0.0f) ? (current / max) : 0.0f;
    }
}

static float HUD_GetValue(u64 entity) {
    auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::ProgressBar);
    return el ? el->data.progressValue * el->data.bindMaxValue : 0.0f;
}

static float HUD_GetMaxValue(u64 entity) {
    auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::ProgressBar);
    return el ? el->data.bindMaxValue : 0.0f;
}

static void HUD_SetFillColor(u64 entity, float r, float g, float b) {
    if (auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::ProgressBar)) {
        el->data.progressFillColor = Math::Vector3(r, g, b);
    }
}

static void HUD_SetTextColor(u64 entity, float r, float g, float b) {
    if (auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::Label)) {
        el->style.textColor = Math::Vector3(r, g, b);
    }
}

static void HUD_SetPosition(u64 entity, float anchorX, float anchorY) {
    if (auto* c = HudCanvas(entity)) {
        for (auto& el : c->elements) {
            if (el.parentId != 0) continue;  // roots carry the widget position
            el.anchor.anchorMin = Math::Vector2(anchorX, anchorY);
            el.anchor.anchorMax = Math::Vector2(anchorX, anchorY);
        }
    }
}

static void HUD_SetSize(u64 entity, float width, float height) {
    // width/height are viewport fractions (legacy HUD semantics) — mapped to
    // design-resolution offsets from the anchored point
    if (auto* c = HudCanvas(entity)) {
        for (auto& el : c->elements) {
            if (el.parentId != 0) continue;
            el.anchor.offsetLeft = 0.0f;
            el.anchor.offsetTop = 0.0f;
            el.anchor.offsetRight = width * c->designWidth;
            el.anchor.offsetBottom = height * c->designHeight;
        }
    }
}

static void HUD_SetFontSize(u64 entity, float size) {
    if (auto* el = FirstOfType(HudCanvas(entity), GUI::UIWidgetType::Label)) {
        el->style.fontSize = size;
    }
}

static void HUD_SetBindField(u64 entity, const std::string& field) {
    if (auto* c = HudCanvas(entity)) {
        // "custom" meant script-driven in the old HUD — canvas equivalent is
        // no binding (scripts drive via HUD_SetValue/HUD_SetText)
        std::string mapped = (field == "custom") ? std::string("") : field;
        for (auto& el : c->elements) el.data.bindField = mapped;
    }
}

// World-space elements: track another entity's transform at draw time. This is
// the lag-free way to pin a tag on a moving object - the UI system reads the
// source's position after physics has run, not the position a script cached.
static void HUD_SetSourceEntity(u64 entity, u64 source) {
    if (auto* c = HudCanvas(entity)) {
        for (auto& el : c->elements) el.data.worldSourceEntity = source;
    }
}

static void HUD_SetWorldOffset(u64 entity, const Math::Vector3& offset) {
    if (auto* c = HudCanvas(entity)) {
        for (auto& el : c->elements) el.data.worldOffset = offset;
    }
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterHUDBindings(asIScriptEngine* engine) {
    // Visibility
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetVisible(uint64, bool)",
        ENJIN_AS_FN(HUD_SetVisible), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HUD_IsVisible(uint64)",
        ENJIN_AS_FN(HUD_IsVisible), ENJIN_AS_CALL_CDECL));

    // Text
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetText(uint64, const string &in)",
        ENJIN_AS_FN(HUD_SetText), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string HUD_GetText(uint64)",
        ENJIN_AS_FN(HUD_GetText), ENJIN_AS_CALL_CDECL));

    // Values (for bars)
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetValue(uint64, float, float)",
        ENJIN_AS_FN(HUD_SetValue), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float HUD_GetValue(uint64)",
        ENJIN_AS_FN(HUD_GetValue), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float HUD_GetMaxValue(uint64)",
        ENJIN_AS_FN(HUD_GetMaxValue), ENJIN_AS_CALL_CDECL));

    // Colors
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetFillColor(uint64, float, float, float)",
        ENJIN_AS_FN(HUD_SetFillColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetTextColor(uint64, float, float, float)",
        ENJIN_AS_FN(HUD_SetTextColor), ENJIN_AS_CALL_CDECL));

    // Layout
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetPosition(uint64, float, float)",
        ENJIN_AS_FN(HUD_SetPosition), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetSize(uint64, float, float)",
        ENJIN_AS_FN(HUD_SetSize), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetFontSize(uint64, float)",
        ENJIN_AS_FN(HUD_SetFontSize), ENJIN_AS_CALL_CDECL));

    // Data binding
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetBindField(uint64, const string &in)",
        ENJIN_AS_FN(HUD_SetBindField), ENJIN_AS_CALL_CDECL));

    // World-space tracking
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetSourceEntity(uint64, uint64)",
        ENJIN_AS_FN(HUD_SetSourceEntity), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetWorldOffset(uint64, const Vector3 &in)",
        ENJIN_AS_FN(HUD_SetWorldOffset), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
