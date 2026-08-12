#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Enjin {
namespace Assets {

// ---------------------------------------------------------------------------
// MeshAssetCache
//
// Loads a source mesh file (FBX/glTF/OBJ/...) ONCE and serves the geometry for a
// given mesh index, so a scene can reference an imported mesh instead of storing
// its vertices inline. Every entity that references the same file shares one load,
// which is what turns "hundreds of copies of one asset" into one CPU/VRAM footprint.
//
// Main-thread only (it drives the importer, which builds ECS data in a temp world).
// The cache is a process-wide singleton; call Clear() when closing a project so a
// re-imported source file is picked up fresh.
// ---------------------------------------------------------------------------
class ENJIN_API MeshAssetCache {
public:
    static MeshAssetCache& Get();

    // Base directory that project-relative source paths resolve against. The editor
    // sets this to the project root on project load; the player sets it to the asset
    // root at boot. Relative refs (the portable form) are joined onto this; absolute
    // refs are used as-is. Setting a different root clears the cache.
    void SetSearchRoot(const std::string& root);
    const std::string& GetSearchRoot() const { return m_SearchRoot; }

    // Directory where baked binary mesh caches (.enjmesh) are written/read. When empty,
    // it's derived from the search root (<root>/.enjin/meshcache/); if that's also empty
    // baking is disabled and the cache falls back to re-importing the source every time.
    // The baked cache turns a multi-second Assimp/glTF re-parse into a raw buffer read.
    void SetCacheDir(const std::string& dir) { m_CacheDir = dir; }
    const std::string& GetCacheDir() const { return m_CacheDir; }

    // Fill out.vertices/indices/subMeshes from the referenced source mesh. Loads and
    // caches the file on first use. Returns true on success. When ref.contentHash is
    // non-zero it is verified against the reloaded geometry; a mismatch (source file
    // changed on disk, or was imported with different options) fails and logs, leaving
    // `out` geometry untouched so the caller can fall back rather than show wrong data.
    bool Resolve(const ECS::MeshComponent::SourceRef& ref, ECS::MeshComponent& out);

    // True if `ref` can be reloaded right now (source file present, mesh index found,
    // content hash matches). Used at SAVE time to decide whether it is safe to drop a
    // mesh's inline vertices in favor of the reference — if this is false the caller
    // keeps the inline geometry, so a reference is never written unless it is known to
    // reload. Loads/caches the file as a side effect, same as Resolve.
    bool CanResolve(const ECS::MeshComponent::SourceRef& ref);

    void Clear();  // drop all cached files (e.g., on project/scene close)

private:
    struct CachedMesh {
        std::vector<ECS::MeshComponent::Vertex> vertices;
        std::vector<u32> indices;
        std::vector<ECS::MeshComponent::SubMesh> subMeshes;
        u64 contentHash = 0;
    };
    struct CachedFile {
        bool loaded = false;   // set once a load has been attempted (success or fail)
        std::unordered_map<i32, CachedMesh> byMeshIndex;
    };
    std::unordered_map<std::string, CachedFile> m_Files;
    std::string m_SearchRoot;   // base for resolving project-relative source paths
    std::string m_CacheDir;     // where baked .enjmesh files live (empty = derive from root)

    // Resolve a (possibly project-relative) source path to an absolute filesystem path.
    std::string ResolvePath(const std::string& sourcePath) const;
    CachedFile& LoadFile(const std::string& path);

    // Baked binary cache: skip the importer when a valid .enjmesh already exists.
    // BakedPath returns "" when no cache directory is available (baking disabled).
    // LoadBaked verifies the source's size+mtime and fills `out` on success (leaving it
    // untouched on any failure so the caller re-imports). WriteBaked persists a file after
    // a successful import.
    std::string BakedPath(const std::string& resolvedSource) const;
    bool LoadBaked(const std::string& resolvedSource, CachedFile& out) const;
    void WriteBaked(const std::string& resolvedSource, const CachedFile& file) const;
    // Locate the cached geometry for a ref (loads the file if needed, verifies hash).
    // Returns nullptr if unresolvable. logMismatch controls whether a miss is logged.
    const CachedMesh* Find(const ECS::MeshComponent::SourceRef& ref, bool logMismatch);
};

} // namespace Assets
} // namespace Enjin
