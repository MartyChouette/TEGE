// A script marked disabled must not run, compile, or exist.
//
// Marty, 2026-09-13, found while isolating a crash: `enabled: false` on a script
// component still compiled the module and created the instance. Every per-frame
// dispatcher checked `enabled`; InitScript checked only `initialized`. So the
// script did not tick and did everything else -- which means turning a script
// off to find out whether it is the problem does not take it out of the run.
// That is the one job that switch has.
//
// The old behaviour had a defensible reading (keep it warm so a runtime toggle
// is instant), and the problem was that nothing said which reading was in
// force. It is stated now, at the gate, and pinned here.

#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Transform.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Scripting;

namespace {

namespace fs = std::filesystem;

fs::path MakeDir(const char* leaf) {
    fs::path dir = fs::temp_directory_path() / "enjin_enabled_flag" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

struct Fixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    fs::path dir;
    ECS::Entity entity = ECS::INVALID_ENTITY;

    explicit Fixture(const char* leaf, bool enabled) {
        dir = MakeDir(leaf);
        {
            std::ofstream f(dir / "Probe.as", std::ios::trunc);
            f << "class Probe : TegeBehavior {\n"
              << "    int ticks = 0;\n"
              << "    void OnStart() { ticks = 1; }\n"
              << "}\n";
        }

        engine.Initialize();
        RegisterAllBindings(engine.GetASEngine());
        engine.SetScriptDirectory(dir.string());
        engine.CompileScript((dir / "Probe.as").string());

        system.SetScriptEngine(&engine);
        system.SetCoroutineScheduler(&scheduler);
        system.SetWorld(&world);
        system.SetScriptRoot(dir.string());
        system.SetEnabled(true);

        entity = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(entity);
        ECS::ScriptComponent sc;
        ECS::ScriptAttachment att;
        att.scriptPath = "Probe.as";
        att.className = "Probe";
        att.enabled = enabled;
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

    ECS::ScriptAttachment* Script() {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(entity);
        return (sc && !sc->scripts.empty()) ? &sc->scripts[0] : nullptr;
    }
};

}  // namespace

ENJIN_TEST(ScriptEnabledFlag, ADisabledScriptIsNeverInstantiated) {
    Fixture fx("disabled", /*enabled=*/false);

    auto* s = fx.Script();
    ENJIN_ASSERT_NOT_NULL(s);
    std::printf("    initialized=%d instance=%s\n",
                (int)s->initialized, s->instance ? "yes" : "NO");

    // Both of these used to be true for a disabled script.
    ENJIN_EXPECT_TRUE(!s->initialized);
    ENJIN_EXPECT_TRUE(s->instance == nullptr);

    // And a frame of ticking must not quietly bring it up either.
    fx.system.Update(1.0f / 60.0f);
    auto* after = fx.Script();
    ENJIN_ASSERT_NOT_NULL(after);
    ENJIN_EXPECT_TRUE(!after->initialized);
    ENJIN_EXPECT_TRUE(!after->started);
}

// The control. If the gate were simply "never initialize", the feature would be
// broken rather than fixed.
ENJIN_TEST(ScriptEnabledFlag, AnEnabledScriptStillComesUpAndStarts) {
    Fixture fx("enabled", /*enabled=*/true);

    auto* s = fx.Script();
    ENJIN_ASSERT_NOT_NULL(s);
    ENJIN_EXPECT_TRUE(s->initialized);
    ENJIN_EXPECT_TRUE(s->instance != nullptr);

    fx.system.Update(1.0f / 60.0f);
    auto* after = fx.Script();
    ENJIN_ASSERT_NOT_NULL(after);
    std::printf("    started=%d\n", (int)after->started);
    ENJIN_EXPECT_TRUE(after->started);
}

// Enabling at runtime must bring it up, or "disabled" would mean "permanently
// dead" and a toggle in the inspector would be one-way.
ENJIN_TEST(ScriptEnabledFlag, EnablingAtRuntimeBringsItUp) {
    Fixture fx("toggle", /*enabled=*/false);
    ENJIN_ASSERT_TRUE(!fx.Script()->initialized);

    fx.Script()->enabled = true;
    fx.system.Update(1.0f / 60.0f);

    auto* after = fx.Script();
    ENJIN_ASSERT_NOT_NULL(after);
    std::printf("    after enabling: initialized=%d instance=%s\n",
                (int)after->initialized, after->instance ? "yes" : "NO");
    ENJIN_EXPECT_TRUE(after->initialized);
    ENJIN_EXPECT_TRUE(after->instance != nullptr);
}

// Disabling at runtime keeps the instance. Destroying it would throw away the
// script's state, so toggling off and on again would silently reset it.
ENJIN_TEST(ScriptEnabledFlag, DisablingAtRuntimeKeepsTheInstance) {
    Fixture fx("keepwarm", /*enabled=*/true);
    fx.system.Update(1.0f / 60.0f);
    ENJIN_ASSERT_TRUE(fx.Script()->instance != nullptr);

    fx.Script()->enabled = false;
    fx.system.Update(1.0f / 60.0f);

    auto* after = fx.Script();
    ENJIN_ASSERT_NOT_NULL(after);
    ENJIN_EXPECT_TRUE(after->instance != nullptr);
    ENJIN_EXPECT_TRUE(after->initialized);
}

ENJIN_TEST_MAIN()
