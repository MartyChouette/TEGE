#pragma once

// The editor has three modes. Until now it had two and never said so.
//
//   Developer   the full editor: hierarchy, inspector, every panel. What you
//               get by default, and what everything in this codebase calls
//               simply "the editor".
//   Creative    the build rail. A tool at a time, a viewport, and a Play
//               button. Reached by Ctrl+B or the View menu.
//   Tutorial    Creative, with a walkthrough running over it and a guide that
//               says what to try next.
//
// Two of those already existed and were never presented as a pair. Creative
// Mode had a "Full Editor" button to leave by, so the way OUT was visible from
// inside it -- but from the full editor there was nothing at all saying another
// mode existed, short of a Ctrl+B you would have to already know about. A mode
// you can only leave is not a mode; it is a place you end up.
//
// So the mode is one value now, owned in one place, shown in one control, and
// saved between sessions.

#include "Enjin/Platform/Types.h"
#include "Enjin/Platform/Platform.h"

namespace Enjin::Editor {

enum class EditorMode : u8 {
    Developer = 0,
    Creative,
    Tutorial,
    Count
};

// Tutorial IS Creative with a walkthrough on top -- the same rail, the same
// tools, the same gestures. Anything that asks "is the build surface up" wants
// this rather than a comparison against Creative alone, or the tutorial would
// silently lose the thing it is teaching.
inline bool ModeUsesBuildSurface(EditorMode mode) {
    return mode == EditorMode::Creative || mode == EditorMode::Tutorial;
}

// The name a person sees. Also what the mode is saved as, so a settings file
// stays readable and a renamed mode does not silently reset everybody to
// Developer.
ENJIN_API const char* EditorModeName(EditorMode mode);

// One line saying what the mode is for, shown beside the name wherever the mode
// is chosen. A switcher that lists three words and explains none of them makes
// you pick by guessing.
ENJIN_API const char* EditorModeDescription(EditorMode mode);

// Parse a saved name. Unknown input returns Developer, because that is the mode
// with everything in it -- a settings file from a future version should drop you
// somewhere you can still work, not somewhere with fewer tools.
ENJIN_API EditorMode EditorModeFromName(const char* name);

} // namespace Enjin::Editor
