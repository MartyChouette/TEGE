// The three cloud flags SaveSystemComponent has always carried and nothing read.
//
// enableCloudSync, syncOnSave and syncOnLoad were serialized, unauthorable and
// unread: SyncToCloud existed and was called from exactly one place, a button in
// the editor's save debug panel, so a GAME could never ask for an upload -- only
// a developer with that panel open could. Found by tools/unread_field_audit.py
// after its own filter bug was fixed.
//
// These tests use in-memory backends, so they touch no disk and no network.
#include "EnjinTest.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/SaveBackend.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"

#include <map>
#include <string>

using namespace Enjin;

namespace {

// Counts what reaches it, so a test can ask whether a sync HAPPENED rather than
// whether a function exists.
class CountingBackend : public Gameplay::ISaveBackend {
public:
    bool Write(const std::string& key, const std::string& data) override {
        ++writes;
        store[key] = data;
        return true;
    }
    bool Read(const std::string& key, std::string& outData) override {
        ++reads;
        auto it = store.find(key);
        if (it == store.end()) return false;
        outData = it->second;
        return true;
    }
    bool Delete(const std::string& key) override { return store.erase(key) > 0; }
    bool Exists(const std::string& key) override { return store.count(key) > 0; }
    std::string GetName() const override { return "counting"; }

    int writes = 0;
    int reads = 0;
    std::map<std::string, std::string> store;
};

// Configures IN PLACE: ECS::World is deliberately non-copyable, so a helper
// that hands one back by value does not compile.
void GiveWorldSaveConfig(ECS::World& world, bool cloud, bool onSave, bool onLoad) {
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    ECS::SaveSystemComponent cfg;
    cfg.enableCloudSync = cloud;
    cfg.syncOnSave = onSave;
    cfg.syncOnLoad = onLoad;
    world.AddComponent<ECS::SaveSystemComponent>(e, cfg);
}

} // namespace

ENJIN_TEST(SaveCloudSync, test_the_component_flags_reach_the_save_system) {
    // Arrange
    ECS::World world;
    GiveWorldSaveConfig(world, true, false, true);
    Gameplay::TieredSaveSystem save;

    // Act
    save.ApplyConfigFromWorld(&world);

    // Assert
    const auto& cfg = save.GetCloudSyncConfig();
    ENJIN_EXPECT_TRUE(cfg.enabled);
    ENJIN_EXPECT_FALSE(cfg.onSave);
    ENJIN_EXPECT_TRUE(cfg.onLoad);
}

ENJIN_TEST(SaveCloudSync, test_a_save_uploads_when_asked_and_a_backend_exists) {
    // Arrange
    ECS::World world;
    GiveWorldSaveConfig(world, true, true, false);
    auto local = std::make_shared<CountingBackend>();
    auto cloud = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem save;
    save.SetBackend(local);
    save.SetCloudBackend(cloud);
    save.ApplyConfigFromWorld(&world);

    // Act
    const bool ok = save.SaveToSlot(0, &world, "TestScene");

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_TRUE(local->writes > 0);
    ENJIN_EXPECT_TRUE(cloud->writes > 0);
}

ENJIN_TEST(SaveCloudSync, test_a_save_does_not_upload_when_sync_is_off) {
    // The flag has to mean something in BOTH directions, or it is decoration.
    // Arrange
    ECS::World world;
    GiveWorldSaveConfig(world, false, true, true);
    auto local = std::make_shared<CountingBackend>();
    auto cloud = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem save;
    save.SetBackend(local);
    save.SetCloudBackend(cloud);
    save.ApplyConfigFromWorld(&world);

    // Act
    ENJIN_ASSERT_TRUE(save.SaveToSlot(0, &world, "TestScene"));

    // Assert
    ENJIN_EXPECT_TRUE(local->writes > 0);
    ENJIN_EXPECT_EQ(cloud->writes, 0);
}

ENJIN_TEST(SaveCloudSync, test_a_save_still_succeeds_with_sync_on_and_no_backend) {
    // The guard that matters most. A game that asks for cloud sync without
    // installing a backend must get its LOCAL save, not a failure: the save has
    // already succeeded by then and the upload is the extra.
    // Arrange
    ECS::World world;
    GiveWorldSaveConfig(world, true, true, true);
    auto local = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem save;
    save.SetBackend(local);            // no cloud backend at all
    save.ApplyConfigFromWorld(&world);

    // Act
    const bool ok = save.SaveToSlot(0, &world, "TestScene");

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_TRUE(local->writes > 0);
}

ENJIN_TEST(SaveCloudSync, test_the_default_is_no_cloud_traffic) {
    // A component left alone must not start talking to a backend somebody
    // installed for another reason.
    // Arrange
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);
    world.AddComponent<ECS::SaveSystemComponent>(e);   // defaults
    auto local = std::make_shared<CountingBackend>();
    auto cloud = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem save;
    save.SetBackend(local);
    save.SetCloudBackend(cloud);
    save.ApplyConfigFromWorld(&world);

    // Act
    ENJIN_ASSERT_TRUE(save.SaveToSlot(0, &world, "TestScene"));

    // Assert
    ENJIN_EXPECT_EQ(cloud->writes, 0);
}

ENJIN_TEST_MAIN()
