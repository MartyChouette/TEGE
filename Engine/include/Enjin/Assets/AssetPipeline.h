#pragma once
#include <string>

namespace Enjin {
namespace Assets {

// Copy srcPath into <projectRoot>/<subdir>/<filename>, creating dirs as needed.
// Returns the absolute path to the copy on success.
// Returns srcPath unchanged if:
//   - srcPath is already under projectRoot (no-op)
//   - projectRoot is empty
//   - the source file does not exist
//   - the copy fails
// If the destination file already exists, skips the copy and returns the
// existing destination path (idempotent for re-import).
std::string CopyToProjectAssets(const std::string& srcPath,
                                const std::string& projectRoot,
                                const std::string& subdir = "assets/textures");

} // namespace Assets
} // namespace Enjin
