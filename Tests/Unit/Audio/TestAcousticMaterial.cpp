// What a surface does to sound that hits it.
//
// Every triangle in the audio scene was handed the same material -- one default
// described in a comment as "roughly concrete/wood" -- so a carpeted basement
// and a tiled kitchen reflected identically. Marty: "Concrete, wood, carpet
// have to differ or both floors sound the same."
//
// The tests that matter here are about DIFFERENCE. A table that compiled, had a
// row per material, and gave them all similar numbers would look completely
// correct and change nothing about how a room sounds.

#include "EnjinTest.h"
#include "Enjin/Audio/AcousticMaterial.h"

#include <cmath>
#include <set>

using namespace Enjin;
using namespace Enjin::Audio;
using Enjin::ECS::SurfaceMaterial;

ENJIN_TEST(AcousticMaterial, EverySurfaceHasAName) {
    // Arrange / Act / Assert: a material with no name is a blank entry in the
    // inspector dropdown.
    for (u8 i = 0; i < static_cast<u8>(SurfaceMaterial::Count); ++i) {
        const char* name = ECS::SurfaceMaterialName(static_cast<SurfaceMaterial>(i));
        ENJIN_EXPECT_TRUE(name != nullptr);
        ENJIN_EXPECT_FALSE(std::string(name) == "Unknown");
    }
}

ENJIN_TEST(AcousticMaterial, TheOriginalTenKeptTheirOrdinals) {
    // Arrange / Act / Assert: the ordinal is what a scene file stores. Six
    // materials were APPENDED for acoustics, and if any had been inserted
    // instead, every saved Glass would have become Flesh on the next load.
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Default), (u8)0);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Metal),   (u8)1);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Wood),    (u8)2);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Stone),   (u8)3);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Glass),   (u8)4);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Flesh),   (u8)5);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Water),   (u8)6);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Dirt),    (u8)7);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Grass),   (u8)8);
    ENJIN_EXPECT_EQ(static_cast<u8>(SurfaceMaterial::Ice),     (u8)9);
}

ENJIN_TEST(AcousticMaterial, EveryCoefficientIsAFractionOfEnergy) {
    // Arrange / Act / Assert: absorption and transmission are fractions. A value
    // above one is a surface that returns more energy than hit it, which in a
    // reflection simulation is a room that gets louder forever.
    for (u8 i = 0; i < static_cast<u8>(SurfaceMaterial::Count); ++i) {
        const AcousticProperties p = AcousticsFor(static_cast<SurfaceMaterial>(i));
        for (u32 b = 0; b < kAcousticBands; ++b) {
            ENJIN_EXPECT_TRUE(p.absorption[b] >= 0.0f && p.absorption[b] <= 1.0f);
            ENJIN_EXPECT_TRUE(p.transmission[b] >= 0.0f && p.transmission[b] <= 1.0f);
            // And no surface may both swallow everything and pass everything.
            ENJIN_EXPECT_TRUE(p.absorption[b] + p.transmission[b] <= 1.0f);
        }
        ENJIN_EXPECT_TRUE(p.scattering >= 0.0f && p.scattering <= 1.0f);
    }
}

// The whole point of the feature.
ENJIN_TEST(AcousticMaterial, ConcreteAndCarpetAreNothingAlike) {
    // Arrange
    const AcousticProperties concrete = AcousticsFor(SurfaceMaterial::Concrete);
    const AcousticProperties carpet = AcousticsFor(SurfaceMaterial::Carpet);

    // Act / Assert: concrete returns nearly everything at every frequency.
    for (u32 b = 0; b < kAcousticBands; ++b) {
        ENJIN_EXPECT_TRUE(concrete.absorption[b] < 0.05f);
    }

    // Carpet swallows the top and returns the bottom, which is the shape of
    // "dull rather than quiet" -- the single biggest reason two rooms of the
    // same size sound different.
    ENJIN_EXPECT_TRUE(carpet.absorption[2] > carpet.absorption[0] * 3.0f);
    ENJIN_EXPECT_TRUE(carpet.absorption[2] > 0.4f);

    // And at high frequency they are an order of magnitude apart.
    ENJIN_EXPECT_TRUE(carpet.absorption[2] > concrete.absorption[2] * 10.0f);
}

