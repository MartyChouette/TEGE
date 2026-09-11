// The two creative surfaces had two definitions of the same objects.
//
// The Build Palette (a quick-place window in the full editor) and Creative
// Mode's tool rail both place grass, shrubs, trees, balls, lights, physics
// boxes, barrels and spawn points. Both are deliberate surfaces. What was not
// deliberate is that each had its own copy of what those things ARE.
//
// When the rail gained Plants and Prop, the density curves and the vertical drop
// offsets were COPIED out of the palette rather than shared -- so the two agreed
// only because the same numbers had been typed twice. The drift that follows
// from that is invisible: grass placed one way and grass placed the other would
// differ by an amount nobody would think to compare.
//
// One definition now, and these are the properties worth pinning.
#include "EnjinTest.h"
#include "Enjin/Editor/ScenePlacement.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"

using namespace Enjin;
using namespace Enjin::Editor;

namespace {

ECS::Entity MakeEntity(ECS::World& w) {
    ECS::Entity e = w.CreateEntity();
    w.AddComponent<ECS::TransformComponent>(e);
    return e;
}

} // namespace

// ---------------------------------------------------------------------------
// Density
// ---------------------------------------------------------------------------

ENJIN_TEST(ScenePlacement, DensityGrowsWithArea) {
    // The curve's whole point: a bigger patch gets more in it.
    const u32 small = PlantDensity(PlantKind::Grass, 1.0f, 1.0f, 1.0f);
    const u32 big   = PlantDensity(PlantKind::Grass, 8.0f, 8.0f, 1.0f);
    ENJIN_EXPECT_TRUE(big > small);
}

ENJIN_TEST(ScenePlacement, EachKindHasItsOwnCurve) {
    // These three numbers are the difference between grass and trees, and they
    // are the ones that were duplicated. A grove at grass density is a solid
    // wall of trunks; grass at tree density is four blades in a field.
    const f32 h = 10.0f;
    const u32 grass  = PlantDensity(PlantKind::Grass,  h, h, 1.0f);
    const u32 shrubs = PlantDensity(PlantKind::Shrubs, h, h, 1.0f);
    const u32 trees  = PlantDensity(PlantKind::Trees,  h, h, 1.0f);

    ENJIN_EXPECT_TRUE(grass > shrubs);
    ENJIN_EXPECT_TRUE(shrubs > trees);
}

ENJIN_TEST(ScenePlacement, DensityIsClampedAtBothEnds) {
    // A patch the size of a doormat still reads as grass, and one the size of a
    // field does not try to place a hundred thousand blades.
    const u32 tiny = PlantDensity(PlantKind::Grass, 0.01f, 0.01f, 1.0f);
    ENJIN_EXPECT_TRUE(tiny >= 32u);

    const u32 huge = PlantDensity(PlantKind::Grass, 5000.0f, 5000.0f, 1.0f);
    ENJIN_EXPECT_TRUE(huge <= 6000u);

    const u32 manyTrees = PlantDensity(PlantKind::Trees, 5000.0f, 5000.0f, 1.0f);
    ENJIN_EXPECT_TRUE(manyTrees <= 120u);
}

ENJIN_TEST(ScenePlacement, TheScaleMultipliesTheSameCurve) {
    // The rail has a Density setting; the palette's drag does not and passes 1.
    // Both have to be the same curve or the two surfaces diverge the moment
    // anybody touches that slider.
    const u32 once  = PlantDensity(PlantKind::Shrubs, 6.0f, 6.0f, 1.0f);
    const u32 twice = PlantDensity(PlantKind::Shrubs, 6.0f, 6.0f, 2.0f);
    ENJIN_EXPECT_TRUE(twice > once);

    // A scale of 1 is exactly the palette's behaviour -- that is the equality
    // that keeps the two surfaces agreeing.
    ENJIN_EXPECT_EQ(PlantDensity(PlantKind::Grass, 4.0f, 3.0f, 1.0f),
                    PlantDensity(PlantKind::Grass, 4.0f, 3.0f, 1.0f));
}

ENJIN_TEST(ScenePlacement, AMirroredDragStillCountsAsArea) {
    // Dragging right-to-left gives a negative span upstream. Treating that as a
    // tiny patch would give the minimum instance count for a large rectangle.
    ENJIN_EXPECT_EQ(PlantDensity(PlantKind::Grass, -6.0f, -4.0f, 1.0f),
                    PlantDensity(PlantKind::Grass, 6.0f, 4.0f, 1.0f));
}

// ---------------------------------------------------------------------------
// Volumes
// ---------------------------------------------------------------------------

