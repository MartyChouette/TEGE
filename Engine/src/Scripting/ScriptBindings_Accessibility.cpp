#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Accessibility/SubtitleSystem.h"
#include "Enjin/Accessibility/Announcer.h"
#include "Enjin/Accessibility/AccessibilitySettings.h"
#include "Enjin/Logging/Log.h"
#include <angelscript.h>
#include <cassert>
#include <string>
#include <functional>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

static Accessibility::SubtitleSystem* s_BindingsSubtitles = nullptr;
static Accessibility::AccessibilityAnnouncer* s_BindingsAnnouncer = nullptr;
static Accessibility::RuntimeAccessibilitySettings* s_BindingsAccessibility = nullptr;
static std::function<void()> s_SaveAccessibilityCallback;
static std::function<void(bool)> s_DyslexiaFontCallback;
// Host re-applies settings to consumers that only read them when pushed
// (UISystem motor toggles, subtitle config, announcer). Fired by the motor
// setters below so script changes take effect immediately.
static std::function<void()> s_ApplyAccessibilityCallback;

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

void SetBindingsAccessibilitySaveCallback(std::function<void()> callback) {
    s_SaveAccessibilityCallback = std::move(callback);
}

void SetBindingsDyslexiaFontCallback(std::function<void(bool)> callback) {
    s_DyslexiaFontCallback = std::move(callback);
}

void SetBindingsAccessibilityApplyCallback(std::function<void()> callback) {
    s_ApplyAccessibilityCallback = std::move(callback);
}

// ---------------------------------------------------------------------------
// Public forwarders for VisualScript nodes. These reuse the same runtime
// settings pointer + apply/save callbacks the AngelScript API uses, so node
// changes take effect (and persist) exactly like scripted ones. Set helpers
// fire the apply callback; readers fall back to sane defaults when unwired.
// ---------------------------------------------------------------------------
static void VSAccessApply() { if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback(); }

void VSAccessSetFontScale(f32 v)        { if (s_BindingsAccessibility) { s_BindingsAccessibility->fontScale = v; VSAccessApply(); } }
f32  VSAccessGetFontScale()             { return s_BindingsAccessibility ? s_BindingsAccessibility->fontScale : 1.0f; }
void VSAccessSetReducedMotion(bool v)   { if (s_BindingsAccessibility) { s_BindingsAccessibility->reducedMotion = v; VSAccessApply(); } }
bool VSAccessGetReducedMotion()         { return s_BindingsAccessibility ? s_BindingsAccessibility->reducedMotion : false; }
void VSAccessSetDisableScreenShake(bool v) { if (s_BindingsAccessibility) { s_BindingsAccessibility->disableScreenShake = v; VSAccessApply(); } }
bool VSAccessGetDisableScreenShake()    { return s_BindingsAccessibility ? s_BindingsAccessibility->disableScreenShake : false; }
void VSAccessSetContrast(f32 v)         { if (s_BindingsAccessibility) { s_BindingsAccessibility->screenContrast = v; VSAccessApply(); } }
f32  VSAccessGetContrast()              { return s_BindingsAccessibility ? s_BindingsAccessibility->screenContrast : 1.0f; }
void VSAccessSetColorblindStrength(f32 v) { if (s_BindingsAccessibility) { s_BindingsAccessibility->colorblindStrength = v; VSAccessApply(); } }
f32  VSAccessGetColorblindStrength()    { return s_BindingsAccessibility ? s_BindingsAccessibility->colorblindStrength : 1.0f; }
void VSAccessSetSubtitles(bool v)       { if (s_BindingsAccessibility) { s_BindingsAccessibility->subtitlesEnabled = v; VSAccessApply(); } }
bool VSAccessGetSubtitles()             { return s_BindingsAccessibility ? s_BindingsAccessibility->subtitlesEnabled : false; }
void VSAccessSetDyslexiaFont(bool v)    { if (s_BindingsAccessibility) { s_BindingsAccessibility->dyslexiaFriendly = v; if (s_DyslexiaFontCallback) s_DyslexiaFontCallback(v); VSAccessApply(); } }
bool VSAccessGetDyslexiaFont()          { return s_BindingsAccessibility ? s_BindingsAccessibility->dyslexiaFriendly : false; }
void VSAccessSetScreenReader(bool v)    { if (s_BindingsAccessibility) { s_BindingsAccessibility->screenReaderEnabled = v; VSAccessApply(); } }
bool VSAccessGetScreenReader()          { return s_BindingsAccessibility ? s_BindingsAccessibility->screenReaderEnabled : false; }
void VSAccessSave()                     { if (s_SaveAccessibilityCallback) s_SaveAccessibilityCallback(); }

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
    if (mode < 0 || mode > 8) return;
    s_BindingsAccessibility->colorblindMode = static_cast<Accessibility::ColorblindMode>(mode);
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static int Colorblind_GetMode() {
    if (!s_BindingsAccessibility) return 0;
    return static_cast<int>(s_BindingsAccessibility->colorblindMode);
}