ENJIN_TEST(AcousticMaterial, ATiledKitchenAndACarpetedBasementAreTellableApart) {
    // Arrange: the acceptance case, as data.
    const AcousticProperties tile = AcousticsFor(SurfaceMaterial::Tile);
    const AcousticProperties carpet = AcousticsFor(SurfaceMaterial::Carpet);

    // Act / Assert
    ENJIN_EXPECT_FALSE(AcousticsEqual(tile, carpet));
    ENJIN_EXPECT_TRUE(carpet.absorption[1] > tile.absorption[1] * 5.0f);
}

ENJIN_TEST(AcousticMaterial, WoodAbsorbsLowsMoreThanHighsWhichIsBackwardsFromIntuition) {
    // Arrange / Act
    const AcousticProperties wood = AcousticsFor(SurfaceMaterial::Wood);

    // Assert: panelling over a cavity is a bass trap. Getting this the usual
    // way round would make every wooden room boom instead of sounding warm, and
    // it is the kind of value that gets "corrected" by someone who has not
    // looked it up.
    ENJIN_EXPECT_TRUE(wood.absorption[0] > wood.absorption[2]);
}

ENJIN_TEST(AcousticMaterial, RoughSurfacesScatterAndSmoothOnesMirror) {
    // Arrange / Act / Assert: this is what separates brick from glass even
    // though both are hard and absorb almost nothing.
    ENJIN_EXPECT_TRUE(AcousticsFor(SurfaceMaterial::Brick).scattering >
                      AcousticsFor(SurfaceMaterial::Glass).scattering * 5.0f);
    ENJIN_EXPECT_TRUE(AcousticsFor(SurfaceMaterial::Stone).scattering >
                      AcousticsFor(SurfaceMaterial::Concrete).scattering);
}

ENJIN_TEST(AcousticMaterial, LowsPassThroughWallsMoreEasilyThanHighs) {
    // Arrange / Act / Assert: why a distant party is audible as bass and not as
    // words. Every material must agree on this or the engine contradicts the
    // most familiar acoustic experience there is.
    for (u8 i = 0; i < static_cast<u8>(SurfaceMaterial::Count); ++i) {
        const AcousticProperties p = AcousticsFor(static_cast<SurfaceMaterial>(i));
        ENJIN_EXPECT_TRUE(p.transmission[0] >= p.transmission[1]);
        ENJIN_EXPECT_TRUE(p.transmission[1] >= p.transmission[2]);
    }
}

ENJIN_TEST(AcousticMaterial, TheHardMaterialsAreActuallyHarderThanTheSoftOnes) {
    // Arrange: a table with a row per material that gave them all similar
    // numbers would compile, look complete, and change nothing about how a room
    // sounds. This is the test that would fail if that happened.
    const SurfaceMaterial hard[] = { SurfaceMaterial::Concrete, SurfaceMaterial::Tile,
                                     SurfaceMaterial::Glass,    SurfaceMaterial::Metal,
                                     SurfaceMaterial::Water };
    const SurfaceMaterial soft[] = { SurfaceMaterial::Carpet, SurfaceMaterial::Fabric,
                                     SurfaceMaterial::Grass,  SurfaceMaterial::Dirt };

    // Act: mid-band absorption, which is where speech and most sound sits.
    f32 hardest = 0.0f;
    for (SurfaceMaterial m : hard) hardest = std::max(hardest, AcousticsFor(m).absorption[1]);
    f32 softest = 1.0f;
    for (SurfaceMaterial m : soft) softest = std::min(softest, AcousticsFor(m).absorption[1]);

    // Assert: the two groups do not overlap at all.
    ENJIN_EXPECT_TRUE(softest > hardest);
}

ENJIN_TEST(AcousticMaterial, EnoughMaterialsAreDistinctToBeWorthHaving) {
    // Arrange / Act: count how many genuinely different acoustic rows exist.
    std::vector<AcousticProperties> distinct;
    for (u8 i = 0; i < static_cast<u8>(SurfaceMaterial::Count); ++i) {
        const AcousticProperties p = AcousticsFor(static_cast<SurfaceMaterial>(i));
        bool seen = false;
        for (const auto& d : distinct) {
            if (AcousticsEqual(d, p)) { seen = true; break; }
        }
        if (!seen) distinct.push_back(p);
    }

    // Assert: most of the sixteen are their own thing. A couple collapsing
    // together is fine and expected -- water and ice really are both mirrors --
    // but a table where half the entries were duplicates would be a table that
    // had been filled in rather than looked up.
    ENJIN_EXPECT_TRUE(distinct.size() >= 12);
}

