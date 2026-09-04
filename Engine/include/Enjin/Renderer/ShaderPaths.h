#pragma once

// Where to look on disk for a compiled shader.
//
// Nineteen copies of this list were written by hand across nine files, each
// slightly different, and not one of them looked in the directory the installer
// actually puts shaders in. `cmake --install` and the DEB package place them at
// <prefix>/share/enjin/shaders while the binary runs from <prefix>/bin, so an
// installed build found nothing and quietly fell back to whatever the caller
// does when a shader is missing. Clustered lighting is on by default, which
// makes that the normal path for anyone installing rather than running from a
// build tree.
//
// Only shaders NOT baked into the binary go through this. Most of the engine's
// SPIR-V is embedded in ShaderData.h; these are the compute and visibility
// shaders that still load from a file.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <string>
#include <string_view>
#include <vector>

namespace Enjin {
namespace Renderer {

// Candidate paths for one compiled shader, in the order they should be tried.
// fileName is the bare name, e.g. "cull.comp.spv". The working directory is the
// executable's own directory (Application::InitializeEngine changes to it), so
// every entry is relative to that.
ENJIN_API std::vector<std::string> ShaderSearchPaths(std::string_view fileName);

} // namespace Renderer
} // namespace Enjin
