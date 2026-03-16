#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Scene/SceneSerializer.h"

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Gameplay;

// ===========================================================================
// Meta-Progression Key-Value Store
// ===========================================================================

ENJIN_TEST(MetaKV, SetAndGetFloat) {
    TieredSaveSystem save;
    save.SetMetaFloat("score", 42.5f);
    ENJIN_EXPECT_FLOAT_EQ(save.GetMetaFloat("score"), 42.5f);
}

ENJIN_TEST(MetaKV, SetAndGetInt) {
    TieredSaveSystem save;
    save.SetMetaInt("level", 7);
    ENJIN_EXPECT_EQ(save.GetMetaInt("level"), 7);
}

ENJIN_TEST(MetaKV, SetAndGetBool) {
    TieredSaveSystem save;
    save.SetMetaBool("unlocked_sword", true);
    ENJIN_EXPECT_TRUE(save.GetMetaBool("unlocked_sword"));
}

ENJIN_TEST(MetaKV, SetAndGetString) {
    TieredSaveSystem save;
    save.SetMetaString("player_name", "Hero");
    ENJIN_EXPECT_STR_EQ(save.GetMetaString("player_name").c_str(), "Hero");
}

ENJIN_TEST(MetaKV, FallbackOnMissingKey) {
    TieredSaveSystem save;
    ENJIN_EXPECT_FLOAT_EQ(save.GetMetaFloat("missing", 99.0f), 99.0f);
    ENJIN_EXPECT_EQ(save.GetMetaInt("missing", -1), -1);
    ENJIN_EXPECT_FALSE(save.GetMetaBool("missing", false));
    ENJIN_EXPECT_STR_EQ(save.GetMetaString("missing", "default").c_str(), "default");
}

ENJIN_TEST(MetaKV, OverwriteExistingKey) {
    TieredSaveSystem save;
    save.SetMetaInt("coins", 10);
    ENJIN_EXPECT_EQ(save.GetMetaInt("coins"), 10);
    save.SetMetaInt("coins", 50);
    ENJIN_EXPECT_EQ(save.GetMetaInt("coins"), 50);
}

ENJIN_TEST(MetaKV, MultipleKeyTypes) {
    TieredSaveSystem save;
    save.SetMetaFloat("f", 1.5f);
    save.SetMetaInt("i", 42);
    save.SetMetaBool("b", true);
    save.SetMetaString("s", "hello");

    ENJIN_EXPECT_FLOAT_EQ(save.GetMetaFloat("f"), 1.5f);
    ENJIN_EXPECT_EQ(save.GetMetaInt("i"), 42);
    ENJIN_EXPECT_TRUE(save.GetMetaBool("b"));
    ENJIN_EXPECT_STR_EQ(save.GetMetaString("s").c_str(), "hello");
}

ENJIN_TEST(MetaKV, MapsAreAccessible) {
    TieredSaveSystem save;
    save.SetMetaFloat("a", 1.0f);
    save.SetMetaFloat("b", 2.0f);
    save.SetMetaInt("x", 10);

    ENJIN_EXPECT_EQ(save.GetMetaFloats().size(), (size_t)2);
    ENJIN_EXPECT_EQ(save.GetMetaInts().size(), (size_t)1);
    ENJIN_EXPECT_EQ(save.GetMetaBools().size(), (size_t)0);
    ENJIN_EXPECT_EQ(save.GetMetaStrings().size(), (size_t)0);
}

// ===========================================================================
// Slot Info
// ===========================================================================

ENJIN_TEST(SlotInfo, AllSlotsInitiallyEmpty) {
    TieredSaveSystem save;
    auto slots = save.GetAllSlots();
    ENJIN_EXPECT_EQ(slots.size(), (size_t)TieredSaveSystem::MAX_SLOTS);
    for (auto& s : slots) {
        ENJIN_EXPECT_TRUE(s.isEmpty);
    }
}

