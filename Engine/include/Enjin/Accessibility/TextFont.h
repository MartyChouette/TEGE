#pragma once
// The accessibility font choice, for every path in the engine that bakes
// glyphs.
//
// Three text stacks exist: ImGui's atlas (editor chrome and the ImGui menus),
// TextRasterizer (bitmap in-world text) and FontAtlas (SDF text). Turning on
// the dyslexia-friendly font only ever reached ImGui's, via
// FontLibrary::SetFont writing io.FontDefault -- so the setting changed the
// editor's own furniture and nothing the game drew.
//
// Worse, the other two had no default font of their own and used the embedded
// OpenDyslexic bytes as "the bundled default", so authored text was ALWAYS in
// the accessibility face and the toggle had nothing left to switch.
//
// This is the one place that answers "which face should this text bake in".
// It deliberately does not include ImGui: the engine text paths must not
// depend on the editor's UI library.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>

namespace Enjin {
namespace Accessibility {

// Set from the accessibility settings (ApplyTextScale pushes it, so every
// runtime gets it from the same call that already carries font scale).
ENJIN_API void SetDyslexiaFontEnabled(bool enabled);
ENJIN_API bool IsDyslexiaFontEnabled();

// The bytes a text path should bake for `requestedPath`.
//
// Returns nullptr only when the caller should read `requestedPath` from disk
// itself. When the dyslexia font is on it wins over any requested path --
// that is the point of the setting -- and an empty path yields the engine's
// normal body face.
ENJIN_API const u8* ResolveFontBytes(const std::string& requestedPath, usize& outSize);

// Font caches are keyed by path. Two different faces can answer for one path
// as the setting is toggled, so the key has to carry the choice or the cache
// keeps handing back the face baked before the toggle.
ENJIN_API std::string FontCacheKey(const std::string& requestedPath);

} // namespace Accessibility
} // namespace Enjin
