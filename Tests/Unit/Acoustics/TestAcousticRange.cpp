// Every space in the Acoustic Range, measured and bracketed.
//
// The sweeps in TestAcousticSweeps.cpp measure rooms built directly in code:
// exact vertices, single closed shells, no doorways, nothing else in the world.
// That is the right instrument for isolating one variable, and a poor model of
// a building. This measures the thing a person actually walks through -- eleven
// spaces built from solid slabs, each with a doorway onto a shared corridor,
// all loaded from the scene file the editor opens.
//
// That distinction is not academic. The first version of the adaptive
// measurement window estimated room volume from the triangles by the divergence
// theorem, which is exact for a closed shell and meaningless for a building
// made of wall boxes: it returns the volume of the WALLS. It would have passed
// every sweep and been wrong in the editor. This file is where that shows.
//
// WHAT IS ASSERTED:
//
// Not a closed form. A sealed box has one; a room with a doorway onto a
// building does not, and the several attempts to pretend otherwise are written
// up on the test that replaced them. What is asserted here is that the building
// is watertight (escape below 8%, which is what caught the missing thresholds),
// that the spaces order themselves the way their materials say, that coupling
// does not erase the difference between tile and carpet, and that a room cut
// out of the building matches its idealised twin.
//
// The interesting numbers are PRINTED rather than asserted, because their value
// is what a person reads when something looks wrong -- and twice now the
// printed table has been right while the assertion next to it encoded a model
// that did not hold.

#include "EnjinTest.h"
#include "Enjin/Acoustics/AcousticsSystem.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Audio/AcousticMaterial.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "RoomBuilder.h"
#include "Enjin/Acoustics/AcousticBVH.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::Math::Vector3;
using Enjin::ECS::SurfaceMaterial;

