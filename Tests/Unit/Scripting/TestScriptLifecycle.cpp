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

// ===========================================================================
// Statement budget: a big call is SLICED across frames, not killed.
//
// The instruction ceiling is a runaway guard. Used alone it doubles as a frame
// budget, and those are different problems: `while(true)` and "this level solve
// is genuinely big" got the same maximal punishment, the script switched off for
// the rest of the session. A call that runs out of slice is now suspended and
// resumed on the next dispatch; only a call that keeps doing it is a runaway.
// ===========================================================================

namespace {

// ~3.6M statements: several slices deep, so the call is STILL parked after a
// resume. That matters for the teardown test: with a call that finishes on its
// first resume, the teardown bug hides. Module globals survive the instance,
// which is how the test sees OnDestroy after teardown released it.
void WriteHeavyProbe(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    f << "int g_done = 0;\n"
      << "int g_destroyed = 0;\n"
      << "class Probe : TegeBehavior {\n"
      << "    void OnUpdate(float dt) {\n"
      << "        int n = 0;\n"
      << "        for (int i = 0; i < 1200000; i++) { n += i; }\n"
      << "        g_done += 1;\n"
      << "    }\n"
      << "    void OnDestroy() { g_destroyed = 1; }\n"
      << "}\n";
}

struct HeavyFixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    fs::path dir;
    ECS::Entity entity = ECS::INVALID_ENTITY;

    explicit HeavyFixture(const char* leaf) {
        dir = MakeScriptDir(leaf);
        WriteHeavyProbe(dir / "Probe.as");

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
        system.InitializeAllScripts();
    }

    ~HeavyFixture() {
        system.SetWorld(nullptr);
        engine.Shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    // Module globals outlive the script instance, unlike its members.
    int Global(const char* name) {
        asIScriptEngine* as = engine.GetASEngine();
        if (!as) return -1;
        asIScriptModule* mod = as->GetModuleByIndex(0);
        if (!mod) return -1;
        for (asUINT i = 0; i < mod->GetGlobalVarCount(); ++i) {
            const char* n = nullptr;
            mod->GetGlobalVar(i, &n);
            if (n && std::string(n) == name) {
                return *reinterpret_cast<int*>(mod->GetAddressOfGlobalVar(i));
            }
        }
        return -1;
    }

    ECS::ScriptAttachment* Att() {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(entity);
        return (sc && !sc->scripts.empty()) ? &sc->scripts[0] : nullptr;
    }
};

} // namespace

ENJIN_TEST(ScriptBudgetSlices, BigCallParksInsteadOfBeingKilled) {
    HeavyFixture fx("parks");

    fx.system.Update(kFrame);   // OnStart only; ticks begin next frame
    fx.system.Update(kFrame);   // OnUpdate starts and runs out of slice

    ENJIN_ASSERT_NOT_NULL(fx.Att());
    ENJIN_EXPECT_NOT_NULL(fx.Att()->pendingContext);   // parked, not abandoned
    ENJIN_EXPECT_FALSE(fx.Att()->hasError);            // and NOT disabled
    ENJIN_EXPECT_EQ(fx.Global("g_done"), 0);           // hasn't finished yet
}

ENJIN_TEST(ScriptBudgetSlices, ParkedCallResumesAndFinishes) {
    HeavyFixture fx("resumes");

    for (int i = 0; i < 12; ++i) fx.system.Update(kFrame);

    ENJIN_EXPECT_FALSE(fx.Att()->hasError);
    ENJIN_EXPECT_TRUE(fx.Global("g_done") > 0);   // it completed rather than dying
}

// Regression: every lifecycle dispatch begins with ResumePending, so tearing
// down an entity whose call was parked spent the OnDisable and OnDestroy
// dispatches resuming a call that was about to be thrown away, and neither hook
// ran. OnDestroy is the cleanup hook; silently skipping it is the despawn-leak
// class of bug over again. The pending call is discarded FIRST now.
ENJIN_TEST(ScriptBudgetSlices, DestroyWhileParkedStillRunsOnDestroy) {
    HeavyFixture fx("destroy_parked");

    fx.system.Update(kFrame);
    fx.system.Update(kFrame);
    ENJIN_ASSERT_NOT_NULL(fx.Att());
    ENJIN_ASSERT_NOT_NULL(fx.Att()->pendingContext);   // genuinely parked

    fx.system.ShutdownAllScripts();

    ENJIN_EXPECT_EQ(fx.Global("g_destroyed"), 1);
}

// ===========================================================================
// A thrown exception says WHAT and WHERE.
//
// ClassifyExecuteResult put ctx->GetExceptionString() into script.lastError and
// then called HandleScriptError, which overwrote it with ScriptEngine's
// m_LastError -- a COMPILE-time field, set when a module fails to build and
// never touched again. So the one useful string in the path was thrown away
// every time. At runtime that field is empty, and what reached the log was
// "Unknown error in OnUpdate". Worse, it is not always empty: a stale compile
// failure from another module would have been reported as this frame's
// exception.
//
// The message now carries the fault, the function it threw in, and the section
// and line, because in a module spread over eight files the fault without the
// place is not actionable.
// ===========================================================================

namespace {

void WriteThrowingProbe(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    // The null access is on line 5, and the test asserts that number: a
    // message naming the file but not the place is the thing being fixed.
    f << "class Probe : TegeBehavior {\n"
      << "    int v = 0;\n"
      << "    void OnUpdate(float dt) {\n"
      << "        Probe@ boom = null;\n"
      << "        boom.v = 1;\n"
      << "    }\n"
      << "}\n";
}

struct ThrowFixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    fs::path dir;
    ECS::Entity entity = ECS::INVALID_ENTITY;

    explicit ThrowFixture(const char* leaf) {
        dir = MakeScriptDir(leaf);
        WriteThrowingProbe(dir / "Probe.as");

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
        system.InitializeAllScripts();
    }

    ~ThrowFixture() {
        system.SetWorld(nullptr);
        engine.Shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    ECS::ScriptAttachment* Att() {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(entity);
        return (sc && !sc->scripts.empty()) ? &sc->scripts[0] : nullptr;
    }
};

bool Contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

} // namespace

ENJIN_TEST(ScriptExceptionMessage, KeepsWhatAngelScriptThrew) {
    ThrowFixture fx("throws");

    fx.system.Update(kFrame);   // OnStart only
    fx.system.Update(kFrame);   // OnUpdate throws

    ENJIN_ASSERT_NOT_NULL(fx.Att());
    ENJIN_ASSERT_TRUE(fx.Att()->hasError);

    const std::string& msg = fx.Att()->lastError;
    ENJIN_EXPECT_TRUE(Contains(msg, "Null pointer access"));
    ENJIN_EXPECT_FALSE(Contains(msg, "Unknown error"));
}

ENJIN_TEST(ScriptExceptionMessage, SaysWhereItThrew) {
    ThrowFixture fx("throws_where");

    fx.system.Update(kFrame);
    fx.system.Update(kFrame);

    ENJIN_ASSERT_NOT_NULL(fx.Att());
    ENJIN_ASSERT_TRUE(fx.Att()->hasError);

    const std::string& msg = fx.Att()->lastError;
    ENJIN_EXPECT_TRUE(Contains(msg, "OnUpdate"));   // the function that threw
    ENJIN_EXPECT_TRUE(Contains(msg, "Probe.as"));   // the section
    ENJIN_EXPECT_TRUE(Contains(msg, ":5:"));        // the line boom is on
}

ENJIN_TEST_MAIN()
