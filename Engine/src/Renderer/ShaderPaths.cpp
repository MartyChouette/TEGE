#include "Enjin/Renderer/ShaderPaths.h"

namespace Enjin {
namespace Renderer {

std::vector<std::string> ShaderSearchPaths(std::string_view fileName) {
    // Ordered cheapest and most likely first.
    //
    // The first entry covers a packaged build that ships shaders next to the
    // executable. The Engine/shaders entries walk up out of a build tree, which
    // is where a developer runs from. The share/enjin entry is the installed
    // layout: the binary sits in <prefix>/bin, the shaders in
    // <prefix>/share/enjin/shaders, and until this list existed nothing ever
    // looked there.
    static const char* const kPrefixes[] = {
        "shaders/",
        "Engine/shaders/",
        "../Engine/shaders/",
        "../../Engine/shaders/",
        "../../../Engine/shaders/",
        "bin/shaders/",
        // The Inno installer layout: the exe goes to {app}/bin and the
        // shaders to {app}/shaders, so from the executable they are one
        // level up. Nothing here matched that -- "shaders/" resolves to
        // {app}/bin/shaders, and the share/enjin entries are the CMake
        // layout, which the installer does not use. A Windows install therefore
        // found no compute or ray-tracing shaders at all, which is the same bug
        // this list was created to fix, still live for the one layout most
        // users actually get.
        "../shaders/",
        "../share/enjin/shaders/",
        "../../share/enjin/shaders/",
    };

    std::vector<std::string> out;
    out.reserve(sizeof(kPrefixes) / sizeof(kPrefixes[0]));
    for (const char* prefix : kPrefixes) {
        std::string p(prefix);
        p.append(fileName);
        out.push_back(std::move(p));
    }
    return out;
}

} // namespace Renderer
} // namespace Enjin
