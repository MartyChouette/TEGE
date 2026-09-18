// The ENTITY cull, as opposed to the point-vs-plane maths TestFrustumCull pins.
//
// IsEntityInFrustum is what actually decides whether a mesh is drawn, and its
// whole design is an asymmetry: a cull that is too generous costs a few draw
// calls, a cull that is too tight deletes geometry a player can see. The second
// is invisible until someone stands in exactly the wrong place and reports that
// a wall vanished, so every uncertain case has to KEEP the entity -- no mesh,
// an AABB that was never computed, an authored opt-out. Those rules were
// written down in comments and nothing tested them.
//
// No GPU: the render system is constructed with a null backend, which is safe
// because Shutdown early-returns when it was never initialised.
#include "EnjinTest.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/MeshRenderer.h"
#include "Enjin/Renderer/Camera.h"

using namespace Enjin;

namespace {

// Six planes for a camera at the origin looking down -Z, near 0.1, far 100.
void PlanesLookingDownNegZ(Math::Vector4 out[6]) {
    Renderer::Camera cam;
    cam.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    cam.SetPosition(Math::Vector3(0.0f, 0.0f, 0.0f));
    cam.SetLookAt(Math::Vector3(0.0f, 0.0f, 0.0f),
                  Math::Vector3(0.0f, 0.0f, -1.0f),
                  Math::Vector3(0.0f, 1.0f, 0.0f));
    ECS::RenderSystem::ExtractFrustumPlanes(cam.GetProjectionMatrix() * cam.GetViewMatrix(), out);
}

// A unit-cube mesh with its AABB already computed, at a world position.
ECS::Entity MakeBox(ECS::World& world, const Math::Vector3& at, f32 half = 0.5f) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = at;
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::MeshComponent mesh;
    mesh.cachedAABBMin = Math::Vector3(-half, -half, -half);
    mesh.cachedAABBMax = Math::Vector3(half, half, half);
    mesh.aabbDirty = false;
    world.AddComponent<ECS::MeshComponent>(e, mesh);
    return e;
}

} // namespace

ENJIN_TEST(EntityFrustumCull, test_a_box_in_front_of_the_camera_is_kept) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeBox(world, Math::Vector3(0.0f, 0.0f, -10.0f));
    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act
    const bool visible = rs.IsEntityInFrustum(e, planes);

    // Assert
    ENJIN_EXPECT_TRUE(visible);
}

ENJIN_TEST(EntityFrustumCull, test_a_box_behind_the_camera_is_culled) {
    // The one case culling exists for. If this passes nothing else, the
    // feature does nothing.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeBox(world, Math::Vector3(0.0f, 0.0f, 50.0f));
    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act
    const bool visible = rs.IsEntityInFrustum(e, planes);

    // Assert
    ENJIN_EXPECT_FALSE(visible);
}

ENJIN_TEST(EntityFrustumCull, test_a_box_straddling_a_plane_is_kept) {
    // The dangerous direction. A box whose centre is outside but whose corner
    // is not must survive, or geometry pops at the edge of the screen as the
    // camera turns.
    // Arrange
    ECS::World world;
    // Centre behind the near plane, but a large box, so it reaches in front.
    ECS::Entity e = MakeBox(world, Math::Vector3(0.0f, 0.0f, 2.0f), 8.0f);
    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act
    const bool visible = rs.IsEntityInFrustum(e, planes);

    // Assert
    ENJIN_EXPECT_TRUE(visible);
}

ENJIN_TEST(EntityFrustumCull, test_an_entity_with_no_mesh_is_kept) {
    // Never cull on ignorance: there is nothing to measure, and guessing at a
    // box is how a cull becomes too tight.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act + Assert
    ENJIN_EXPECT_TRUE(rs.IsEntityInFrustum(e, planes));
}

ENJIN_TEST(EntityFrustumCull, test_an_uncomputed_aabb_is_kept_even_when_far_outside) {
    // min > max is how the engine signals "this box was never computed". An
    // entity in that state sits at a position that WOULD be culled, and must
    // not be: the AABB is unknown, not empty.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = Math::Vector3(0.0f, 0.0f, 500.0f);   // far behind the camera
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::MeshComponent mesh;                            // defaults signal dirty
    mesh.cachedAABBMin = Math::Vector3(1.0f, 1.0f, 1.0f);
    mesh.cachedAABBMax = Math::Vector3(-1.0f, -1.0f, -1.0f);
    world.AddComponent<ECS::MeshComponent>(e, mesh);

    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act + Assert
    ENJIN_EXPECT_TRUE(rs.IsEntityInFrustum(e, planes));
}

ENJIN_TEST(EntityFrustumCull, test_the_authored_opt_out_survives_being_behind_the_camera) {
    // Skyboxes, viewmodels and view-anchored effects must draw wherever the
    // camera looks. frustumCull = false is that opt-out, and it has to beat a
    // box that is unambiguously outside.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeBox(world, Math::Vector3(0.0f, 0.0f, 50.0f));
    ECS::MeshRendererComponent mr;
    mr.frustumCull = false;
    world.AddComponent<ECS::MeshRendererComponent>(e, mr);

    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act + Assert
    //
    // This is the assertion that found the bug: the desktop RefreshStorageCache
    // nulled m_CachedMeshRendererStorage and never assigned it, so the opt-out
    // was read through a permanently null cache on Vulkan and honoured on web.
    ENJIN_EXPECT_TRUE(rs.IsEntityInFrustum(e, planes));
}

ENJIN_TEST(EntityFrustumCull, test_a_default_mesh_renderer_still_culls) {
    // The other half of the opt-out: having a MeshRenderer at all must not
    // disable culling, or adding one to a scene quietly doubles its draw calls.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeBox(world, Math::Vector3(0.0f, 0.0f, 50.0f));
    world.AddComponent<ECS::MeshRendererComponent>(e);   // frustumCull defaults true

    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act + Assert
    ENJIN_EXPECT_FALSE(rs.IsEntityInFrustum(e, planes));
}

ENJIN_TEST(EntityFrustumCull, test_scale_is_taken_from_the_world_matrix) {
    // The AABB is in LOCAL space and the test transforms its corners. A small
    // box scaled up until it reaches the camera must come back visible, or
    // scaled geometry is culled while it is on screen.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeBox(world, Math::Vector3(0.0f, 0.0f, 30.0f), 0.5f);
    auto* xf = world.GetComponent<ECS::TransformComponent>(e);
    xf->scale = Math::Vector3(100.0f, 100.0f, 100.0f);   // reaches well in front

    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act + Assert
    ENJIN_EXPECT_TRUE(rs.IsEntityInFrustum(e, planes));
}

ENJIN_TEST(EntityFrustumCull, test_a_box_beyond_the_far_plane_is_culled) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeBox(world, Math::Vector3(0.0f, 0.0f, -500.0f));
    ECS::RenderSystem rs(&world, nullptr);
    rs.RefreshStorageCache();
    Math::Vector4 planes[6];
    PlanesLookingDownNegZ(planes);

    // Act + Assert
    ENJIN_EXPECT_FALSE(rs.IsEntityInFrustum(e, planes));
}

ENJIN_TEST_MAIN()
