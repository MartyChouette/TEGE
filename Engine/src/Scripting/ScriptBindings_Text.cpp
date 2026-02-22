#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Text.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

// ============================================================================
// TextComponent access
// ============================================================================

static void Text_SetContent(u64 entity, const std::string& text) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc) { tc->text = text; tc->dirty = true; }
}

static std::string Text_GetContent(u64 entity) {
    if (!s_BindingsWorld) return "";
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    return tc ? tc->text : std::string("");
}

static void Text_SetFontSize(u64 entity, float size) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc) { tc->fontSize = size; tc->dirty = true; }
}

static void Text_SetColor(u64 entity, float r, float g, float b) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc) { tc->textColor = Math::Vector3(r, g, b); tc->dirty = true; }
}

static void Text_SetBgColor(u64 entity, float r, float g, float b) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc) { tc->bgColor = Math::Vector3(r, g, b); tc->dirty = true; }
}

static void Text_SetBgOpacity(u64 entity, float opacity) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc) { tc->bgOpacity = opacity; tc->dirty = true; }
}

static void Text_SetAlignment(u64 entity, int align) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc && align >= 0 && align <= 2) {
        tc->horizontalAlign = static_cast<ECS::TextAlign>(align);
        tc->dirty = true;
    }
}

static void Text_SetWrapWidth(u64 entity, float width) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc) { tc->wrapWidth = width; tc->dirty = true; }
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterTextBindings(asIScriptEngine* engine) {
    // Content
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetContent(uint64, const string &in)",
        asFUNCTION(Text_SetContent), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Text_GetContent(uint64)",
        asFUNCTION(Text_GetContent), asCALL_CDECL));

    // Font
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetFontSize(uint64, float)",
        asFUNCTION(Text_SetFontSize), asCALL_CDECL));

    // Colors
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetColor(uint64, float, float, float)",
        asFUNCTION(Text_SetColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetBgColor(uint64, float, float, float)",
        asFUNCTION(Text_SetBgColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetBgOpacity(uint64, float)",
        asFUNCTION(Text_SetBgOpacity), asCALL_CDECL));

    // Layout
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetAlignment(uint64, int)",
        asFUNCTION(Text_SetAlignment), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetWrapWidth(uint64, float)",
        asFUNCTION(Text_SetWrapWidth), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