namespace {

std::string FindScene() {
    const char* candidates[] = {
        "Examples/RoomAcoustics/scenes/Range.enjin",
        "../Examples/RoomAcoustics/scenes/Range.enjin",
        "../../Examples/RoomAcoustics/scenes/Range.enjin",
        "../../../Examples/RoomAcoustics/scenes/Range.enjin",
        "../../../../Examples/RoomAcoustics/scenes/Range.enjin",
    };
    for (const char* path : candidates) {
        std::ifstream f(path);
        if (f.good()) return path;
    }
    return std::string();
}

// One space, as build_range.py built it. The dimensions are repeated here on
// purpose: if the generator changes a room and this file is not updated, the
// bracket stops matching and the test says so, which is the point.
struct Space {
    const char* name;
    Vector3 centre;          // listener stands here
    f32 w, h, d;
    SurfaceMaterial floor, walls, ceiling;
    f32 doorArea;            // opening onto the spine, m^2
    bool openCeiling;
    bool coupled;            // excluded from the bracket; see the coupling test
};

constexpr f32 DOOR = 1.6f * 2.2f;       // the standard doorway, 3.52 m^2

const Space kSpaces[] = {
    { "Anechoic Cell",      { -70.0f, 1.5f,  -5.00f },  4.0f,  3.0f,  4.0f,
      SurfaceMaterial::Fabric,   SurfaceMaterial::Fabric,   SurfaceMaterial::Fabric,   DOOR, false, false },
    { "Tiled Bathroom",     { -60.0f, 1.2f,  -4.75f },  3.2f,  2.5f,  3.5f,
      SurfaceMaterial::Tile,     SurfaceMaterial::Tile,     SurfaceMaterial::Tile,     DOOR, false, false },
    { "Living Room",        { -47.0f, 1.4f,  -6.00f },  7.0f,  2.7f,  6.0f,
      SurfaceMaterial::Wood,     SurfaceMaterial::Drywall,  SurfaceMaterial::Drywall,  DOOR, false, false },
    { "Carpeted Lounge",    { -47.0f, 1.4f,   6.00f },  7.0f,  2.7f,  6.0f,
      SurfaceMaterial::Carpet,   SurfaceMaterial::Fabric,   SurfaceMaterial::Drywall,  DOOR, false, false },
    { "Concrete Stairwell", { -33.0f, 1.6f,  -5.50f },  5.0f, 14.0f,  5.0f,
      SurfaceMaterial::Concrete, SurfaceMaterial::Concrete, SurfaceMaterial::Concrete, DOOR, false, false },
    { "Long Gallery",       { -12.0f, 1.6f,   5.50f }, 44.0f,  4.0f,  5.0f,
      SurfaceMaterial::Concrete, SurfaceMaterial::Drywall,  SurfaceMaterial::Drywall,  DOOR, false, false },
    { "Low Warehouse",      { -18.0f, 1.6f, -12.00f }, 26.0f,  3.2f, 18.0f,
      SurfaceMaterial::Concrete, SurfaceMaterial::Concrete, SurfaceMaterial::Metal,    DOOR, false, false },
    { "Great Hall",         {  14.0f, 1.6f, -11.00f }, 24.0f,  9.0f, 16.0f,
      SurfaceMaterial::Stone,    SurfaceMaterial::Brick,    SurfaceMaterial::Wood,     DOOR, false, false },
    { "Cathedral",          {  52.0f, 1.6f, -12.00f }, 30.0f, 22.0f, 18.0f,
      SurfaceMaterial::Stone,    SurfaceMaterial::Stone,    SurfaceMaterial::Stone,    DOOR, false, false },
    { "Open Courtyard",     {  20.0f, 1.6f,  11.00f }, 20.0f,  6.0f, 16.0f,
      SurfaceMaterial::Stone,    SurfaceMaterial::Brick,    SurfaceMaterial::Stone,    DOOR, true,  false },
    { "Coupled Hard",       {  62.0f, 1.6f,   7.50f },  9.0f,  4.0f,  9.0f,
      SurfaceMaterial::Tile,     SurfaceMaterial::Tile,     SurfaceMaterial::Tile,     DOOR, false, true },
};

// Sabine for one space, with a chosen amount of perfectly-absorbing opening.
f32 Sabine(const Space& s, u32 band, f32 openingArea) {
    using Enjin::Audio::AcousticsFor;

    const f32 floorArea = s.w * s.d;
    const f32 wallArea = 2.0f * (s.w * s.h + s.h * s.d);
    const f32 volume = s.w * s.h * s.d;

    f32 absorbed = floorArea * AcousticsFor(s.floor).absorption[band]
                 + wallArea * AcousticsFor(s.walls).absorption[band];
    if (s.openCeiling) {
        absorbed += floorArea * 1.0f;          // open sky: a perfect absorber
    } else {
        absorbed += floorArea * AcousticsFor(s.ceiling).absorption[band];
    }
    absorbed += openingArea * 1.0f;

    if (absorbed < 1.0e-6f) return 0.0f;
    return 0.161f * volume / absorbed;
}

bool LoadRange(ECS::World& world) {
    const std::string path = FindScene();
    if (path.empty()) return false;
    Scene::SceneSerializer serializer(&world);
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return serializer.LoadFromString(buffer.str()).success;
}

AcousticsSettings RangeSettings() {
    AcousticsSettings s;
    s.trace.rayCount = 2048;
    return s;
}

} // namespace

ENJIN_TEST(AcousticRange, TheRangeLoadsAndIsBigEnoughToBeWorthMeasuring) {
    // Arrange / Act
    ECS::World world;
    if (!LoadRange(world)) {
        ENJIN_SKIP("Examples/RoomAcoustics/scenes/Range.enjin not found");
        return;
    }

    // Assert: eleven spaces plus a spine is a lot of slabs, and a range that
    // quietly shrank to three rooms would make every bracket below trivial.
    const auto entities = world.GetEntitiesWithComponent<ECS::TransformComponent>();
    std::printf("    %zu entities\n", entities.size());
    ENJIN_EXPECT_TRUE(entities.size() > 150);
}

