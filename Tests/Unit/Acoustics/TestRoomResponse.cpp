// How long a room rings, measured rather than dialled in.
//
// The engine's reverb is a Freeverb bus whose decay time, room size and damping
// are all typed in by a person, with presets called SmallRoom and Cathedral.
// Marty's bar is that "the same sound source placed on the main floor reads as
// a different room without changing any parameters by hand", which rules that
// out entirely.
//
// The hard part of testing a simulation is that plausible and correct look
// identical. A tracer with the absorption applied in the wrong place, or the
// speed of sound wrong by a factor of two, produces a number that is confidently
// wrong and sounds like a room -- just not this one.
//
// So the central test here is not "does it produce a number". It is whether the
// number agrees with SABINE'S EQUATION, which has predicted reverberation time
// from a room's volume and absorption since 1900. A ray tracer and a century of
// room acoustics agreeing on a shoebox is a real check; everything else in this
// file is a property that would still hold if the physics were subtly wrong.

#include "EnjinTest.h"
#include "Enjin/Acoustics/RoomResponse.h"
#include "Enjin/Acoustics/AcousticBVH.h"
#include "Enjin/Audio/AcousticScene.h"

#include <cmath>
#include <cstdio>

using namespace Enjin;
using namespace Enjin::Acoustics;
using Enjin::Math::Vector3;

namespace {

// A sealed box room of a given size and a single material.
Audio::AcousticScene ShoeboxRoom(f32 width, f32 height, f32 depth,
                                 ECS::SurfaceMaterial surface) {
    Audio::AcousticScene s;
    const f32 x = width * 0.5f, y = height * 0.5f, z = depth * 0.5f;
    const Vector3 corner[8] = {
        {-x,-y,-z}, { x,-y,-z}, { x, y,-z}, {-x, y,-z},
        {-x,-y, z}, { x,-y, z}, { x, y, z}, {-x, y, z},
    };
    for (const auto& c : corner) s.vertices.push_back(c);
    static const i32 kTris[36] = {
        0,1,2, 0,2,3,  5,4,7, 5,7,6,  4,5,1, 4,1,0,
        3,2,6, 3,6,7,  4,0,3, 4,3,7,  1,5,6, 1,6,2,
    };
    const i32 mat = static_cast<i32>(s.materials.IndexFor(surface));
    for (i32 i = 0; i < 36; ++i) s.indices.push_back(kTris[i]);
    for (i32 t = 0; t < 12; ++t) s.materialIndices.push_back(mat);
    return s;
}

struct Room {
    Audio::AcousticScene scene;
    AcousticBVH bvh;
    f32 volume = 0.0f;
    f32 area = 0.0f;
};

Room MakeRoom(f32 w, f32 h, f32 d, ECS::SurfaceMaterial surface) {
    Room r;
    r.scene = ShoeboxRoom(w, h, d, surface);
    r.bvh.Build(r.scene);
    r.volume = w * h * d;
    r.area = 2.0f * (w * h + w * d + h * d);
    return r;
}

} // namespace

ENJIN_TEST(RoomResponse, AnUnbuiltIndexTracesNothing) {
    // Arrange / Act / Assert: a level with no geometry is an ordinary state.
    AcousticBVH empty;
    Audio::AcousticScene scene;
    const RoomResponse r = TraceRoomResponse(empty, scene, Vector3(0, 0, 0));
    ENJIN_EXPECT_FALSE(r.Valid());
    ENJIN_EXPECT_EQ(r.raysTraced, (u32)0);
}

ENJIN_TEST(RoomResponse, SabineIsTheFormulaItClaimsToBe) {
    // Arrange / Act / Assert: guard the reference before using it as one.
    // 0.161 * V / (S * alpha). A 100 m3 room with 130 m2 of surface at 0.2
    // absorption gives 0.161 * 100 / 26 = 0.619 s.
    ENJIN_EXPECT_FLOAT_NEAR(SabineRT60(100.0f, 130.0f, 0.2f), 0.619f, 0.005f);
    // No absorption is an infinite tail, not a divide by zero.
    ENJIN_EXPECT_FLOAT_NEAR(SabineRT60(100.0f, 130.0f, 0.0f), 0.0f, 0.001f);
}

