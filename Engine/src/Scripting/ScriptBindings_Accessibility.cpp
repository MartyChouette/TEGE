#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>
#include <string>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); assert(_r >= 0); (void)_r; } while(0)

static Accessibility::SubtitleSystem* s_BindingsSubtitles = nullptr;
static Accessibility::AccessibilityAnnouncer* s_BindingsAnnouncer = nullptr;
static Accessibility::RuntimeAccessibilitySettings* s_BindingsAccessibility = nullptr;

namespace Enjin {
namespace Scripting {

void SetBindingsSubtitles(Accessibility::SubtitleSystem* subtitles) {
    s_BindingsSubtitles = subtitles;
}

void SetBindingsAnnouncer(Accessibility::AccessibilityAnnouncer* announcer) {
    s_BindingsAnnouncer = announcer;
}

void SetBindingsAccessibilitySettings(Accessibility::RuntimeAccessibilitySettings* settings) {
    s_BindingsAccessibility = settings;
}

} // namespace Scripting
} // namespace Enjin

// ============================================================================
// Subtitle bindings
// ============================================================================

static void Subtitle_Show(const std::string& text, const std::string& speaker, float duration) {
    if (!s_BindingsSubtitles) return;
    s_BindingsSubtitles->ShowSubtitle(text, speaker, Math::Vector3(1, 1, 1), duration);
}

static void Subtitle_ShowWithColor(const std::string& text, const std::string& speaker,
                                   float r, float g, float b, float duration) {
    if (!s_BindingsSubtitles) return;
    s_BindingsSubtitles->ShowSubtitle(text, speaker, Math::Vector3(r, g, b), duration);
}

static void Subtitle_ShowCaption(const std::string& text, float duration) {
    if (!s_BindingsSubtitles) return;
    s_BindingsSubtitles->ShowCaption(text, Accessibility::SubtitleDirection::None, duration);
}

static void Subtitle_Clear() {
    if (s_BindingsSubtitles) s_BindingsSubtitles->Clear();
}

static void Subtitle_SetEnabled(bool enabled) {
    if (!s_BindingsSubtitles) return;
    s_BindingsSubtitles->GetConfig().enabled = enabled;
}

static bool Subtitle_IsEnabled() {
    if (!s_BindingsSubtitles) return false;
    return s_BindingsSubtitles->GetConfig().enabled;
}

static void Subtitle_SetFontSize(float size) {
    if (!s_BindingsSubtitles) return;
    s_BindingsSubtitles->GetConfig().fontSize = size;
}

static float Subtitle_GetFontSize() {
    if (!s_BindingsSubtitles) return 24.0f;
    return s_BindingsSubtitles->GetConfig().fontSize;
}

// ============================================================================
// Announcer bindings (screen reader support)
// ============================================================================

static void Announcer_Announce(const std::string& text) {
    if (!s_BindingsAnnouncer) return;
    s_BindingsAnnouncer->Announce(text, Accessibility::AnnouncePriority::Normal);
}

static void Announcer_AnnounceHighPriority(const std::string& text) {
    if (!s_BindingsAnnouncer) return;
    s_BindingsAnnouncer->Announce(text, Accessibility::AnnouncePriority::High);
}

static void Announcer_Clear() {
    if (s_BindingsAnnouncer) s_BindingsAnnouncer->Clear();
}

static void Announcer_SetEnabled(bool enabled) {
    if (s_BindingsAnnouncer) s_BindingsAnnouncer->enabled = enabled;
}

static bool Announcer_IsEnabled() {
    return s_BindingsAnnouncer ? s_BindingsAnnouncer->enabled : false;
}

// ============================================================================
// Colorblind mode bindings
// ============================================================================

static void Colorblind_SetMode(int mode) {
    if (!s_BindingsAccessibility) return;
    if (mode < 0 || mode > 7) return;
    s_BindingsAccessibility->colorblindMode = static_cast<Accessibility::ColorblindMode>(mode);
}

static int Colorblind_GetMode() {
    if (!s_BindingsAccessibility) return 0;
    return static_cast<int>(s_BindingsAccessibility->colorblindMode);
}

static void Colorblind_SetStrength(float strength) {
    if (!s_BindingsAccessibility) return;
    s_BindingsAccessibility->colorblindStrength = strength;
}

static float Colorblind_GetStrength() {
    if (!s_BindingsAccessibility) return 1.0f;
    return s_BindingsAccessibility->colorblindStrength;
}

static void Accessibility_SetReducedMotion(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->reducedMotion = enabled;
}

static bool Accessibility_GetReducedMotion() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->reducedMotion : false;
}

static void Accessibility_SetScreenShake(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->disableScreenShake = !enabled;
}

static bool Accessibility_GetScreenShake() {
    return s_BindingsAccessibility ? !s_BindingsAccessibility->disableScreenShake : true;
}

// ============================================================================
// Registration
// ============================================================================

namespace Enjin {
namespace Scripting {

void RegisterAccessibilityBindings(asIScriptEngine* engine) {
    // Colorblind mode enum constants
    AS_CHECK(engine->RegisterEnum("ColorblindMode"));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_OFF", 0));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_PROTANOPIA", 1));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_DEUTERANOPIA", 2));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_TRITANOPIA", 3));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_PROTANOMALY", 4));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_DEUTERANOMALY", 5));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_TRITANOMALY", 6));
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_ACHROMATOPSIA", 7));

    // Subtitles
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_Show(const string&in, const string&in = \"\", float = 3.0)",
        asFUNCTION(Subtitle_Show), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_ShowWithColor(const string&in, const string&in, float, float, float, float = 3.0)",
        asFUNCTION(Subtitle_ShowWithColor), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_ShowCaption(const string&in, float = 2.5)",
        asFUNCTION(Subtitle_ShowCaption), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_Clear()",
        asFUNCTION(Subtitle_Clear), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_SetEnabled(bool)",
        asFUNCTION(Subtitle_SetEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Subtitle_IsEnabled()",
        asFUNCTION(Subtitle_IsEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_SetFontSize(float)",
        asFUNCTION(Subtitle_SetFontSize), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Subtitle_GetFontSize()",
        asFUNCTION(Subtitle_GetFontSize), asCALL_CDECL));

    // Announcer (screen reader)
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_Announce(const string&in)",
        asFUNCTION(Announcer_Announce), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_AnnounceHighPriority(const string&in)",
        asFUNCTION(Announcer_AnnounceHighPriority), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_Clear()",
        asFUNCTION(Announcer_Clear), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_SetEnabled(bool)",
        asFUNCTION(Announcer_SetEnabled), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Announcer_IsEnabled()",
        asFUNCTION(Announcer_IsEnabled), asCALL_CDECL));

    // Colorblind filter
    AS_CHECK(engine->RegisterGlobalFunction("void Colorblind_SetMode(int)",
        asFUNCTION(Colorblind_SetMode), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Colorblind_GetMode()",
        asFUNCTION(Colorblind_GetMode), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Colorblind_SetStrength(float)",
        asFUNCTION(Colorblind_SetStrength), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Colorblind_GetStrength()",
        asFUNCTION(Colorblind_GetStrength), asCALL_CDECL));

    // General accessibility
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetReducedMotion(bool)",
        asFUNCTION(Accessibility_SetReducedMotion), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetReducedMotion()",
        asFUNCTION(Accessibility_GetReducedMotion), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetScreenShake(bool)",
        asFUNCTION(Accessibility_SetScreenShake), asCALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetScreenShake()",
        asFUNCTION(Accessibility_GetScreenShake), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
