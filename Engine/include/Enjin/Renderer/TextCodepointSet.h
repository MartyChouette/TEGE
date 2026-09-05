#pragma once
// The codepoints a project's text actually contains.
//
// The SDF atlas cannot bake every codepoint a font has. At kBasePx with
// kPadding a glyph occupies roughly 64x64 in a 1024 atlas, so about 250 fit --
// ASCII plus Latin-1 is already 191, and adding the whole Cyrillic block would
// overflow and silently drop glyphs.
//
// But what a project USES is far smaller than what its font can express. A
// Russian game needs 66 letters, not the 255-codepoint block; a Greek one needs
// 48. Collecting the codepoints actually present in the loaded string tables
// and the scene's text turns "does not fit" into "fits comfortably", and it is
// why in-world text can be localised at all.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

#include <string>
#include <vector>

namespace Enjin {

namespace ECS { class World; }

namespace Renderer {

// Every codepoint above Latin-1 appearing in the loaded localization tables
// and in the world's TextComponents, sorted and deduplicated.
//
// Latin-1 and below are omitted: the atlas bakes those unconditionally, so
// returning them would only make the set bigger and the cache key noisier.
//
// `world` may be null, which collects from the string tables alone.
ENJIN_API std::vector<u32> CollectProjectCodepoints(ECS::World* world);

// A stable digest of a codepoint set, for a font-atlas cache key.
//
// The atlas is cached per font path; two different sets of extras against one
// path are two different atlases, and without this the first one baked would be
// handed back forever -- so switching locale would look like it did nothing.
ENJIN_API u64 HashCodepoints(const std::vector<u32>& codepoints);

} // namespace Renderer
} // namespace Enjin
