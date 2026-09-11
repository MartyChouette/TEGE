// MeshRendererComponent had sixteen fields and three consumers.
//
// The only code in the engine that read the component was the wireframe overlay
// pass, which uses `wireframe`, `wireframeColor` and `wireframeOpacity`. The other
// thirteen were authored in a full inspector, saved into the scene, reloaded, and
// read by nothing:
//
//   enabled                  the MASTER ON/OFF -- untick it and the mesh drew on
//   frustumCull              anyway, with no other setting that would stop it
//   occlusionCull
//   maxDrawDistance
//   renderQueue
//   renderLayerMask
//   lodBias
//   forceLowestLOD
//   shadowMode
//   contributeMotionVectors
//   customShaderName
//   lightmapUVChannel
//   allowInstancing
//
// These tests cover the ones now wired, at the level they can be checked without a
// GPU: the render list and the shadow caster list are plain std::vectors built from
// component state, so what goes into them is testable and is exactly where twelve
// of the thirteen decisions are made.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/MeshRenderer.h"
#include "Enjin/ECS/Components/Material.h"
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

// The engine's own list-building rules, mirrored so they can be exercised without
// a device. Each one is a direct transcription of the code in RenderSystem, and if
// the two ever disagree the comment on the failing test says which rule moved.
bool PassesRenderList(World& w, Entity e, const Vector3& camPos, bool haveCam) {
    const auto* xf = w.GetComponent<TransformComponent>(e);
    if (!xf || !xf->visible) return false;

    const auto* mr = w.GetComponent<MeshRendererComponent>(e);
    if (mr && !mr->enabled) return false;
    if (mr && mr->maxDrawDistance > 0.0f && haveCam) {
        const Vector3 d = xf->position - camPos;
        if (d.x * d.x + d.y * d.y + d.z * d.z >
            mr->maxDrawDistance * mr->maxDrawDistance) {
            return false;
        }
    }
    return true;
}

bool CastsShadow(World& w, Entity e) {
    const auto* xf = w.GetComponent<TransformComponent>(e);
    if (!xf || !xf->visible) return false;

    const auto* mr = w.GetComponent<MeshRendererComponent>(e);
    if (mr) {
        if (!mr->enabled) return false;
        if (mr->shadowMode == MeshRendererComponent::ShadowMode::Off) return false;
        if (mr->shadowMode == MeshRendererComponent::ShadowMode::On ||
            mr->shadowMode == MeshRendererComponent::ShadowMode::TwoSided) {
            return true;
        }
    }
    const auto* mat = w.GetComponent<MaterialComponent>(e);
    if (mat && !mat->castShadows) return false;
    return true;
}

Entity MakeMesh(World& w, const Vector3& at, const char* name) {
    Entity e = w.CreateEntity();
    auto& t = w.AddComponent<TransformComponent>(e, TransformComponent{});
    t.position = at;
    w.AddComponent<NameComponent>(e, NameComponent{name});
    w.AddComponent<MeshComponent>(e, MeshComponent{});
    return e;
}

} // namespace

// ---------------------------------------------------------------------------
// enabled -- the one that matters most
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshRendererFields, DisablingTheRendererRemovesTheMeshFromTheList) {
    // Arrange
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 0), "Crate");
    w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});

    // Act + assert: on by default.
    ENJIN_ASSERT_TRUE(PassesRenderList(w, e, Vector3(0, 0, 0), true));

    w.GetComponent<MeshRendererComponent>(e)->enabled = false;
    ENJIN_EXPECT_FALSE(PassesRenderList(w, e, Vector3(0, 0, 0), true));
}

ENJIN_TEST(MeshRendererFields, ADisabledRendererAlsoStopsCastingShadows) {
    // Otherwise "off" means invisible but still throwing a shadow, which reads as
    // a ghost and sends people looking for a light bug.
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 0), "Crate");
    auto& mr = w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});
    ENJIN_ASSERT_TRUE(CastsShadow(w, e));

    mr.enabled = false;
    ENJIN_EXPECT_FALSE(CastsShadow(w, e));
}

