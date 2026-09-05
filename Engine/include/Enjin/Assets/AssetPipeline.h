#pragma once
#include <string>
#include <vector>

namespace Enjin {
namespace Assets {

// Copy srcPath into <projectRoot>/<subdir>/<filename>, creating dirs as needed.
// Returns the path to the copy RELATIVE to projectRoot (forward slashes), which
// is the form that survives moving or shipping the project.
// Returns srcPath unchanged if:
//   - projectRoot is empty
//   - the source file does not exist
//   - the copy fails
// If srcPath is already under projectRoot, no copy is made and the existing
// file's project-relative path is returned.
// If the destination file already exists, skips the copy and returns its
// project-relative path (idempotent for re-import).
std::string CopyToProjectAssets(const std::string& srcPath,
                                const std::string& projectRoot,
                                const std::string& subdir = "assets/textures");

// Copy a MODEL and everything it references into the project, and return the
// model's new project-relative path.
//
// A model is rarely one file: an .obj names its .mtl, an .mtl names its
// textures, a .gltf names its .bin buffers and images. Copying only the file
// the user picked produces a model with no materials, which is a quieter
// failure than the one it replaces. So the whole referenced set moves together
// into <projectRoot>/<subdir>/<model stem>/, keeping each reference's relative
// layout so the copies still resolve each other.
//
// A reference that is absolute, or that escapes the model's own folder, is
// flattened next to the model and the referring file is REWRITTEN to match -
// otherwise the copy would point back at the author's machine, which is the
// whole problem this exists to remove.
//
// Handles .obj (mtllib + map_* textures) and .gltf (buffers + images). A
// self-contained model (.glb, .fbx with embedded media) copies as a single
// file. Any engine sidecar (<model>.enjinasset) comes along too.
//
// Returns srcPath unchanged when projectRoot is empty, the source is missing,
// or the copy fails. Already-inside-the-project models are returned relative
// without copying. `copiedFiles`, when given, receives every file written.
std::string CopyModelToProjectAssets(const std::string& srcPath,
                                     const std::string& projectRoot,
                                     const std::string& subdir = "assets/models",
                                     std::vector<std::string>* copiedFiles = nullptr);

} // namespace Assets
} // namespace Enjin