ENJIN_TEST(ScenePlacement, SizingAVolumeSetsBothExtentsAndCount) {
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    AddPlantVolume(&world, e, PlantKind::Grass);
    SizePlantVolume(&world, e, PlantKind::Grass, 5.0f, 3.0f, 1.0f);

    auto* gv = world.GetComponent<ECS::GrassVolumeComponent>(e);
    ENJIN_ASSERT_NOT_NULL(gv);
    ENJIN_EXPECT_FLOAT_NEAR(gv->halfExtents.x, 5.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(gv->halfExtents.z, 3.0f, 0.001f);
    ENJIN_EXPECT_EQ(gv->density, PlantDensity(PlantKind::Grass, 5.0f, 3.0f, 1.0f));
}

ENJIN_TEST(ScenePlacement, AddingTwiceDoesNotStackComponents) {
    // The palette creates on click and resizes every frame of the drag, so this
    // gets called repeatedly on the same entity.
    ECS::World world;
    ECS::Entity e = MakeEntity(world);

    AddPlantVolume(&world, e, PlantKind::Trees);
    AddPlantVolume(&world, e, PlantKind::Trees);
    ENJIN_EXPECT_TRUE(world.HasComponent<ECS::TreeVolumeComponent>(e));

    for (int i = 1; i <= 5; ++i) {
        SizePlantVolume(&world, e, PlantKind::Trees, static_cast<f32>(i), 2.0f, 1.0f);
    }
    auto* tv = world.GetComponent<ECS::TreeVolumeComponent>(e);
    ENJIN_ASSERT_NOT_NULL(tv);
    ENJIN_EXPECT_FLOAT_NEAR(tv->halfExtents.x, 5.0f, 0.001f);
}

// ---------------------------------------------------------------------------
// Props
// ---------------------------------------------------------------------------

ENJIN_TEST(ScenePlacement, EveryPropKindIsNamed) {
    for (u8 i = 0; i < static_cast<u8>(PropKind::Count); ++i) {
        const char* n = PropKindName(static_cast<PropKind>(i));
        ENJIN_ASSERT_TRUE(n != nullptr);
        ENJIN_EXPECT_TRUE(n[0] != '\0');
    }
}

ENJIN_TEST(ScenePlacement, EveryPlantKindIsNamed) {
    for (u8 i = 0; i < static_cast<u8>(PlantKind::Count); ++i) {
        const char* n = PlantKindName(static_cast<PlantKind>(i));
        ENJIN_ASSERT_TRUE(n != nullptr);
        ENJIN_EXPECT_TRUE(n[0] != '\0');
    }
}

ENJIN_TEST(ScenePlacement, PropsRiseOffTheGroundTheyLandOn) {
    // These all land on a ground hit. A sphere of radius 0.5 placed AT the hit
    // is buried to its equator, so the offsets are correctness rather than
    // decoration -- and they are exactly what was duplicated between the two
    // surfaces.
    ECS::World world;
    const Math::Vector3 ground(2.0f, 7.0f, -3.0f);

    ECS::Entity ball = MakeEntity(world);
    CreateProp(&world, ball, PropKind::Ball, ground);
    ENJIN_EXPECT_FLOAT_NEAR(world.GetComponent<ECS::TransformComponent>(ball)->position.y,
                            7.5f, 0.001f);

    // A light on the floor lights the floor and nothing else.
    ECS::Entity light = MakeEntity(world);
    CreateProp(&world, light, PropKind::Light, ground);
    ENJIN_EXPECT_TRUE(world.GetComponent<ECS::TransformComponent>(light)->position.y > 8.0f);

    // A physics box flush with the floor never visibly falls, which reads as
    // physics being broken rather than as a placement choice.
    ECS::Entity box = MakeEntity(world);
    CreateProp(&world, box, PropKind::PhysicsBox, ground);
    ENJIN_EXPECT_TRUE(world.GetComponent<ECS::TransformComponent>(box)->position.y >= 9.9f);

    // Horizontal placement is untouched in every case.
    ENJIN_EXPECT_FLOAT_NEAR(world.GetComponent<ECS::TransformComponent>(ball)->position.x,
                            2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(world.GetComponent<ECS::TransformComponent>(ball)->position.z,
                            -3.0f, 0.001f);
}

ENJIN_TEST(ScenePlacement, APhysicsBoxIsActuallyPhysical) {
    // The point of the kind. Without a dynamic body and a collider it is a
    // crate-shaped decoration, which is the honest-looking failure.
    ECS::World world;
    ECS::Entity e = MakeEntity(world);
    CreateProp(&world, e, PropKind::PhysicsBox, Math::Vector3(0.0f));

    auto* rb = world.GetComponent<ECS::RigidbodyComponent>(e);
    ENJIN_ASSERT_NOT_NULL(rb);
    ENJIN_EXPECT_TRUE(rb->bodyType == ECS::RigidbodyComponent::BodyType::Dynamic);
    ENJIN_EXPECT_TRUE(rb->useGravity);
    ENJIN_EXPECT_TRUE(world.HasComponent<ECS::BoxColliderComponent>(e));
    ENJIN_EXPECT_TRUE(world.HasComponent<ECS::MeshComponent>(e));
}

ENJIN_TEST(ScenePlacement, ABarrelBreaks) {
    ECS::World world;
    ECS::Entity e = MakeEntity(world);
    CreateProp(&world, e, PropKind::Barrel, Math::Vector3(0.0f));

    auto* d = world.GetComponent<ECS::DestructibleComponent>(e);
    ENJIN_ASSERT_NOT_NULL(d);
    ENJIN_EXPECT_TRUE(d->destroyOnHit);
}

ENJIN_TEST(ScenePlacement, APropWithNoTransformIsLeftAlone) {
    // CreateProp writes the transform, so an entity without one is a caller
    // mistake. Declining beats adding components around a position that was
    // never set.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();   // no transform
    CreateProp(&world, e, PropKind::Ball, Math::Vector3(1.0f, 2.0f, 3.0f));
    ENJIN_EXPECT_FALSE(world.HasComponent<ECS::MeshComponent>(e));
}

ENJIN_TEST(ScenePlacement, ANullWorldIsSurvived) {
    // Both surfaces can reach here mid-teardown.
    CreateProp(nullptr, 1, PropKind::Ball, Math::Vector3(0.0f));
    AddPlantVolume(nullptr, 1, PlantKind::Grass);
    SizePlantVolume(nullptr, 1, PlantKind::Grass, 1.0f, 1.0f, 1.0f);
    ENJIN_SURVIVED("null world during teardown");
}

ENJIN_TEST_MAIN()