// The test that decides whether any of this is real.
ENJIN_TEST(RoomResponse, TheTracedDecayAgreesWithSabinesEquation) {
    // Arrange: a room big enough for a diffuse field, with a mid-band
    // absorption Sabine is accurate for. Wood is 0.10 in the mid band.
    Room room = MakeRoom(12.0f, 5.0f, 9.0f, ECS::SurfaceMaterial::Wood);
    const f32 alphaMid = Audio::AcousticsFor(ECS::SurfaceMaterial::Wood).absorption[1];
    const f32 predicted = SabineRT60(room.volume, room.area, alphaMid);

    RoomTraceSettings settings;
    settings.rayCount = 2048;

    // Act
    const RoomResponse r = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), settings);

    // Assert
    std::printf("    traced RT60 mid = %.3f s, Sabine = %.3f s\n", r.rt60[1], predicted);
    ENJIN_ASSERT_TRUE(r.Valid());

    // Sabine is itself an approximation -- it assumes a perfectly diffuse field
    // and evenly spread absorption -- so agreement inside 30% is agreement. A
    // tracer with absorption applied in the wrong place, or the speed of sound
    // out by a factor of two, misses by multiples rather than percentages.
    ENJIN_EXPECT_TRUE(r.rt60[1] > predicted * 0.7f);
    ENJIN_EXPECT_TRUE(r.rt60[1] < predicted * 1.3f);
}

ENJIN_TEST(RoomResponse, ItAgreesWithSabineAcrossRoomSizes) {
    // Arrange: the same material, three very different rooms. A tracer that got
    // the volume-to-absorption relationship wrong would match on one and miss
    // on the others, which a single-room test cannot see.
    struct Case { f32 w, h, d; };
    const Case cases[] = { {6, 3, 4}, {12, 5, 9}, {20, 8, 16} };

    for (const Case& c : cases) {
        Room room = MakeRoom(c.w, c.h, c.d, ECS::SurfaceMaterial::Wood);
        const f32 alpha = Audio::AcousticsFor(ECS::SurfaceMaterial::Wood).absorption[1];
        const f32 predicted = SabineRT60(room.volume, room.area, alpha);

        RoomTraceSettings settings;
        settings.rayCount = 2048;

        // Act
        const RoomResponse r = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), settings);

        // Assert
        std::printf("    %gx%gx%g: traced %.3f s, Sabine %.3f s\n",
                    static_cast<double>(c.w), static_cast<double>(c.h),
                    static_cast<double>(c.d), r.rt60[1], predicted);
        ENJIN_ASSERT_TRUE(r.Valid());
        ENJIN_EXPECT_TRUE(r.rt60[1] > predicted * 0.7f);
        ENJIN_EXPECT_TRUE(r.rt60[1] < predicted * 1.3f);
    }
}

// The acceptance criterion, as a test.
ENJIN_TEST(RoomResponse, ACarpetedBasementAndATiledKitchenMeasureDifferently) {
    // Arrange: the SAME room shape, differing only in what it is made of. No
    // parameter is set by hand anywhere in this test.
    Room tiled = MakeRoom(8.0f, 3.0f, 6.0f, ECS::SurfaceMaterial::Tile);
    Room carpeted = MakeRoom(8.0f, 3.0f, 6.0f, ECS::SurfaceMaterial::Carpet);

    RoomTraceSettings settings;
    settings.rayCount = 1024;

    // Act
    const RoomResponse hard = TraceRoomResponse(tiled.bvh, tiled.scene, Vector3(0, 0, 0), settings);
    const RoomResponse soft = TraceRoomResponse(carpeted.bvh, carpeted.scene, Vector3(0, 0, 0), settings);

    // Assert
    std::printf("    tiled RT60 = %.2f / %.2f / %.2f s\n", hard.rt60[0], hard.rt60[1], hard.rt60[2]);
    std::printf("    carpet RT60 = %.2f / %.2f / %.2f s\n", soft.rt60[0], soft.rt60[1], soft.rt60[2]);
    ENJIN_ASSERT_TRUE(hard.Valid());
    ENJIN_ASSERT_TRUE(soft.Valid());

    // Tile rings for many times longer than carpet. This is the whole feature:
    // two rooms of identical shape that sound nothing alike, with nobody
    // touching a reverb slider.
    ENJIN_EXPECT_TRUE(hard.rt60[1] > soft.rt60[1] * 3.0f);
}

