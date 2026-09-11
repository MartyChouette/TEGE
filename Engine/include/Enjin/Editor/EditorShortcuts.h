#pragma once

// One table of editor keyboard shortcuts, read by everything that shows or
// handles one.
//
// There were three lists and they disagreed:
//
//   * The Keyboard Shortcuts window (Help > Keyboard Shortcuts) listed
//     "Ctrl+P  Play / Stop" and, four rows above, "Ctrl+P  Command Palette".
//     Ctrl+P is the palette. It also listed "Ctrl+Shift+P  Pause / Resume" and
//     "F10  Toggle input action map", neither of which has a handler anywhere.
//   * The menu bar labelled seven accelerators that were never implemented:
//     Ctrl+N, Ctrl+O, Ctrl+Shift+S, Ctrl+I, Ctrl+X, Ctrl+C, Ctrl+V. Every one
//     of those menu items works when clicked; the key did nothing.
//   * The menu bar also gave Ctrl+B to BOTH "Build Game..." and "Creative
//     Mode". The handler toggles creative mode.
//
// And five real shortcuts were in none of the lists: F5, F11, Ctrl+B,
// Ctrl+Shift+B and Ctrl+1..5.
//
// A label that names a key which does nothing is worse than no label: it is the
// software telling you about itself, wrongly, in the one place you go when you
// do not know. So the table below is the only list, `Pressed()` evaluates the
// same chord string the window displays and the menu prints, and the three can
// no longer drift apart.

#include "Enjin/Platform/Types.h"
#include "Enjin/Platform/Platform.h"

namespace Enjin::Editor {

// Every shortcut the editor handles itself. Play-mode keys a GAME binds are not
// here -- those live in InputActionMap and are rebindable.
enum class ShortcutAction : u8 {
    NewScene, OpenScene, SaveScene, SaveSceneAs, ImportModel, BuildGame,
    Undo, Redo, Cut, Copy, Paste, Duplicate, DeleteSelected,
    FocusSelected, ClearSelection,
    GizmoTranslate, GizmoRotate, GizmoScale, GizmoToggleSpace, FrameGround,
    CommandPalette, CreativeMode, ShortcutsHelp, ReportBug, QuickBugReport,
    GameDebug, EngineDebug, FocusMode, Console,
    FocusHierarchy, FocusInspector, FocusViewport, FocusConsole, FocusAssets,
    Count
};

struct ShortcutEntry {
    ShortcutAction action;
    const char* category;
    const char* chord;        // "Ctrl+Shift+S", "F5", "1", "` (Backtick)"
    const char* description;
};

// The table. Ordered by category, because the help window renders it in order.
ENJIN_API const ShortcutEntry* ShortcutTable();
ENJIN_API usize ShortcutCount();

ENJIN_API const ShortcutEntry& ShortcutFor(ShortcutAction action);

// The chord string for a menu item's accelerator column, so a menu label cannot
// name a key the handler does not honour.
ENJIN_API const char* ShortcutChord(ShortcutAction action);

// Was this shortcut's chord pressed THIS frame?
//
// Parses the same string the window shows. That is the point: a chord nobody
// can press would have to be misspelled in the one place it is written, and the
// test asserts every chord in the table parses.
ENJIN_API bool ShortcutPressed(ShortcutAction action);

// Exposed for the test: whether a chord string is one this can evaluate.
// A chord that does not parse is a shortcut the editor displays and cannot
// honour, which is exactly the bug this file exists to prevent.
ENJIN_API bool ShortcutChordParses(const char* chord);

} // namespace Enjin::Editor
