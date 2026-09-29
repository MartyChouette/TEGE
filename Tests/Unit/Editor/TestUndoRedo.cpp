#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Editor/InspectorUndo.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Renderer/SceneRenderSettings.h"
#include <nlohmann/json.hpp>
#include <vector>

#include <memory>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Editor;

// ===========================================================================
// EntityEditCommand — the generic inspector-edit undo (JSON before/after)
// ===========================================================================

ENJIN_TEST(EntityEdit, UndoRedoRestoresPropertyValues) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    auto& light = w.AddComponent<LightComponent>(e);
    light.intensity = 1.0f;
    std::string before = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);

    w.GetComponent<LightComponent>(e)->intensity = 5.0f;  // the "inspector edit"
    std::string after = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);

    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<EntityEditCommand>(&w, e, before, after));
    // First Execute is a no-op: the edit is already live
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<LightComponent>(e)->intensity, 5.0f);

    mgr.Undo();
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<LightComponent>(e)->intensity, 1.0f);
    mgr.Redo();
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<LightComponent>(e)->intensity, 5.0f);
}

ENJIN_TEST(SceneSettingsEdit, test_scene_settings_edit_undo_redo_apply_snapshots) {
    // Arrange: the render settings as the Scene tab snapshots them. The edit
    // is live when the command is recorded, so the first Execute applies nothing.
    Renderer::SceneRenderSettings before;
    Renderer::SceneRenderSettings after;
    after.bloomIntensity = before.bloomIntensity + 1.0f;
    const std::string beforeJson = Renderer::SerializeRenderSettings(before).dump();
    const std::string afterJson = Renderer::SerializeRenderSettings(after).dump();
    std::vector<std::string> applied;
    UndoRedoManager mgr;

    // Act
    mgr.Execute(std::make_unique<SceneSettingsEditCommand>(
        [&](const std::string& j) { applied.push_back(j); }, beforeJson, afterJson));
    const usize afterExecute = applied.size();
    mgr.Undo();
    mgr.Redo();

    // Assert
    ENJIN_EXPECT_EQ(afterExecute, (usize)0);
    ENJIN_ASSERT_TRUE(applied.size() == 2);
    ENJIN_EXPECT_TRUE(applied[0] == beforeJson);
    ENJIN_EXPECT_TRUE(applied[1] == afterJson);
    ENJIN_EXPECT_FLOAT_EQ(Renderer::DeserializeRenderSettings(nlohmann::json::parse(applied[0])).bloomIntensity,
                          before.bloomIntensity);
}

ENJIN_TEST(UndoManager, test_undo_manager_insert_below_top_keeps_edit_order) {
    // Arrange: a raw edit (x: 0 -> 1) found after a wrapped command (y: 0 -> 5)
    // already landed on top of it. The raw one goes UNDER, so undo walks back
    // y first and then x, the order they happened in.
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    const std::string base = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);
    w.GetComponent<TransformComponent>(e)->position.x = 1.0f;
    const std::string rawAfter = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);
    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<PropertyEditCommand<f32>>("Y", 0.0f, 5.0f,
        [&](const f32& v) { w.GetComponent<TransformComponent>(e)->position.y = v; }));
    const u64 serialBefore = mgr.GetChangeSerial();

    // Act
    auto raw = std::make_unique<EntityEditCommand>(&w, e, base, rawAfter);
    raw->Execute();   // consume the first-Execute no-op, as the inspector does
    mgr.InsertBelowTop(std::move(raw));
    mgr.Undo();
    const f32 xAfterOne = w.GetComponent<TransformComponent>(e)->position.x;
    const f32 yAfterOne = w.GetComponent<TransformComponent>(e)->position.y;
    mgr.Undo();
    const f32 xAfterTwo = w.GetComponent<TransformComponent>(e)->position.x;
    mgr.Redo();
    mgr.Redo();

    // Assert
    ENJIN_EXPECT_TRUE(mgr.GetChangeSerial() > serialBefore);
    ENJIN_EXPECT_FLOAT_EQ(yAfterOne, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(xAfterOne, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(xAfterTwo, 0.0f);
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<TransformComponent>(e)->position.x, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<TransformComponent>(e)->position.y, 5.0f);
}

