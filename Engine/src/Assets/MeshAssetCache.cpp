#include "Enjin/Assets/MeshAssetCache.h"
#include "Enjin/Assets/SceneImporter.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Logging/Log.h"
#include <filesystem>

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
    return file;
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