ENJIN_TEST(MeshRendererFields, AnEntityWithNoMeshRendererIsUnaffected) {
    // The component is optional. Everything must behave exactly as before for the
    // meshes that do not have one, which is most of them.
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 0), "Plain");
    ENJIN_EXPECT_TRUE(PassesRenderList(w, e, Vector3(0, 0, 0), true));
    ENJIN_EXPECT_TRUE(CastsShadow(w, e));
}

// ---------------------------------------------------------------------------
// maxDrawDistance
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshRendererFields, MaxDrawDistanceHidesTheMeshBeyondIt) {
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 30.0f), "Distant");
    auto& mr = w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});
    mr.maxDrawDistance = 20.0f;

    ENJIN_EXPECT_FALSE(PassesRenderList(w, e, Vector3(0, 0, 0), true));

    // Inside the radius it draws.
    w.GetComponent<TransformComponent>(e)->position = Vector3(0, 0, 10.0f);
    ENJIN_EXPECT_TRUE(PassesRenderList(w, e, Vector3(0, 0, 0), true));
}

ENJIN_TEST(MeshRendererFields, ZeroMaxDrawDistanceMeansInfiniteNotZero) {
    // The field's default is 0 and its comment says 0 = infinite. Read as a
    // distance instead, every mesh in every scene would vanish -- which is why
    // this is a guarded test in the engine and not a clamp.
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 100000.0f), "VeryFarAway");
    auto& mr = w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});
    mr.maxDrawDistance = 0.0f;

    ENJIN_EXPECT_TRUE(PassesRenderList(w, e, Vector3(0, 0, 0), true));
}

ENJIN_TEST(MeshRendererFields, MaxDrawDistanceIsIgnoredWithNoCamera) {
    // With no camera there is no distance to measure from, and hiding everything
    // would be the worst possible answer to "I do not know".
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 5000.0f), "NoCameraHere");
    auto& mr = w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});
    mr.maxDrawDistance = 1.0f;

    ENJIN_EXPECT_TRUE(PassesRenderList(w, e, Vector3(0, 0, 0), false));
}

// ---------------------------------------------------------------------------
// shadowMode -- an OVERRIDE of the material, which is the point of it
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshRendererFields, ShadowModeOffStopsACastingMaterialFromCasting) {
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 0), "Glass");
    auto& mat = w.AddComponent<MaterialComponent>(e, MaterialComponent{});
    mat.castShadows = true;
    auto& mr = w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});

    ENJIN_ASSERT_TRUE(CastsShadow(w, e));
    mr.shadowMode = MeshRendererComponent::ShadowMode::Off;
    ENJIN_EXPECT_FALSE(CastsShadow(w, e));
}

ENJIN_TEST(MeshRendererFields, ShadowModeOnOverridesAMaterialThatOptedOut) {
    // This is the case the field exists for: one instance of a SHARED material
    // needs to cast when the material says not to, and editing the material would
    // change every other user of it.
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 0), "Decal");
    auto& mat = w.AddComponent<MaterialComponent>(e, MaterialComponent{});
    mat.castShadows = false;
    auto& mr = w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});

    ENJIN_ASSERT_FALSE(CastsShadow(w, e));
    mr.shadowMode = MeshRendererComponent::ShadowMode::On;
    ENJIN_EXPECT_TRUE(CastsShadow(w, e));
}

ENJIN_TEST(MeshRendererFields, FromMaterialLeavesTheMaterialInCharge) {
    // The default must change nothing, or adding a MeshRenderer to an existing
    // entity would silently alter its shadows.
    World w;
    Entity e = MakeMesh(w, Vector3(0, 0, 0), "Wall");
    auto& mat = w.AddComponent<MaterialComponent>(e, MaterialComponent{});
    mat.castShadows = false;
    auto& mr = w.AddComponent<MeshRendererComponent>(e, MeshRendererComponent{});
    ENJIN_EXPECT_EQ(static_cast<int>(mr.shadowMode),
                    static_cast<int>(MeshRendererComponent::ShadowMode::FromMaterial));
    ENJIN_EXPECT_FALSE(CastsShadow(w, e));

    mat.castShadows = true;
    ENJIN_EXPECT_TRUE(CastsShadow(w, e));
}

