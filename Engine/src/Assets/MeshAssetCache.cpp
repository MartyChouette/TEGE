#include "Enjin/Assets/MeshAssetCache.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Logging/Log.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <system_error>
#include <type_traits>

namespace {
// Baked mesh cache format. Bump kBakedVersion whenever the Vertex layout or this
// serialization changes so stale .enjmesh files are rejected instead of misread.
constexpr Enjin::u32 kBakedMagic   = 0x48534D45u;  // 'EMSH'
constexpr Enjin::u32 kBakedVersion = 1u;
static_assert(std::is_trivially_copyable_v<Enjin::ECS::MeshComponent::Vertex>,
              "Vertex must stay trivially copyable for raw baked-cache I/O");
}

namespace Enjin {
namespace Assets {

MeshAssetCache& MeshAssetCache::Get() {
    static MeshAssetCache instance;
    return instance;
}

void MeshAssetCache::Clear() {
    m_Files.clear();
}

void MeshAssetCache::SetSearchRoot(const std::string& root) {
    if (root == m_SearchRoot) return;
    m_SearchRoot = root;
    m_Files.clear();   // cached geometry was keyed against the old root — drop it
}

std::string MeshAssetCache::ResolvePath(const std::string& sourcePath) const {
    std::filesystem::path p(sourcePath);
    if (p.is_absolute() || m_SearchRoot.empty()) return sourcePath;
    std::error_code ec;
    std::filesystem::path joined = std::filesystem::path(m_SearchRoot) / p;
    std::filesystem::path norm = joined.lexically_normal();
    return norm.string();
}

MeshAssetCache::CachedFile& MeshAssetCache::LoadFile(const std::string& path) {
    // Key by the resolved absolute path so the same file referenced via different
    // relative forms shares one load.
    std::string resolved = ResolvePath(path);
    CachedFile& file = m_Files[resolved];
    if (file.loaded) return file;
    file.loaded = true;  // mark up-front so a failed load doesn't re-import for every mesh

    // Fast path: a valid baked binary skips the importer entirely.
    if (LoadBaked(resolved, file)) return file;

    // Re-import into a throwaway world with default options. Geometry only depends on
    // the file + axis conversion + skinned-vertex bake, all of which the default
    // auto-detecting import reproduces; materials/lights are irrelevant to the
    // vertices we cache, but animations/skeletons must stay ON because skinned meshes
    // bake the node world transform into their vertices (skeleton context required).
    ImportOptions opt;
    opt.importMaterials = false;
    opt.importLights = false;
    opt.generateColliders = false;
    opt.generateLODs = false;

    ECS::World temp;
    ImportResult res = SceneImporter::Import(resolved, &temp, opt);
    if (!res.success) {
        ENJIN_LOG_ERROR(Asset, "MeshAssetCache: failed to load '%s' (resolved '%s'): %s",
                        path.c_str(), resolved.c_str(), res.errorMessage.c_str());
        return file;
    }

    for (ECS::Entity e : temp.GetEntitiesWithComponent<ECS::MeshComponent>()) {
        auto* mc = temp.GetComponent<ECS::MeshComponent>(e);
        if (!mc || mc->source.meshIndex < 0) continue;
        CachedMesh cm;
        cm.vertices = mc->vertices;   // copied out before the temp world is destroyed
        cm.indices = mc->indices;
        cm.subMeshes = mc->subMeshes;
        cm.contentHash = mc->source.contentHash;
        file.byMeshIndex[mc->source.meshIndex] = std::move(cm);
    }

    ENJIN_LOG_INFO(Asset, "MeshAssetCache: loaded '%s' (%zu mesh(es))",
                   path.c_str(), file.byMeshIndex.size());

    // Persist a baked binary so the next session skips the importer.
    WriteBaked(resolved, file);
    return file;
}

std::string MeshAssetCache::BakedPath(const std::string& resolvedSource) const {
    std::string dir = m_CacheDir;
    if (dir.empty()) {
        if (m_SearchRoot.empty()) return "";  // nowhere to bake — baking disabled
        dir = (std::filesystem::path(m_SearchRoot) / ".enjin" / "meshcache").string();
    }
    // FNV-1a of the resolved path -> stable hex filename (avoids path-length/charset issues).
    u64 h = 1469598103934665603ULL;
    for (unsigned char c : resolvedSource) { h ^= c; h *= 1099511628211ULL; }
    char name[32];
    std::snprintf(name, sizeof(name), "%016llx.enjmesh", static_cast<unsigned long long>(h));
    return (std::filesystem::path(dir) / name).string();
}

bool MeshAssetCache::LoadBaked(const std::string& resolvedSource, CachedFile& out) const {
    const std::string bakedPath = BakedPath(resolvedSource);
    if (bakedPath.empty()) return false;
    std::error_code ec;
    if (!std::filesystem::exists(bakedPath, ec)) return false;

    std::ifstream in(bakedPath, std::ios::binary);
    if (!in) return false;
    auto rd = [&](void* p, usize n) { in.read(reinterpret_cast<char*>(p), static_cast<std::streamsize>(n)); };

    u32 magic = 0, version = 0;
    rd(&magic, 4); rd(&version, 4);
    if (!in || magic != kBakedMagic || version != kBakedVersion) return false;

    i64 storedMtime = 0; u64 storedSize = 0;
    rd(&storedMtime, 8); rd(&storedSize, 8);
    if (!in) return false;
    // Invalidate when the source is present but has changed (size or mtime). When the
    // source is missing we accept the baked copy — the geometry is still valid.
    if (std::filesystem::exists(resolvedSource, ec)) {
        u64 curSize = static_cast<u64>(std::filesystem::file_size(resolvedSource, ec));
        i64 curMtime = static_cast<i64>(std::filesystem::last_write_time(resolvedSource, ec).time_since_epoch().count());
        if (ec || curSize != storedSize || curMtime != storedMtime) return false;
    }

    u32 meshCount = 0; rd(&meshCount, 4);
    if (!in || meshCount > 100000u) return false;

    CachedFile tmp;   // fill locally; only commit on full success
    for (u32 m = 0; m < meshCount; ++m) {
        i32 meshIndex = 0; u64 hash = 0, vcount = 0, icount = 0; u32 smcount = 0;
        rd(&meshIndex, 4); rd(&hash, 8); rd(&vcount, 8);
        if (!in || vcount > 100000000ull) return false;
        CachedMesh cm; cm.contentHash = hash;
        cm.vertices.resize(static_cast<usize>(vcount));
        if (vcount) rd(cm.vertices.data(), static_cast<usize>(vcount) * sizeof(ECS::MeshComponent::Vertex));
        rd(&icount, 8);
        if (!in || icount > 500000000ull) return false;
        cm.indices.resize(static_cast<usize>(icount));
        if (icount) rd(cm.indices.data(), static_cast<usize>(icount) * sizeof(u32));
        if (!in) return false;
        // Self-validate: the stored hash is ComputeContentHash(verts, indices) from import.
        // A mismatch means the file is corrupt or the format drifted — reject the whole
        // baked file so LoadFile falls back to a clean re-import. The cache can never serve
        // wrong geometry.
        if (ECS::MeshComponent::ComputeContentHash(cm.vertices, cm.indices) != hash) return false;
        rd(&smcount, 4);
        if (!in || smcount > 100000u) return false;
        cm.subMeshes.resize(smcount);
        for (u32 s = 0; s < smcount; ++s) {
            auto& sm = cm.subMeshes[s];
            rd(&sm.indexOffset, 4); rd(&sm.indexCount, 4); rd(&sm.materialSlot, 4);
            u32 nlen = 0; rd(&nlen, 4);
            if (!in || nlen > 4096u) return false;
            sm.name.resize(nlen);
            if (nlen) rd(sm.name.data(), nlen);
        }
        if (!in) return false;
        tmp.byMeshIndex[meshIndex] = std::move(cm);
    }
    if (!in) return false;

    tmp.loaded = true;
    out.byMeshIndex = std::move(tmp.byMeshIndex);
    ENJIN_LOG_INFO(Asset, "MeshAssetCache: loaded baked '%s' (%zu mesh(es))",
                   resolvedSource.c_str(), out.byMeshIndex.size());
    return true;
}

void MeshAssetCache::WriteBaked(const std::string& resolvedSource, const CachedFile& file) const {
    const std::string bakedPath = BakedPath(resolvedSource);
    if (bakedPath.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(bakedPath).parent_path(), ec);

    std::ofstream out(bakedPath, std::ios::binary | std::ios::trunc);
    if (!out) return;
    auto wr = [&](const void* p, usize n) { out.write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(n)); };

