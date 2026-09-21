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

    // Turn a project-relative asset path into one that can actually be opened.
    //
    // The process CWD is never the project: the editor and the player both run
    // from their exe directory, so fopen("assets/foo.png") resolves somewhere
    // meaningless and fails. Everything that opens an authored path has to join
    // it onto the search root first, and this is the one place that knows how.
    //
    // It exists because that join was written inline in the texture loader and
    // NOT in the sprite atlas, so a sprite with a texture silently loaded
    // nothing and drew nothing, in every project, for as long as the atlas has
    // existed. Absolute paths and paths that already resolve are returned
    // unchanged, so calling this on an already-good path is free and safe.
    static std::string ResolveAgainstSearchRoot(const std::string& path);

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

    // Put THIS geometry in the cache under THIS reference, before a caller
    // throws the geometry away.
    //
    // The bug this exists for (Gobliny, 2026-09-13): the cache answers a
    // reference by RE-IMPORTING the source file with DEFAULT ImportOptions, on
    // the stated assumption that "geometry only depends on the file + axis
    // conversion + skinned-vertex bake". That assumption is false. ImportOptions
    // also carries `scale`, `normalizeScale`, `convertAxes` and three axis
    // flips, all of which change vertices, and SourceRef records only the two
    // axis booleans. So a model imported with anything but the defaults came
    // back from the cache as different geometry -- in that project, 62 times
    // larger -- and the scene serializer had already dropped the real vertices
    // in favour of the reference.
    //
    // Recording the options and replaying them would work, and would still be a
    // bet that two imports agree. Storing what the scene ACTUALLY had removes
    // the bet: a reference and the baked copy it replaced are then the same
    // bytes by construction, whatever options produced them, including options
    // that do not exist yet.
    //
    // Returns false if the geometry is empty or the ref is unusable, and the
    // caller must then keep its inline vertices.
    bool Adopt(const ECS::MeshComponent::SourceRef& ref, const ECS::MeshComponent& mesh);

    // Delete baked files whose source no longer exists.
    //
    // A baked file is named for the FNV-1a of its RESOLVED source path, so
    // moving or renaming an asset orphans its bake permanently and nothing ever
    // collects it. Returns how many were removed.
    usize SweepOrphanedBakes();

    // Reload CPU vertices/indices into `mc` when they've been freed (to reclaim RAM after
    // GPU upload) but the mesh is source-reproducible. No-op when data is already present.
    // Returns true if `mc` has usable CPU geometry afterward. This is the reload half of
    // "free CPU mesh data after upload": consumers that need the vertices again (physics
    // collider build, precise picking, device-loss re-upload) call this first.
    bool EnsureCpuData(ECS::MeshComponent& mc);

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
        // Keyed by (meshIndex, lodLevel) via CacheKey, not by meshIndex alone. A
        // generated LOD level shares its source path and mesh index with LOD 0 and
        // differs only in the level, so a level-blind key makes level 3 overwrite the
        // full-detail mesh in the cache -- and the baked file -- for every entity that
        // references it.
        std::unordered_map<u64, CachedMesh> byMeshIndex;
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

    // The cache key. lodLevel is bounded by LODComponent::MAX_LEVELS (5), so three bits
    // is room to spare; meshIndex keeps its full range above them.
    static u64 CacheKey(i32 meshIndex, i32 lodLevel) {
        const u64 lvl = static_cast<u64>(lodLevel < 0 ? 0 : lodLevel) & 0x7ull;
        return (static_cast<u64>(static_cast<u32>(meshIndex)) << 3) | lvl;
    }
};

} // namespace Assets
} // namespace Enjin
