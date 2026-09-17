#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Gameplay/SavePointSystem.h"
#include "Enjin/Gameplay/TieredSaveSystem.h"
#include "Enjin/Gameplay/SaveBackend.h"
#include "Enjin/Gameplay/SaveIndicator.h"

#include <map>
#include <memory>
#include <string>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Gameplay;

// SavePointComponent had five script setters and no reader until 2026-09-16: a
// configured save point compiled, ran and never saved. These pin the behaviour
// that replaced that, and each is written so a regression shows up as a save
// that did NOT happen rather than as a crash.

namespace {

// In-memory save backend. Nothing here touches the filesystem, so these run in
// milliseconds, cannot be disturbed by a developer's own play-session saves,
// and leave nothing to clean up.
//
// The first version of this file used LocalSaveBackend against a temp
// directory, copying an existing test that had to isolate itself for exactly
// that reason. Implementing the five-method ISaveBackend interface is less code
// than the directory juggling it replaces.
class MemorySaveBackend : public ISaveBackend {
public:
    bool Write(const std::string& key, const std::string& data) override {
        m_Data[key] = data;
        return true;
    }
    bool Read(const std::string& key, std::string& outData) override {
        auto it = m_Data.find(key);
        if (it == m_Data.end()) return false;
        outData = it->second;
        return true;
    }
    bool Delete(const std::string& key) override { return m_Data.erase(key) > 0; }
    bool Exists(const std::string& key) override { return m_Data.count(key) > 0; }
    std::string GetName() const override { return "Memory"; }

private:
    std::map<std::string, std::string> m_Data;
};

struct Fixture {
    World world;
    TieredSaveSystem save;
    SavePointSystem points;
    SaveIndicator indicator;
    Entity player = INVALID_ENTITY;

    Fixture() {
        save.SetBackend(std::make_shared<MemorySaveBackend>());

        player = world.CreateEntity();
        world.AddComponent<NameComponent>(player, NameComponent{"Player"});
        world.AddComponent<TransformComponent>(player, TransformComponent{});

        points.SetWorld(&world);
        points.SetSaveSystem(&save);
        points.SetSceneName("TestScene");
        // The display goes through SubtitleSystem, so the tests can assert what
        // a player would actually be shown rather than an internal string.
        indicator.SetWorld(&world);
        points.SetIndicator(&indicator);
        // No input map on purpose: a test that simulated key presses would be
        // testing the input system. Points here either save on enter, or are
        // checked for NOT saving.
    }

    Entity AddPoint(Math::Vector3 at, bool saveOnEnter = true) {
        Entity e = world.CreateEntity();
        TransformComponent xf;
        xf.position = at;
        world.AddComponent<TransformComponent>(e, xf);
        SavePointComponent sp;
        sp.saveOnEnter = saveOnEnter;
        sp.radius = 2.0f;
        world.AddComponent<SavePointComponent>(e, sp);
        return e;
    }

    // Both, in the order the runtimes do it. Ticking only the save point system
    // left the indicator's timer frozen, and a test then read "the prompt was
    // re-issued every frame" from what was really "nothing ever expired it".
    void Tick(f32 dt = 0.016f) {
        points.Update(dt);
        indicator.Update(dt);
    }

    void MovePlayer(Math::Vector3 to) {
        world.GetComponent<TransformComponent>(player)->position = to;
    }

    bool AnySlotUsed() {
        for (const auto& s : save.GetAllSlots()) {
            if (!s.isEmpty) return true;
        }
        return false;
    }
};

} // namespace

ENJIN_TEST(SavePoint, SavesWhenThePlayerWalksIn) {
    // Arrange: a save point at the origin, player far away.
    Fixture f;
    Entity point = f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));
    f.MovePlayer(Math::Vector3(50.0f, 0.0f, 0.0f));
    f.points.Update(0.016f);
    // Asserted first because it is the half that would pass even with the
    // system deleted; the walk-in below is the half that would not.
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());
    ENJIN_EXPECT_FALSE(f.world.GetComponent<SavePointComponent>(point)->playerInRange);

    // Act: walk into the radius.
    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    f.points.Update(0.016f);

    // Assert
    ENJIN_EXPECT_TRUE(f.world.GetComponent<SavePointComponent>(point)->playerInRange);
    ENJIN_EXPECT_TRUE(f.world.GetComponent<SavePointComponent>(point)->used);
    ENJIN_EXPECT_TRUE(f.AnySlotUsed());
}

ENJIN_TEST(SavePoint, RadiusIsRespected) {
    Fixture f;
    Entity point = f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));
    f.world.GetComponent<SavePointComponent>(point)->radius = 2.0f;

    // 2.5 away from a radius of 2 is outside. Just outside rather than far
    // away, because an off-by-one in the comparison is what this catches.
    f.MovePlayer(Math::Vector3(2.5f, 0.0f, 0.0f));
    f.points.Update(0.016f);
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());

    // 1.9 is inside.
    f.MovePlayer(Math::Vector3(1.9f, 0.0f, 0.0f));
    f.points.Update(0.016f);
    ENJIN_EXPECT_TRUE(f.AnySlotUsed());
}

