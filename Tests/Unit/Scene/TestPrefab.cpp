#include "EnjinTest.h"
#include "Enjin/Assets/Prefab.h"

#include <string>

using namespace Enjin;
using namespace Enjin::Assets;

// ===========================================================================
// PrefabComponentData
// ===========================================================================

ENJIN_TEST(ComponentData, EmptyByDefault) {
    PrefabComponentData cd;
    ENJIN_EXPECT_TRUE(cd.typeName.empty());
    ENJIN_EXPECT_EQ(cd.stringProperties.size(), (size_t)0);
    ENJIN_EXPECT_EQ(cd.floatProperties.size(), (size_t)0);
    ENJIN_EXPECT_EQ(cd.intProperties.size(), (size_t)0);
    ENJIN_EXPECT_EQ(cd.boolProperties.size(), (size_t)0);
}

ENJIN_TEST(ComponentData, PropertyStorage) {
    PrefabComponentData cd;
    cd.typeName = "TransformComponent";
    cd.floatProperties["posX"] = 1.0f;
    cd.floatProperties["posY"] = 2.0f;
    cd.stringProperties["name"] = "Player";
    cd.boolProperties["visible"] = true;
    cd.intProperties["layer"] = 5;

    ENJIN_EXPECT_FLOAT_EQ(cd.floatProperties["posX"], 1.0f);
    ENJIN_EXPECT_STR_EQ(cd.stringProperties["name"].c_str(), "Player");
    ENJIN_EXPECT_TRUE(cd.boolProperties["visible"]);
    ENJIN_EXPECT_EQ(cd.intProperties["layer"], 5);
}

// ===========================================================================
// PrefabEntityData
// ===========================================================================

ENJIN_TEST(EntityData, DefaultValues) {
    PrefabEntityData ed;
    ENJIN_EXPECT_TRUE(ed.name.empty());
    ENJIN_EXPECT_EQ(ed.parentIndex, -1);
    ENJIN_EXPECT_EQ(ed.components.size(), (size_t)0);
}

ENJIN_TEST(EntityData, AddComponents) {
    PrefabEntityData ed;
    ed.name = "Cube";
    PrefabComponentData c1;
    c1.typeName = "MeshComponent";
    PrefabComponentData c2;
    c2.typeName = "MaterialComponent";
    ed.components.push_back(c1);
    ed.components.push_back(c2);
    ENJIN_EXPECT_EQ(ed.components.size(), (size_t)2);
    ENJIN_EXPECT_STR_EQ(ed.components[0].typeName.c_str(), "MeshComponent");
}

// ===========================================================================
// Prefab Class
// ===========================================================================

ENJIN_TEST(Prefab, DefaultState) {
    Prefab prefab;
    ENJIN_EXPECT_STR_EQ(prefab.GetName().c_str(), "Prefab");
    ENJIN_EXPECT_TRUE(prefab.GetPath().empty());
    ENJIN_EXPECT_TRUE(prefab.IsEmpty());
}

ENJIN_TEST(Prefab, NamedConstruction) {
    Prefab prefab("MyPrefab");
    ENJIN_EXPECT_STR_EQ(prefab.GetName().c_str(), "MyPrefab");
}

ENJIN_TEST(Prefab, SetNameAndPath) {
    Prefab prefab;
    prefab.SetName("Tree");
    prefab.SetPath("/assets/tree.enjprefab");
    ENJIN_EXPECT_STR_EQ(prefab.GetName().c_str(), "Tree");
    ENJIN_EXPECT_STR_EQ(prefab.GetPath().c_str(), "/assets/tree.enjprefab");
}

ENJIN_TEST(Prefab, AddEntity) {
    Prefab prefab;
    PrefabEntityData entity;
    entity.name = "Root";
    prefab.AddEntity(entity);
    ENJIN_EXPECT_FALSE(prefab.IsEmpty());
    ENJIN_EXPECT_EQ(prefab.GetEntities().size(), (size_t)1);
    ENJIN_EXPECT_STR_EQ(prefab.GetEntities()[0].name.c_str(), "Root");
}

ENJIN_TEST(Prefab, Clear) {
    Prefab prefab;
    PrefabEntityData e;
    e.name = "Child";
    prefab.AddEntity(e);
    prefab.Clear();
    ENJIN_EXPECT_TRUE(prefab.IsEmpty());
}

ENJIN_TEST(Prefab, UniqueIds) {
    Prefab a("A");
    Prefab b("B");
    ENJIN_EXPECT_TRUE(a.GetId() != b.GetId());
}

ENJIN_TEST(Prefab, RootCount) {
    Prefab prefab;
    PrefabEntityData root;
    root.name = "Root";
    root.parentIndex = -1;
    PrefabEntityData child;
    child.name = "Child";
    child.parentIndex = 0;
    prefab.AddEntity(root);
    prefab.AddEntity(child);
    ENJIN_EXPECT_EQ(prefab.GetRootCount(), 1u);
}

// ===========================================================================
// PrefabInstanceComponent
// ===========================================================================

ENJIN_TEST(Instance, DefaultValues) {
    PrefabInstanceComponent inst;
    ENJIN_EXPECT_EQ(inst.prefabId, (u64)0);
    ENJIN_EXPECT_TRUE(inst.prefabPath.empty());
    ENJIN_EXPECT_FALSE(inst.overridesApplied);
}

ENJIN_TEST_MAIN()
