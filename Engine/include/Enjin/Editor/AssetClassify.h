#pragma once
// What kind of thing a file in the Asset Browser is, where the extension alone
// does not say.

#include "Enjin/Platform/Types.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace Enjin::Editor {

// A .json is a scene only when it reads like one. Scenes carry a top-level
// "entities" array; a localization table, a meta.json or a data file does not,
// and the Asset Browser opening one of those as a scene cleared the world
// (GR-9). Reads at most the first 4 KB.
inline bool JsonLooksLikeScene(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::string head(4096, '\0');
    f.read(head.data(), static_cast<std::streamsize>(head.size()));
    head.resize(static_cast<usize>(f.gcount()));
    return head.find("\"entities\"") != std::string::npos;
}

} // namespace Enjin::Editor