ENJIN_TEST(UndoManager, test_undo_manager_merged_execute_still_moves_serial) {
    // Arrange: two edits of one field merge into one command, so the undo
    // COUNT stays at 1 and cannot tell the second one landed
    f32 v = 0.0f;
    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<PropertyEditCommand<f32>>("V", 0.0f, 1.0f, [&](const f32& x) { v = x; }));
    const u64 s1 = mgr.GetChangeSerial();

    // Act
    mgr.Execute(std::make_unique<PropertyEditCommand<f32>>("V", 1.0f, 2.0f, [&](const f32& x) { v = x; }));

    // Assert
    ENJIN_EXPECT_EQ(mgr.GetUndoCount(), 1u);
    ENJIN_EXPECT_TRUE(mgr.LastExecuteMerged());
    ENJIN_EXPECT_TRUE(mgr.GetChangeSerial() == s1 + 1);
}

ENJIN_TEST(EntityEdit, UndoRemovesComponentAddedDuringEdit) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    std::string before = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);

    w.AddComponent<LightComponent>(e).intensity = 2.0f;
    std::string after = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);

    UndoRedoManager mgr;
    mgr.Execute(std::make_unique<EntityEditCommand>(&w, e, before, after));

    mgr.Undo();
    ENJIN_EXPECT_FALSE(w.HasComponent<LightComponent>(e));  // before-state has no light
    mgr.Redo();
    ENJIN_EXPECT_TRUE(w.HasComponent<LightComponent>(e));
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<LightComponent>(e)->intensity, 2.0f);
}

ENJIN_TEST(EntityEdit, DescriptionNamesTheChangedComponent) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    auto& light = w.AddComponent<LightComponent>(e);
    light.intensity = 1.0f;
    std::string before = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);
    w.GetComponent<LightComponent>(e)->intensity = 3.0f;
    std::string after = Scene::SceneSerializer::SerializeEntityToString(&w, e, false);

    EntityEditCommand cmd(&w, e, before, after);
    // History panel readability: "Edit light", not a generic label
    ENJIN_EXPECT_TRUE(std::string(cmd.GetDescription()).find("light") != std::string::npos);
}

// ===========================================================================
// History enumeration + JumpTo (the History panel's backbone)
// ===========================================================================

ENJIN_TEST(History, JumpToWalksBothDirections) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);

    UndoRedoManager mgr;
    mgr.SetMergeEnabled(false);
    for (int i = 1; i <= 3; ++i) {
        TransformComponent oldT = *w.GetComponent<TransformComponent>(e);
        TransformComponent newT = oldT;
        newT.position.x = static_cast<f32>(i) * 10.0f;
        mgr.Execute(std::make_unique<TransformCommand>(&w, e, oldT, newT));
    }
    ENJIN_EXPECT_EQ(mgr.GetUndoCount(), 3u);
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<TransformComponent>(e)->position.x, 30.0f);

    mgr.JumpTo(1);  // back to after action 1
    ENJIN_EXPECT_EQ(mgr.GetUndoCount(), 1u);
    ENJIN_EXPECT_EQ(mgr.GetRedoCount(), 2u);
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<TransformComponent>(e)->position.x, 10.0f);

    mgr.JumpTo(3);  // forward to the tip
    ENJIN_EXPECT_EQ(mgr.GetRedoCount(), 0u);
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<TransformComponent>(e)->position.x, 30.0f);

    mgr.JumpTo(0);  // "(start)"
    ENJIN_EXPECT_EQ(mgr.GetUndoCount(), 0u);
    ENJIN_EXPECT_FLOAT_EQ(w.GetComponent<TransformComponent>(e)->position.x, 0.0f);
}

ENJIN_TEST(History, DescriptionsEnumerateOldestFirst) {
    World w;
    Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e);
    w.AddComponent<NameComponent>(e, "A");

    UndoRedoManager mgr;
    mgr.SetMergeEnabled(false);
    TransformComponent t0 = *w.GetComponent<TransformComponent>(e);
    TransformComponent t1 = t0;
    t1.position.x = 5.0f;
    mgr.Execute(std::make_unique<TransformCommand>(&w, e, t0, t1));
    mgr.Execute(std::make_unique<RenameEntityCommand>(&w, e, "A", "B"));

    ENJIN_EXPECT_TRUE(std::string(mgr.GetUndoDescriptionAt(0)) == "Transform");
    ENJIN_EXPECT_TRUE(std::string(mgr.GetUndoDescriptionAt(1)) == "Rename");
    mgr.Undo();
    ENJIN_EXPECT_TRUE(std::string(mgr.GetRedoDescriptionAt(0)) == "Rename");
}

ENJIN_TEST_MAIN()
