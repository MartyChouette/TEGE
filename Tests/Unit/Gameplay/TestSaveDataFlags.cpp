// The four flags on SaveDataComponent, whose entire job is deciding what
// persists, and which had no reader.
//
// saveEnabled, savePosition, saveRotation and saveScale were serialized, shown
// in the inspector, and ignored: CollectEntitiesByTier serialized the whole
// entity through SerializeEntityToString whatever they said. Unticking
// saveEnabled saved the entity anyway. Found by tools/unread_field_audit.py.
//
// Of the failures a save system can have, "the thing you turned off is still
// on" is the one a person cannot see until the load, by which point the wrong
// data is already in the file.
//
// These read the SAVE JSON rather than round-tripping through a load, because
// what the record contains is the property -- an absent key is what makes the
// loader leave the scene's own value alone, and a test that only checked the
// final position would pass on a record that saved the value and then happened
// to restore the same one.
#include "EnjinTest.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/SaveBackend.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/StableId.h"

#include <map>
#include <string>

using namespace Enjin;

namespace {

class MemoryBackend : public Gameplay::ISaveBackend {
public:
    bool Write(const std::string& key, const std::string& data) override {
        store[key] = data; return true;
    }
    bool Read(const std::string& key, std::string& outData) override {
        auto it = store.find(key);
        if (it == store.end()) return false;
        outData = it->second; return true;
    }
    bool Delete(const std::string& key) override { return store.erase(key) > 0; }
    bool Exists(const std::string& key) override { return store.count(key) > 0; }
    std::string GetName() const override { return "memory"; }
    std::map<std::string, std::string> store;
};

// An entity that opts into persistence, with a stable id so the save system
// will accept it -- without one it is dropped with a warning, which would make
// every test below pass for the wrong reason.
ECS::Entity MakeSaved(ECS::World& world, u64 stableId) {
    ECS::Entity e = world.CreateEntity();
    ECS::TransformComponent xf;
    xf.position = Math::Vector3(1.0f, 2.0f, 3.0f);
    xf.scale    = Math::Vector3(4.0f, 4.0f, 4.0f);
    world.AddComponent<ECS::TransformComponent>(e, xf);

    ECS::StableIdComponent sid;
    sid.id = stableId;
    world.AddComponent<ECS::StableIdComponent>(e, sid);

    ECS::SaveDataComponent sd;
    sd.tier = ECS::PersistenceTier::RunState;
    world.AddComponent<ECS::SaveDataComponent>(e, sd);
    return e;
}

// The whole save file for slot 0, as text. Every assertion below is about what
// is in it.
std::string SaveAndRead(ECS::World& world) {
    auto backend = std::make_shared<MemoryBackend>();
    Gameplay::TieredSaveSystem saves;
    saves.SetBackend(backend);
    saves.SaveToSlot(0, &world, "TestScene");

    for (const auto& [key, data] : backend->store) {
        if (data.find("runState") != std::string::npos) return data;
    }
    return backend->store.empty() ? std::string() : backend->store.begin()->second;
}

bool Mentions(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

} // namespace

ENJIN_TEST(SaveDataFlags, test_an_entity_with_save_disabled_is_left_out_of_the_record) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSaved(world, 777);
    world.GetComponent<ECS::SaveDataComponent>(e)->saveEnabled = false;

    // Act
    const std::string saved = SaveAndRead(world);

    // Assert: the stable id is the join key, so its absence is the entity's
    // absence. Nothing else in the file carries 777.
    ENJIN_ASSERT_TRUE(!saved.empty());
    ENJIN_EXPECT_FALSE(Mentions(saved, "777"));
}

ENJIN_TEST(SaveDataFlags, test_an_entity_with_save_enabled_is_in_the_record) {
    // The control for the test above: without it, a save that wrote nothing at
    // all would pass and look like the flag working.
    // Arrange
    ECS::World world;
    MakeSaved(world, 777);

    // Act
    const std::string saved = SaveAndRead(world);

    // Assert
    ENJIN_ASSERT_TRUE(!saved.empty());
    ENJIN_EXPECT_TRUE(Mentions(saved, "777"));
}

ENJIN_TEST(SaveDataFlags, test_position_is_dropped_from_the_record_when_it_is_not_wanted) {
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSaved(world, 778);
    auto* sd = world.GetComponent<ECS::SaveDataComponent>(e);
    sd->savePosition = false;
    sd->saveRotation = true;

    // Act
    const std::string saved = SaveAndRead(world);

    // Assert: the key is GONE, not zeroed. DeserializeTransformComponent applies
    // position only `if (j.contains("position"))`, so an absent key is what
    // leaves the loaded entity where the scene put it.
    ENJIN_ASSERT_TRUE(Mentions(saved, "778"));
    ENJIN_EXPECT_FALSE(Mentions(saved, "\"position\""));
    ENJIN_EXPECT_TRUE(Mentions(saved, "\"rotation\""));
}

ENJIN_TEST(SaveDataFlags, test_rotation_is_dropped_independently_of_position) {
    // Each flag has to act alone, or the three are really one flag.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSaved(world, 779);
    auto* sd = world.GetComponent<ECS::SaveDataComponent>(e);
    sd->savePosition = true;
    sd->saveRotation = false;

    // Act
    const std::string saved = SaveAndRead(world);

    // Assert
    ENJIN_ASSERT_TRUE(Mentions(saved, "779"));
    ENJIN_EXPECT_TRUE(Mentions(saved, "\"position\""));
    ENJIN_EXPECT_FALSE(Mentions(saved, "\"rotation\""));
}

ENJIN_TEST(SaveDataFlags, test_scale_is_saved_only_when_asked_for) {
    // saveScale defaults to FALSE, unlike the other two. So the default record
    // now carries no scale, where before it always did.
    // Arrange
    ECS::World defaultWorld, optedInWorld;
    MakeSaved(defaultWorld, 780);
    ECS::Entity opted = MakeSaved(optedInWorld, 781);
    optedInWorld.GetComponent<ECS::SaveDataComponent>(opted)->saveScale = true;

    // Act
    const std::string byDefault = SaveAndRead(defaultWorld);
    const std::string optedIn   = SaveAndRead(optedInWorld);

    // Assert
    ENJIN_ASSERT_TRUE(Mentions(byDefault, "780"));
    ENJIN_ASSERT_TRUE(Mentions(optedIn, "781"));
    ENJIN_EXPECT_FALSE(Mentions(byDefault, "\"scale\""));
    ENJIN_EXPECT_TRUE(Mentions(optedIn, "\"scale\""));
}

ENJIN_TEST(SaveDataFlags, test_turning_all_three_off_leaves_no_empty_transform_behind) {
    // An empty "transform": {} would be a component the loader walks for
    // nothing, and it would make the record look like it holds a transform.
    // Arrange
    ECS::World world;
    ECS::Entity e = MakeSaved(world, 782);
    auto* sd = world.GetComponent<ECS::SaveDataComponent>(e);
    sd->savePosition = false;
    sd->saveRotation = false;
    sd->saveScale    = false;

    // Act
    const std::string saved = SaveAndRead(world);

    // Assert
    ENJIN_ASSERT_TRUE(Mentions(saved, "782"));
    ENJIN_EXPECT_FALSE(Mentions(saved, "\"transform\""));
}

ENJIN_TEST_MAIN()
