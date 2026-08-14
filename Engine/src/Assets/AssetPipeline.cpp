#include "Enjin/Assets/AssetPipeline.h"
#include <filesystem>
#include <system_error>

namespace Enjin {
namespace Assets {

std::string CopyToProjectAssets(const std::string& srcPath,
                                const std::string& projectRoot,
                                const std::string& subdir)
{
    namespace fs = std::filesystem;

    if (srcPath.empty() || projectRoot.empty()) return srcPath;

    std::error_code ec;
    fs::path src(srcPath);
    if (!fs::exists(src, ec)) return srcPath;

    // Already under the project root — no copy needed.
    fs::path rel = fs::relative(src, projectRoot, ec);
    if (!ec && !rel.empty()) {
        bool escapes = false;
        for (const auto& part : rel) {
            if (part == "..") { escapes = true; break; }
        }
        if (!escapes) return src.generic_string();
    }

    fs::path destDir = fs::path(projectRoot) / subdir;
    fs::create_directories(destDir, ec);
    if (ec) return srcPath;

    fs::path dest = destDir / src.filename();
    if (!fs::exists(dest, ec)) {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) return srcPath;
    }
    return dest.generic_string();
}

} // namespace Assets
} // namespace Enjin
