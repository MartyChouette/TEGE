// Destroying an entity must tear down its scripts.
//
// ReleaseInstance was reachable only from ShutdownAllScripts and the hot-reload
// retry path. Nothing ran per entity. So in a spawn-heavy game every despawn
// leaked its asIScriptObject, left its coroutines ticking and left its event
// listeners firing on a dead entity until the 1024-per-event cap started
// rejecting new registrations -- and OnDestroy never fired during play at all,
// only at Stop.
//
// The cause was structural: World had exactly one destroy-observer slot and
// PlayMode had taken it, so nothing else could watch. The slot is now a list,
// and ScriptSystem registers the teardown that ShutdownAllScripts also uses --
// one teardown, two callers, rather than a second copy for despawn.
#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Transform.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Scripting;

namespace {

namespace fs = std::filesystem;

fs::path MakeScriptDir(const char* leaf) {
    fs::path dir = fs::temp_directory_path() / "enjin_teardown_test" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

// The shape every script in Examples/ uses.
void WriteProbe(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    f << "class Probe : TegeBehavior {\n"
      << "    int ticks = 0;\n"
      << "    void OnStart() { ticks = 1; }\n"
      << "    void OnDestroy() { ticks = -1; }\n"
      << "}\n";
}

// A world with one entity carrying a compiled script instance.
struct Fixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    fs::path dir;
    ECS::Entity entity = ECS::INVALID_ENTITY;

    explicit Fixture(const char* leaf) {
        dir = MakeScriptDir(leaf);
        WriteProbe(dir / "Probe.as");

        engine.Initialize();
        RegisterAllBindings(engine.GetASEngine());
        engine.SetScriptDirectory(dir.string());
        engine.CompileScript((dir / "Probe.as").string());

        system.SetScriptEngine(&engine);
        system.SetCoroutineScheduler(&scheduler);
        system.SetWorld(&world);       // installs the destroy observer
        system.SetScriptRoot(dir.string());
        system.SetEnabled(true);

        entity = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(entity);
        ECS::ScriptComponent sc;
        ECS::ScriptAttachment att;
        att.scriptPath = "Probe.as";
        att.className = "Probe";
        att.enabled = true;
        sc.scripts.push_back(att);
        world.AddComponent<ECS::ScriptComponent>(entity, sc);

        system.InitializeAllScripts();
    }

    ~Fixture() {
        system.SetWorld(nullptr);
        engine.Shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    ECS::ScriptAttachment* Attachment() {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(entity);
        return (sc && !sc->scripts.empty()) ? &sc->scripts[0] : nullptr;
    }
};

} // namespace

ENJIN_TEST(ScriptTeardown, ScriptInstanceIsReleasedWhenTheEntityIsDestroyed) {
    // Arrange: an entity with a live script instance.
    Fixture fx("release");
    auto* att = fx.Attachment();
    ENJIN_ASSERT_TRUE(att != nullptr);
    ENJIN_ASSERT_TRUE(att->instance != nullptr);

    // Act: destroy it the way gameplay does. DestroyEntity is deferred, so the
    // observer fires on the flush, while the component data is still intact.
    fx.world.DestroyEntity(fx.entity);
    fx.world.FlushPendingDestructions();

    // Assert: the instance pointer is cleared, which is what proves
    // ReleaseInstance ran. Before the fix nothing per-entity ever ran.
    ENJIN_EXPECT_TRUE(att->instance == nullptr);
    ENJIN_EXPECT_TRUE(!att->initialized);
}

ENJIN_TEST(ScriptTeardown, ImmediateDestroyTearsDownToo) {
    // Arrange: the other destruction path. Both go through
    // DestroyEntityInternal, which is the single choke point the observer
    // hangs on -- this test is what keeps that true.
    Fixture fx("immediate");
    auto* att = fx.Attachment();
    ENJIN_ASSERT_TRUE(att != nullptr && att->instance != nullptr);

    // Act
    fx.world.DestroyEntityImmediate(fx.entity);

    // Assert
    ENJIN_EXPECT_TRUE(att->instance == nullptr);
}

ENJIN_TEST(ScriptTeardown, TeardownIsSafeOnAnEntityWithNoScripts) {
    // Arrange: the observer fires for EVERY destroyed entity, not just scripted
    // ones, so the common case must be a cheap no-op rather than a crash.
    Fixture fx("noscript");
    const ECS::Entity plain = fx.world.CreateEntity();
    fx.world.AddComponent<ECS::TransformComponent>(plain);

    // Act / Assert
    fx.world.DestroyEntityImmediate(plain);
    ENJIN_EXPECT_TRUE(!fx.world.IsValid(plain));
}

ENJIN_TEST(ScriptTeardown, TearingDownTwiceDoesNotDoubleRelease) {
    // Arrange: ShutdownAllScripts and the destroy observer share one teardown,
    // so a Stop right after a despawn runs it twice on the same attachment. The
    // second pass must find nothing left to release rather than double-freeing.
    Fixture fx("twice");
    auto* att = fx.Attachment();
    ENJIN_ASSERT_TRUE(att != nullptr && att->instance != nullptr);

    // Act
    fx.system.TeardownEntityScripts(fx.entity);
    ENJIN_ASSERT_TRUE(att->instance == nullptr);
    fx.system.TeardownEntityScripts(fx.entity);

    // Assert
    ENJIN_EXPECT_TRUE(att->instance == nullptr);
}

ENJIN_TEST(ScriptTeardown, ShutdownStillTearsDownEveryEntity) {
    // Arrange: the path that always worked must keep working now that it shares
    // its implementation with the despawn path.
    Fixture fx("shutdown");
    auto* att = fx.Attachment();
    ENJIN_ASSERT_TRUE(att != nullptr && att->instance != nullptr);

    // Act
    fx.system.ShutdownAllScripts();

    // Assert
    ENJIN_EXPECT_TRUE(att->instance == nullptr);
    ENJIN_EXPECT_TRUE(!att->initialized);
}

ENJIN_TEST(ScriptTeardown, DetachingTheWorldRemovesTheObserver) {
    // Arrange: the observer captures `this`. A ScriptSystem that stops pointing
    // at a World must stop observing it, or the next despawn calls into a
    // system that no longer manages those scripts.
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e);