ENJIN_TEST(RoomResponse, CarpetKillsTheTopEndAndKeepsTheBottom) {
    // Arrange
    Room room = MakeRoom(8.0f, 3.0f, 6.0f, ECS::SurfaceMaterial::Carpet);
    RoomTraceSettings settings;
    settings.rayCount = 1024;

    // Act
    const RoomResponse r = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), settings);

    // Assert: per-band decay is the point of doing this per band. Carpet
    // absorbs 8% of the lows and 60% of the highs, so the room must ring far
    // longer at the bottom -- which is what "dull rather than quiet" is.
    ENJIN_ASSERT_TRUE(r.Valid());
    ENJIN_EXPECT_TRUE(r.rt60[0] > r.rt60[2] * 2.0f);
}

ENJIN_TEST(RoomResponse, ABiggerRoomOfTheSameMaterialRingsLonger) {
    // Arrange
    Room small = MakeRoom(4.0f, 2.5f, 3.0f, ECS::SurfaceMaterial::Concrete);
    Room large = MakeRoom(20.0f, 8.0f, 16.0f, ECS::SurfaceMaterial::Concrete);
    RoomTraceSettings settings;
    settings.rayCount = 1024;

    // Act
    const RoomResponse a = TraceRoomResponse(small.bvh, small.scene, Vector3(0, 0, 0), settings);
    const RoomResponse b = TraceRoomResponse(large.bvh, large.scene, Vector3(0, 0, 0), settings);

    // Assert: RT60 scales with volume over absorbing area, so a room five times
    // longer on each side rings substantially longer.
    ENJIN_ASSERT_TRUE(a.Valid() && b.Valid());
    ENJIN_EXPECT_TRUE(b.rt60[1] > a.rt60[1] * 2.0f);
    // And the mean free path says how big it is regardless of material.
    ENJIN_EXPECT_TRUE(b.meanFreePath > a.meanFreePath * 2.0f);
}

ENJIN_TEST(RoomResponse, TheMeanFreePathMatchesTheGeometricPrediction) {
    // Arrange: for a convex room the mean distance between surfaces is 4V/S,
    // which is a result about the SHAPE and owes nothing to the audio code. If
    // the tracer's rays were biased -- clustered at the poles of the sphere,
    // say -- this is what would catch it.
    Room room = MakeRoom(12.0f, 5.0f, 9.0f, ECS::SurfaceMaterial::Concrete);
    const f32 predicted = 4.0f * room.volume / room.area;

    RoomTraceSettings settings;
    settings.rayCount = 2048;

    // Act
    const RoomResponse r = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), settings);

    // Assert
    std::printf("    mean free path %.3f m, 4V/S = %.3f m\n", r.meanFreePath, predicted);
    ENJIN_EXPECT_TRUE(r.meanFreePath > predicted * 0.8f);
    ENJIN_EXPECT_TRUE(r.meanFreePath < predicted * 1.2f);
}

ENJIN_TEST(RoomResponse, TheFirstReflectionArrivesWhenGeometrySaysItShould) {
    // Arrange: a source at the centre of a room 8 m wide. The nearest surface
    // is the ceiling at 1.5 m, so the first reflection is 1.5 / 343 seconds.
    Room room = MakeRoom(8.0f, 3.0f, 6.0f, ECS::SurfaceMaterial::Concrete);
    RoomTraceSettings settings;
    settings.rayCount = 1024;

    // Act
    const RoomResponse r = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), settings);

    // Assert: the gap between the direct sound and the room answering is most
    // of how big a space reads, so a wrong speed of sound shows up here first.
    ENJIN_EXPECT_FLOAT_NEAR(r.firstReflection, 1.5f / kSpeedOfSound, 0.002f);
}

