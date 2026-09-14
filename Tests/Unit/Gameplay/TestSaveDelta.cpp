// A save is a delta, not a copy of the world.
//
// ENG-001, raised out of Shells. The save system was not a stub -- twenty slots,
// three tiers, per-entity opt-in, corruption detection, a backend abstraction
// and fifteen script bindings all worked. It had three defects that made it
// unusable for a shipping game, and the first two are what these tests cover.
//
// S1: BuildSaveJson wrote the ENTIRE level into every slot. A 5.7 MB level times
// twenty slots is over 100 MB of saves for a one-level demo -- but the size was
// the least of it. The save became the authority on the level, so shipping a
// patch that moved a building or fixed a collider was silently reverted by every
// existing save. And it made the tier design pointless: runState and sceneStates
// were collected twenty lines above and then rendered redundant by a dump of
// everything.
//
// S2: runState was WRITE-ONLY. BuildSaveJson wrote it every time; ApplySaveJson
// read sceneData, playTime, sceneStates and sceneName, and never read runState
// back. The tier documented as holding health, inventory and quest progress was
// saved on every save and restored never. It appeared to work only because the
// full scene dump happened to contain those entities anyway -- so fixing S1
// without S2 would have stopped run state persisting entirely. The ticket asked
// for exactly this test: "save, mutate, load, assert".
//
// S3: records are matched by StableIdComponent, never by runtime Entity id,
// because those are generational and do not survive a load.

#include "EnjinTest.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/SaveBackend.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/ECS/Components/Name.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <filesystem>
#include <string>

using namespace Enjin;

namespace {

namespace fs = std::filesystem;

struct SaveScope {
    fs::path dir;
    Gameplay::TieredSaveSystem save;
    explicit SaveScope(const char* leaf) {
        dir = fs::temp_directory_path() / "enjin_save_delta" / leaf;
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        save.SetBackend(std::make_shared<Gameplay::LocalSaveBackend>(dir.string()));
    }
    ~SaveScope() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

// One entity that opts into RunState persistence, with a stable identity.
ECS::Entity MakePersistent(ECS::World& world, u64 stableId, const Math::Vector3& pos) {
    const ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::StableIdComponent>(e, ECS::StableIdComponent(stableId));
    world.AddComponent<ECS::NameComponent>(e).name = "Player";
    auto& t = world.AddComponent<ECS::TransformComponent>(e);
    t.position = pos;
    auto& sd = world.AddComponent<ECS::SaveDataComponent>(e);
    sd.tier = ECS::PersistenceTier::RunState;
    sd.savePosition = true;
    return e;
}

usize FileSizeIn(const fs::path& dir) {
    usize total = 0;
    std::error_code ec;
    for (const auto& f : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) break;
        if (f.is_regular_file(ec)) total += static_cast<usize>(f.file_size(ec));
    }
    return total;
}

}  // namespace

