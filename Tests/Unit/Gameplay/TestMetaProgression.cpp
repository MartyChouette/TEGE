// enableMetaProgression and autoSaveMeta, two more fields on
// SaveSystemComponent that had no reader.
//
// Meta progression is the tier that survives deleting every save slot:
// unlocks, achievements, "you have died 40 times". Both flags serialize, both
// show in the inspector, and neither was consumed -- so the tier was always on
// however it was configured, and nothing was ever written out unless a game
// remembered to call SaveMeta() by hand. Part of the save-point rock, which the
// backlog described as a feature that saves nothing.
//
// The interesting decision is what DISABLED means. A setter that stores the
// value and simply never persists it would make GetMeta* return it for the rest
// of the session and lose it at shutdown -- which looks like it works right up
// until the game is restarted, and is the worse failure. So a disabled setter
// does nothing at all, and that is what these pin.
#include "EnjinTest.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/SaveBackend.h"

#include <map>
#include <string>

using namespace Enjin;

namespace {

// Counts writes so a test can ask whether a save HAPPENED, not whether a
// function exists.
class CountingBackend : public Gameplay::ISaveBackend {
public:
    bool Write(const std::string& key, const std::string& data) override {
        ++writes; store[key] = data; return true;
    }
    bool Read(const std::string& key, std::string& outData) override {
        auto it = store.find(key);
        if (it == store.end()) return false;
        outData = it->second; return true;
    }
    bool Delete(const std::string& key) override { return store.erase(key) > 0; }
    bool Exists(const std::string& key) override { return store.count(key) > 0; }
    std::string GetName() const override { return "counting"; }

    int writes = 0;
    std::map<std::string, std::string> store;
};

Gameplay::MetaProgressionConfig Config(bool enabled, bool autoSave) {
    Gameplay::MetaProgressionConfig cfg;
    cfg.enabled = enabled;
    cfg.autoSave = autoSave;
    return cfg;
}

} // namespace

ENJIN_TEST(MetaProgression, test_a_value_set_with_the_tier_enabled_reads_back) {
    // Arrange
    auto backend = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem saves;
    saves.SetBackend(backend);
    saves.SetMetaConfig(Config(true, false));

    // Act
    saves.SetMetaInt("deaths", 40);

    // Assert
    ENJIN_EXPECT_EQ(saves.GetMetaInt("deaths", -1), 40);
}

ENJIN_TEST(MetaProgression, test_the_tier_switched_off_stores_nothing_at_all) {
    // Not "stores it and skips the write". A value that reads back for the rest
    // of the session and vanishes at shutdown is the failure this avoids.
    // Arrange
    auto backend = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem saves;
    saves.SetBackend(backend);
    saves.SetMetaConfig(Config(false, true));

    // Act
    saves.SetMetaInt("deaths", 40);
    saves.SetMetaFloat("bestTime", 12.5f);
    saves.SetMetaBool("unlockedHardMode", true);
    saves.SetMetaString("lastCharacter", "knight");

    // Assert: every getter falls back, and nothing was written.
    ENJIN_EXPECT_EQ(saves.GetMetaInt("deaths", -1), -1);
    ENJIN_EXPECT_FLOAT_EQ(saves.GetMetaFloat("bestTime", -1.0f), -1.0f);
    ENJIN_EXPECT_FALSE(saves.GetMetaBool("unlockedHardMode", false));
    ENJIN_EXPECT_STR_EQ(saves.GetMetaString("lastCharacter", "none"), "none");
    ENJIN_EXPECT_EQ(backend->writes, 0);
}

ENJIN_TEST(MetaProgression, test_auto_save_writes_on_every_change) {
    // What autoSaveMeta is for: a game that never calls SaveMeta still keeps
    // its unlocks across a crash.
    // Arrange
    auto backend = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem saves;
    saves.SetBackend(backend);
    saves.SetMetaConfig(Config(true, true));

    // Act
    saves.SetMetaInt("deaths", 1);
    saves.SetMetaInt("deaths", 2);

    // Assert
    ENJIN_EXPECT_EQ(backend->writes, 2);
}

ENJIN_TEST(MetaProgression, test_auto_save_off_keeps_the_value_and_writes_nothing) {
    // The tier is on, so the value is live; it just is not persisted until the
    // game asks. Distinguishing this from the disabled case is the whole point
    // of having two flags rather than one.
    // Arrange
    auto backend = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem saves;
    saves.SetBackend(backend);
    saves.SetMetaConfig(Config(true, false));

    // Act
    saves.SetMetaInt("deaths", 7);

    // Assert
    ENJIN_EXPECT_EQ(saves.GetMetaInt("deaths", -1), 7);
    ENJIN_EXPECT_EQ(backend->writes, 0);

    // ... until asked.
    ENJIN_EXPECT_TRUE(saves.SaveMeta());
    ENJIN_EXPECT_EQ(backend->writes, 1);
}

ENJIN_TEST(MetaProgression, test_save_and_load_meta_refuse_when_the_tier_is_off) {
    // Both are public, so a game can call them directly and go round the
    // setters. A disabled tier must not leave a file on disk, and must not
    // repopulate itself from one left over from before it was turned off.
    // Arrange
    auto backend = std::make_shared<CountingBackend>();
    Gameplay::TieredSaveSystem saves;
    saves.SetBackend(backend);
    saves.SetMetaConfig(Config(true, false));
    saves.SetMetaInt("deaths", 3);
    ENJIN_ASSERT_TRUE(saves.SaveMeta());        // a file exists now
    const int writesWhileOn = backend->writes;

    // Act
    saves.SetMetaConfig(Config(false, false));

    // Assert
    ENJIN_EXPECT_FALSE(saves.SaveMeta());
    ENJIN_EXPECT_FALSE(saves.LoadMeta());
    ENJIN_EXPECT_EQ(backend->writes, writesWhileOn);
}

ENJIN_TEST_MAIN()
