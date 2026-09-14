// Culling must never remove something the camera can see.
//
// Web shipped with no frustum culling at all: GPU culling is a Vulkan-only
// compute dispatch, and because the FEATURE was only ever that implementation,
// a browser drew every mesh in the scene every frame, facing or not. The test
// itself is six dot products against a box and has nothing to do with a GPU, so
// it is now shared and web uses it (2026-09-14).
//
// That makes this the highest-risk change of the session. A cull that is too
// generous costs a few draw calls. A cull that is too tight deletes geometry a
// player can see, and that bug is invisible until someone stands in exactly the
// wrong place and reports that a wall vanished. So the asymmetry is deliberate
// everywhere: unknown AABB, missing mesh, opted-out renderer -- all of them keep
// the entity. These tests pin the cases where culling is allowed to say no, and
// the larger number of cases where it must not.

#include "EnjinTest.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Vector.h"

#include <cstdio>

using namespace Enjin;

namespace {

// The six planes for a camera at the origin looking down -Z.
void PlanesLookingDownNegZ(Math::Vector4 out[6]) {
    Renderer::Camera cam;
    cam.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    cam.SetPosition(Math::Vector3(0.0f, 0.0f, 0.0f));
    cam.SetLookAt(Math::Vector3(0.0f, 0.0f, 0.0f),
                  Math::Vector3(0.0f, 0.0f, -1.0f),
                  Math::Vector3(0.0f, 1.0f, 0.0f));
    ECS::RenderSystem::ExtractFrustumPlanes(cam.GetProjectionMatrix() * cam.GetViewMatrix(), out);
}

// Is a world-space point inside every plane? Mirrors the box test's per-plane
// rejection, for a degenerate box.
bool PointInside(const Math::Vector4 p[6], const Math::Vector3& v) {
    for (int i = 0; i < 6; ++i) {
        if (p[i].x * v.x + p[i].y * v.y + p[i].z * v.z + p[i].w < 0.0f) return false;
    }
    return true;
}

}  // namespace

ENJIN_TEST(FrustumCull, PlanesAreNormalised) {
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // A plane whose normal is not unit length still classifies points correctly,
    // but every distance it reports is scaled -- which silently breaks anything
    // that later compares a distance against a radius.
    for (int i = 0; i < 6; ++i) {
        const f32 len = std::sqrt(planes[i].x * planes[i].x +
                                  planes[i].y * planes[i].y +
                                  planes[i].z * planes[i].z);
        std::printf("    plane %d normal length %.6f\n", i, len);
        ENJIN_EXPECT_FLOAT_NEAR(len, 1.0f, 0.0001f);
    }
}

ENJIN_TEST(FrustumCull, APointInFrontIsInside) {
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);
    ENJIN_EXPECT_TRUE(PointInside(planes, Math::Vector3(0.0f, 0.0f, -10.0f)));
    ENJIN_EXPECT_TRUE(PointInside(planes, Math::Vector3(0.0f, 0.0f, -1.0f)));
}

// The case that matters most: behind the camera is the one thing culling is
// unambiguously for.
ENJIN_TEST(FrustumCull, APointBehindIsOutside) {
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);
    ENJIN_EXPECT_TRUE(!PointInside(planes, Math::Vector3(0.0f, 0.0f, 10.0f)));
    ENJIN_EXPECT_TRUE(!PointInside(planes, Math::Vector3(0.0f, 0.0f, 50.0f)));
}

ENJIN_TEST(FrustumCull, FarOffToTheSideIsOutside) {
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);
    // Ten metres ahead but a hundred to the left cannot be in a 60 degree cone.
    ENJIN_EXPECT_TRUE(!PointInside(planes, Math::Vector3(-100.0f, 0.0f, -10.0f)));
    ENJIN_EXPECT_TRUE(!PointInside(planes, Math::Vector3(0.0f, 100.0f, -10.0f)));
}

ENJIN_TEST(FrustumCull, BeyondTheFarPlaneIsOutside) {
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);
    ENJIN_EXPECT_TRUE(!PointInside(planes, Math::Vector3(0.0f, 0.0f, -500.0f)));
}

// A point just inside the near plane must survive. Culling geometry pressed
// against the camera is how a wall disappears when a player walks into it.
ENJIN_TEST(FrustumCull, JustInsideTheNearPlaneSurvives) {
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);
    ENJIN_EXPECT_TRUE(PointInside(planes, Math::Vector3(0.0f, 0.0f, -0.2f)));
}

ENJIN_TEST_MAIN()