// THE ONE THE TICKET ASKED FOR. Save, mutate, load, assert.
ENJIN_TEST(SaveDelta, RunStateComesBackAfterALoad) {
    SaveScope s("runstate");

    ECS::World world;
    const ECS::Entity player = MakePersistent(world, 4242, Math::Vector3(1.0f, 2.0f, 3.0f));

    ENJIN_ASSERT_TRUE(s.save.SaveToSlot(0, &world, "Level_01"));

    // Mutate after saving, the way a player carries on playing.
    world.GetComponent<ECS::TransformComponent>(player)->position = Math::Vector3(99.0f, 99.0f, 99.0f);

    ENJIN_ASSERT_TRUE(s.save.LoadFromSlot(0, &world));

    const Math::Vector3 back = world.GetComponent<ECS::TransformComponent>(player)->position;
    std::printf("    restored position (%.2f, %.2f, %.2f)\n", back.x, back.y, back.z);

    // Before S2 this stayed at 99: runState was written and never read.
    ENJIN_EXPECT_FLOAT_NEAR(back.x, 1.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(back.y, 2.0f, 0.001f);
    ENJIN_EXPECT_FLOAT_NEAR(back.z, 3.0f, 0.001f);
}

// S1. The save must not contain the level. Measured as a size ceiling, because
// that is the symptom anyone notices first.
ENJIN_TEST(SaveDelta, ASaveDoesNotContainTheLevel) {
    SaveScope s("size");

    ECS::World world;
    MakePersistent(world, 1, Math::Vector3(0.0f, 0.0f, 0.0f));

    // A lot of level: entities that are NOT opted in to persistence and so have
    // no business being in a save file.
    for (u32 i = 0; i < 400; ++i) {
        const ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::NameComponent>(e).name = "Scenery_" + std::to_string(i);
        world.AddComponent<ECS::TransformComponent>(e).position =
            Math::Vector3(static_cast<f32>(i), 0.0f, 0.0f);
    }

    ENJIN_ASSERT_TRUE(s.save.SaveToSlot(0, &world, "Level_01"));

    const usize bytes = FileSizeIn(s.dir);
    std::printf("    save size with 400 unsaved entities in the world: %zu bytes\n", bytes);

    // One opted-in entity. The 400 must not be in there. The ticket's
    // acceptance was "under 100 KB"; this world is small, so hold a tighter
    // line -- the point is that it does not scale with the LEVEL.
    ENJIN_EXPECT_TRUE(bytes < 16 * 1024);
}

// S3. A record naming an entity that no longer exists is skipped, not fatal.
// Deleting a crate from a level must not make older saves unloadable.
ENJIN_TEST(SaveDelta, ARecordForADeletedEntityIsSkipped) {
    SaveScope s("missing");

    ECS::World world;
    const ECS::Entity keep = MakePersistent(world, 100, Math::Vector3(5.0f, 0.0f, 0.0f));
    const ECS::Entity gone = MakePersistent(world, 200, Math::Vector3(7.0f, 0.0f, 0.0f));
    ENJIN_ASSERT_TRUE(s.save.SaveToSlot(0, &world, "Level_01"));

    // The designer removes an entity from the level, and the player's old save
    // still mentions it.
    world.DestroyEntity(gone);
    world.Update(0.0f);   // flush the deferred destroy
    world.GetComponent<ECS::TransformComponent>(keep)->position = Math::Vector3(0.0f, 0.0f, 0.0f);

    ENJIN_EXPECT_TRUE(s.save.LoadFromSlot(0, &world));

    // The surviving entity still restored.
    const Math::Vector3 back = world.GetComponent<ECS::TransformComponent>(keep)->position;
    std::printf("    survivor restored to x=%.2f\n", back.x);
    ENJIN_EXPECT_FLOAT_NEAR(back.x, 5.0f, 0.001f);
}

// S5. A save from a future build is refused rather than parsed as though it were
// this one, which would read every changed key as absent or as something else.
ENJIN_TEST(SaveDelta, AFutureFormatIsRefusedNotGuessedAt) {
    SaveScope s("version");

    ECS::World world;
    MakePersistent(world, 1, Math::Vector3(1.0f, 0.0f, 0.0f));
    ENJIN_ASSERT_TRUE(s.save.SaveToSlot(0, &world, "Level_01"));

    // Rewrite the slot claiming a format this build has never heard of.
    bool rewrote = false;
    std::error_code ec;
    for (const auto& f : fs::recursive_directory_iterator(s.dir, ec)) {
        if (ec) break;
        if (!f.is_regular_file(ec)) continue;
        std::string text;
        {
            std::ifstream in(f.path(), std::ios::binary);
            text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        }
        const std::string needle = "\"version\": " + std::to_string(Gameplay::TieredSaveSystem::kSaveFormatVersion);
        const auto at = text.find(needle);
        if (at == std::string::npos) continue;
        text.replace(at, needle.size(), "\"version\": 99");
        std::ofstream out(f.path(), std::ios::binary | std::ios::trunc);
        out << text;
        rewrote = true;
        break;
    }
    if (!rewrote) { ENJIN_SKIP("could not find the version field to poison"); return; }

    const bool loaded = s.save.LoadFromSlot(0, &world);
    std::printf("    future-format save loaded: %s\n", loaded ? "yes" : "no");
    ENJIN_EXPECT_TRUE(!loaded);
}

ENJIN_TEST_MAIN()