    {
        ScriptSystem sys;
        sys.SetWorld(&world);
        sys.SetWorld(nullptr);   // must unregister from `world`
    }

    // Act / Assert: destroying after the system is gone must not crash.
    world.DestroyEntityImmediate(e);
    ENJIN_EXPECT_TRUE(!world.IsValid(e));
}

ENJIN_TEST(ScriptTeardown, OutlivingTheWorldIsNotACrash) {
    // The shape that crashed every exported game on shutdown.
    //
    // GamePlayer::Shutdown destroys the World with an explicit m_World.reset()
    // and only then lets its members destruct. So ~ScriptSystem ran with
    // m_World pointing at freed memory, and unregistering the destroy observer
    // dereferenced it -- an access violation after "Player shutting down...",
    // reproducible in the exported-game smoke and in nothing else, because the
    // editor never resets the World out from under its systems.
    //
    // ScriptSystem holds a weak handle from World::LifeToken() and skips the
    // unregister when it has expired, so no caller has to remember an ordering
    // rule.
    ScriptSystem sys;
    {
        auto world = std::make_unique<ECS::World>();
        const ECS::Entity e = world->CreateEntity();
        world->AddComponent<ECS::TransformComponent>(e);
        sys.SetWorld(world.get());
        world.reset();          // the World dies first, as the player does it
    }

    // Act / Assert: the destructor must not touch the dead World. Reaching the
    // end of this test at all is the assertion.
    ENJIN_EXPECT_TRUE(true);
}

ENJIN_TEST(ScriptTeardown, ANewWorldReplacesTheObserverCleanly) {
    // A scene load can hand the system a different World. The old registration
    // must go without touching a World that may already be gone.
    ScriptSystem sys;
    auto first = std::make_unique<ECS::World>();
    sys.SetWorld(first.get());
    first.reset();

    ECS::World second;
    sys.SetWorld(&second);      // must not deref the freed first world

    const ECS::Entity e = second.CreateEntity();
    second.AddComponent<ECS::TransformComponent>(e);
    second.DestroyEntityImmediate(e);

    ENJIN_EXPECT_TRUE(!second.IsValid(e));
    sys.SetWorld(nullptr);
}

ENJIN_TEST_MAIN()