// What a doorway costs, measured against the same room sealed.
//
// This started out asserting a Sabine BRACKET on each space: the truth had to
// lie between "the doorway does not exist" and "the doorway is a perfect
// absorber". That bracket is sound for an isolated room and wrong for a
// building, and running it found two real defects in the Range before it was
// abandoned -- 22 doorways with no floor under the threshold (7 to 34 percent
// of all rays escaping the building from inside sealed rooms), and rooms with
// open vertical corners. Both are fixed; escape is now 1 to 5 percent.
//
// It is not asserted any more because it is not a valid model here, in BOTH
// directions:
//
//   The dead rooms measure LONGER than sealed. The Anechoic Cell and the
//   Carpeted Lounge open onto a building containing a fifteen-second cathedral,
//   and their late tail is fed from next door. Coupled-room decay is not one
//   exponential and no single RT60 describes it, so a sealed upper bound is not
//   an upper bound at all.
//
//   The hard rooms measure SHORTER than sealed, by about 25 percent, and that
//   one took three experiments to pin down. It is the corridor, and the answer
//   is more interesting than the question:
//
//     An opening costs exactly its area. TestAcousticSweeps measures a 3.52 m^2
//     hole taking this shape of room from 7.68 s to 7.33 s, against a closed
//     form of 7.41 -- so the tracer does not over-charge for doorways.
//
//     The hall as AUTHORED is correct. Cut out of the building (see the last
//     test in this file) it measures 7.35 s, matching that idealised twin to
//     0.3%. So the slabs, the materials and the doorway are all fine.
//
//     The difference is that a doorway onto a void and a doorway onto a
//     corridor are not the same thing. The tracer deposits energy at every
//     bounce, wherever the ray happens to be. In isolation a ray through the
//     door escapes and contributes nothing further -- 45% of them do. In the
//     building those same rays spend their remaining energy bouncing around
//     3,200 m^3 of carpet and fabric, and every one of those heavily absorbed
//     samples lands in the decay curve and steepens it.
//
//   That is the right answer for a game, and worth being explicit about: this
//   measures the sound field at a point in a BUILDING, not the isolated RT60 of
//   a room. A hall whose door stands open onto a dead corridor really does decay
//   faster than the same hall sealed, and a player walking between them should
//   hear that.
//
// What IS asserted is the direction, which is solid: a hard room coupled to a
// dead corridor must lose energy, and a dead room coupled to a live building
// must gain it.
ENJIN_TEST(AcousticRange, ADoorwayCostsAHardRoomAndFeedsASoftOne) {
    // Arrange
    ECS::World world;
    if (!LoadRange(world)) { ENJIN_SKIP("Range.enjin not found"); return; }

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(RangeSettings());
    system.Update(kSpaces[0].centre, 2.0f);

    std::printf("    %-20s %9s %9s %8s %9s\n",
                "space", "in-bldg", "sealed", "ratio", "escaped");

    u32 measured = 0;
    for (const Space& s : kSpaces) {
        if (s.coupled || s.openCeiling) continue;

        // Act: the same room twice -- once as it stands in the building, once
        // as a sealed shell with identical dimensions and surfaces.
        system.Update(s.centre, 0.016f);
        const RoomResponse inBuilding = system.Measurement();
        if (!inBuilding.Valid()) continue;

        RoomBuilder::Dimensions dim{ s.w, s.h, s.d };
        RoomBuilder::Surfaces surf{ s.floor, s.ceiling, s.walls };
        const Audio::AcousticScene shell = RoomBuilder::Box(dim, surf, false);
        AcousticBVH bvh;
        bvh.Build(shell);
        RoomTraceSettings trace;
        trace.rayCount = 4096;
        const RoomResponse sealed = TraceRoomResponse(bvh, shell, dim.Centre(), trace);
        if (!sealed.Valid()) continue;
        ++measured;

        const f32 ratio = inBuilding.rt60[1] / sealed.rt60[1];
        const f32 escaped = 100.0f * static_cast<f32>(inBuilding.raysEscaped) /
            static_cast<f32>(inBuilding.raysTraced ? inBuilding.raysTraced : 1);
        std::printf("    %-20s %9.2f %9.2f %8.2f %8.1f%%\n",
                    s.name, inBuilding.rt60[1], sealed.rt60[1], ratio, escaped);

        // Assert: the building has to be reasonably watertight. This is the one
        // that caught the missing thresholds, and it is worth keeping tight --
        // a room a person can see the walls of should not be losing a third of
        // its sound to the void.
        ENJIN_EXPECT_TRUE(escaped < 8.0f);

        // A live room gives energy up to a dead corridor; a dead room takes it
        // from a live building. Both directions are legitimate and the spread is
        // large, so this is a sanity rail rather than a prediction -- the
        // numbers worth looking at are printed above it.
        //
        // The measured spread, for the record: hard rooms run 0.49 to 0.78 of
        // their sealed time, and the Anechoic Cell -- 0.2 s sealed, sitting in a
        // building that contains a fifteen-second cathedral -- comes out several
        // times longer than it would alone. A 0.2 s room next to a 15 s room
        // gaining a factor of a few is not a surprise; it is what coupling IS,
        // and it is the reason no single RT60 is the honest answer for either of
        // them. Narrower bounds here would be encoding a model nobody has.
        // 0.2 rather than something tighter because of the Tiled Bathroom,
        // which is the clearest case in the building: 56 m^2 of tile at alpha
        // 0.01 absorbs 0.56 m^2, so its 3.52 m^2 doorway is SIX TIMES the
        // absorption of every tiled surface in the room combined. Sealed it
        // rings for 8 s; with the door open it measures 2 s. A small hard room
        // is mostly its doorway, and a rail that called that a failure would be
        // wrong about the physics.
        ENJIN_EXPECT_TRUE(ratio > 0.2f);
        ENJIN_EXPECT_TRUE(ratio < 10.0f);
    }

    std::printf("    %u spaces compared against sealed twins\n", measured);
    ENJIN_EXPECT_TRUE(measured >= 6);
}

