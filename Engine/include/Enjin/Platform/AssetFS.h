#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <functional>
#include <string>
#include <vector>

// One way to read a game's files, whether they sit loose on disk or inside
// the build's package.
//
// The web player reads everything from its pak, because it exposes every pak
// entry as a file in the browser's in-memory filesystem (WebLazyFS), so the
// loaders' plain fopen calls find them. The desktop player had nothing like
// that: it read its manifest, scenes and scripts from the pak and every other
// loader (textures, models, audio, fonts, prefabs...) read the disk. Anything
// the build did not copy loose was missing from the game, and the assets that
// were copied loose shipped twice, once obfuscated and once plain (EP-18).
//
// A loader asks here instead of opening the file itself. With a package
// mounted, a path under the mount root (or a relative one) is looked up in the
// package first and on disk after; with none, it is the disk. Saves, settings,
// logs and anything else a game WRITES stay on the disk and do not come here.
namespace Enjin::Platform::AssetFS {

// Reads one package entry by its virtual path ("assets/tex/wood.png").
// Returns false when the package has no such entry.
using PackageReader = std::function<bool(const std::string& virtualPath, std::vector<u8>& out)>;
// Whether the package has an entry, without reading it
using PackageContains = std::function<bool(const std::string& virtualPath)>;

// Mounts a package. `root` is the directory package paths are relative to:
// the directory the loose files would sit in (the player's own folder), so a
// loader holding "<root>/assets/a.png" and one holding "assets/a.png" both
// reach the entry "assets/a.png".
ENJIN_API void Mount(const std::string& root, PackageReader reader, PackageContains contains);
ENJIN_API void Unmount();
ENJIN_API bool IsMounted();

// The package entry a path names, or "" when it names none (outside the
// root, or nothing mounted). Exposed for loaders that hand a library a
// virtual path and for tests.
ENJIN_API std::string ToVirtualPath(const std::string& path);

// The file's bytes: the package's copy when it has one, else the disk's.
ENJIN_API bool ReadBytes(const std::string& path, std::vector<u8>& out);
ENJIN_API bool ReadText(const std::string& path, std::string& out);
// Whether ReadBytes would find it
ENJIN_API bool Exists(const std::string& path);

} // namespace Enjin::Platform::AssetFS
