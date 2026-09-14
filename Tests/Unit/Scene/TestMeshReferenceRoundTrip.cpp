// A mesh reference and the baked copy it replaced must render identically.
//
// Reported from Gobliny, 2026-09-13. SceneSerializer drops a mesh's inline
// vertices and writes a `source` reference whenever MeshAssetCache::CanResolve
// says yes. What came back was different geometry -- 62 times larger -- so a
// scene that saved correctly reloaded wrong, with no error, no warning, and a
// correct-looking scene file.
//
// Two things made it possible and both are fixed:
//
//   - MeshAssetCache answers a reference by RE-IMPORTING the source with DEFAULT
//     ImportOptions, on the stated assumption that geometry depends only on the
//     file, the axis conversion and the skinned-vertex bake. ImportOptions also
//     has `scale`, `normalizeScale`, `convertAxes` and three axis flips, and
//     SourceRef records only the two axis booleans. Anything not imported with
//     the defaults came back as something else.
//   - The content hash would have caught that, except `Find` skipped the
//     comparison when the reference carried 0. The project had come to depend on
//     that bypass to make references resolve at all.
//
// The report said a round trip -- save with a resolvable mesh, reload, compare
// bounds -- would have failed on the first run. It would. So here it is.

#include "EnjinTest.h"
#include "Enjin/Assets/MeshAssetCache.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>

using namespace Enjin;

namespace {

namespace fs = std::filesystem;

// A cube whose size is a parameter, so "the geometry changed" is visible as a
// number rather than as a checksum.
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

Math::Vector3 BoundsOf(const ECS::MeshComponent& mc) {
    if (mc.vertices.empty()) return Math::Vector3(0.0f, 0.0f, 0.0f);
    Math::Vector3 lo = mc.vertices[0].position, hi = lo;
    for (const auto& v : mc.vertices) {
        lo.x = std::min(lo.x, v.position.x); hi.x = std::max(hi.x, v.position.x);
        lo.y = std::min(lo.y, v.position.y); hi.y = std::max(hi.y, v.position.y);
        lo.z = std::min(lo.z, v.position.z); hi.z = std::max(hi.z, v.position.z);
    }
    return Math::Vector3(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
}

// FNV-1a over the vertex+index bytes, which is what SourceRef::contentHash is.
u64 HashGeometry(const ECS::MeshComponent& mc) {
    u64 hash = 1469598103934665603ull;
    auto feed = [&](const void* p, usize n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (usize i = 0; i < n; ++i) { hash ^= b[i]; hash *= 1099511628211ull; }
    };
    if (!mc.vertices.empty()) feed(mc.vertices.data(), mc.vertices.size() * sizeof(ECS::MeshComponent::Vertex));
    if (!mc.indices.empty())  feed(mc.indices.data(),  mc.indices.size() * sizeof(u32));
    return hash;
}

struct CacheScope {
    fs::path dir;
    explicit CacheScope(const char* leaf) {
        dir = fs::temp_directory_path() / "enjin_meshref" / leaf;
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

// THE ONE THAT MATTERS. Save a scene whose mesh can be written as a reference,
// reload it, and compare the geometry that comes back.
ENJIN_TEST(MeshReferenceRoundTrip, AReferencedMeshReloadsWithTheSameGeometry) {
    CacheScope cache("roundtrip");

    // A mesh that is 2 units across, carrying a source ref the way an imported
    // mesh does. Nothing on disk to re-import from -- which is the point: before
    // Adopt, the ONLY way to answer this reference was a re-import, so a source
    // that cannot be re-imported identically had no correct answer at all.
    ECS::World world;
    const ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    world.AddComponent<ECS::NameComponent>(e).name = "Goblin";

    ECS::MeshComponent mc = MakeBox(1.0f);
    mc.source.sourcePath = "assets/goblin.fbx";
    mc.source.meshIndex = 1;
    mc.source.contentHash = HashGeometry(mc);
    const Math::Vector3 before = BoundsOf(mc);
    world.AddComponent<ECS::MeshComponent>(e, mc);

    Scene::SceneSerializer saver(&world);
    Scene::SerializationOptions opts;
    opts.useMeshReferences = true;   // the mode the bug lives in
    const std::string text = saver.SaveToString(opts);

    const bool wroteReference = text.find("\"vertices\"") == std::string::npos;
    std::printf("    saved as a reference: %s (%zu bytes)\n",
                wroteReference ? "yes" : "no", text.size());

    ECS::World reloaded;
    Scene::SceneSerializer loader(&reloaded);
    const auto result = loader.LoadFromString(text);
    ENJIN_ASSERT_TRUE(result.success);

    ECS::Entity found = ECS::INVALID_ENTITY;
    for (ECS::Entity re : result.entities) {
        if (reloaded.HasComponent<ECS::MeshComponent>(re)) { found = re; break; }
    }
    ENJIN_ASSERT_TRUE(found != ECS::INVALID_ENTITY);

    auto* back = reloaded.GetComponent<ECS::MeshComponent>(found);
    ENJIN_ASSERT_NOT_NULL(back);
    Assets::MeshAssetCache::Get().EnsureCpuData(*back);

    const Math::Vector3 after = BoundsOf(*back);
    std::printf("    bounds before %.4f x %.4f x %.4f\n", before.x, before.y, before.z);
    std::printf("    bounds after  %.4f x %.4f x %.4f\n", after.x, after.y, after.z);

    // The whole bug in one line. It came back 62x in the reported case.
    ENJIN_EXPECT_FLOAT_NEAR(after.x, before.x, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(after.y, before.y, 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(after.z, before.z, 0.0001f);
    ENJIN_EXPECT_EQ(back->vertices.size(), mc.vertices.size());
}

// REMOVED: a hashless-reference test that passed either way.
//
// It asserted that CanResolve refuses a reference carrying contentHash 0. It
// does refuse one -- but the test still printed "resolvable: no" with the old
// bypass deliberately restored and the engine rebuilt, so something earlier in
// Find was refusing it first and the assertion was measuring that instead. A
// green test that would not have gone red is worth less than no test, because
// it is claimed as coverage.
//
// The behaviour is real and is exercised by the round trip above and the drift
// case below. What is missing is a test that isolates the hash-0 path
// specifically, and it needs someone to find out what refuses it first rather
// than to assert around it.

// Geometry that disagrees with the recorded hash must not be served. This is the
// check that was switched off, and it is what turns a wrong-size model into a
// larger scene file instead.
ENJIN_TEST(MeshReferenceRoundTrip, DriftedGeometryIsRefusedRatherThanServed) {
    CacheScope cache("drift");

    ECS::MeshComponent small = MakeBox(1.0f);
    small.source.sourcePath = "assets/goblin.fbx";
    small.source.meshIndex = 1;
    small.source.contentHash = HashGeometry(small);

    // The cache holds a 62x version -- exactly the reported failure.
    ECS::MeshComponent big = MakeBox(62.0f);
    big.source = small.source;
    big.source.contentHash = HashGeometry(big);
    ENJIN_ASSERT_TRUE(Assets::MeshAssetCache::Get().Adopt(big.source, big));

    // The scene's reference names the small geometry. It must not be handed the
    // big one.
    ENJIN_EXPECT_TRUE(!Assets::MeshAssetCache::Get().CanResolve(small.source));
}

ENJIN_TEST_MAIN()
