// Copy and Cut put the WHOLE SCENE on the clipboard.
//
// Both called SceneSerializer::SaveToString with no way to say "just these" --
// the function had no selection parameter at all and iterated
// World::GetAllEntities(). So copying one cube in a five-entity scene and
// pasting gave you ten entities. The menu looked right, the paste looked like
// it worked, and the entity count was the only tell. Found by the feature-audit
// swarm 2026-09-18.
//
// SerializationOptions::onlyEntities is the fix, and these pin it. They test
// the SERIALIZER rather than the menu, because that is where the capability was
// missing and the menu needs an ImGui frame.
#include "EnjinTest.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"

#include <string>
#include <vector>

using namespace Enjin;

namespace {

ECS::Entity MakeNamed(ECS::World& world, const char* name) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    world.AddComponent<ECS::NameComponent>(e, ECS::NameComponent{name});
    return e;
}

// How many entities the written scene actually contains. Counted from the
// "stableId" markers, one per serialized entity, rather than trusting the
// header -- a header that disagreed with its own array is one of the things
// this change had to fix.
usize CountEntities(const std::string& json) {
    usize n = 0;
    for (usize i = json.find("\"stableId\""); i != std::string::npos;
         i = json.find("\"stableId\"", i + 1)) {
        ++n;
    }
    return n;
}

// Matches the QUOTED name, not the bare substring.
//
// Searching for Alpha alone matches bgAlpha, boxAlpha, startAlpha, endAlpha and
// imageAlpha -- serialized fields on unrelated components -- so the test
// reported a leak that was not there and sent me looking in the serializer.
bool MentionsName(const std::string& json, const char* name) {
    return json.find(std::string("\"") + name + "\"") != std::string::npos;
}

} // namespace

ENJIN_TEST(SelectionSerialize, test_an_empty_selection_still_writes_the_whole_world) {
    // Every other caller -- saving a scene, the play-mode snapshot, the build
    // pipeline -- passes no selection and must keep getting everything.
    // Arrange
    ECS::World world;
    MakeNamed(world, "Alpha");
    MakeNamed(world, "Beta");
    MakeNamed(world, "Gamma");

    // Act
    Scene::SceneSerializer s(&world);
    Scene::SerializationOptions opts;   // onlyEntities left empty
    const std::string json = s.SaveToString(opts);

    // Assert
    ENJIN_EXPECT_EQ(CountEntities(json), static_cast<usize>(3));
    ENJIN_EXPECT_TRUE(MentionsName(json, "Alpha"));
    ENJIN_EXPECT_TRUE(MentionsName(json, "Gamma"));
}

ENJIN_TEST(SelectionSerialize, test_one_selected_entity_writes_one_entity) {
    // The actual bug: this used to write all three.
    // Arrange
    ECS::World world;
    MakeNamed(world, "Alpha");
    ECS::Entity beta = MakeNamed(world, "Beta");
    MakeNamed(world, "Gamma");

    // Act
    Scene::SceneSerializer s(&world);
    Scene::SerializationOptions opts;
    opts.onlyEntities = {beta};
    const std::string json = s.SaveToString(opts);

    // Assert
    ENJIN_EXPECT_EQ(CountEntities(json), static_cast<usize>(1));
    ENJIN_EXPECT_TRUE(MentionsName(json, "Beta"));
    ENJIN_EXPECT_FALSE(MentionsName(json, "Alpha"));
    ENJIN_EXPECT_FALSE(MentionsName(json, "Gamma"));
}

ENJIN_TEST(SelectionSerialize, test_a_multi_selection_writes_exactly_those) {
    // Arrange
    ECS::World world;
    ECS::Entity alpha = MakeNamed(world, "Alpha");
    MakeNamed(world, "Beta");
    ECS::Entity gamma = MakeNamed(world, "Gamma");

    // Act
    Scene::SceneSerializer s(&world);
    Scene::SerializationOptions opts;
    opts.onlyEntities = {alpha, gamma};
    const std::string json = s.SaveToString(opts);

    // Assert
    ENJIN_EXPECT_EQ(CountEntities(json), static_cast<usize>(2));
    ENJIN_EXPECT_TRUE(MentionsName(json, "Alpha"));
    ENJIN_EXPECT_TRUE(MentionsName(json, "Gamma"));
    ENJIN_EXPECT_FALSE(MentionsName(json, "Beta"));
}

ENJIN_TEST(SelectionSerialize, test_the_written_count_matches_what_was_written) {
    // entityCount was taken from the WORLD, so a filtered save wrote a header
    // saying 3 above an array of 1. A loader trusts that header.
    // Arrange
    ECS::World world;
    MakeNamed(world, "Alpha");
    ECS::Entity beta = MakeNamed(world, "Beta");
    MakeNamed(world, "Gamma");

    // Act
    Scene::SceneSerializer s(&world);
    Scene::SerializationOptions opts;
    opts.onlyEntities = {beta};
    const std::string json = s.SaveToString(opts);

    // Assert: the header says 1, not 3.
    ENJIN_EXPECT_TRUE(json.find("\"entityCount\": 1") != std::string::npos ||
                      json.find("\"entityCount\":1") != std::string::npos);
}

ENJIN_TEST(SelectionSerialize, test_a_selection_naming_a_dead_entity_writes_nothing_for_it) {
    // A clipboard can outlive what it points at: copy, delete, paste. The
    // filter must not resurrect a stale id or trip over it.
    // Arrange
    ECS::World world;
    ECS::Entity alpha = MakeNamed(world, "Alpha");
    ECS::Entity doomed = MakeNamed(world, "Doomed");
    world.DestroyEntity(doomed);
    world.Update(0.0f);   // destroys are deferred, flushed here

    // Act
    Scene::SceneSerializer s(&world);
    Scene::SerializationOptions opts;
    opts.onlyEntities = {alpha, doomed};
    const std::string json = s.SaveToString(opts);

    // Assert: the live one only.
    ENJIN_EXPECT_EQ(CountEntities(json), static_cast<usize>(1));
    ENJIN_EXPECT_TRUE(MentionsName(json, "Alpha"));
    ENJIN_EXPECT_FALSE(MentionsName(json, "Doomed"));
}

ENJIN_TEST(SelectionSerialize, test_a_filtered_save_reloads_as_just_those_entities) {
    // The round trip, which is what Paste actually does: LoadFromString on the
    // clipboard JSON. Without this the tests above only prove the text looks
    // right, not that it loads.
    // Arrange
    ECS::World source;
    MakeNamed(source, "Alpha");
    ECS::Entity beta = MakeNamed(source, "Beta");
    MakeNamed(source, "Gamma");

    Scene::SceneSerializer out(&source);
    Scene::SerializationOptions opts;
    opts.onlyEntities = {beta};
    const std::string json = out.SaveToString(opts);

    // Act
    ECS::World dest;
    Scene::SceneSerializer in(&dest);
    auto result = in.LoadFromString(json, false);

    // Assert
    ENJIN_ASSERT_TRUE(result.success);
    ENJIN_EXPECT_EQ(result.entities.size(), static_cast<usize>(1));
}

ENJIN_TEST_MAIN()
