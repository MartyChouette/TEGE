// Entity ids and parent references, as they reach a file and come back.
//
// A scene writer stored the whole 64-bit entity handle where it meant the
// 32-bit slot index, so any entity whose slot had been recycled once was
// written as (generation << 32) | index. Marty's scan of 55 scenes across 20
// projects found four of them, every one generation 1, and in each case the
// low word was exactly the id missing from an otherwise contiguous sequence.
//
// Nothing looked wrong: the loader keyed its remap table on whatever the file
// said, so a corrupt file was self-consistent. It rendered, transformed and
// parented correctly, and only a tool reading the file could tell -- which is
// what makes it worth a test rather than a fix and a shrug.

#include "EnjinTest.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"

#include <nlohmann/json.hpp>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;
using json = nlohmann::json;

namespace {

// Recycle a slot so the next entity out of it carries generation 1. That is
// the precondition for the whole bug, and it costs two lines.
Entity MakeRecycledEntity(World& world, const char* name) {
    Entity throwaway = world.CreateEntity();
    world.DestroyEntity(throwaway);
    world.Update(0.0f);                 // destroys are deferred to the flush

    Entity e = world.CreateEntity();
    world.AddComponent<NameComponent>(e, NameComponent{ name });
    world.AddComponent<TransformComponent>(e);
    return e;
}

json ParseScene(const std::string& text) {
    return json::parse(text, nullptr, false);
}

} // namespace

ENJIN_TEST(SceneEntityIds, RecycledSlotStillWritesAPlainIndex) {
    World world;
    Entity e = MakeRecycledEntity(world, "Recycled");

    // The precondition: this handle really does carry a generation.
    ENJIN_ASSERT_TRUE(static_cast<u64>(e) > 0xFFFFFFFFull);

    Scene::SceneSerializer serializer(&world);
    json scene = ParseScene(serializer.SaveToString());
    ENJIN_ASSERT_FALSE(scene.is_discarded());
    ENJIN_ASSERT_TRUE(scene.contains("entities"));

    for (const auto& ej : scene["entities"]) {
        ENJIN_ASSERT_TRUE(ej.contains("id"));
        const u64 id = ej["id"].get<u64>();
        // The whole point: no generation in the high word.
        ENJIN_EXPECT_TRUE(id <= 0xFFFFFFFFull);
        ENJIN_EXPECT_EQ(id, static_cast<u64>(EntityIndex(
            static_cast<Entity>(id | (static_cast<u64>(EntityGeneration(e)) << 32)))));
    }
}

ENJIN_TEST(SceneEntityIds, ParentReferenceIsAPlainIndexToo) {
    // The id and the parent ref have to change together: the loader looks a
    // parent up in a table keyed by id, so masking one and not the other would
    // break parenting in every scene.
    World world;
    Entity parent = MakeRecycledEntity(world, "Parent");
    Entity child = MakeRecycledEntity(world, "Child");
    world.AddComponent<ParentComponent>(child).parent = parent;

    Scene::SceneSerializer serializer(&world);
    json scene = ParseScene(serializer.SaveToString());
    ENJIN_ASSERT_FALSE(scene.is_discarded());

    bool sawParentRef = false;
    for (const auto& ej : scene["entities"]) {
        if (!ej.contains("parent")) continue;
        sawParentRef = true;
        ENJIN_EXPECT_TRUE(ej["parent"].get<u64>() <= 0xFFFFFFFFull);
    }
    ENJIN_ASSERT_TRUE(sawParentRef);
}

