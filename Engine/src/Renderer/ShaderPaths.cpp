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