ENJIN_TEST(SavePoint, OneTimeUseStopsAfterTheFirst) {
    Fixture f;
    Entity point = f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));
    f.world.GetComponent<SavePointComponent>(point)->oneTimeUse = true;

    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    f.points.Update(0.016f);
    ENJIN_EXPECT_TRUE(f.world.GetComponent<SavePointComponent>(point)->used);

    // Walk out and back in. `used` latches, so the second entry must not save.
    // Proven by deleting the slot first: if it saves again, the slot comes back.
    f.save.DeleteSlot(0);
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());

    f.MovePlayer(Math::Vector3(50.0f, 0.0f, 0.0f));
    f.points.Update(0.016f);
    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    f.points.Update(0.016f);
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());
}

ENJIN_TEST(SavePoint, RequiringInputDoesNotSaveOnItsOwn) {
    Fixture f;
    // saveOnEnter false, and no input map attached, so nothing can press
    // Interact. Standing in it must not save.
    Entity point = f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f), /*saveOnEnter=*/false);

    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    for (int i = 0; i < 10; ++i) f.points.Update(0.016f);

    ENJIN_EXPECT_FALSE(f.AnySlotUsed());
    ENJIN_EXPECT_FALSE(f.world.GetComponent<SavePointComponent>(point)->used);
    // The prompt is how a player is told the point is there at all.
    ENJIN_EXPECT_TRUE(!f.points.GetPrompt().empty());
}

ENJIN_TEST(SavePoint, DisablingInWorldPointsTurnsThemAllOff) {
    // Arrange: a game manager that switches in-world points off.
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.allowInWorldSavePoints = false;
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);
    f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));

    // Act: stand in a point that would otherwise save.
    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    f.points.Update(0.016f);

    // Assert
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());
}

ENJIN_TEST(SavePoint, ZeroRadiusFallsBackToTheSceneDefault) {
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.savePointRadius = 10.0f;      // generous, and not the component default
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);

    Entity point = f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));
    f.world.GetComponent<SavePointComponent>(point)->radius = 0.0f;   // "use the default"

    // 6 away: outside the component's own 2.0 default, inside the scene's 10.
    // If the fallback is dropped this saves nothing.
    f.MovePlayer(Math::Vector3(6.0f, 0.0f, 0.0f));
    f.points.Update(0.016f);
    ENJIN_EXPECT_TRUE(f.AnySlotUsed());
}

ENJIN_TEST(SavePoint, SlotTargetIsHonoured) {
    // Arrange: a point that names slot 4 rather than "next available".
    Fixture f;
    Entity point = f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));
    f.world.GetComponent<SavePointComponent>(point)->slotTarget = 4;

    // Act
    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    f.points.Update(0.016f);

    // Assert: slot 4 written, and slot 0 -- where "next available" would have
    // gone -- untouched.
    ENJIN_EXPECT_FALSE(f.save.GetSlotInfo(4).isEmpty);
    ENJIN_EXPECT_TRUE(f.save.GetSlotInfo(0).isEmpty);
}

ENJIN_TEST(SavePoint, NoPlayerMeansNoSaveAndNoCrash) {
    Fixture f;
    // A scene can legitimately have save points and no player yet -- during a
    // load, or in a menu scene. It must not save, and it must not fall over.
    f.world.DestroyEntity(f.player);
    f.world.Update(0.0f);            // flush the deferred destroy

    f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));
    f.points.Update(0.016f);

    ENJIN_EXPECT_FALSE(f.AnySlotUsed());
}

// ===========================================================================
// Auto-save configuration
//
// AutoSaveConfig and SaveSystemComponent's auto-save block are the same six
// settings written twice, and nothing connected them: ConfigureAutoSave had no
// callers, so `enabled` stayed false and TieredSaveSystem::Update returned on
// its first line. Timed auto-save had never run in a shipped game.
// ===========================================================================

ENJIN_TEST(AutoSaveConfig, ComponentTurnsAutoSaveOn) {
    // Arrange: a game manager asking for timed auto-save.
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.autoSaveEnabled = true;
    cfg.autoSaveOnInterval = true;
    cfg.autoSaveIntervalSeconds = 60.0f;
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);
    ENJIN_EXPECT_FALSE(f.save.GetAutoSaveConfig().enabled);   // off until it is read

    // Act: one tick of the save system.
    f.save.Update(0.016f, &f.world, "TestScene");

    // Assert
    ENJIN_EXPECT_TRUE(f.save.GetAutoSaveConfig().enabled);
    ENJIN_EXPECT_TRUE(f.save.GetAutoSaveConfig().onTimedInterval);
    ENJIN_EXPECT_FLOAT_EQ(f.save.GetAutoSaveConfig().intervalSeconds, 60.0f);
}