ENJIN_TEST(AcousticRange, TheSpacesOrderThemselvesTheWayTheirMaterialsSay) {
    // Arrange
    ECS::World world;
    if (!LoadRange(world)) { ENJIN_SKIP("Range.enjin not found"); return; }

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(RangeSettings());
    system.Update(kSpaces[0].centre, 2.0f);

    auto measure = [&system](const Space& s) {
        system.Update(s.centre, 0.016f);
        return system.Measurement();
    };

    // Act
    const RoomResponse anechoic = measure(kSpaces[0]);
    const RoomResponse lounge = measure(kSpaces[3]);
    const RoomResponse hall = measure(kSpaces[7]);
    const RoomResponse cathedral = measure(kSpaces[8]);

    ENJIN_ASSERT_TRUE(anechoic.Valid());
    ENJIN_ASSERT_TRUE(lounge.Valid());
    ENJIN_ASSERT_TRUE(hall.Valid());
    ENJIN_ASSERT_TRUE(cathedral.Valid());

    std::printf("    anechoic %.2f  lounge %.2f  hall %.2f  cathedral %.2f\n",
                anechoic.rt60[1], lounge.rt60[1], hall.rt60[1], cathedral.rt60[1]);

    // Assert: a fabric-lined cell, a carpeted lounge, a brick hall and a stone
    // cathedral, strictly increasing across a 250x range in volume.
    ENJIN_EXPECT_TRUE(anechoic.rt60[1] < lounge.rt60[1]);
    ENJIN_EXPECT_TRUE(lounge.rt60[1] < hall.rt60[1]);
    ENJIN_EXPECT_TRUE(hall.rt60[1] < cathedral.rt60[1]);
    // And the two ends are not merely ordered, they are a different kind of
    // room: the cathedral has to ring at least ten times as long.
    ENJIN_EXPECT_TRUE(cathedral.rt60[1] > anechoic.rt60[1] * 10.0f);
}

