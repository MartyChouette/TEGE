// The dialogue box must land ON SCREEN.
//
// It did not. BuildDialogueBoxUI wrote its anchor offsets in the intuitive sign
// (positive inset on the right, negative on the bottom), but UISystem resolves
// an edge as anchor * parentSize + offset, so the panel's top edge landed a
// whole design-height above the bottom anchor and every child had its right and
// bottom edges inverted. The result was a dialogue box nobody could see, which
// reads in-game as "the character text is missing".
//
// These tests drive the real DialogueSystem and the real UISystem layout, so
// they fail if either the offsets or the resolver convention drift apart again.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Systems/DialogueSystem.h"
#include "Enjin/GUI/DialogueTree.h"
#include "Enjin/GUI/UICanvas.h"
#include "Enjin/GUI/UISystem.h"

using namespace Enjin;
using namespace Enjin::ECS;

namespace {

constexpr f32 kViewW = 1280.0f;
constexpr f32 kViewH = 720.0f;

// An NPC carrying one line of dialogue, mid-conversation.
Entity MakeTalker(World& w) {
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"NPC"});
    w.AddComponent<TransformComponent>(e, TransformComponent{});

    DialogueComponent d;
    // Tree mode: DialogueSystem only drives entities whose tree has nodes.
    const u32 id = d.dialogueTree.AddNode(GUI::DialogueNodeType::Text);
    GUI::DialogueNode* node = d.dialogueTree.GetNode(id);
    node->speakerName = "Keeper";
    node->text = "Mind the shells.";
    d.dialogueTree.rootNodeId = id;
    w.AddComponent<DialogueComponent>(e, d);

    // The box UI is built only where DialogueBox, UICanvas and Dialogue sit on
    // the SAME entity (DialogueSystem::Update), which is how a scene authors it.
    w.AddComponent<GUI::UICanvasComponent>(e, GUI::UICanvasComponent{});
    w.AddComponent<DialogueBoxComponent>(e, DialogueBoxComponent{});
    return e;
}

// Lay the canvas out exactly as the runtime does and return the box's panel.
const GUI::UIElement* LayoutAndFindPanel(World& w, Entity talker) {
    GUI::UISystem ui;
    for (Entity e : w.GetEntitiesWithComponent<GUI::UICanvasComponent>()) {
        auto* canvas = w.GetComponent<GUI::UICanvasComponent>(e);
        auto* box = w.GetComponent<DialogueBoxComponent>(e);
        if (!canvas || !box) continue;
        // Layout only: UISystem::Update also DRAWS, and a headless test has no
        // ImGui context to draw into.
        ui.ComputeLayoutForCanvas(*canvas, kViewW, kViewH);
        return canvas->GetElement(box->panelElementId);
    }
    (void)talker;
    return nullptr;
}

} // namespace

ENJIN_TEST(DialogueLayout, BoxIsOnScreen) {
    // Arrange
    World w;
    Entity npc = MakeTalker(w);
    DialogueSystem sys;

    // Act
    sys.StartDialogue(&w, npc);
    sys.Update(&w, 1.0f / 60.0f);
    const GUI::UIElement* panel = LayoutAndFindPanel(w, npc);

    // Assert: a real rect, inside the viewport. The bug put the top edge a full
    // design height above the screen, so this is the assertion that catches it.
    ENJIN_ASSERT_TRUE(panel != nullptr);
    const auto& r = panel->computedRect;
    ENJIN_EXPECT_TRUE(r.w > 0.0f);
    ENJIN_EXPECT_TRUE(r.h > 0.0f);
    ENJIN_EXPECT_TRUE(r.y >= 0.0f);
    ENJIN_EXPECT_TRUE(r.y + r.h <= kViewH + 1.0f);
    ENJIN_EXPECT_TRUE(r.x >= 0.0f);
    ENJIN_EXPECT_TRUE(r.x + r.w <= kViewW + 1.0f);
}

ENJIN_TEST(DialogueLayout, BoxSitsAtTheBottom) {
    // A dialogue box belongs across the bottom of the screen, not floating in
    // the middle: it is anchored there and should span most of the width.
    World w;
    Entity npc = MakeTalker(w);
    DialogueSystem sys;

    sys.StartDialogue(&w, npc);
    sys.Update(&w, 1.0f / 60.0f);
    const GUI::UIElement* panel = LayoutAndFindPanel(w, npc);

    ENJIN_ASSERT_TRUE(panel != nullptr);
    const auto& r = panel->computedRect;
    ENJIN_EXPECT_TRUE(r.y > kViewH * 0.5f);      // lower half
    ENJIN_EXPECT_TRUE(r.w > kViewW * 0.5f);      // most of the width
}

ENJIN_TEST(DialogueLayout, TextElementsAreInsideThePanel) {
    // Speaker name and body text are children of the panel. With inverted
    // offsets they resolved outside it, which is the other half of the bug.
    World w;
    Entity npc = MakeTalker(w);
    DialogueSystem sys;

    sys.StartDialogue(&w, npc);
    sys.Update(&w, 1.0f / 60.0f);

    GUI::UISystem ui;
    bool checked = false;
    for (Entity e : w.GetEntitiesWithComponent<GUI::UICanvasComponent>()) {
        auto* canvas = w.GetComponent<GUI::UICanvasComponent>(e);
        auto* box = w.GetComponent<DialogueBoxComponent>(e);
        if (!canvas || !box) continue;
        ui.ComputeLayoutForCanvas(*canvas, kViewW, kViewH);
        const GUI::UIElement* panel = canvas->GetElement(box->panelElementId);
        ENJIN_ASSERT_TRUE(panel != nullptr);
        const auto& p = panel->computedRect;

        const u32 ids[2] = {box->speakerElementId, box->textElementId};
        for (u32 id : ids) {
            const GUI::UIElement* el = canvas->GetElement(id);
            if (!el) continue;
            const auto& r = el->computedRect;
            checked = true;
            ENJIN_EXPECT_TRUE(r.w > 0.0f);
            ENJIN_EXPECT_TRUE(r.h > 0.0f);
            ENJIN_EXPECT_TRUE(r.x >= p.x - 1.0f);
            ENJIN_EXPECT_TRUE(r.y >= p.y - 1.0f);
            ENJIN_EXPECT_TRUE(r.x + r.w <= p.x + p.w + 1.0f);
        }
    }
    ENJIN_EXPECT_TRUE(checked);
}

ENJIN_TEST_MAIN()