ENJIN_TEST(SceneEntityIds, ParentingSurvivesTheRoundTrip) {
    World world;
    Entity parent = MakeRecycledEntity(world, "Parent");
    Entity child = MakeRecycledEntity(world, "Child");
    world.AddComponent<ParentComponent>(child).parent = parent;

    Scene::SceneSerializer serializer(&world);
    const std::string text = serializer.SaveToString();

    World loaded;
    Scene::SceneSerializer deserializer(&loaded);
    auto result = deserializer.LoadFromString(text);
    ENJIN_ASSERT_TRUE(result.success);

    Entity newParent = loaded.FindEntityByName("Parent");
    Entity newChild = loaded.FindEntityByName("Child");
    ENJIN_ASSERT_TRUE(newParent != INVALID_ENTITY);
    ENJIN_ASSERT_TRUE(newChild != INVALID_ENTITY);

    auto* pc = loaded.GetComponent<ParentComponent>(newChild);
    ENJIN_ASSERT_NOT_NULL(pc);
    ENJIN_EXPECT_EQ(pc->parent, newParent);
}

ENJIN_TEST(SceneEntityIds, AFileAlreadyCarryingPackedHandlesStillLoads) {
    // Two scenes on disk are corrupt right now and unscanned projects likely
    // hold more, so the loader masks on read. Written by hand in the exact
    // shape the scan found: 0x00000001_0000000N.
    const u64 packedParent = (1ull << 32) | 1ull;
    const u64 packedChild  = (1ull << 32) | 2ull;

    json scene;
    scene["version"] = "1.0";           // MUST be the string, not the number
    scene["entityCount"] = 2;
    json entities = json::array();

    json p;
    p["id"] = packedParent;
    p["name"] = { {"name", "Parent"} };
    p["transform"] = { {"position", {0.0, 0.0, 0.0}},
                       {"rotation", {0, 0, 0, 1}},
                       {"scale", {1, 1, 1}} };
    entities.push_back(p);

    json c;
    c["id"] = packedChild;
    c["parent"] = packedParent;
    c["name"] = { {"name", "Child"} };
    c["transform"] = { {"position", {1.0, 0.0, 0.0}},
                       {"rotation", {0, 0, 0, 1}},
                       {"scale", {1, 1, 1}} };
    entities.push_back(c);

    scene["entities"] = entities;

    World world;
    Scene::SceneSerializer deserializer(&world);
    auto result = deserializer.LoadFromString(scene.dump());
    ENJIN_ASSERT_TRUE(result.success);

    Entity newParent = world.FindEntityByName("Parent");
    Entity newChild = world.FindEntityByName("Child");
    ENJIN_ASSERT_TRUE(newParent != INVALID_ENTITY);
    ENJIN_ASSERT_TRUE(newChild != INVALID_ENTITY);

    auto* pc = world.GetComponent<ParentComponent>(newChild);
    ENJIN_ASSERT_NOT_NULL(pc);
    ENJIN_EXPECT_EQ(pc->parent, newParent);
}

ENJIN_TEST(SceneEntityIds, HealedFileWritesCleanIdsOnItsNextSave) {
    // The reason masking on read is enough: load a corrupt file, save it, and
    // the corruption is gone with no renumbering and nothing to migrate.
    const u64 packed = (1ull << 32) | 7ull;

    json scene;
    scene["version"] = "1.0";
    scene["entityCount"] = 1;
    json e;
    e["id"] = packed;
    e["name"] = { {"name", "WasCorrupt"} };
    e["transform"] = { {"position", {0.0, 0.0, 0.0}},
                       {"rotation", {0, 0, 0, 1}},
                       {"scale", {1, 1, 1}} };
    scene["entities"] = json::array({ e });

    World world;
    Scene::SceneSerializer deserializer(&world);
    ENJIN_ASSERT_TRUE(deserializer.LoadFromString(scene.dump()).success);

    Scene::SceneSerializer resaver(&world);
    json again = ParseScene(resaver.SaveToString());
    ENJIN_ASSERT_FALSE(again.is_discarded());

    for (const auto& ej : again["entities"]) {
        ENJIN_EXPECT_TRUE(ej["id"].get<u64>() <= 0xFFFFFFFFull);
    }
}

ENJIN_TEST_MAIN()