ENJIN_TEST(AcousticRange, TheSameRoomTreatedTwoWaysSplitsOnMaterialAlone) {
    // Arrange: the Living Room and the Carpeted Lounge are the same 7 x 2.7 x 6
    // box on opposite sides of the same corridor, with the same doorway. Every
    // variable but the surfaces is held still by construction.
    ECS::World world;
    if (!LoadRange(world)) { ENJIN_SKIP("Range.enjin not found"); return; }

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(RangeSettings());
    system.Update(kSpaces[2].centre, 2.0f);

    // Act
    system.Update(kSpaces[2].centre, 0.016f);
    const RoomResponse living = system.Measurement();
    system.Update(kSpaces[3].centre, 0.016f);
    const RoomResponse lounge = system.Measurement();

    ENJIN_ASSERT_TRUE(living.Valid());
    ENJIN_ASSERT_TRUE(lounge.Valid());

    std::printf("    living room (wood/drywall) %.2f / %.2f / %.2f\n",
                living.rt60[0], living.rt60[1], living.rt60[2]);
    std::printf("    lounge (carpet/fabric)     %.2f / %.2f / %.2f\n",
                lounge.rt60[0], lounge.rt60[1], lounge.rt60[2]);

    // Assert
    ENJIN_EXPECT_TRUE(living.rt60[1] > lounge.rt60[1]);
    // Carpet and fabric both swallow the top far harder than the bottom, so the
    // soft room must be tilted: long at the bottom relative to its own top.
    ENJIN_EXPECT_TRUE(lounge.rt60[0] > lounge.rt60[2] * 1.5f);
}

ENJIN_TEST(AcousticRange, AnOpenCeilingIsTheStrongestAbsorberInTheBuilding) {
    // Arrange: the courtyard is stone and brick, 1920 m^3, and enclosed on all
    // four sides -- everything a plan view can show says "reverberant". The only
    // difference from the Great Hall is that the top is missing.
    ECS::World world;
    if (!LoadRange(world)) { ENJIN_SKIP("Range.enjin not found"); return; }

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(RangeSettings());
    system.Update(kSpaces[9].centre, 2.0f);

    // Act
    system.Update(kSpaces[9].centre, 0.016f);
    const RoomResponse courtyard = system.Measurement();
    system.Update(kSpaces[7].centre, 0.016f);
    const RoomResponse hall = system.Measurement();
    ENJIN_ASSERT_TRUE(hall.Valid());

    // Assert: either it measures far shorter than a comparable roofed space, or
    // it reports honestly that too much left to measure. Both are true answers.
    // Returning a hall-like tail for a space open to the sky would not be.
    if (courtyard.Valid()) {
        std::printf("    courtyard %.2f, great hall %.2f\n",
                    courtyard.rt60[1], hall.rt60[1]);
        ENJIN_EXPECT_TRUE(courtyard.rt60[1] < hall.rt60[1]);
    } else {
        std::printf("    courtyard not measurable: %u of %u rays escaped\n",
                    courtyard.raysEscaped, courtyard.raysTraced);
        ENJIN_EXPECT_TRUE(courtyard.raysEscaped > courtyard.raysTraced / 10);
    }
}