ENJIN_TEST(AutoSaveConfig, TimedAutoSaveActuallyFires) {
    // Arrange: a short interval, at the clamp floor so the test stays quick.
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.autoSaveEnabled = true;
    cfg.autoSaveOnInterval = true;
    cfg.autoSaveIntervalSeconds = TieredSaveSystem::kMinAutoSaveInterval;
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);

    // Act: tick past the interval.
    for (int i = 0; i < 4; ++i) {
        f.save.Update(2.0f, &f.world, "TestScene");
    }

    // Assert: something was written. Before this wiring nothing ever was.
    ENJIN_EXPECT_TRUE(f.AnySlotUsed());
}

ENJIN_TEST(AutoSaveConfig, ZeroIntervalIsClampedNotObeyed) {
    // Arrange: 0 seconds, which would save every single frame.
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.autoSaveEnabled = true;
    cfg.autoSaveOnInterval = true;
    cfg.autoSaveIntervalSeconds = 0.0f;
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);

    // Act
    f.save.Update(0.016f, &f.world, "TestScene");

    // Assert: clamped to the floor, not taken literally and not silently
    // turned into "never".
    ENJIN_EXPECT_FLOAT_EQ(f.save.GetAutoSaveConfig().intervalSeconds,
                          TieredSaveSystem::kMinAutoSaveInterval);
}

ENJIN_TEST(AutoSaveConfig, NoComponentLeavesAutoSaveAlone) {
    // Arrange: a world with no game manager at all.
    Fixture f;

    // Act
    f.save.Update(0.016f, &f.world, "TestScene");

    // Assert: still off, and nothing saved. A scene without the component must
    // not start auto-saving behind the author's back.
    ENJIN_EXPECT_FALSE(f.save.GetAutoSaveConfig().enabled);
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());
}

ENJIN_TEST(AutoSaveConfig, SceneTransitionSavesWhenAsked) {
    // Arrange: auto-save on scene transition, which is what a project ticks to
    // get "save when the player leaves a level".
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.autoSaveEnabled = true;
    cfg.autoSaveOnInterval = false;          // only the transition should fire
    cfg.autoSaveOnSceneTransition = true;
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);
    f.save.Update(0.016f, &f.world, "LevelOne");   // adopt the config
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());

    // Act: leave the scene.
    f.save.OnSceneTransition("LevelOne", "LevelTwo", &f.world);

    // Assert
    ENJIN_EXPECT_TRUE(f.AnySlotUsed());
}

ENJIN_TEST(AutoSaveConfig, SceneTransitionStaysQuietWhenNotAsked) {
    // Arrange: auto-save on, transition saving explicitly OFF. The master
    // switch being on must not save every time a scene changes.
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.autoSaveEnabled = true;
    cfg.autoSaveOnInterval = false;
    cfg.autoSaveOnSceneTransition = false;
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);
    f.save.Update(0.016f, &f.world, "LevelOne");

    // Act
    f.save.OnSceneTransition("LevelOne", "LevelTwo", &f.world);

    // Assert
    ENJIN_EXPECT_FALSE(f.AnySlotUsed());
}

ENJIN_TEST(SavePoint, ConfirmationReachesTheDisplay) {
    // Arrange: a save point that saves on entry.
    Fixture f;
    f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));
    ENJIN_EXPECT_TRUE(f.indicator.GetText().empty());

    // Act
    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    f.points.Update(0.016f);

    // Assert: the player is TOLD, with no opt-in in the way. This briefly went
    // through SubtitleSystem::ShowCaption, which early-returns unless captions
    // are enabled -- so a save was invisible to anyone who had not turned
    // captions on, which is most people. A caption describes a SOUND; a save
    // confirmation is the save system talking.
    ENJIN_EXPECT_EQ(f.indicator.GetText(), std::string("Game Saved"));
}

ENJIN_TEST(SavePoint, PromptIsShownOnceNotEveryFrame) {
    // Arrange: a point that needs a key press, so it prompts while you stand
    // in it.
    Fixture f;
    f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f), /*saveOnEnter=*/false);

    // Act: stand in it for sixty frames.
    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    for (int i = 0; i < 60; ++i) f.Tick();

    // Assert: still showing, and its timer has not been reset to full by the
    // last frame. Re-issuing every frame would leave it pinned at its duration
    // forever and it would never expire after the player walks away.
    ENJIN_EXPECT_FALSE(f.indicator.GetText().empty());
    ENJIN_EXPECT_TRUE(f.indicator.GetRemaining() < 4.0f);
}

ENJIN_TEST(SavePoint, ShowSaveIndicatorOffMeansNoMessage) {
    // Arrange: a project that asked for no save indicator. It still SAVES; it
    // just does not say so, which is the difference between a setting and a
    // switch.
    Fixture f;
    Entity manager = f.world.CreateEntity();
    SaveSystemComponent cfg;
    cfg.showSaveIndicator = false;
    f.world.AddComponent<SaveSystemComponent>(manager, cfg);
    f.AddPoint(Math::Vector3(0.0f, 0.0f, 0.0f));

    // Act
    f.MovePlayer(Math::Vector3(0.5f, 0.0f, 0.0f));
    f.Tick();

    // Assert: saved, and silent.
    ENJIN_EXPECT_TRUE(f.AnySlotUsed());
    ENJIN_EXPECT_TRUE(f.indicator.GetText().empty());
}

ENJIN_TEST_MAIN()