ENJIN_TEST(SlotInfo, SlotIndexRange) {
    TieredSaveSystem save;
    for (u32 i = 0; i < TieredSaveSystem::MAX_SLOTS; ++i) {
        auto info = save.GetSlotInfo(i);
        ENJIN_EXPECT_EQ(info.slotIndex, i);
    }
}

ENJIN_TEST(SlotInfo, AutoSaveSlotIdentification) {
    TieredSaveSystem save;
    for (u32 i = 0; i < TieredSaveSystem::MAX_SLOTS; ++i) {
        auto info = save.GetSlotInfo(i);
        bool expectedAuto = (i >= TieredSaveSystem::AUTO_SAVE_SLOT_START);
        ENJIN_EXPECT_EQ(info.isAutoSave, expectedAuto);
    }
}

ENJIN_TEST(SlotInfo, ManualSlotCount) {
    ENJIN_EXPECT_EQ(TieredSaveSystem::MANUAL_SLOTS, 17u);
    ENJIN_EXPECT_EQ(TieredSaveSystem::AUTO_SAVE_SLOT_START, 17u);
    ENJIN_EXPECT_EQ(TieredSaveSystem::MAX_SLOTS, 20u);
}

// ===========================================================================
// Play Time Tracking
// ===========================================================================

ENJIN_TEST(PlayTime, InitiallyZero) {
    TieredSaveSystem save;
    ENJIN_EXPECT_FLOAT_EQ(save.GetSessionPlayTime(), 0.0f);
}

ENJIN_TEST(PlayTime, AccumulatesCorrectly) {
    TieredSaveSystem save;
    save.AddPlayTime(1.5f);
    save.AddPlayTime(2.5f);
    ENJIN_EXPECT_FLOAT_NEAR(save.GetSessionPlayTime(), 4.0f, 0.01f);
}

// ===========================================================================
// AutoSave Configuration
// ===========================================================================

ENJIN_TEST(AutoSave, DefaultConfig) {
    AutoSaveConfig config;
    ENJIN_EXPECT_FALSE(config.enabled);
    ENJIN_EXPECT_TRUE(config.onSceneTransition);
    ENJIN_EXPECT_FALSE(config.onTimedInterval);
    ENJIN_EXPECT_FLOAT_EQ(config.intervalSeconds, 300.0f);
    ENJIN_EXPECT_TRUE(config.onCheckpoint);
    ENJIN_EXPECT_EQ(config.autoSaveSlotCount, 3u);
}

// ===========================================================================
// SaveDataComponent
// ===========================================================================

ENJIN_TEST(SaveDataComponent, Defaults) {
    SaveDataComponent sd;
    ENJIN_EXPECT_TRUE(sd.savePosition);
    ENJIN_EXPECT_TRUE(sd.saveRotation);
    ENJIN_EXPECT_FALSE(sd.saveScale);
    ENJIN_EXPECT_TRUE(sd.saveEnabled);
    ENJIN_EXPECT_EQ((int)sd.tier, (int)PersistenceTier::RunState);
}

ENJIN_TEST(SaveDataComponent, SetAndGetData) {
    SaveDataComponent sd;
    sd.SetData("health", "100");
    sd.SetData("weapon", "sword");

    ENJIN_EXPECT_STR_EQ(sd.GetData("health").c_str(), "100");
    ENJIN_EXPECT_STR_EQ(sd.GetData("weapon").c_str(), "sword");
    ENJIN_EXPECT_STR_EQ(sd.GetData("missing", "none").c_str(), "none");
}

ENJIN_TEST(SaveDataComponent, OverwriteData) {
    SaveDataComponent sd;
    sd.SetData("key", "old");
    sd.SetData("key", "new");
    ENJIN_EXPECT_STR_EQ(sd.GetData("key").c_str(), "new");
}

