#pragma once

#include "Enjin/Platform/Platform.h"

#include <string>
#include <unordered_map>
#include <vector>

struct ImFont;

namespace Enjin {
namespace GUI {

// The faces a game's UI asks for, by project-relative path.
//
// Why a registry rather than a field holding an ImFont*: ImGui builds its glyph
// atlas up front and `io.Fonts->Clear()` invalidates EVERY ImFont* ever handed
// out. A canvas that cached a pointer would be holding a dangling one after the
// next rebuild -- and rebuilds happen whenever the editor's own font settings
// change. So the durable thing is the PATH, and pointers are re-resolved after
// each build.
//
// The registry therefore keeps two things: the set of paths anyone has asked
// for (durable, survives rebuilds) and the pointers from the most recent build
// (discarded on Clear).
//
// Threading: main thread only, like everything else that touches ImGui.
class ENJIN_API UIFontRegistry {
public:
    static UIFontRegistry& Get();

    // Where project-relative font paths resolve from. A path that escapes this
    // root is refused, so a scene file cannot name C:\Windows\Fonts or walk out
    // with "..". Empty root = no game fonts can load, which is the correct
    // state for a runtime that has not opened a project yet.
    void SetRoot(const std::string& absoluteProjectRoot);
    const std::string& GetRoot() const { return m_Root; }

    // Ask for a face. Returns false if the path is unsafe or the root is unset;
    // true if it is now wanted (whether or not it was already).
    //
    // This does NOT load anything: loading has to happen inside whoever owns the
    // atlas, during its font-building step. Callers that need the face on screen
    // check NeedsRebuild() afterwards.
    bool Request(const std::string& relativePath);

    // True when something has been requested that the last build did not include.
    // The atlas owner rebuilds when it sees this, and only then -- a rebuild
    // stalls the GPU, so it must not happen per frame.
    bool NeedsRebuild() const { return m_NeedsRebuild; }

    // The face for a path, or nullptr when it is not loaded (never requested,
    // failed to load, or the atlas has not been rebuilt since it was asked for).
    // Callers fall back to the ambient font; a missing face must never mean
    // missing text.
    ImFont* Find(const std::string& relativePath) const;

    // Every path anyone has asked for, in request order.
    const std::vector<std::string>& RequestedPaths() const { return m_Requested; }

    // Resolved absolute path for a request, or "" if it is not safe/known.
    std::string ResolvedPath(const std::string& relativePath) const;

    // Record the face a build produced. Called by the atlas owner as it adds
    // each font; a null face records the failure so we do not retry every frame.
    void SetLoaded(const std::string& relativePath, ImFont* font);

    // The atlas was cleared: every pointer we handed out is dangling. The
    // request SET survives, because that is what the next build re-adds.
    void OnAtlasCleared();

    // Called by the atlas owner once a build finishes, so NeedsRebuild goes
    // quiet until something new is asked for.
    void MarkBuilt() { m_NeedsRebuild = false; }

    // Forget everything, including requests. For tests and project close.
    void Reset();

private:
    UIFontRegistry() = default;

    std::string m_Root;
    std::vector<std::string> m_Requested;                  // request order, deduped
    std::unordered_map<std::string, ImFont*> m_Loaded;     // valid until OnAtlasCleared
    bool m_NeedsRebuild = false;
};

} // namespace GUI
} // namespace Enjin