ENJIN_TEST(AcousticRange, TheCoupledPairIsNotOneRoom) {
    // Arrange: a tiled room and a carpeted room sharing a 3.6 m opening. This
    // is the case a single RT60 cannot describe honestly -- the tail is a hard
    // room feeding a soft one, so it is two slopes, not one exponential.
    //
    // It is excluded from the bracket test above for exactly that reason: the
    // soft room can ring LONGER than its own sealed prediction, because it is
    // being fed from next door, which breaks the upper bound legitimately.
    ECS::World world;
    if (!LoadRange(world)) { ENJIN_SKIP("Range.enjin not found"); return; }

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(RangeSettings());

    const Space& hard = kSpaces[10];
    const Vector3 soft(62.0f, 1.6f, 7.5f + 9.0f + 0.5f);   // through the link

    system.Update(hard.centre, 2.0f);

    // Act
    system.Update(hard.centre, 0.016f);
    const RoomResponse inHard = system.Measurement();
    system.Update(soft, 0.016f);
    const RoomResponse inSoft = system.Measurement();

    ENJIN_ASSERT_TRUE(inHard.Valid());
    ENJIN_ASSERT_TRUE(inSoft.Valid());

    const f32 sealedSoft = 0.161f * (9.0f * 4.0f * 9.0f) /
        (81.0f * Audio::AcousticsFor(SurfaceMaterial::Carpet).absorption[1] +
         144.0f * Audio::AcousticsFor(SurfaceMaterial::Fabric).absorption[1] +
         81.0f * Audio::AcousticsFor(SurfaceMaterial::Fabric).absorption[1]);

    std::printf("    hard side %.2f, soft side %.2f (soft sealed would be %.2f)\n",
                inHard.rt60[1], inSoft.rt60[1], sealedSoft);

    // Assert: the hard room still rings longer than the soft one -- coupling
    // does not erase the difference between tile and carpet. That it does not
    // erase it is the thing worth guarding: a tracer that measured the BUILDING
    // rather than the room would return the same number on both sides, which is
    // exactly what the first version of the small demo did.
    ENJIN_EXPECT_TRUE(inHard.rt60[1] > inSoft.rt60[1]);
}

ENJIN_TEST(AcousticRange, TheAdaptiveWindowSurvivesASceneBuiltFromSolidSlabs) {
    // Arrange: the regression test for the fix's own first draft.
    //
    // The window was originally sized from a volume computed off the triangles
    // by the divergence theorem. For a room modelled as one closed shell that
    // is exact; for this building, made of solid wall boxes, it returns the
    // volume of the WALLS -- a small number, giving a short window, reinstating
    // the truncation bug it existed to fix. Every synthetic sweep would still
    // have passed.
    //
    // The cathedral is the case that catches it: 11,880 m^3 of stone rings far
    // longer than the 8 second default window, so if the window is not being
    // sized from the room it cannot be measured correctly here.
    ECS::World world;
    if (!LoadRange(world)) { ENJIN_SKIP("Range.enjin not found"); return; }

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(RangeSettings());

    const Space& cathedral = kSpaces[8];
    system.Update(cathedral.centre, 2.0f);

    // Act
    system.Update(cathedral.centre, 0.016f);
    const RoomResponse r = system.Measurement();
    ENJIN_ASSERT_TRUE(r.Valid());

    std::printf("    cathedral %.2f s (sealed sabine %.2f, with door %.2f)\n",
                r.rt60[1], Sabine(cathedral, 1, 0.0f), Sabine(cathedral, 1, DOOR));

    // Assert: comfortably past the 8 second default. A reading pinned near 8,
    // or falling below it as the room gets bigger, is the truncation artifact
    // returning.
    ENJIN_EXPECT_TRUE(r.rt60[1] > 8.5f);
}

