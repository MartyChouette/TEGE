// N references to one mesh must cost ONE CPU copy of its geometry, not N.
//
// Found on web, 2026-09-30. Twister's corn field is 7344 entities referencing
// one 1456-vertex mesh. The scene loader answered every reference with
// MeshAssetCache::Resolve, which COPIES the vertices into the component, so the
// field asked for 7344 x 1456 x 136 bytes = 1.45 GB against a 512 MB wasm heap.
// operator new threw, nothing caught it, and the page aborted before its first
// frame with no message beyond "Aborted()".
//
// ResolveDeferred answers a reference without the copy: bounds and submeshes
// filled, vertices left in the cache, cpuDeferred set. These tests hold it to
// that. The decision to USE it at load is web-only (SceneSerializer's
// DefersMeshCpu), so it is not exercised here; what is tested is the part every
// platform compiles.

#include "EnjinTest.h"
#include "Enjin/Assets/MeshAssetCache.h"
#include "Enjin/ECS/Components/Mesh.h"

#include <cstdio>
#include <filesystem>
#include <vector>

using namespace Enjin;

namespace {

namespace fs = std::filesystem;

ECS::MeshComponent MakeBox(f32 halfExtent) {
    ECS::MeshComponent mc;
    const f32 h = halfExtent;
    const Math::Vector3 corners[8] = {
        {-h,-h,-h}, { h,-h,-h}, { h, h,-h}, {-h, h,-h},
        {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h},
    };
    for (const auto& c : corners) {
        ECS::MeshComponent::Vertex v;
        v.position = c;
        v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
        mc.vertices.push_back(v);
    }
    const u32 idx[36] = {0,1,2, 0,2,3, 4,5,6, 4,6,7, 0,4,7, 0,7,3,
                         1,5,6, 1,6,2, 3,2,6, 3,6,7, 0,1,5, 0,5,4};
    for (u32 i : idx) mc.indices.push_back(i);
    return mc;
}

// A mesh carrying a source reference the way an imported one does, adopted
// into the cache so the reference resolves with nothing on disk.
ECS::MeshComponent AdoptBox(f32 halfExtent, const char* path) {
    ECS::MeshComponent mc = MakeBox(halfExtent);
    mc.source.sourcePath = path;
    mc.source.meshIndex = 0;
    mc.source.contentHash = ECS::MeshComponent::ComputeContentHash(mc.vertices, mc.indices);
    Assets::MeshAssetCache::Get().Adopt(mc.source, mc);
    return mc;
}

struct CacheScope {
    fs::path dir;
    explicit CacheScope(const char* leaf) {
        dir = fs::temp_directory_path() / "enjin_meshdefer" / leaf;
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        Assets::MeshAssetCache::Get().Clear();
        Assets::MeshAssetCache::Get().SetCacheDir(dir.string());
    }
    ~CacheScope() {
        Assets::MeshAssetCache::Get().Clear();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

}  // namespace

// THE ONE THAT MATTERS. Many references, one copy.
ENJIN_TEST(DeferredMeshShare, ManyReferencesHoldOneCpuCopy) {
    CacheScope cache("many");
    const ECS::MeshComponent src = AdoptBox(1.0f, "assets/corn.glb");

    const std::vector<ECS::MeshComponent::Vertex>* shared =
        Assets::MeshAssetCache::Get().PeekVertices(src.source);
    ENJIN_ASSERT_NOT_NULL(shared);
    ENJIN_ASSERT_EQ(shared->size(), src.vertices.size());

    constexpr int kStalks = 7344;
    std::vector<ECS::MeshComponent> field(kStalks);
    usize residentVertices = 0;
    for (auto& stalk : field) {
        stalk.source = src.source;
        ENJIN_ASSERT_TRUE(Assets::MeshAssetCache::Get().ResolveDeferred(stalk.source, stalk));
        residentVertices += stalk.vertices.size();
        // Every reference is answered by the same storage, not a copy of it.
        ENJIN_ASSERT_TRUE(Assets::MeshAssetCache::Get().PeekVertices(stalk.source) == shared);
    }
    std::printf("    %d references, %zu vertices held by components, %zu in the cache\n",
                kStalks, residentVertices, shared->size());

    // Before: 7344 x 8. After: none, and the cache's own 8.
    ENJIN_EXPECT_EQ(residentVertices, static_cast<usize>(0));
    ENJIN_EXPECT_TRUE(field[0].cpuDeferred);
    ENJIN_EXPECT_TRUE(!field[0].IsValid());
}

// A deferred mesh must still know its size: culling, LOD and probe bounds read
// the cached AABB, and a mesh with no vertices has no other way to say it.
ENJIN_TEST(DeferredMeshShare, BoundsArriveWithoutTheVertices) {
    CacheScope cache("bounds");
    const ECS::MeshComponent src = AdoptBox(2.5f, "assets/barrel.glb");

    ECS::MeshComponent mc;
    mc.source = src.source;
    ENJIN_ASSERT_TRUE(Assets::MeshAssetCache::Get().ResolveDeferred(mc.source, mc));
    ENJIN_EXPECT_TRUE(!mc.aabbDirty);
    ENJIN_EXPECT_FLOAT_NEAR(mc.cachedAABBMin.x, -2.5f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(mc.cachedAABBMax.y,  2.5f, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(mc.cachedAABBMax.z,  2.5f, 0.0001f);
}

// The escape hatch: anything that needs the vertices after all gets them back.
ENJIN_TEST(DeferredMeshShare, EnsureCpuDataBringsTheVerticesBack) {
    CacheScope cache("ensure");
    const ECS::MeshComponent src = AdoptBox(1.0f, "assets/rock.glb");

    ECS::MeshComponent mc;
    mc.source = src.source;
    ENJIN_ASSERT_TRUE(Assets::MeshAssetCache::Get().ResolveDeferred(mc.source, mc));
    ENJIN_ASSERT_TRUE(mc.vertices.empty());
    ENJIN_ASSERT_TRUE(Assets::MeshAssetCache::Get().EnsureCpuData(mc));
    ENJIN_EXPECT_EQ(mc.vertices.size(), src.vertices.size());
    ENJIN_EXPECT_EQ(mc.indices.size(), src.indices.size());
}

// A skinned mesh reads its bone data per entity, so it is refused and left
// untouched for the caller to resolve the ordinary way.
ENJIN_TEST(DeferredMeshShare, SkinnedMeshIsRefused) {
    CacheScope cache("skinned");
    ECS::MeshComponent skinned = MakeBox(1.0f);
    for (auto& v : skinned.vertices) v.boneWeights = Math::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    skinned.source.sourcePath = "assets/dog.glb";
    skinned.source.meshIndex = 0;
    skinned.source.contentHash = ECS::MeshComponent::ComputeContentHash(skinned.vertices, skinned.indices);
    ENJIN_ASSERT_TRUE(Assets::MeshAssetCache::Get().Adopt(skinned.source, skinned));

    ECS::MeshComponent mc;
    mc.source = skinned.source;
    ENJIN_EXPECT_TRUE(!Assets::MeshAssetCache::Get().ResolveDeferred(mc.source, mc));
    ENJIN_EXPECT_TRUE(!mc.cpuDeferred);
    ENJIN_EXPECT_TRUE(mc.aabbDirty);
}

// A reference nothing answers is refused, not deferred into an empty mesh that
// would then draw nothing and say nothing.
ENJIN_TEST(DeferredMeshShare, UnresolvableReferenceIsRefused) {
    CacheScope cache("missing");
    ECS::MeshComponent mc;
    mc.source.sourcePath = "assets/not_there.glb";
    mc.source.meshIndex = 0;
    mc.source.contentHash = 12345;
    ENJIN_EXPECT_TRUE(!Assets::MeshAssetCache::Get().ResolveDeferred(mc.source, mc));
    ENJIN_EXPECT_TRUE(!mc.cpuDeferred);
}

ENJIN_TEST_MAIN()