static void Colorblind_SetStrength(float strength) {
    if (!s_BindingsAccessibility) return;
    s_BindingsAccessibility->colorblindStrength = strength;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static float Colorblind_GetStrength() {
    if (!s_BindingsAccessibility) return 1.0f;
    return s_BindingsAccessibility->colorblindStrength;
}

static void Accessibility_SetReducedMotion(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->reducedMotion = enabled;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetReducedMotion() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->reducedMotion : false;
}

static void Accessibility_SetScreenShake(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->disableScreenShake = !enabled;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetScreenShake() {
    return s_BindingsAccessibility ? !s_BindingsAccessibility->disableScreenShake : true;
}

static void Accessibility_SaveSettings() {
    if (s_SaveAccessibilityCallback) s_SaveAccessibilityCallback();
}

static void Accessibility_SetFlashingLights(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->disableFlashingLights = !enabled;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetFlashingLights() {
    return s_BindingsAccessibility ? !s_BindingsAccessibility->disableFlashingLights : true;
}

static void Accessibility_SetFontScale(float scale) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->fontScale = scale;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static float Accessibility_GetFontScale() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->fontScale : 1.0f;
}

static void Accessibility_SetBrightness(float v) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->screenBrightness = v;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static float Accessibility_GetBrightness() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->screenBrightness : 0.0f;
}

static void Accessibility_SetDyslexiaFont(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->dyslexiaFriendly = enabled;
    if (s_DyslexiaFontCallback) s_DyslexiaFontCallback(enabled);
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetDyslexiaFont() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->dyslexiaFriendly : false;
}

static void Accessibility_SetContrast(float v) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->screenContrast = v;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static float Accessibility_GetContrast() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->screenContrast : 1.0f;
}

// ============================================================================
// Motor accessibility bindings — write the settings struct, then have the host
// push them into UISystem (these are not read per-frame like the visual ones)
// ============================================================================

static void Accessibility_SetDwellClick(bool enabled, float dwellTime) {
    if (s_BindingsAccessibility) {
        s_BindingsAccessibility->dwellClickEnabled = enabled;
        if (dwellTime > 0.0f) s_BindingsAccessibility->dwellClickTime = dwellTime;
    }
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetDwellClick() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->dwellClickEnabled : false;
}

static void Accessibility_SetSwitchAccess(bool enabled, float scanSpeed) {
    if (s_BindingsAccessibility) {
        s_BindingsAccessibility->switchAccessEnabled = enabled;
        if (scanSpeed > 0.0f) s_BindingsAccessibility->switchScanSpeed = scanSpeed;
    }
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetSwitchAccess() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->switchAccessEnabled : false;
}

static void Accessibility_SetStickyDrag(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->stickyDragEnabled = enabled;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetStickyDrag() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->stickyDragEnabled : false;
}

static void Accessibility_SetScreenReader(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->screenReaderEnabled = enabled;
    if (s_BindingsAnnouncer) s_BindingsAnnouncer->enabled = enabled;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetScreenReader() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->screenReaderEnabled : false;
}

static void Accessibility_SetAudioIndicators(bool enabled) {
    if (s_BindingsAccessibility) s_BindingsAccessibility->audioIndicatorsEnabled = enabled;
    if (s_ApplyAccessibilityCallback) s_ApplyAccessibilityCallback();
}

