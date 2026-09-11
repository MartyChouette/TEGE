#include "Enjin/Editor/EditorShortcuts.h"
#include "Enjin/Platform/Input.h"

#include <cstring>
#include <cstdlib>

namespace Enjin::Editor {

namespace {

// The chord vocabulary. A chord is modifiers plus exactly one key, and every
// name here must map to a KeyCode or the chord cannot be honoured -- which is
// why ShortcutChordParses exists and why a test walks the whole table.
struct NamedKey { const char* name; KeyCode key; };

constexpr NamedKey kKeys[] = {
    { "A", KeyCode::A }, { "B", KeyCode::B }, { "C", KeyCode::C },
    { "D", KeyCode::D }, { "F", KeyCode::F }, { "I", KeyCode::I },
    { "N", KeyCode::N }, { "O", KeyCode::O }, { "P", KeyCode::P },
    { "S", KeyCode::S }, { "V", KeyCode::V }, { "X", KeyCode::X },
    { "Y", KeyCode::Y }, { "Z", KeyCode::Z },
    { "1", KeyCode::Num1 }, { "2", KeyCode::Num2 }, { "3", KeyCode::Num3 },
    { "4", KeyCode::Num4 }, { "5", KeyCode::Num5 },
    { "/", KeyCode::Slash },
    { "F1", KeyCode::F1 }, { "F2", KeyCode::F2 }, { "F5", KeyCode::F5 },
    { "F11", KeyCode::F11 },
    { "Delete", KeyCode::Delete },
    { "Escape", KeyCode::Escape },
    { "Home", KeyCode::Home },
    { "Backtick", KeyCode::GraveAccent },
};

bool LookupKey(const char* name, usize len, KeyCode& out) {
    for (const auto& k : kKeys) {
        if (std::strlen(k.name) == len && std::strncmp(k.name, name, len) == 0) {
            out = k.key;
            return true;
        }
    }
    return false;
}

struct ParsedChord {
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    KeyCode key = KeyCode::Unknown;
    bool valid = false;
};

ParsedChord Parse(const char* chord) {
    ParsedChord out;
    if (!chord || !*chord) return out;

    const char* p = chord;
    while (*p) {
        // A trailing parenthetical is a human note, not part of the chord:
        // "` (Backtick)" displays the character and names it.
        if (*p == ' ' || *p == '(') break;

        const char* start = p;
        while (*p && *p != '+' && *p != ' ' && *p != '(') ++p;
        const usize len = static_cast<usize>(p - start);

        if (len == 4 && std::strncmp(start, "Ctrl", 4) == 0)       out.ctrl = true;
        else if (len == 5 && std::strncmp(start, "Shift", 5) == 0) out.shift = true;
        else if (len == 3 && std::strncmp(start, "Alt", 3) == 0)   out.alt = true;
        else if (len == 1 && *start == '`')                        out.key = KeyCode::GraveAccent;
        else if (!LookupKey(start, len, out.key))                  return out;   // invalid

        if (*p == '+') ++p;
        else break;
    }

    out.valid = (out.key != KeyCode::Unknown);
    return out;
}

// The one table.
constexpr ShortcutEntry kShortcuts[] = {
    // --- File ---
    { ShortcutAction::NewScene,     "File", "Ctrl+N",       "New scene" },
    { ShortcutAction::OpenScene,    "File", "Ctrl+O",       "Open scene" },
    { ShortcutAction::SaveScene,    "File", "Ctrl+S",       "Save scene" },
    { ShortcutAction::SaveSceneAs,  "File", "Ctrl+Shift+S", "Save scene as" },
    { ShortcutAction::ImportModel,  "File", "Ctrl+I",       "Import model" },
    // Build Game has no shortcut. The menu used to print Ctrl+B beside it, which
    // is Creative Mode -- so the one key it named opened a different feature.
    { ShortcutAction::BuildGame,    "File", "",             "Build game (menu only)" },

    // --- Edit ---
    { ShortcutAction::Undo,           "Edit", "Ctrl+Z", "Undo" },
    { ShortcutAction::Redo,           "Edit", "Ctrl+Y", "Redo (also Ctrl+Shift+Z)" },
    { ShortcutAction::Cut,            "Edit", "Ctrl+X", "Cut entity" },
    { ShortcutAction::Copy,           "Edit", "Ctrl+C", "Copy entity" },
    { ShortcutAction::Paste,          "Edit", "Ctrl+V", "Paste entity" },
    { ShortcutAction::Duplicate,      "Edit", "Ctrl+D", "Duplicate selected" },
    { ShortcutAction::DeleteSelected, "Edit", "Delete", "Delete selected entity" },

    // --- Viewport ---
    { ShortcutAction::FocusSelected,    "Viewport", "F",      "Focus on selected entity" },
    { ShortcutAction::ClearSelection,   "Viewport", "Escape", "Clear selection" },
    { ShortcutAction::GizmoTranslate,   "Viewport", "1",      "Translate gizmo" },
    { ShortcutAction::GizmoRotate,      "Viewport", "2",      "Rotate gizmo" },
    { ShortcutAction::GizmoScale,       "Viewport", "3",      "Scale gizmo" },
    { ShortcutAction::GizmoToggleSpace, "Viewport", "4",      "Toggle local/world space" },
    { ShortcutAction::FrameGround,      "Viewport", "Home",   "Frame the ground for building" },

    // --- Tools ---
    { ShortcutAction::CommandPalette, "Tools", "Ctrl+P",       "Command palette" },
    { ShortcutAction::CreativeMode,   "Tools", "Ctrl+B",       "Creative mode" },
    { ShortcutAction::ShortcutsHelp,  "Tools", "Ctrl+Shift+/", "Show this help" },
    { ShortcutAction::ReportBug,      "Tools", "Ctrl+Shift+B", "Report a bug" },
    { ShortcutAction::QuickBugReport, "Tools", "F5",           "Quick bug report" },

    // --- Debug & layout ---
    { ShortcutAction::GameDebug,   "Debug", "F1",           "Game Debug panels" },
    { ShortcutAction::EngineDebug, "Debug", "F2",           "Debug Workstation" },
    { ShortcutAction::FocusMode,   "Debug", "F11",          "Focus mode (fullscreen game view)" },
    { ShortcutAction::Console,     "Debug", "` (Backtick)", "Drop-down console" },

    // --- Panel focus (keyboard navigation) ---
    { ShortcutAction::FocusHierarchy, "Panel focus", "Ctrl+1", "Focus Hierarchy" },
    { ShortcutAction::FocusInspector, "Panel focus", "Ctrl+2", "Focus Inspector" },
    { ShortcutAction::FocusViewport,  "Panel focus", "Ctrl+3", "Focus Viewport" },
    { ShortcutAction::FocusConsole,   "Panel focus", "Ctrl+4", "Focus Console" },
    { ShortcutAction::FocusAssets,    "Panel focus", "Ctrl+5", "Focus Asset Browser" },
};

static_assert(sizeof(kShortcuts) / sizeof(kShortcuts[0]) ==
              static_cast<usize>(ShortcutAction::Count),
              "every ShortcutAction needs exactly one row, and the row order must "
              "stay in step with the enum -- ShortcutFor indexes by action");

} // namespace

const ShortcutEntry* ShortcutTable() { return kShortcuts; }

usize ShortcutCount() { return sizeof(kShortcuts) / sizeof(kShortcuts[0]); }

const ShortcutEntry& ShortcutFor(ShortcutAction action) {
    for (const auto& e : kShortcuts) {
        if (e.action == action) return e;
    }
    return kShortcuts[0];   // unreachable: the static_assert covers the enum
}

const char* ShortcutChord(ShortcutAction action) {
    return ShortcutFor(action).chord;
}

bool ShortcutChordParses(const char* chord) {
    // An empty chord is legitimate -- it says "menu only", which is a true
    // statement about a command with no key.
    if (!chord || !*chord) return true;
    return Parse(chord).valid;
}

bool ShortcutPressed(ShortcutAction action) {
    const ParsedChord c = Parse(ShortcutFor(action).chord);
    if (!c.valid) return false;

    const bool ctrl = Input::IsKeyDown(KeyCode::LeftControl) ||
                      Input::IsKeyDown(KeyCode::RightControl);
    const bool shift = Input::IsKeyDown(KeyCode::LeftShift) ||
                       Input::IsKeyDown(KeyCode::RightShift);
    const bool alt = Input::IsKeyDown(KeyCode::LeftAlt) ||
                     Input::IsKeyDown(KeyCode::RightAlt);

    // Modifiers must match EXACTLY, not merely be present. Without this,
    // Ctrl+Shift+S would also fire plain Ctrl+S on its way through -- the save
    // dialog and a silent save to the old path, from one keystroke.
    if (ctrl != c.ctrl || shift != c.shift || alt != c.alt) return false;

    return Input::IsKeyPressed(c.key);
}

} // namespace Enjin::Editor