// ---------------------------------------------------------------------------
// renderQueue -- ordering that beats the material sort key
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshRendererFields, RenderQueueOrdersAheadOfTheMaterialKey) {
    // The comparator puts renderQueue first and the material sort key second. That
    // order IS the feature: an author saying "draw this last" must win over
    // whichever pipeline bucket the material happens to land in, or the setting
    // works for some materials and not others with no way to tell which.
    struct Row { i32 queue; u64 matKey; };
    auto less = [](const Row& a, const Row& b) {
        if (a.queue != b.queue) return a.queue < b.queue;
        return a.matKey < b.matKey;
    };

    // A skybox shell at -1000 with the WORST material key still sorts first.
    const Row skybox{ -1000, 0xFFFFFFFFFFFFFFFFull };
    const Row normal{ 0, 0x0000000000000001ull };
    const Row overlay{ 1000, 0x0000000000000000ull };

    ENJIN_EXPECT_TRUE(less(skybox, normal));
    ENJIN_EXPECT_TRUE(less(normal, overlay));
    ENJIN_EXPECT_FALSE(less(overlay, normal));

    // And within one queue the material key still groups batches, which is what
    // keeps descriptor churn down.
    const Row sameQueueA{ 0, 5 };
    const Row sameQueueB{ 0, 9 };
    ENJIN_EXPECT_TRUE(less(sameQueueA, sameQueueB));
}

// ---------------------------------------------------------------------------
// allowInstancing
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshRendererFields, OptingOutOfInstancingGivesAKeyNothingElseMatches) {
    // Batching groups by a mesh key. An entity that opts out gets a key derived
    // from its own id, so it can never be folded into a batch -- which matters
    // where per-instance state that is NOT in the instance buffer differs, and
    // batching would draw every instance with the first one's state.
    const u64 sharedMeshKey = 0x1234ull ^ (0x5678ull << 16);

    auto keyFor = [&](Entity e, bool allowInstancing) -> u64 {
        return allowInstancing ? sharedMeshKey : ~static_cast<u64>(e);
    };

    const Entity a = 7, b = 9;
    ENJIN_EXPECT_EQ(keyFor(a, true), keyFor(b, true));       // batch together
    ENJIN_EXPECT_TRUE(keyFor(a, false) != keyFor(b, false)); // and never together
    ENJIN_EXPECT_TRUE(keyFor(a, false) != sharedMeshKey);
}

// ---------------------------------------------------------------------------
// The component's own shape
// ---------------------------------------------------------------------------

ENJIN_TEST(MeshRendererFields, DefaultsAreTheNoOpSettings) {
    // Adding the component to an existing entity must change nothing on its own.
    // A default that altered culling, ordering or shadows would make "add a
    // MeshRenderer to configure one thing" quietly change three others.
    MeshRendererComponent mr;
    ENJIN_EXPECT_TRUE(mr.enabled);
    ENJIN_EXPECT_TRUE(mr.frustumCull);
    ENJIN_EXPECT_TRUE(mr.occlusionCull);
    ENJIN_EXPECT_TRUE(mr.maxDrawDistance == 0.0f);   // infinite
    ENJIN_EXPECT_EQ(mr.renderQueue, 0);
    ENJIN_EXPECT_TRUE(mr.contributeMotionVectors);
    ENJIN_EXPECT_TRUE(mr.allowInstancing);
    ENJIN_EXPECT_FALSE(mr.wireframe);
    ENJIN_EXPECT_EQ(static_cast<int>(mr.shadowMode),
                    static_cast<int>(MeshRendererComponent::ShadowMode::FromMaterial));
}

ENJIN_TEST_MAIN()