ENJIN_TEST(SaveDataComponent, TagOperations) {
    SaveDataComponent sd;
    sd.tags.push_back("persistent");
    sd.tags.push_back("important");

    ENJIN_EXPECT_TRUE(sd.HasTag("persistent"));
    ENJIN_EXPECT_TRUE(sd.HasTag("important"));
    ENJIN_EXPECT_FALSE(sd.HasTag("nonexistent"));
}

ENJIN_TEST(SaveDataComponent, PersistenceTierEnum) {
    ENJIN_EXPECT_EQ((int)PersistenceTier::SceneState, 0);
    ENJIN_EXPECT_EQ((int)PersistenceTier::RunState, 1);
    ENJIN_EXPECT_EQ((int)PersistenceTier::MetaProgression, 2);
    ENJIN_EXPECT_EQ((int)PersistenceTier::COUNT, 3);
}

// ===========================================================================
// SaveDataComponent Serialization Round-Trip
// ===========================================================================

ENJIN_TEST(SaveDataSerialization, RoundTrip) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    auto& sd = world.AddComponent<SaveDataComponent>(e);
    sd.savePosition = true;
    sd.saveRotation = false;
    sd.saveScale = true;
    sd.tier = PersistenceTier::MetaProgression;
    sd.tags.push_back("unlock");
    sd.tags.push_back("achievement");
    sd.SetData("stars", "3");
    sd.SetData("time", "120.5");

    Scene::SceneSerializer serializer(&world);
    std::string json = serializer.SaveToString();
    ENJIN_EXPECT_GT(json.size(), (size_t)0);

    World world2;
    Scene::SceneSerializer deserializer(&world2);
    auto result = deserializer.LoadFromString(json);
    ENJIN_ASSERT_TRUE(result.success);
    ENJIN_ASSERT_TRUE(result.entities.size() > 0);

    auto* sd2 = world2.GetComponent<SaveDataComponent>(result.entities[0]);
    ENJIN_ASSERT_NOT_NULL(sd2);
    ENJIN_EXPECT_TRUE(sd2->savePosition);
    ENJIN_EXPECT_FALSE(sd2->saveRotation);
    ENJIN_EXPECT_TRUE(sd2->saveScale);
    ENJIN_EXPECT_EQ((int)sd2->tier, (int)PersistenceTier::MetaProgression);
    ENJIN_EXPECT_TRUE(sd2->HasTag("unlock"));
    ENJIN_EXPECT_TRUE(sd2->HasTag("achievement"));
    ENJIN_EXPECT_STR_EQ(sd2->GetData("stars").c_str(), "3");
    ENJIN_EXPECT_STR_EQ(sd2->GetData("time").c_str(), "120.5");
}

ENJIN_TEST(SaveDataSerialization, EmptyComponentRoundTrip) {
    World world;
    Entity e = world.CreateEntity();
    world.AddComponent<TransformComponent>(e);
    world.AddComponent<SaveDataComponent>(e); // all defaults

    Scene::SceneSerializer serializer(&world);
    std::string json = serializer.SaveToString();

    World world2;
    Scene::SceneSerializer deserializer(&world2);
    auto result = deserializer.LoadFromString(json);
    ENJIN_ASSERT_TRUE(result.success);

    auto* sd2 = world2.GetComponent<SaveDataComponent>(result.entities[0]);
    ENJIN_ASSERT_NOT_NULL(sd2);
    ENJIN_EXPECT_TRUE(sd2->savePosition);
    ENJIN_EXPECT_TRUE(sd2->saveRotation);
    ENJIN_EXPECT_FALSE(sd2->saveScale);
    ENJIN_EXPECT_EQ((int)sd2->tier, (int)PersistenceTier::RunState);
    ENJIN_EXPECT_EQ(sd2->tags.size(), (size_t)0);
    ENJIN_EXPECT_EQ(sd2->customData.size(), (size_t)0);
}

ENJIN_TEST_MAIN()
