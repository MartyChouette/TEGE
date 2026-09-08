#include "Enjin/GUI/UIFontRegistry.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>

namespace Enjin {
namespace GUI {

UIFontRegistry& UIFontRegistry::Get() {
    static UIFontRegistry instance;
    return instance;
}

void UIFontRegistry::SetRoot(const std::string& absoluteProjectRoot) {
    if (m_Root == absoluteProjectRoot) return;
    m_Root = absoluteProjectRoot;

    // A different project's fonts are different files. Anything already loaded
    // was resolved against the old root, so it has to go -- keeping it would
    // silently render one project's HUD in another project's typeface.
    m_Loaded.clear();
    m_Requested.clear();
    m_NeedsRebuild = false;
}

std::string UIFontRegistry::ResolvedPath(const std::string& relativePath) const {
    if (relativePath.empty() || m_Root.empty()) return "";
    if (!Platform::IsSafeRelativePath(relativePath)) return "";
    return Platform::ResolveWithinRoot(m_Root, relativePath);
}

bool UIFontRegistry::Request(const std::string& relativePath) {
    if (relativePath.empty()) return false;

    if (m_Root.empty()) {
        ENJIN_LOG_WARN(Editor, "UI font '%s' requested before a project root was set",
                       relativePath.c_str());
        return false;
    }
    if (ResolvedPath(relativePath).empty()) {
        // A scene file is untrusted input: it can name anything.
        ENJIN_LOG_WARN(Editor, "UI font '%s' rejected: outside the project root",
                       relativePath.c_str());
        return false;
    }

    if (std::find(m_Requested.begin(), m_Requested.end(), relativePath) == m_Requested.end()) {
        m_Requested.push_back(relativePath);
    }
    // Already loaded from a previous build means nothing new to do.
    if (m_Loaded.find(relativePath) == m_Loaded.end()) {
        m_NeedsRebuild = true;
    }
    return true;
}

ImFont* UIFontRegistry::Find(const std::string& relativePath) const {
    if (relativePath.empty()) return nullptr;
    auto it = m_Loaded.find(relativePath);
    return (it != m_Loaded.end()) ? it->second : nullptr;
}

void UIFontRegistry::SetLoaded(const std::string& relativePath, ImFont* font) {
    if (relativePath.empty()) return;
    // A null entry is recorded on purpose: it is the difference between "not
    // tried yet" and "tried and failed", and only the first should ask for
    // another atlas rebuild.
    m_Loaded[relativePath] = font;
}

void UIFontRegistry::OnAtlasCleared() {
    m_Loaded.clear();
    // The requests survive; they are what the next build re-adds. If anything
    // was ever wanted, the next build has to happen.
    m_NeedsRebuild = !m_Requested.empty();
}

void UIFontRegistry::Reset() {
    m_Root.clear();
    m_Requested.clear();
    m_Loaded.clear();
    m_NeedsRebuild = false;
}

} // namespace GUI
} // namespace Enjin
