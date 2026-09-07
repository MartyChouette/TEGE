// OnStart and the first OnUpdate used to land in the SAME ScriptSystem::Update.
//
// A script that started on frame N was handed that whole frame's deltaTime,
// including all the time before it existed, and with the fixed-step accumulator
// sitting between OnStart and OnUpdate it also took every pending
// OnFixedUpdate tick -- up to six of them at the 0.1s dt clamp in
// Application::Update. Anything integrating `pos += v * dt` teleported on its
// first frame alive.
//
// Ticks now begin on the frame AFTER OnStart. Same-frame ordering was never the
// problem; handing the newcomer the accumulated time was.
#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Transform.h"
#include <angelscript.h>   // reading the probe's counters off the instance

#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Scripting;

namespace {

namespace fs = std::filesystem;

fs::path MakeScriptDir(const char* leaf) {
    fs::path dir = fs::temp_directory_path() / "enjin_lifecycle_test" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

// Counts every lifecycle call it receives, and records the dt of the first
// OnUpdate so the test can see what the script was actually handed.
void WriteProbe(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    f << "class Probe : TegeBehavior {\n"
      << "    int starts = 0;\n"
      << "    int updates = 0;\n"
      << "    int lateUpdates = 0;\n"
      << "    int fixedUpdates = 0;\n"
      << "    void OnStart() { starts += 1; }\n"
      << "    void OnUpdate(float dt) { updates += 1; }\n"
      << "    void OnLateUpdate(float dt) { lateUpdates += 1; }\n"
      << "    void OnFixedUpdate(float dt) { fixedUpdates += 1; }\n"
      << "}\n";
}

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
        system.SetWorld(&world);
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

        // Compiles and runs OnCreate only. OnStart belongs to the first Update.
        system.InitializeAllScripts();
    }

    ~Fixture() {
        system.SetWorld(nullptr);
        engine.Shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    // Read one int member off the live script instance.
    int Count(const char* member) {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(entity);
        if (!sc || sc->scripts.empty() || !sc->scripts[0].instance) return -1;
        auto* obj = static_cast<asIScriptObject*>(sc->scripts[0].instance);
        for (asUINT i = 0; i < obj->GetPropertyCount(); ++i) {
            const char* name = obj->GetPropertyName(i);
            if (name && std::string(name) == member) {
                return *reinterpret_cast<int*>(obj->GetAddressOfProperty(i));
            }
        }
        return -1;
    }

    bool StartedThisFrameFlag() {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(entity);
        return sc && !sc->scripts.empty() && sc->scripts[0].startedThisFrame;
    }
};

constexpr f32 kFrame = 1.0f / 60.0f;

} // namespace

// The regression: one Update used to run OnStart and then OnUpdate on the same
// pass, so `updates` came back 1 here.
ENJIN_TEST(ScriptStartFrame, FirstUpdateStartsButDoesNotTick) {
    Fixture fx("first_update");

    fx.system.Update(kFrame);

    ENJIN_EXPECT_EQ(fx.Count("starts"), 1);
    ENJIN_EXPECT_EQ(fx.Count("updates"), 0);
}

ENJIN_TEST(ScriptStartFrame, TicksResumeOnTheNextFrame) {
    Fixture fx("second_update");

    fx.system.Update(kFrame);
    fx.system.Update(kFrame);

    ENJIN_EXPECT_EQ(fx.Count("starts"), 1);   // OnStart is still once, ever
    ENJIN_EXPECT_EQ(fx.Count("updates"), 1);
}

// The accumulator sits between OnStart and OnUpdate, so this was the worse
// half: a script could take several ticks of time that predated it.
ENJIN_TEST(ScriptStartFrame, NoFixedTicksOnTheStartFrame) {
    Fixture fx("fixed_ticks");

    // A tenth of a second is the dt clamp in Application::Update, which is what
    // a load hitch delivers. Six fixed steps of 1/60 fit inside it.
    fx.system.Update(0.1f);

    ENJIN_EXPECT_EQ(fx.Count("starts"), 1);
    ENJIN_EXPECT_EQ(fx.Count("fixedUpdates"), 0);
}

ENJIN_TEST(ScriptStartFrame, FixedTicksResumeOnTheNextFrame) {
    Fixture fx("fixed_resume");

    fx.system.Update(kFrame);
    fx.system.Update(0.1f);

    ENJIN_EXPECT_TRUE(fx.Count("fixedUpdates") > 0);
}

// OnLateUpdate is gated with OnUpdate: a LateUpdate on a frame whose Update was
// skipped is a stranger state than either choice on its own.
ENJIN_TEST(ScriptStartFrame, LateUpdateIsGatedWithUpdate) {
    Fixture fx("late_update");

    fx.system.Update(kFrame);
    ENJIN_EXPECT_EQ(fx.Count("lateUpdates"), 0);

    fx.system.Update(kFrame);
    ENJIN_EXPECT_EQ(fx.Count("lateUpdates"), 1);
}

// The flag is cleared at the END of the Update that set it, so the external
// fixed clock -- which runs its step loop BEFORE Update -- sees a clean flag on
// the following frame rather than skipping a second one.
ENJIN_TEST(ScriptStartFrame, FlagIsClearedByTheEndOfTheStartUpdate) {
    Fixture fx("flag_cleared");

    fx.system.Update(kFrame);

    ENJIN_EXPECT_FALSE(fx.StartedThisFrameFlag());
}

ENJIN_TEST_MAIN()
