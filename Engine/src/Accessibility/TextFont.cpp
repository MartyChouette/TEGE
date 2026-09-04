#include "Enjin/Accessibility/TextFont.h"
#include "Enjin/Accessibility/OpenDyslexicFont.h"
#include "Enjin/GUI/EmbeddedFonts.h"

#include <atomic>

namespace Enjin {
namespace Accessibility {

namespace {
// Read on the render thread while the settings are written on the main
// thread, so this is atomic rather than a plain bool.
std::atomic<bool> g_DyslexiaFont{false};
}

void SetDyslexiaFontEnabled(bool enabled) {
    g_DyslexiaFont.store(enabled, std::memory_order_relaxed);
}

bool IsDyslexiaFontEnabled() {
    return g_DyslexiaFont.load(std::memory_order_relaxed);
}

const u8* ResolveFontBytes(const std::string& requestedPath, usize& outSize) {
    if (IsDyslexiaFontEnabled()) {
        // Overrides the authored font too. A player who needs this face needs
        // it on the text the game chose a font for as much as on the rest.
        outSize = s_OpenDyslexicFontDataSize;
        return s_OpenDyslexicFontData;
    }
    if (requestedPath.empty()) {
        // The engine's body face. This used to be OpenDyslexic, which is why
        // authored text has always rendered in the accessibility font.
        outSize = GUI::RobotoMediumTTFSize;
        return GUI::RobotoMediumTTF;
    }
    outSize = 0;
    return nullptr;   // caller loads the file
}

std::string FontCacheKey(const std::string& requestedPath) {
    // The suffix is what makes a toggle rebuild rather than return the face
    // baked before it.
    return IsDyslexiaFontEnabled() ? (requestedPath + "\x01dyslexic") : requestedPath;
}

} // namespace Accessibility
} // namespace Enjin
