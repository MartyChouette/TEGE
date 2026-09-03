// Cloth draping over a collidable rope (RopeComponent::collidable).
//
// A rope with collidable set contributes one capsule per simulated segment to
// the cloth collision gather, so fabric interacts with the line instead of
// passing through it.
//
// The subtle part is collisionRadius. Cloth is a grid of POINTS, so a point is
// only caught if it lands within the capsule radius; a rope thinner than about
// the cloth's point spacing is invisible to it and the sheet drops straight
// through. That was measured, not guessed - see ThinCollisionRadiusSlipsThrough.
//
// These tests also record a LIMITATION found while building the Playground
// clothesline: the contact model has no static friction, so a fully unpinned
// sheet laid across a rope always feeds over it and falls off eventually, at
// every collision radius and cloth resolution tried. The shipped clothesline
// therefore pins each garment's top edge to the line (still colliding with it),
// which is stable and reads correctly.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Cloth.h"
#include "Enjin/ECS/Components/Rope.h"
#include "Enjin/Gameplay/ClothSystem.h"
#include <algorithm>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

constexpr f32 kLineY = 4.0f;
constexpr f32 kLineZ = 12.0f;
constexpr f32 kSheetW = 1.2f;
constexpr f32 kSheetH = 1.6f;

// A near-taut horizontal rope along +X at (.., kLineY, kLineZ), anchored both ends.
Entity MakeLine(World& w, bool collidable, f32 collisionRadius = 0.22f) {
    Entity anchorB = w.CreateEntity();
    w.AddComponent<NameComponent>(anchorB, NameComponent{"LineEnd"});
    TransformComponent tb;
    tb.position = Vector3(7.0f, kLineY, kLineZ);
    w.AddComponent<TransformComponent>(anchorB, tb);

    Entity rope = w.CreateEntity();
    w.AddComponent<NameComponent>(rope, NameComponent{"Line"});
    TransformComponent tr;
    tr.position = Vector3(0.0f, kLineY, kLineZ);
    w.AddComponent<TransformComponent>(rope, tr);
    RopeComponent r;
    r.length = 7.05f;          // barely longer than the 7.0 span: a gentle sag
    r.segments = 26;
    r.thickness = 0.05f;       // drawn thin...
    r.collisionRadius = collisionRadius;   // ...but collides fat
    r.iterations = 16;
    r.endAttachName = "LineEnd";
    r.pinBottom = true;        // second anchor, not a dangling weight
    r.useWeatherWind = false;
    r.collidable = collidable;
    w.AddComponent<RopeComponent>(rope, r);
    w.AddComponent<MeshComponent>(rope, MeshComponent{});
    return rope;
}

// An unpinned sheet lying flat above the line, centred across it. -90 degrees
// about X turns the default hang-down sheet into a horizontal one.
Entity MakeSheet(World& w, f32 x, f32 y, i32 rx = 10, i32 ry = 12, f32 drop = 0.15f) {
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Sheet"});
    TransformComponent t;
    t.position = Vector3(x, y, kLineZ - kSheetH * 0.5f);
    t.rotation = Quaternion::FromEulerDegrees(Vector3(-90.0f, 0.0f, 0.0f));
    w.AddComponent<TransformComponent>(e, t);
    ClothComponent c;
    c.width = kSheetW;
    c.height = kSheetH;
    c.resX = rx;
    c.resY = ry;
    c.pin = ClothPin::None;    // nothing holds it up but the rope
    c.tearable = false;
    c.collide = true;
    c.friction = 0.85f;
    c.useWeatherWind = false;
    w.AddComponent<ClothComponent>(e, c);
    w.AddComponent<MeshComponent>(e, MeshComponent{});
    return e;
}

void Simulate(World& w, int steps) {
    Gameplay::ClothSystem sys;
    for (int i = 0; i < steps; ++i) sys.Update(&w, 1.0f / 60.0f);
}

f32 MinY(const ClothComponent& c) {
    f32 m = 1e9f;
    for (const auto& p : c.positions) m = std::min(m, p.y);
    return m;
}

} // namespace

ENJIN_TEST(ClothDrape, CollidableRopeSlowsAnUnpinnedSheet) {
    // A collidable rope DOES collide: an unpinned sheet dropped on it is held up
    // for a while and falls far less than one in free space. It does not stay
    // there, though -- see UnpinnedSheetEventuallySlidesOff for why, and why the
    // Playground pins its laundry to the line instead.
    World w;
    MakeLine(w, /*collidable=*/true);
    Entity sheet = MakeSheet(w, 3.5f, kLineY + 0.15f);

    Simulate(w, 60);   // one second

    const auto* c = w.GetComponent<ClothComponent>(sheet);
    ENJIN_ASSERT_TRUE(c != nullptr);
    // Free fall for 1s from 4.15 would be about -0.75; the rope keeps it up.
    ENJIN_EXPECT_TRUE(MinY(*c) > 0.0f);
}

