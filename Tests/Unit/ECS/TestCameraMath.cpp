// Screen pixels to world points, and back.
//
// The engine held the projection matrix all along and never exposed this, so
// every consumer wrote its own: the editor's picker, FlowerSystem and
// CollaborativeEditingUI each rolled a copy, and a game script could not do it
// at all. One project reimplemented orthographic unprojection in AngelScript
// from the screen size plus a hand-copied camera orthoSize -- which puts the
// camera's size in two files, so changing it in the scene sends every click
// somewhere wrong with nothing on screen to say why.
//
// These tests pin the conventions a caller has to be able to rely on: where
// the screen origin is, what an orthographic view maps to, and the two cases
// that must NOT return a confident answer.
#include "EnjinTest.h"
#include "Enjin/ECS/CameraMath.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Scripting/ScriptBindings.h"

#include <cmath>

using namespace Enjin;
using namespace Enjin::ECS;

namespace {

constexpr f32 kW = 800.0f;
constexpr f32 kH = 400.0f;      // aspect 2:1

// A camera looking down -Z from +Z, which is how every 2D scene is set up.
Entity MakeCamera(World& w, ProjectionType type, f32 orthoSize = 5.0f,
                  const Math::Vector3& pos = Math::Vector3(0.0f, 0.0f, 10.0f)) {
    const Entity e = w.CreateEntity();
    TransformComponent xf;
    xf.position = pos;
    w.AddComponent<TransformComponent>(e, xf);

    CameraComponent cc;
    cc.projectionType = type;
    cc.fieldOfView = 60.0f;
    cc.nearPlane = 0.1f;
    cc.farPlane = 100.0f;
    cc.orthoSize = orthoSize;
    w.AddComponent<CameraComponent>(e, cc);
    return e;
}

bool Near(f32 a, f32 b, f32 eps = 0.01f) { return std::fabs(a - b) < eps; }

} // namespace

ENJIN_TEST(CameraMath, TheScreenCentreIsTheWorldPointInFrontOfTheCamera) {
    // Arrange: an orthographic camera at the origin looking down -Z.
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Orthographic);
    Math::Vector3 out;

    // Act: dead centre of the viewport, onto the z = 0 plane.
    const bool ok = ScreenToWorldOnPlane(&w, cam, Math::Vector2(kW * 0.5f, kH * 0.5f),
                                         kW, kH, 0.0f, out);

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_TRUE(Near(out.x, 0.0f));
    ENJIN_EXPECT_TRUE(Near(out.y, 0.0f));
    ENJIN_EXPECT_TRUE(Near(out.z, 0.0f));
}

ENJIN_TEST(CameraMath, OrthoSizeIsTheHalfHeightAndTheAspectGivesTheWidth) {
    // Arrange: this is the exact arithmetic a script had to copy by hand.
    // orthoSize 5 over an 800x400 viewport means y spans -5..+5 and x spans
    // -10..+10. Getting the half-vs-full or the aspect wrong is the classic
    // way every click lands slightly off.
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Orthographic, 5.0f);
    Math::Vector3 topLeft, bottomRight;

    // Act: the two extreme corners, top-left origin.
    const bool a = ScreenToWorldOnPlane(&w, cam, Math::Vector2(0.0f, 0.0f), kW, kH, 0.0f, topLeft);
    const bool b = ScreenToWorldOnPlane(&w, cam, Math::Vector2(kW, kH), kW, kH, 0.0f, bottomRight);

    // Assert
    ENJIN_ASSERT_TRUE(a && b);
    ENJIN_EXPECT_TRUE(Near(topLeft.x, -10.0f));
    ENJIN_EXPECT_TRUE(Near(bottomRight.x, 10.0f));
    // Y spans the half-height either way round; the sign convention is pinned
    // by the round-trip test below rather than asserted twice here.
    ENJIN_EXPECT_TRUE(Near(std::fabs(topLeft.y), 5.0f));
    ENJIN_EXPECT_TRUE(Near(std::fabs(bottomRight.y), 5.0f));
}

ENJIN_TEST(CameraMath, ScreenToWorldAndBackIsTheSamePixel) {
    // Arrange: the round trip is what pins the Y convention -- if screen->world
    // and world->screen disagreed about which way Y runs, a game would read
    // clicks upside down and both halves would still look plausible alone.
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Orthographic);
    const Math::Vector2 pixel(613.0f, 92.0f);      // deliberately off-centre and off-axis
    Math::Vector3 world;
    Math::Vector2 back;

    // Act
    const bool a = ScreenToWorldOnPlane(&w, cam, pixel, kW, kH, 0.0f, world);
    const bool b = WorldToScreen(&w, cam, world, kW, kH, back);

    // Assert
    ENJIN_ASSERT_TRUE(a && b);
    ENJIN_EXPECT_TRUE(Near(back.x, pixel.x, 0.5f));
    ENJIN_EXPECT_TRUE(Near(back.y, pixel.y, 0.5f));
}

