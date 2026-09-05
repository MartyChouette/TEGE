#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include "Enjin/ECS/Components/Text.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/TextEncoding.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;
extern ECS::RenderSystem* s_BindingsRenderSystem;

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
// Coloured runs, reveal, and measurement
//
// Colouring PART of a string used to mean one entity per colour, and every
// caller then owned its own font arithmetic to place the pieces. These four
// let one text entity do the whole job.
// ============================================================================

// Colour codepoints [start, start+length) - length <= 0 runs to the end.
// Runs apply in order and may overlap; the last one covering a codepoint wins.
static void Text_SetRun(u64 entity, int start, int length, float r, float g, float b) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (!tc) return;
    ECS::TextRun run;
    run.start = start;
    run.length = length;
    run.color = Math::Vector3(r, g, b);
    tc->runs.push_back(run);
    tc->dirty = true;
}

static void Text_ClearRuns(u64 entity) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (tc && !tc->runs.empty()) { tc->runs.clear(); tc->dirty = true; }
}

// Draw only the first `count` codepoints; a negative count draws everything.
// Layout still runs over the whole string, so revealing never reflows: a word
// that will wrap is already on its final line before it appears.
static void Text_RevealTo(u64 entity, int count) {
    if (!s_BindingsWorld) return;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (!tc) return;
    const int v = count < 0 ? -1 : count;
    if (tc->revealCount != v) { tc->revealCount = v; tc->dirty = true; }
}

// Length in CODEPOINTS, which is what runs and reveal count in.
static int Text_Length(u64 entity) {
    if (!s_BindingsWorld) return 0;
    auto* tc = s_BindingsWorld->GetComponent<ECS::TextComponent>(entity);
    if (!tc) return 0;
    return static_cast<int>(Renderer::DecodeUTF8All(tc->text).size());
}

// Local-space top-left of a character, for carets and highlights. Add the
// entity's position to place it in the world.
static Math::Vector3 Text_MeasureTo(u64 entity, int codepointIndex) {
    if (!s_BindingsRenderSystem) return Math::Vector3(0.0f, 0.0f, 0.0f);
    return s_BindingsRenderSystem->MeasureTextTo(static_cast<ECS::Entity>(entity), codepointIndex);
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterTextBindings(asIScriptEngine* engine) {
    // Content
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetContent(uint64, const string &in)",
        ENJIN_AS_FN(Text_SetContent), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("string Text_GetContent(uint64)",
        ENJIN_AS_FN(Text_GetContent), ENJIN_AS_CALL_CDECL));

    // Font
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetFontSize(uint64, float)",
        ENJIN_AS_FN(Text_SetFontSize), ENJIN_AS_CALL_CDECL));

    // Colors
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetColor(uint64, float, float, float)",
        ENJIN_AS_FN(Text_SetColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetBgColor(uint64, float, float, float)",
        ENJIN_AS_FN(Text_SetBgColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetBgOpacity(uint64, float)",
        ENJIN_AS_FN(Text_SetBgOpacity), ENJIN_AS_CALL_CDECL));

    // Layout
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetAlignment(uint64, int)",
        ENJIN_AS_FN(Text_SetAlignment), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_SetWrapWidth(uint64, float)",
        ENJIN_AS_FN(Text_SetWrapWidth), ENJIN_AS_CALL_CDECL));

    // Runs, reveal, measurement
    AS_CHECK(engine->RegisterGlobalFunction(
        "void Text_SetRun(uint64, int, int, float, float, float)",
        ENJIN_AS_FN(Text_SetRun), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_ClearRuns(uint64)",
        ENJIN_AS_FN(Text_ClearRuns), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Text_RevealTo(uint64, int)",
        ENJIN_AS_FN(Text_RevealTo), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Text_Length(uint64)",
        ENJIN_AS_FN(Text_Length), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("Vector3 Text_MeasureTo(uint64, int)",
        ENJIN_AS_FN(Text_MeasureTo), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