ENJIN_TEST(ClothDrape, UnpinnedSheetEventuallySlidesOff) {
    // Documented limitation, measured not assumed. The contact model damps
    // velocity but has no STATIC friction, so a sheet lying across a round
    // support slowly feeds over it and drops off the end. Sweeping collision
    // radius 0.05 -> 0.8 and cloth resolution 10x12 -> 40x48 never changed the
    // outcome, only how long it took. Making a real unpinned drape hold needs
    // capstan/static friction in ResolvePoint, which is a solver change, not a
    // tuning one. Until then, laundry is pinned to the line.
    World w;
    MakeLine(w, /*collidable=*/true);
    Entity sheet = MakeSheet(w, 3.5f, kLineY + 0.15f);

    Simulate(w, 420);   // seven seconds

    const auto* c = w.GetComponent<ClothComponent>(sheet);
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_EXPECT_TRUE(MinY(*c) < kLineY - 6.0f);
}

ENJIN_TEST(ClothDrape, PinnedClothHangsFromTheLine) {
    // What the Playground actually ships: the garment's top edge is pinned along
    // the rope, so it hangs off the line and stays there. This is the stable
    // configuration and the one that reads as laundry.
    World w;
    MakeLine(w, /*collidable=*/true);

    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Garment"});
    TransformComponent t;
    t.position = Vector3(3.5f, 3.3f, kLineZ);   // on the sagging line
    w.AddComponent<TransformComponent>(e, t);
    ClothComponent c;
    c.width = kSheetW;
    c.height = kSheetH;
    c.resX = 10;
    c.resY = 12;
    c.pin = ClothPin::TopEdge;
    c.tearable = false;
    c.collide = true;
    c.useWeatherWind = false;
    w.AddComponent<ClothComponent>(e, c);
    w.AddComponent<MeshComponent>(e, MeshComponent{});

    Simulate(w, 420);

    const auto* cc = w.GetComponent<ClothComponent>(e);
    ENJIN_ASSERT_TRUE(cc != nullptr);
    // Hangs its full height below the line and stays put.
    ENJIN_EXPECT_TRUE(MinY(*cc) < 3.3f);
    ENJIN_EXPECT_TRUE(MinY(*cc) > 3.3f - kSheetH - 0.5f);
}

ENJIN_TEST(ClothDrape, NonCollidableRopeLetsClothFallThrough) {
    // Arrange: identical setup with collidable OFF - the control case, and the
    // state the Playground was in before (laundry hung beside the line).
    World w;
    MakeLine(w, /*collidable=*/false);
    Entity sheet = MakeSheet(w, 3.5f, kLineY + 0.15f);

    // Act
    Simulate(w, 420);

    // Assert: nothing catches it
    const auto* c = w.GetComponent<ClothComponent>(sheet);
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_EXPECT_TRUE(MinY(*c) < kLineY - 6.0f);
}

ENJIN_TEST(ClothDrape, ThinCollisionRadiusSlipsThrough) {
    // Cloth is a grid of points, so a rope thinner than the point spacing has
    // nothing to catch: this is why collisionRadius exists separately from the
    // rendered thickness. Sheet spacing here is 1.2/10 = 0.12; a 0.05 radius
    // (the Playground clothesline's VISUAL thickness) is below that.
    World w;
    MakeLine(w, /*collidable=*/true, /*collisionRadius=*/0.05f);
    Entity sheet = MakeSheet(w, 3.5f, kLineY + 0.15f);

    Simulate(w, 420);

    const auto* c = w.GetComponent<ClothComponent>(sheet);
    ENJIN_ASSERT_TRUE(c != nullptr);
    ENJIN_EXPECT_TRUE(MinY(*c) < kLineY - 6.0f);   // slipped between the points
}

ENJIN_TEST(ClothDrape, CollisionRadiusDefaultsToThickness) {
    // collisionRadius 0 must mean "use the rendered thickness", so ropes
    // authored before the field existed keep their old behaviour.
    RopeComponent r;
    ENJIN_EXPECT_FLOAT_EQ(r.collisionRadius, 0.0f);
    ENJIN_EXPECT_FALSE(r.collidable);
}

ENJIN_TEST(ClothDrape, RopeDoesNotCollideWithItself) {
    // A collidable rope must skip its OWN capsules (srcIndex self-exclusion) or
    // every segment fights its neighbours and the line explodes.
    World w;
    Entity rope = MakeLine(w, /*collidable=*/true);
    Simulate(w, 240);

    const auto* r = w.GetComponent<RopeComponent>(rope);
    ENJIN_ASSERT_TRUE(r != nullptr);
    ENJIN_ASSERT_TRUE(r->positions.size() > 2);
    // Still a sane line: inside the span, near the anchor height.
    for (const auto& p : r->positions) {
        ENJIN_EXPECT_TRUE(p.x > -1.0f && p.x < 8.0f);
        ENJIN_EXPECT_TRUE(p.y < kLineY + 0.5f && p.y > kLineY - 3.0f);
    }
}

ENJIN_TEST_MAIN()