    u32 magic = kBakedMagic, version = kBakedVersion;
    wr(&magic, 4); wr(&version, 4);

    i64 mtime = 0; u64 size = 0;
    if (std::filesystem::exists(resolvedSource, ec)) {
        size = static_cast<u64>(std::filesystem::file_size(resolvedSource, ec));
        mtime = static_cast<i64>(std::filesystem::last_write_time(resolvedSource, ec).time_since_epoch().count());
    }
    wr(&mtime, 8); wr(&size, 8);

    u32 meshCount = static_cast<u32>(file.byMeshIndex.size());
    wr(&meshCount, 4);
    for (const auto& [idx, cm] : file.byMeshIndex) {
        i32 meshIndex = idx; u64 hash = cm.contentHash;
        u64 vcount = cm.vertices.size(), icount = cm.indices.size();
        wr(&meshIndex, 4); wr(&hash, 8); wr(&vcount, 8);
        if (vcount) wr(cm.vertices.data(), static_cast<usize>(vcount) * sizeof(ECS::MeshComponent::Vertex));
        wr(&icount, 8);
        if (icount) wr(cm.indices.data(), static_cast<usize>(icount) * sizeof(u32));
        u32 smcount = static_cast<u32>(cm.subMeshes.size());
        wr(&smcount, 4);
        for (const auto& sm : cm.subMeshes) {
            wr(&sm.indexOffset, 4); wr(&sm.indexCount, 4); wr(&sm.materialSlot, 4);
            u32 nlen = static_cast<u32>(sm.name.size());
            wr(&nlen, 4);
            if (nlen) wr(sm.name.data(), nlen);
        }
    }
}

const MeshAssetCache::CachedMesh* MeshAssetCache::Find(const ECS::MeshComponent::SourceRef& ref,
                                                       bool logMismatch) {
    if (!ref.Valid()) return nullptr;

    CachedFile& file = LoadFile(ref.sourcePath);
    auto it = file.byMeshIndex.find(ref.meshIndex);
    if (it == file.byMeshIndex.end()) {
        if (logMismatch) {
            ENJIN_LOG_ERROR(Asset, "MeshAssetCache: mesh index %d not found in '%s'",
                            ref.meshIndex, ref.sourcePath.c_str());
        }
        return nullptr;
    }

    const CachedMesh& cm = it->second;
    if (ref.contentHash != 0 && cm.contentHash != ref.contentHash) {
        if (logMismatch) {
            ENJIN_LOG_WARN(Asset,
                "MeshAssetCache: '%s' mesh %d content hash mismatch (source changed on disk "
                "or different import options); reference not applied",
                ref.sourcePath.c_str(), ref.meshIndex);
        }
        return nullptr;
    }
    return &cm;
}

bool MeshAssetCache::CanResolve(const ECS::MeshComponent::SourceRef& ref) {
    // Quiet check (no logging) — a "can't resolve" here just means the saver keeps
    // inline geometry, which is not an error worth spamming the log for.
    return Find(ref, /*logMismatch=*/false) != nullptr;
}

bool MeshAssetCache::Resolve(const ECS::MeshComponent::SourceRef& ref, ECS::MeshComponent& out) {
    const CachedMesh* cm = Find(ref, /*logMismatch=*/true);
    if (!cm) return false;

    out.vertices = cm->vertices;
    out.indices = cm->indices;
    if (out.subMeshes.empty()) out.subMeshes = cm->subMeshes;
    return true;
}

} // namespace Assets
} // namespace Enjin
