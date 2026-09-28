// The built-in dialogue box, for a dialogue whose entity has no
// DialogueBoxComponent. It lived in the desktop player, so on web such a
// dialogue ran with nothing on screen. It is one engine function now and both
// players call it; these check it draws, and stands aside for a styled box.
#include "EnjinTest.h"
#include "Enjin/GUI/FallbackDialogueBox.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <imgui.h>

using namespace Enjin;

namespace {

// Runs one ImGui frame around the call and returns how many vertices it drew.
int VerticesDrawn(ECS::World& world, ECS::Entity active) {
    ImGuiContext* ctx = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    ImGui::NewFrame();
    GUI::DrawFallbackDialogueBox(&world, active, nullptr);
    ImGui::Render();
    const int vtx = ImGui::GetDrawData()->TotalVtxCount;
    ImGui::DestroyContext(ctx);
    return vtx;
}

ECS::Entity MakeTalker(ECS::World& world) {
    const ECS::Entity e = world.CreateEntity();
    ECS::DialogueComponent dlg;
    dlg.speakerName = "Guard";
    dlg.StartDialogue({"Halt. Who goes there?"});
    dlg.currentChar = 5;
    world.AddComponent<ECS::DialogueComponent>(e, dlg);
    return e;
}

} // namespace

ENJIN_TEST(FallbackDialogueBox, test_fallback_dialogue_box_active_dialogue_draws) {
    // Arrange
    ECS::World world;
    const ECS::Entity talker = MakeTalker(world);

    // Act
    const int drawn = VerticesDrawn(world, talker);

    // Assert
    ENJIN_EXPECT_TRUE(drawn > 0);
}

ENJIN_TEST(FallbackDialogueBox, test_fallback_dialogue_box_styled_box_draws_nothing) {
    // Arrange: a DialogueBoxComponent is drawn by the DialogueSystem, so the
    // fallback must not put a second box on top of it
    ECS::World world;
    const ECS::Entity talker = MakeTalker(world);
    world.AddComponent<ECS::DialogueBoxComponent>(talker, ECS::DialogueBoxComponent{});

    // Act
    const int drawn = VerticesDrawn(world, talker);

    // Assert
    ENJIN_EXPECT_EQ(drawn, 0);
}

ENJIN_TEST(FallbackDialogueBox, test_fallback_dialogue_box_no_active_entity_draws_nothing) {
    // Arrange
    ECS::World world;
    MakeTalker(world);

    // Act
    const int drawn = VerticesDrawn(world, ECS::Entity{});

    // Assert
    ENJIN_EXPECT_EQ(drawn, 0);
}

ENJIN_TEST_MAIN()
