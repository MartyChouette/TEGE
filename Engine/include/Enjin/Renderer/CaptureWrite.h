#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <vector>

namespace Enjin {
namespace Renderer {

// Write one RGBA8 capture to disk as BOTH `<basePath>.png` and `<basePath>.ppm`.
//
// Two formats on purpose. The PNG is for a person to open and look at; the P6
// PPM is for the comparers, which are deliberately pure-stdlib Python
// (tools/probes/golden_compare.py) so a capture can be checked on a machine with
// no PIL, no numpy and no node.
//
// `rgba` is row-major from the TOP-LEFT, four bytes per pixel, and must hold
// exactly w*h*4 bytes. Returns false without writing anything if it does not,
// because a short buffer written out reads as a corrupt render rather than a
// bad call.
ENJIN_API bool WriteCapture(const std::string& basePath, const std::vector<u8>& rgba,
                            u32 width, u32 height);

} // namespace Renderer
} // namespace Enjin
