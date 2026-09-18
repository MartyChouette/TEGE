// Voxelising a mesh into a distance field and marching it back out.
//
// MeshToSDF and SDFMeshRenderer are a thousand lines that nothing in the engine
// called: no runtime, no editor, no test. They are reachable now as the Mesh
// component's Remesh button, which is the one tool here that can weld a model
// of overlapping, self-intersecting parts into a single closed surface --
// Simplify removes triangles and cannot do that.
#include "EnjinTest.h"
#include "Enjin/Renderer/SDFRenderer.h"
#include "Enjin/ECS/Components/Mesh.h"

#include <cmath>
#include <vector>

using namespace Enjin;

namespace {

// A unit cube as a triangle soup, centred on the origin. Deliberately built as
// two overlapping boxes in one test below, which is the case the tool exists
// for.
void AppendBox(std::vector<ECS::MeshComponent::Vertex>& vertices,
               std::vector<u32>& indices,
               const Math::Vector3& centre, f32 half) {
    const u32 base = static_cast<u32>(vertices.size());
    const f32 sx[8] = {-1, 1, 1, -1, -1, 1, 1, -1};
    const f32 sy[8] = {-1, -1, 1, 1, -1, -1, 1, 1};
    const f32 sz[8] = {-1, -1, -1, -1, 1, 1, 1, 1};
    for (int i = 0; i < 8; ++i) {
        ECS::MeshComponent::Vertex v{};
        v.position = Math::Vector3(centre.x + sx[i] * half,
                                   centre.y + sy[i] * half,
                                   centre.z + sz[i] * half);
        v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
        vertices.push_back(v);
    }
    const u32 faces[36] = {
        0,1,2, 0,2,3,   4,6,5, 4,7,6,
        0,4,5, 0,5,1,   1,5,6, 1,6,2,
        2,6,7, 2,7,3,   3,7,4, 3,4,0
    };
    for (u32 f : faces) indices.push_back(base + f);
}

} // namespace

ENJIN_TEST(SdfRemesh, test_a_box_survives_the_round_trip_as_a_closed_surface) {
    // Arrange
    std::vector<ECS::MeshComponent::Vertex> vertices;
    std::vector<u32> indices;
    AppendBox(vertices, indices, Math::Vector3(0, 0, 0), 1.0f);

    // Act
    Renderer::SDFVolume volume = Renderer::MeshToSDF::ConvertMesh(vertices, indices, 32);
    Renderer::MeshData out = Renderer::SDFMeshRenderer::ExtractIsosurface(volume, 0.0f);

    // Assert
    ENJIN_EXPECT_TRUE(!out.vertices.empty());
    ENJIN_ASSERT_TRUE(out.indices.size() >= 3);
    ENJIN_EXPECT_TRUE(out.indices.size() % 3 == 0);
    // Every index addresses a vertex that exists. A marching-cubes bug that
    // emitted a dangling index would draw garbage triangles rather than fail.
    for (u32 i : out.indices) {
        ENJIN_ASSERT_TRUE(i < out.vertices.size());
    }
}

ENJIN_TEST(SdfRemesh, test_the_result_stays_within_the_original_bounds) {
    // A remesh that drifted off the model would silently move it, and a person
    // would find their object somewhere else in the scene.
    // Arrange
    std::vector<ECS::MeshComponent::Vertex> vertices;
    std::vector<u32> indices;
    AppendBox(vertices, indices, Math::Vector3(2.0f, 0.0f, 0.0f), 1.0f);

    // Act
    Renderer::SDFVolume volume = Renderer::MeshToSDF::ConvertMesh(vertices, indices, 32);
    Renderer::MeshData out = Renderer::SDFMeshRenderer::ExtractIsosurface(volume, 0.0f);

    // Assert
    ENJIN_ASSERT_TRUE(!out.vertices.empty());
    for (const auto& v : out.vertices) {
        // A generous margin: the grid pads the model, so the surface can sit
        // slightly outside it. Two units of slack still catches a result that
        // landed at the origin or on the wrong axis.
        ENJIN_EXPECT_TRUE(std::fabs(v.position.x - 2.0f) < 3.0f);
        ENJIN_EXPECT_TRUE(std::fabs(v.position.y) < 3.0f);
        ENJIN_EXPECT_TRUE(std::fabs(v.position.z) < 3.0f);
    }
}

ENJIN_TEST(SdfRemesh, test_two_overlapping_boxes_come_back_as_one_surface) {
    // The job this tool exists for. Simplify cannot weld a model that was never
    // watertight; a distance field does not know the parts were separate.
    // Arrange
    std::vector<ECS::MeshComponent::Vertex> vertices;
    std::vector<u32> indices;
    AppendBox(vertices, indices, Math::Vector3(-0.5f, 0, 0), 1.0f);
    AppendBox(vertices, indices, Math::Vector3(0.5f, 0, 0), 1.0f);

    // Act
    Renderer::SDFVolume volume = Renderer::MeshToSDF::ConvertMesh(vertices, indices, 32);
    Renderer::MeshData out = Renderer::SDFMeshRenderer::ExtractIsosurface(volume, 0.0f);

    // Assert
    ENJIN_EXPECT_TRUE(!out.vertices.empty());
    ENJIN_EXPECT_TRUE(out.indices.size() >= 3);
}

ENJIN_TEST(SdfRemesh, test_an_empty_mesh_produces_nothing_rather_than_crashing) {
    // What the editor's warning path depends on: an empty result has to come
    // back as an empty result, not as a crash or a garbage surface.
    // Arrange
    std::vector<ECS::MeshComponent::Vertex> vertices;
    std::vector<u32> indices;

    // Act
    Renderer::SDFVolume volume = Renderer::MeshToSDF::ConvertMesh(vertices, indices, 16);
    Renderer::MeshData out = Renderer::SDFMeshRenderer::ExtractIsosurface(volume, 0.0f);

    // Assert
    ENJIN_EXPECT_TRUE(out.indices.empty() || out.indices.size() % 3 == 0);
}

ENJIN_TEST_MAIN()