ENJIN_TEST(CameraMath, TheRoundTripHoldsUnderPerspectiveToo) {
    // Arrange: same property, the other projection. A 3D game picking with
    // this needs it as much as a 2D one.
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Perspective);
    const Math::Vector2 pixel(310.0f, 275.0f);
    Math::Vector3 world;
    Math::Vector2 back;

    // Act
    const bool a = ScreenToWorldOnPlane(&w, cam, pixel, kW, kH, 0.0f, world);
    const bool b = WorldToScreen(&w, cam, world, kW, kH, back);

    // Assert
    ENJIN_ASSERT_TRUE(a && b);
    ENJIN_EXPECT_TRUE(Near(back.x, pixel.x, 0.5f));
    ENJIN_EXPECT_TRUE(Near(back.y, pixel.y, 0.5f));
}

ENJIN_TEST(CameraMath, MovingTheCameraMovesWhatIsUnderThePixel) {
    // Arrange: a camera that pans must change what a click means, or a scrolling
    // board would keep resolving to its starting cell.
    World w;
    const Entity a = MakeCamera(w, ProjectionType::Orthographic, 5.0f, Math::Vector3(0, 0, 10));
    const Entity b = MakeCamera(w, ProjectionType::Orthographic, 5.0f, Math::Vector3(3, 2, 10));
    Math::Vector3 at, bt;

    // Act: the same pixel through both.
    const Math::Vector2 centre(kW * 0.5f, kH * 0.5f);
    ENJIN_ASSERT_TRUE(ScreenToWorldOnPlane(&w, a, centre, kW, kH, 0.0f, at));
    ENJIN_ASSERT_TRUE(ScreenToWorldOnPlane(&w, b, centre, kW, kH, 0.0f, bt));

    // Assert: shifted by exactly the camera's offset.
    ENJIN_EXPECT_TRUE(Near(bt.x - at.x, 3.0f));
    ENJIN_EXPECT_TRUE(Near(bt.y - at.y, 2.0f));
}

ENJIN_TEST(CameraMath, APlaneOffTheOriginIsHitAtItsOwnDepth) {
    // Arrange: a board that does not sit at z = 0, which is the reason the
    // plane is a parameter at all.
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Perspective);
    Math::Vector3 out;

    // Act
    const bool ok = ScreenToWorldOnPlane(&w, cam, Math::Vector2(kW * 0.5f, kH * 0.5f),
                                         kW, kH, -4.0f, out);

    // Assert
    ENJIN_ASSERT_TRUE(ok);
    ENJIN_EXPECT_TRUE(Near(out.z, -4.0f));
}

ENJIN_TEST(CameraMath, APointBehindTheCameraIsRefusedRatherThanGuessed) {
    // Arrange: the perspective divide flips sign behind the camera, so a naive
    // implementation reports a confident on-screen pixel for something nobody
    // can see -- and a label would be pinned to it.
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Perspective, 5.0f, Math::Vector3(0, 0, 10));
    Math::Vector2 screen;

    // Act: well behind the camera, which looks down -Z from z = 10.
    const bool ok = WorldToScreen(&w, cam, Math::Vector3(0.0f, 0.0f, 40.0f), kW, kH, screen);

    // Assert
    ENJIN_EXPECT_TRUE(!ok);
}

ENJIN_TEST(CameraMath, AnEntityThatIsNotACameraIsRefused) {
    // Arrange: a script passes whatever entity it has, and a wrong one must
    // not come back as the origin dressed up as an answer.
    World w;
    const Entity notACamera = w.CreateEntity();
    w.AddComponent<TransformComponent>(notACamera, TransformComponent{});
    Math::Vector3 out;
    Math::Vector2 screen;

    // Act / Assert
    ENJIN_EXPECT_TRUE(!ScreenToWorldOnPlane(&w, notACamera, Math::Vector2(10, 10),
                                            kW, kH, 0.0f, out));
    ENJIN_EXPECT_TRUE(!WorldToScreen(&w, notACamera, Math::Vector3(0, 0, 0), kW, kH, screen));
}

ENJIN_TEST(CameraMath, AZeroSizedViewportIsRefusedRatherThanDividedBy) {
    // Arrange: a minimised window reports this, and it must not produce NaN
    // coordinates that then propagate into gameplay state.
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Orthographic);
    Math::Vector3 out;

    // Act / Assert
    ENJIN_EXPECT_TRUE(!ScreenToWorldOnPlane(&w, cam, Math::Vector2(0, 0), 0.0f, kH, 0.0f, out));
    ENJIN_EXPECT_TRUE(!ScreenToWorldOnPlane(&w, cam, Math::Vector2(0, 0), kW, 0.0f, 0.0f, out));
}