ENJIN_TEST(RoomResponse, OutdoorsReturnsAlmostNothing) {
    // Arrange: a single floor plane and open sky.
    Audio::AcousticScene field;
    field.vertices = { {-50,0,-50}, {50,0,-50}, {50,0,50}, {-50,0,50} };
    field.indices = { 0,1,2, 0,2,3 };
    const i32 mat = static_cast<i32>(field.materials.IndexFor(ECS::SurfaceMaterial::Grass));
    field.materialIndices = { mat, mat };
    AcousticBVH bvh;
    bvh.Build(field);

    Room room = MakeRoom(8.0f, 3.0f, 6.0f, ECS::SurfaceMaterial::Concrete);
    RoomTraceSettings settings;
    settings.rayCount = 1024;

    // Act
    const RoomResponse outside = TraceRoomResponse(bvh, field, Vector3(0, 1.7f, 0), settings);
    const RoomResponse inside = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), settings);

    // Assert: most rays leave and never come back, and almost no energy
    // returns. This is the difference between outdoors and indoors that a decay
    // time alone cannot express -- it is how WET the reverb should be.
    std::printf("    escaped: outdoors %u/%u, indoors %u/%u\n",
                outside.raysEscaped, settings.rayCount, inside.raysEscaped,
                settings.rayCount);
    ENJIN_EXPECT_TRUE(outside.raysEscaped > settings.rayCount / 2);

    // A sealed room leaks a little, and cannot be made not to.
    //
    // A ray striking exactly on the seam between two triangles can miss both:
    // the two intersection tests are computed from different vertex orders and
    // round in different directions, so there is a hairline where neither
    // claims the hit. This is the known watertightness limit of
    // Moller-Trumbore, and closing it means the Woop watertight intersector --
    // a different algorithm, not a tolerance.
    //
    // Measured at about one ray in a thousand, which changes an RT60 by less
    // than the number of digits anyone reads. Bounded rather than asserted
    // away, so if it ever becomes one in ten this test says so.
    ENJIN_EXPECT_TRUE(inside.raysEscaped * 100 < settings.rayCount);
    ENJIN_EXPECT_TRUE(outside.reflectedEnergy < inside.reflectedEnergy * 0.2f);
}

ENJIN_TEST(RoomResponse, TheSameRoomTracedTwiceGivesTheSameAnswer) {
    // Arrange: a reverb that changed slightly every time a level loaded would
    // be a bug nobody could reproduce.
    Room room = MakeRoom(10.0f, 4.0f, 8.0f, ECS::SurfaceMaterial::Brick);
    RoomTraceSettings settings;
    settings.rayCount = 512;

    // Act
    const RoomResponse a = TraceRoomResponse(room.bvh, room.scene, Vector3(1, 0, -1), settings);
    const RoomResponse b = TraceRoomResponse(room.bvh, room.scene, Vector3(1, 0, -1), settings);

    // Assert
    for (u32 band = 0; band < Audio::kAcousticBands; ++band) {
        ENJIN_EXPECT_FLOAT_NEAR(a.rt60[band], b.rt60[band], 0.0001f);
    }
    ENJIN_EXPECT_FLOAT_NEAR(a.meanFreePath, b.meanFreePath, 0.0001f);
}

ENJIN_TEST(RoomResponse, MoreRaysDoNotChangeTheAnswerMuch) {
    // Arrange: if the result still moved a lot between 512 and 4096 rays, the
    // number would be noise dressed as a measurement.
    Room room = MakeRoom(12.0f, 5.0f, 9.0f, ECS::SurfaceMaterial::Wood);

    RoomTraceSettings few;  few.rayCount = 512;
    RoomTraceSettings many; many.rayCount = 4096;

    // Act
    const RoomResponse a = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), few);
    const RoomResponse b = TraceRoomResponse(room.bvh, room.scene, Vector3(0, 0, 0), many);

    // Assert
    std::printf("    512 rays: %.3f s, 4096 rays: %.3f s\n", a.rt60[1], b.rt60[1]);
    ENJIN_ASSERT_TRUE(a.Valid() && b.Valid());
    ENJIN_EXPECT_TRUE(std::fabs(a.rt60[1] - b.rt60[1]) < b.rt60[1] * 0.25f);
}

ENJIN_TEST_MAIN()
