// Validates the QEM mesh simplifier and LOD auto-generation. Regression coverage
// for three import/LOD bugs (2026-08-17): decimation that destroyed shape, a LOD
// level generated with 0 triangles (invisible), and the missing stable source
// extent that fed the LOD-selection feedback loop.

#include "EnjinTest.h"
#include "Enjin/Renderer/MeshSimplifier.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/LOD.h"

#include <cmath>

using namespace Enjin;
using Simp = Renderer::MeshSimplifier;
using Fac  = Renderer::MeshFactory;

namespace {
// Largest AABB extent of a mesh (0 if empty).
f32 MaxExtent(const ECS::MeshComponent& m) {
    if (m.vertices.empty()) return 0.0f;
    Math::Vector3 lo = m.vertices[0].position, hi = lo;
    for (const auto& v : m.vertices) {
        lo.x = std::min(lo.x, v.position.x); lo.y = std::min(lo.y, v.position.y); lo.z = std::min(lo.z, v.position.z);
        hi.x = std::max(hi.x, v.position.x); hi.y = std::max(hi.y, v.position.y); hi.z = std::max(hi.z, v.position.z);
    }
    Math::Vector3 e = hi - lo;
    return std::max(std::max(e.x, e.y), e.z);
}

bool AllTrianglesValid(const ECS::MeshComponent& m) {
    if (m.indices.size() % 3 != 0) return false;
    for (u32 i : m.indices) if (i >= m.vertices.size()) return false;
    for (size_t t = 0; t < m.indices.size(); t += 3) {
        u32 a = m.indices[t], b = m.indices[t+1], c = m.indices[t+2];
        if (a == b || b == c || a == c) return false; // degenerate survived
    }
    for (const auto& v : m.vertices) {
        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) || !std::isfinite(v.position.z))
            return false;
    }
    return true;
}
} // namespace

ENJIN_TEST(MeshSimplifier, simplify_halfratio_reduces_triangle_count) {
    // Arrange
    ECS::MeshComponent sphere = Fac::CreateSphere(0.5f, 64, 32);
    size_t sourceTris = sphere.indices.size() / 3;

    // Act
    ECS::MeshComponent lod = Simp::Simplify(sphere, 0.5f);

    // Assert
    size_t lodTris = lod.indices.size() / 3;
    ENJIN_ASSERT_TRUE(lodTris > 0);
    ENJIN_ASSERT_TRUE(lodTris < sourceTris);
    ENJIN_ASSERT_TRUE(AllTrianglesValid(lod));
}

ENJIN_TEST(MeshSimplifier, simplify_more_aggressive_yields_fewer_triangles) {
    // Arrange
    ECS::MeshComponent sphere = Fac::CreateSphere(0.5f, 64, 32);

    // Act
    ECS::MeshComponent half    = Simp::Simplify(sphere, 0.5f);
    ECS::MeshComponent quarter = Simp::Simplify(sphere, 0.25f);

    // Assert
    ENJIN_ASSERT_TRUE(quarter.indices.size() < half.indices.size());
    ENJIN_ASSERT_TRUE(!quarter.indices.empty());
    ENJIN_ASSERT_TRUE(AllTrianglesValid(quarter));
}

ENJIN_TEST(MeshSimplifier, simplify_preserves_overall_shape_within_tolerance) {
    // Arrange — a unit-diameter sphere; QEM must keep the collapsed points near the
    // surface, so the bounding extent should barely move (the old edge-midpoint
    // collapse shrank the shape inward).
    ECS::MeshComponent sphere = Fac::CreateSphere(0.5f, 64, 32);
    f32 sourceExtent = MaxExtent(sphere);

    // Act
    ECS::MeshComponent lod = Simp::Simplify(sphere, 0.2f);

    // Assert
    f32 lodExtent = MaxExtent(lod);
    ENJIN_ASSERT_TRUE(lodExtent > sourceExtent * 0.85f);
    ENJIN_ASSERT_TRUE(lodExtent < sourceExtent * 1.05f);
}

ENJIN_TEST(MeshSimplifier, generate_lods_never_emits_empty_level) {
    // Arrange
    ECS::MeshComponent sphere = Fac::CreateSphere(0.5f, 48, 24);
    ECS::LODComponent lod;

    // Act
    Simp::GenerateLODs(sphere, lod);

    // Assert — every generated level renders something (regression: LOD 4 came out
    // with 0 triangles on a bad mesh and vanished).
    ENJIN_ASSERT_TRUE(lod.levelCount >= 1);
    for (i32 i = 0; i < lod.levelCount; ++i) {
        ENJIN_ASSERT_TRUE(lod.levels[i].triangleCount > 0);
    }
}

ENJIN_TEST(MeshSimplifier, generate_lods_sets_stable_source_extent) {
    // Arrange
    ECS::MeshComponent sphere = Fac::CreateSphere(0.5f, 48, 24);
    ECS::LODComponent lod;

    // Act
    Simp::GenerateLODs(sphere, lod);

    // Assert — the stable extent must be populated so LOD selection doesn't read the
    // swapped mesh's bounds (regression: that feedback loop chugged to a crash).
    ENJIN_ASSERT_TRUE(lod.sourceMaxExtent > 0.0f);
    ENJIN_ASSERT_TRUE(lod.sourceMaxExtent > MaxExtent(sphere) * 0.9f);
}

ENJIN_TEST_MAIN()