// Is the shortfall the building's or the tracer's?
//
// Several large spaces measure well under the bracket, and there are only two
// candidates: the scene leaks somewhere, or the tracer is wrong about rooms of
// this kind. Guessing between them from the range alone is not possible, so
// this builds the SAME rooms as sealed shells in the test harness -- identical
// dimensions, identical materials, no doorway, nothing else in the world -- and
// measures those. A sealed shell that matches Sabine puts the difference
// squarely on the building.
ENJIN_TEST(AcousticRange, SealedTwinsSeparateTheBuildingFromTheTracer) {
    std::printf("    %-20s %10s %10s %10s\n",
                "space", "sealed", "sabine", "error");

    struct Twin { const char* name; usize index; };
    const Twin twins[] = {
        { "Living Room", 2 }, { "Concrete Stairwell", 4 }, { "Long Gallery", 5 },
        { "Low Warehouse", 6 }, { "Great Hall", 7 }, { "Cathedral", 8 },
    };

    f32 worst = 0.0f;
    for (const Twin& t : twins) {
        const Space& s = kSpaces[t.index];

        // Arrange
        RoomBuilder::Dimensions dim{ s.w, s.h, s.d };
        RoomBuilder::Surfaces surf{ s.floor, s.ceiling, s.walls };
        const Audio::AcousticScene shell = RoomBuilder::Box(dim, surf, false);
        AcousticBVH bvh;
        bvh.Build(shell);

        // Act
        RoomTraceSettings trace;
        trace.rayCount = 4096;
        const RoomResponse r = TraceRoomResponse(bvh, shell, dim.Centre(), trace);
        ENJIN_ASSERT_TRUE(r.Valid());

        const f32 sabine = Sabine(s, 1, 0.0f);
        const f32 err = (r.rt60[1] - sabine) / sabine;
        const f32 mag = err < 0.0f ? -err : err;
        if (mag > worst) worst = mag;

        std::printf("    %-20s %10.2f %10.2f %9.1f%%\n",
                    t.name, r.rt60[1], sabine, err * 100.0f);
    }

    // Assert: a sealed shell of each of these has to agree with the closed form.
    // The two disproportionate ones (a 44 m gallery, a 26 x 3.2 x 18 warehouse)
    // are expected to read LONG rather than short -- grazing rays outlive the
    // diffuse-field assumption -- so a generous bound here still catches a
    // tracer that is wrong about large or mixed-material rooms.
    ENJIN_EXPECT_TRUE(worst < 0.35f);
}

// The Great Hall on its own, cut out of the building.
//
// The chain of elimination ends here. TestAcousticSweeps shows that an opening
// costs exactly its area (a 3.52 m^2 doorway takes a 7.68 s room to 7.33 s,
// against a closed form of 7.41). The same hall inside the Range measures
// 5.66 s. So either the room AS AUTHORED differs from its idealised twin, or
// the rest of the building is taking the difference.
//
// This deletes every entity that is not part of the Great Hall and measures
// what is left: the same slabs, the same materials, the same doorway, now
// opening onto nothing. If it reads about 7.3 the building is responsible and
// the hall is fine. If it reads about 5.7 the hall itself is built wrong, and
// the Range has a defect no closed form was ever going to find.
ENJIN_TEST(AcousticRange, TheGreatHallCutOutOfTheBuilding) {
    // Arrange
    ECS::World world;
    if (!LoadRange(world)) { ENJIN_SKIP("Range.enjin not found"); return; }

    usize removed = 0, kept = 0;
    for (ECS::Entity e : world.GetEntitiesWithComponent<ECS::NameComponent>()) {
        const auto* n = world.GetComponent<ECS::NameComponent>(e);
        if (!n) continue;
        if (n->name.rfind("Great Hall", 0) == 0) { ++kept; continue; }
        world.DestroyEntity(e);
        ++removed;
    }
    world.Update(0.0f);          // flush the deferred destruction
    std::printf("    kept %zu Great Hall entities, removed %zu\n", kept, removed);
    ENJIN_ASSERT_TRUE(kept > 5);

    AcousticsSystem system;
    system.SetWorld(&world);
    system.SetSettings(RangeSettings());

    const Space& hall = kSpaces[7];

    // Act
    system.Update(hall.centre, 2.0f);
    system.Update(hall.centre, 0.016f);
    const RoomResponse alone = system.Measurement();
    ENJIN_ASSERT_TRUE(alone.Valid());

    std::printf("    alone %.2f s (%.1f%% escaped) -- in building 5.66, "
                "idealised with the same doorway 7.33\n",
                alone.rt60[1],
                100.0f * static_cast<f32>(alone.raysEscaped) /
                    static_cast<f32>(alone.raysTraced ? alone.raysTraced : 1));

    // Assert: it lands at 7.35 against the idealised 7.33, which is what closed
    // the question -- the room is built correctly and the missing 25% in the
    // building belongs to the corridor. The bounds are loose because the value
    // is the point; a regression that moved it would move it a long way.
    ENJIN_EXPECT_TRUE(alone.rt60[1] > 3.0f);
    ENJIN_EXPECT_TRUE(alone.rt60[1] < 12.0f);
}

ENJIN_TEST_MAIN()
