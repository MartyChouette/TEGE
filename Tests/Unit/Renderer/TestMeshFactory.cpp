// Validates every MeshFactory primitive the editor can create. Triggered by a
// report that adding a Pyramid crashed a project. Checks each mesh is non-empty,
// triangulated, has all indices in range, and has finite positions/normals --
// the conditions that would make the GPU upload or render path fault.

#include "EnjinTest.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/ECS/Components/Mesh.h"

#include <cmath>

using namespace Enjin;
using R = Renderer::MeshFactory;

namespace {
void Validate(const ECS::MeshComponent& m) {
    ENJIN_ASSERT_TRUE(!m.vertices.empty());
    ENJIN_ASSERT_TRUE(!m.indices.empty());
    ENJIN_EXPECT_EQ(m.indices.size() % 3, (size_t)0);  // triangulated

    // Every index must reference a real vertex (out-of-range = GPU crash).
    bool inRange = true;
    for (u32 idx : m.indices) {
        if (idx >= m.vertices.size()) { inRange = false; break; }
    }
    ENJIN_EXPECT_TRUE(inRange);

    // No NaN/Inf in positions or normals.
    bool finite = true;
    for (const auto& v : m.vertices) {
        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) || !std::isfinite(v.position.z) ||
            !std::isfinite(v.normal.x)   || !std::isfinite(v.normal.y)   || !std::isfinite(v.normal.z)) {
            finite = false; break;
        }
    }
    ENJIN_EXPECT_TRUE(finite);
}
} // namespace

// --- 3D primitives (Entity > 3D Object menu) ---
ENJIN_TEST(Primitives, Cube)     { Validate(R::CreateCube(1.0f)); }
ENJIN_TEST(Primitives, Sphere)   { Validate(R::CreateSphere(0.5f)); }
ENJIN_TEST(Primitives, Plane)    { Validate(R::CreatePlane(10.0f, 10.0f)); }
ENJIN_TEST(Primitives, Cylinder) { Validate(R::CreateCylinder(0.5f, 1.0f)); }
ENJIN_TEST(Primitives, Cone)     { Validate(R::CreateCone(0.5f, 1.0f)); }
ENJIN_TEST(Primitives, Capsule)  { Validate(R::CreateCapsule(0.3f, 1.0f)); }
ENJIN_TEST(Primitives, Pyramid)  { Validate(R::CreatePyramid(1.0f, 1.0f)); }

// --- 2D primitives (Entity > 2D Object menu) ---
ENJIN_TEST(Primitives, Triangle)  { Validate(R::CreateTriangle(1.0f)); }
ENJIN_TEST(Primitives, Quad)      { Validate(R::CreateQuad(1.0f, 1.0f)); }
ENJIN_TEST(Primitives, Capsule2D) { Validate(R::CreateCapsule2D(1.0f, 2.0f)); }

ENJIN_TEST_MAIN()
