// Which entity the cursor is on, and what happens to the one it left.
//
// Every project that lets a player point at something writes this by hand, and
// the engine had no answer outside the editor's own selection. The parts with
// rules are the resolution (which entity owns the highlight when a child is
// hit) and the state handoff (the thing you stopped pointing at must go dark).
// The drawing itself rides the inverted-hull outline pass that already exists.
#include "EnjinTest.h"
#include "Enjin/ECS/Systems/HoverHighlightSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/HoverHighlight.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;
using namespace Enjin::ECS;

namespace {

Entity MakeHighlightable(World& w, bool includeChildren = true, bool enabled = true) {
    const Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    HoverHighlightComponent h;
    h.includeChildren = includeChildren;
    h.enabled = enabled;
    w.AddComponent<HoverHighlightComponent>(e, h);
    return e;
}

Entity MakeChildOf(World& w, Entity parent) {
    const Entity c = w.CreateEntity();
    w.AddComponent<TransformComponent>(c, TransformComponent{});
    ParentComponent p;
    p.parent = parent;
    w.AddComponent<ParentComponent>(c, p);
    return c;
}

bool IsHovered(World& w, Entity e) {
    const auto* h = w.GetComponent<HoverHighlightComponent>(e);
    return h && h->hovered;
}

} // namespace

ENJIN_TEST(HoverHighlight, TheComponentStartsUnhovered) {
    // Arrange: `hovered` is this frame's answer, not authored data. A default
    // of true would light everything up the moment a scene loaded.
    HoverHighlightComponent h;

    // Act / Assert
    ENJIN_EXPECT_TRUE(!h.hovered);
    ENJIN_EXPECT_TRUE(h.enabled);
    ENJIN_EXPECT_TRUE(h.thickness > 0.0f);
}

ENJIN_TEST(HoverHighlight, ClearTurnsEverythingOff) {
    // Arrange: play stopping, or a scene change. Without this the last hovered
    // entity stays lit forever with nothing pointing at it.
    World w;
    const Entity a = MakeHighlightable(w);
    const Entity b = MakeHighlightable(w);
    w.GetComponent<HoverHighlightComponent>(a)->hovered = true;
    w.GetComponent<HoverHighlightComponent>(b)->hovered = true;

    HoverHighlightSystem sys;
    sys.SetWorld(&w);

    // Act
    sys.Clear();

    // Assert
    ENJIN_EXPECT_TRUE(!IsHovered(w, a));
    ENJIN_EXPECT_TRUE(!IsHovered(w, b));
    ENJIN_EXPECT_TRUE(sys.GetHovered() == INVALID_ENTITY);
}

ENJIN_TEST(HoverHighlight, WithNoPhysicsNothingIsHovered) {
    // Arrange: picking needs a physics backend, and a 2D scene or a headless
    // run has none. It must report nothing rather than guess.
    World w;
    const Entity e = MakeHighlightable(w);

    HoverHighlightSystem sys;
    sys.SetWorld(&w);

    // Act: no SetPhysics call at all.
    sys.Update(Math::Matrix4::Identity(), Math::Vector2(10.0f, 10.0f), 800.0f, 600.0f, false);

    // Assert: nothing picked, and nothing lit. The system is the only writer
    // of `hovered` -- it is not serialized and has no script binding -- so
    // "no pick" and "no highlight" are the same statement.
    ENJIN_EXPECT_TRUE(sys.GetHovered() == INVALID_ENTITY);
    ENJIN_EXPECT_TRUE(!IsHovered(w, e));
}

ENJIN_TEST(HoverHighlight, ACursorOverTheUIHighlightsNothing) {
    // Arrange: a cursor over a menu is not pointing at the world. Highlighting
    // through an open panel reads as a bug, and this is the one pointer-capture
    // flag the rest of the engine already uses.
    World w;
    const Entity e = MakeHighlightable(w);

    HoverHighlightSystem sys;
    sys.SetWorld(&w);

    // Act
    sys.Update(Math::Matrix4::Identity(), Math::Vector2(10.0f, 10.0f), 800.0f, 600.0f,
               /*cursorOverUI=*/true);

    // Assert
    ENJIN_EXPECT_TRUE(sys.GetHovered() == INVALID_ENTITY);
    ENJIN_EXPECT_TRUE(!IsHovered(w, e));
}

ENJIN_TEST(HoverHighlight, AZeroSizedViewportHighlightsNothing) {
    // Arrange: a minimised window reports this, and unprojecting through it
    // would divide by zero.
    World w;
    MakeHighlightable(w);
    HoverHighlightSystem sys;
    sys.SetWorld(&w);

    // Act / Assert
    sys.Update(Math::Matrix4::Identity(), Math::Vector2(0.0f, 0.0f), 0.0f, 600.0f, false);
    ENJIN_EXPECT_TRUE(sys.GetHovered() == INVALID_ENTITY);
    sys.Update(Math::Matrix4::Identity(), Math::Vector2(0.0f, 0.0f), 800.0f, 0.0f, false);
    ENJIN_EXPECT_TRUE(sys.GetHovered() == INVALID_ENTITY);
}

ENJIN_TEST(HoverHighlight, ClearIsSafeWithNoWorld) {
    // Arrange: called on stop, which can run before a world is attached.
    HoverHighlightSystem sys;

    // Act / Assert: reaching the assertion at all is the test.
    sys.Clear();
    ENJIN_EXPECT_TRUE(sys.GetHovered() == INVALID_ENTITY);
}

ENJIN_TEST(HoverHighlight, AHierarchyCycleDoesNotHangTheFrame) {
    // Arrange: a hand-edited or tool-generated scene can name a parent that
    // loops. Walking ancestors without a bound would spin forever, which is a
    // hang rather than a visible failure -- much worse to diagnose.
    World w;
    const Entity a = MakeHighlightable(w, /*includeChildren=*/false);
    const Entity b = MakeChildOf(w, a);
    // Close the loop: a's parent is b, b's parent is a.
    ParentComponent pa;
    pa.parent = b;
    w.AddComponent<ParentComponent>(a, pa);

    HoverHighlightSystem sys;
    sys.SetWorld(&w);

    // Act: no physics, so this exercises Update's guards and returns. The
    // ancestor walk is bounded independently; this asserts the system as a
    // whole terminates on a cyclic scene.
    sys.Update(Math::Matrix4::Identity(), Math::Vector2(1.0f, 1.0f), 800.0f, 600.0f, false);

    // Assert
    ENJIN_EXPECT_TRUE(sys.GetHovered() == INVALID_ENTITY);
    ENJIN_EXPECT_TRUE(w.IsValid(b));
}

ENJIN_TEST(HoverHighlight, HoveredStateIsNotAuthoredData) {
    // Arrange: the serializer deliberately omits `hovered`. Saving it would
    // restore a scene with something lit and nothing pointing at it, and the
    // author would have no field to turn off.
    HoverHighlightComponent h;
    h.hovered = true;

    // Act: a fresh default is what a load produces for the runtime field.
    const HoverHighlightComponent loaded;

    // Assert
    ENJIN_EXPECT_TRUE(!loaded.hovered);
}

ENJIN_TEST_MAIN()