static bool Accessibility_GetAudioIndicators() {
    return s_BindingsAccessibility ? s_BindingsAccessibility->audioIndicatorsEnabled : false;
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
    AS_CHECK(engine->RegisterEnumValue("ColorblindMode", "CB_ACHROMATOMALY", 8));

    // Subtitles
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_Show(const string&in, const string&in = \"\", float = 3.0)",
        ENJIN_AS_FN(Subtitle_Show), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_ShowWithColor(const string&in, const string&in, float, float, float, float = 3.0)",
        ENJIN_AS_FN(Subtitle_ShowWithColor), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_ShowCaption(const string&in, float = 2.5)",
        ENJIN_AS_FN(Subtitle_ShowCaption), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_Clear()",
        ENJIN_AS_FN(Subtitle_Clear), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_SetEnabled(bool)",
        ENJIN_AS_FN(Subtitle_SetEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Subtitle_IsEnabled()",
        ENJIN_AS_FN(Subtitle_IsEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Subtitle_SetFontSize(float)",
        ENJIN_AS_FN(Subtitle_SetFontSize), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Subtitle_GetFontSize()",
        ENJIN_AS_FN(Subtitle_GetFontSize), ENJIN_AS_CALL_CDECL));

    // Announcer (screen reader)
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_Announce(const string&in)",
        ENJIN_AS_FN(Announcer_Announce), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_AnnounceHighPriority(const string&in)",
        ENJIN_AS_FN(Announcer_AnnounceHighPriority), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_Clear()",
        ENJIN_AS_FN(Announcer_Clear), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Announcer_SetEnabled(bool)",
        ENJIN_AS_FN(Announcer_SetEnabled), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Announcer_IsEnabled()",
        ENJIN_AS_FN(Announcer_IsEnabled), ENJIN_AS_CALL_CDECL));

    // Colorblind filter
    AS_CHECK(engine->RegisterGlobalFunction("void Colorblind_SetMode(int)",
        ENJIN_AS_FN(Colorblind_SetMode), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("int Colorblind_GetMode()",
        ENJIN_AS_FN(Colorblind_GetMode), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Colorblind_SetStrength(float)",
        ENJIN_AS_FN(Colorblind_SetStrength), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Colorblind_GetStrength()",
        ENJIN_AS_FN(Colorblind_GetStrength), ENJIN_AS_CALL_CDECL));

    // General accessibility
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetReducedMotion(bool)",
        ENJIN_AS_FN(Accessibility_SetReducedMotion), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetReducedMotion()",
        ENJIN_AS_FN(Accessibility_GetReducedMotion), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetScreenShake(bool)",
        ENJIN_AS_FN(Accessibility_SetScreenShake), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetScreenShake()",
        ENJIN_AS_FN(Accessibility_GetScreenShake), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetFlashingLights(bool)",
        ENJIN_AS_FN(Accessibility_SetFlashingLights), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetFlashingLights()",
        ENJIN_AS_FN(Accessibility_GetFlashingLights), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetFontScale(float)",
        ENJIN_AS_FN(Accessibility_SetFontScale), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Accessibility_GetFontScale()",
        ENJIN_AS_FN(Accessibility_GetFontScale), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetBrightness(float)",
        ENJIN_AS_FN(Accessibility_SetBrightness), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Accessibility_GetBrightness()",
        ENJIN_AS_FN(Accessibility_GetBrightness), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetContrast(float)",
        ENJIN_AS_FN(Accessibility_SetContrast), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("float Accessibility_GetContrast()",
        ENJIN_AS_FN(Accessibility_GetContrast), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetDyslexiaFont(bool)",
        ENJIN_AS_FN(Accessibility_SetDyslexiaFont), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetDyslexiaFont()",
        ENJIN_AS_FN(Accessibility_GetDyslexiaFont), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SaveSettings()",
        ENJIN_AS_FN(Accessibility_SaveSettings), ENJIN_AS_CALL_CDECL));

    // Motor accessibility + screen reader + indicators
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetDwellClick(bool, float = 0.0)",
        ENJIN_AS_FN(Accessibility_SetDwellClick), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetDwellClick()",
        ENJIN_AS_FN(Accessibility_GetDwellClick), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetSwitchAccess(bool, float = 0.0)",
        ENJIN_AS_FN(Accessibility_SetSwitchAccess), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetSwitchAccess()",
        ENJIN_AS_FN(Accessibility_GetSwitchAccess), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetStickyDrag(bool)",
        ENJIN_AS_FN(Accessibility_SetStickyDrag), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetStickyDrag()",
        ENJIN_AS_FN(Accessibility_GetStickyDrag), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetScreenReader(bool)",
        ENJIN_AS_FN(Accessibility_SetScreenReader), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetScreenReader()",
        ENJIN_AS_FN(Accessibility_GetScreenReader), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("void Accessibility_SetAudioIndicators(bool)",
        ENJIN_AS_FN(Accessibility_SetAudioIndicators), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction("bool Accessibility_GetAudioIndicators()",
        ENJIN_AS_FN(Accessibility_GetAudioIndicators), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
