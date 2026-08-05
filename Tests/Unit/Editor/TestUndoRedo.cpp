#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Editor/UndoRedo.h"
#include "Enjin/Scene/SceneSerializer.h"

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