// --------------------------------------------------------------------------
// Resolving what an entity is made of
// --------------------------------------------------------------------------

ENJIN_TEST(AcousticMaterial, AnUnlabelledSurfaceIsDefaultAndDefaultIsUnremarkable) {
    // Arrange / Act
    const SurfaceMaterial resolved = ResolveSurface(nullptr, nullptr);
    const AcousticProperties p = AcousticsFor(resolved);

    // Assert: not the most reflective option, or every room nobody has labelled
    // rings like a cathedral.
    ENJIN_EXPECT_TRUE(resolved == SurfaceMaterial::Default);
    ENJIN_EXPECT_TRUE(p.absorption[1] > AcousticsFor(SurfaceMaterial::Concrete).absorption[1]);
}

ENJIN_TEST(AcousticMaterial, CollisionAudioWinsBecauseItIsTheMoreSpecificStatement) {
    // Arrange
    ECS::AudioCollisionComponent collision;
    collision.material = SurfaceMaterial::Metal;
    ECS::MaterialComponent material;
    material.surfaceMaterial = SurfaceMaterial::Carpet;

    // Act / Assert: someone who set the collision material was describing this
    // exact object, not the look it shares with everything else using the
    // material.
    ENJIN_EXPECT_TRUE(ResolveSurface(&collision, &material) == SurfaceMaterial::Metal);
}

ENJIN_TEST(AcousticMaterial, TheMaterialAnswersWhenCollisionAudioHasNoOpinion) {
    // Arrange: the ordinary case -- a wall with a look and no impact sounds.
    ECS::AudioCollisionComponent collision;   // left at Default
    ECS::MaterialComponent material;
    material.surfaceMaterial = SurfaceMaterial::Brick;

    // Act / Assert
    ENJIN_EXPECT_TRUE(ResolveSurface(&collision, &material) == SurfaceMaterial::Brick);
    ENJIN_EXPECT_TRUE(ResolveSurface(nullptr, &material) == SurfaceMaterial::Brick);
}

// --------------------------------------------------------------------------
// The table handed to the simulator
// --------------------------------------------------------------------------

ENJIN_TEST(AcousticMaterial, AHundredConcreteWallsCostOneMaterialEntry) {
    // Arrange
    AcousticMaterialTable table;

    // Act
    for (u32 i = 0; i < 100; ++i) {
        ENJIN_EXPECT_EQ(table.IndexFor(SurfaceMaterial::Concrete), (u32)0);
    }

    // Assert: the simulator wants a material ARRAY and a per-triangle index into
    // it. A hundred identical entries would be a hundred times the memory for
    // exactly the same room.
    ENJIN_EXPECT_EQ(table.Count(), (usize)1);
}

ENJIN_TEST(AcousticMaterial, DifferentSurfacesGetDifferentEntries) {
    // Arrange
    AcousticMaterialTable table;

    // Act
    const u32 concrete = table.IndexFor(SurfaceMaterial::Concrete);
    const u32 carpet = table.IndexFor(SurfaceMaterial::Carpet);
    const u32 concreteAgain = table.IndexFor(SurfaceMaterial::Concrete);

    // Assert
    ENJIN_EXPECT_TRUE(concrete != carpet);
    ENJIN_EXPECT_EQ(concrete, concreteAgain);
    ENJIN_EXPECT_EQ(table.Count(), (usize)2);

    // And the entries really carry the right properties, not just distinct slots.
    ENJIN_EXPECT_TRUE(AcousticsEqual(table.At(concrete), AcousticsFor(SurfaceMaterial::Concrete)));
    ENJIN_EXPECT_TRUE(AcousticsEqual(table.At(carpet), AcousticsFor(SurfaceMaterial::Carpet)));
}

ENJIN_TEST_MAIN()
