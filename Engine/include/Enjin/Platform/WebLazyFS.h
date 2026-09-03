#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <functional>
#include <string>
#include <vector>

// Web-only: lazy pak-backed files in the Emscripten MEMFS.
//
// The web player ships a single game.enjpak. Disk-path loaders (cgltf,
// stb_image, miniaudio, font readers, VOX/PLY/SVG...) fopen() project-relative
// paths, so the pak's assets must be visible in the WASM virtual filesystem.
// Extracting every asset up front (the pre-2026-09-03 approach) kept a second
// full copy of the pak's contents resident for the whole session.
//
// RegisterLazyFile() instead creates a zero-cost MEMFS node whose bytes are
// produced by the registered reader the first time any stream op touches it
// (read/seek/mmap). stat() answers from the size passed at registration, so
// existence/size checks never materialize anything. Once materialized the node
// behaves like any MEMFS file (and stays resident, like an extracted file).
//
// Mirrors the technique Emscripten's own FS.createLazyFile uses (override the
// node's stream_ops + usedBytes); the only MEMFS internals relied on are
// node.contents (Uint8Array), node.usedBytes and node.stream_ops.
//
// Everything here is a no-op off Emscripten so callers need no #if.
namespace Enjin::Platform::WebLazyFS {

// Produces the decoded bytes for a virtual path (empty = failure).
using ReadFn = std::function<std::vector<u8>(const std::string& virtualPath)>;

// Install the byte source. Must outlive every registered file.
ENJIN_API void SetReader(ReadFn reader);

// Register `virtualPath` (project-relative, forward slashes, no leading '/')
// as a lazy file of `size` bytes under MEMFS root. Existing paths are left
// alone. Returns true when a lazy node was created.
ENJIN_API bool RegisterLazyFile(const std::string& virtualPath, u64 size);

// Diagnostics: how many lazy files have been materialized so far, and the
// total bytes those materializations occupy.
ENJIN_API u32 GetMaterializedCount();
ENJIN_API u64 GetMaterializedBytes();
ENJIN_API u32 GetRegisteredCount();

} // namespace Enjin::Platform::WebLazyFS