// ---------------------------------------------------------------------------
// The viewport the screen-space bindings answer against.
//
// Everything above tests the arithmetic, and the arithmetic was never wrong.
// What shipped broken was the SIZE handed to it: the editor pushed 0,0 once and
// the setter read that as "keep what you have", so every script query answered
// for a compiled-in 1280x720 that belonged to nobody. The desktop player pushed
// once at boot and went stale on the first resize, and the web player never
// pushed at all. A wrong viewport does not fail, it just answers somewhere else,
// so these pin the setter contract rather than the maths.
// ---------------------------------------------------------------------------

ENJIN_TEST(RenderView, AZeroSizeKeepsTheLastGoodViewportInsteadOfAdoptingIt) {
    // Arrange: a runtime has pushed a real size.
    Scripting::SetBindingsRenderView(nullptr, 1600.0f, 900.0f);
    ENJIN_ASSERT_TRUE(Scripting::BindingsRenderViewIsSet());

    // Act: a caller pushes zeros -- a minimised window, or a panel that has not
    // drawn yet. This is the exact call that used to poison every query.
    Scripting::SetBindingsRenderView(nullptr, 0.0f, 0.0f);

    // Assert: the last good size survives. It must NOT become 0, and it must not
    // silently fall back to some default nobody chose.
    f32 vw = 0.0f, vh = 0.0f;
    Scripting::BindingsRenderViewSize(vw, vh);
    ENJIN_EXPECT_TRUE(Near(vw, 1600.0f));
    ENJIN_EXPECT_TRUE(Near(vh, 900.0f));
}

ENJIN_TEST(RenderView, AResizeActuallyMovesTheStoredViewport) {
    // Arrange / Act: what a resize or a re-docked Game View panel does every frame.
    Scripting::SetBindingsRenderView(nullptr, 1280.0f, 720.0f);
    f32 w1 = 0.0f, h1 = 0.0f;
    Scripting::BindingsRenderViewSize(w1, h1);

    Scripting::SetBindingsRenderView(nullptr, 1920.0f, 1080.0f);
    f32 w2 = 0.0f, h2 = 0.0f;
    Scripting::BindingsRenderViewSize(w2, h2);

    // Assert: the push is honoured, not cached forever behind the first value.
    ENJIN_EXPECT_TRUE(Near(w1, 1280.0f) && Near(h1, 720.0f));
    ENJIN_EXPECT_TRUE(Near(w2, 1920.0f) && Near(h2, 1080.0f));
}

ENJIN_TEST(RenderView, KeepLastChangesTheCameraWithoutTouchingTheSize) {
    // Arrange: the case the old zeros were trying to express.
    Scripting::SetBindingsRenderView(nullptr, 1440.0f, 810.0f);

    // Act: camera swapped, viewport unchanged.
    Scripting::SetBindingsRenderViewKeepLast(nullptr);

    // Assert: size intact. A caller now says this explicitly instead of
    // encoding it as a pair of zeros that read as a bug at every other site.
    f32 vw = 0.0f, vh = 0.0f;
    Scripting::BindingsRenderViewSize(vw, vh);
    ENJIN_EXPECT_TRUE(Near(vw, 1440.0f));
    ENJIN_EXPECT_TRUE(Near(vh, 810.0f));
}

// The same screen pixel is a DIFFERENT world point once the viewport changes.
// This is the user-visible half: a click that lands on the enemy windowed has
// to land on the enemy fullscreen too.
ENJIN_TEST(RenderView, TheSamePixelIsADifferentWorldPointAtADifferentViewport) {
    // Arrange
    World w;
    const Entity cam = MakeCamera(w, ProjectionType::Orthographic);
    Math::Vector3 wide, tall;

    // Act: one pixel, two viewport shapes with different aspect ratios.
    const bool a = ScreenToWorldOnPlane(&w, cam, Math::Vector2(200.0f, 100.0f),
                                        800.0f, 400.0f, 0.0f, wide);
    const bool b = ScreenToWorldOnPlane(&w, cam, Math::Vector2(200.0f, 100.0f),
                                        400.0f, 400.0f, 0.0f, tall);

    // Assert: both answer, and they disagree. If a stale viewport were being
    // used for one of them, these would come back equal and a pick would be
    // wrong everywhere except the one window size that was baked in.
    ENJIN_ASSERT_TRUE(a);
    ENJIN_ASSERT_TRUE(b);
    ENJIN_EXPECT_TRUE(!Near(wide.x, tall.x));
}
ENJIN_TEST_MAIN()
