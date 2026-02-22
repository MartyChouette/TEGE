#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// HUDWidgetComponent access
// ============================================================================

static void HUD_SetVisible(u64 entity, bool visible) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) hud->visible = visible;
}

static bool HUD_IsVisible(u64 entity) {
    if (!s_BindingsWorld) return false;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    return hud ? hud->visible : false;
}

static void HUD_SetText(u64 entity, const std::string& text) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) hud->text = text;
}

static std::string HUD_GetText(u64 entity) {
    if (!s_BindingsWorld) return "";
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    return hud ? hud->text : std::string("");
}

static void HUD_SetValue(u64 entity, float current, float max) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) {
        hud->currentValue = current;
        hud->maxValue = max;
    }
}

static float HUD_GetValue(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    return hud ? hud->currentValue : 0.0f;
}

static float HUD_GetMaxValue(u64 entity) {
    if (!s_BindingsWorld) return 0.0f;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    return hud ? hud->maxValue : 0.0f;
}

static void HUD_SetFillColor(u64 entity, float r, float g, float b) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) hud->fillColor = Math::Vector3(r, g, b);
}

static void HUD_SetTextColor(u64 entity, float r, float g, float b) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) hud->textColor = Math::Vector3(r, g, b);
}

static void HUD_SetPosition(u64 entity, float anchorX, float anchorY) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) {
        hud->anchorX = anchorX;
        hud->anchorY = anchorY;
    }
}

static void HUD_SetSize(u64 entity, float width, float height) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) {
        hud->width = width;
        hud->height = height;
    }
}

static void HUD_SetFontSize(u64 entity, float size) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) hud->fontSize = size;
}

static void HUD_SetBindField(u64 entity, const std::string& field) {
    if (!s_BindingsWorld) return;
    auto* hud = s_BindingsWorld->GetComponent<ECS::HUDWidgetComponent>(entity);
    if (hud) hud->bindField = field;
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterHUDBindings(asIScriptEngine* engine) {
    // Visibility
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetVisible(uint64, bool)",
        asFUNCTION(HUD_SetVisible), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool HUD_IsVisible(uint64)",
        asFUNCTION(HUD_IsVisible), asCALL_CDECL));

    // Text
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetText(uint64, const string &in)",
        asFUNCTION(HUD_SetText), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string HUD_GetText(uint64)",
        asFUNCTION(HUD_GetText), asCALL_CDECL));

    // Values (for bars)
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetValue(uint64, float, float)",
        asFUNCTION(HUD_SetValue), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float HUD_GetValue(uint64)",
        asFUNCTION(HUD_GetValue), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float HUD_GetMaxValue(uint64)",
        asFUNCTION(HUD_GetMaxValue), asCALL_CDECL));

    // Colors
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetFillColor(uint64, float, float, float)",
        asFUNCTION(HUD_SetFillColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetTextColor(uint64, float, float, float)",
        asFUNCTION(HUD_SetTextColor), asCALL_CDECL));

    // Layout
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetPosition(uint64, float, float)",
        asFUNCTION(HUD_SetPosition), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetSize(uint64, float, float)",
        asFUNCTION(HUD_SetSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetFontSize(uint64, float)",
        asFUNCTION(HUD_SetFontSize), asCALL_CDECL));

    // Data binding
    AS_CHECK(engine->RegisterGlobalFunction("void HUD_SetBindField(uint64, const string &in)",
        asFUNCTION(HUD_SetBindField), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
