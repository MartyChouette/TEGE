#include "Enjin/Editor/EditorMode.h"
#include <cstring>

namespace Enjin::Editor {

const char* EditorModeName(EditorMode mode) {
    switch (mode) {
        case EditorMode::Developer: return "Developer";
        case EditorMode::Creative:  return "Creative";
        case EditorMode::Tutorial:  return "Tutorial";
        default:                    return "Developer";
    }
}

const char* EditorModeDescription(EditorMode mode) {
    switch (mode) {
        case EditorMode::Developer:
            return "Every panel. Hierarchy, inspector, graphs, profiler.";
        case EditorMode::Creative:
            return "Build with one tool at a time. No panels in the way.";
        case EditorMode::Tutorial:
            return "Creative, with a walkthrough and a guide alongside it.";
        default:
            return "";
    }
}

EditorMode EditorModeFromName(const char* name) {
    if (!name || !*name) return EditorMode::Developer;
    for (u8 i = 0; i < static_cast<u8>(EditorMode::Count); ++i) {
        const EditorMode m = static_cast<EditorMode>(i);
        if (std::strcmp(EditorModeName(m), name) == 0) return m;
    }
    // Unknown, which includes a mode added by a newer build. Developer is the
    // one with everything in it, so an unreadable setting costs you nothing.
    return EditorMode::Developer;
}

} // namespace Enjin::Editor
