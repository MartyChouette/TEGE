// A script's OnUpdate must see the frame's delta, on every frame.
//
// Found on web, 2026-09-30. ScriptSystem::Update ticks the script clock, then
// runs the fixed-step accumulator, then OnUpdate. FixedUpdate used to tick the
// clock again as TickBindingsTime(0, step), which set the frame delta to 0 and
// counted an extra frame. So on any frame with a fixed step in it -- most frames
// at 60 fps, every frame on a slow one -- OnUpdate read Time_GetDeltaTime() == 0.
// Twister sums Time_GetDeltaTime() into an idle timer and wakes from attract
// mode when a key resets it; the timer never left zero, so nothing could ever
// start a run in the browser, keyboard or touch.

#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Transform.h"
#include <angelscript.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Enjin;
using namespace Enjin::Scripting;

namespace {

namespace fs = std::filesystem;

struct Fixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    fs::path dir;
    ECS::Entity entity = ECS::INVALID_ENTITY;

    Fixture() {
        dir = fs::temp_directory_path() / "enjin_script_clock";
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        {
            std::ofstream f(dir / "Clock.as", std::ios::trunc);
            f << "class Clock : TegeBehavior {\n"
              << "    float seenDt = -1.0f;\n"
              << "    float seenFixed = -1.0f;\n"
              << "    int seenFrames = -1;\n"
              << "    float seenFixedDt = -1.0f;\n"
              << "    void OnFixedUpdate(float dt) { seenFixedDt = Time_GetDeltaTime(); }\n"
              << "    void OnUpdate(float dt) {\n"
              << "        seenDt = Time_GetDeltaTime();\n"
              << "        seenFixed = Time_GetFixedDeltaTime();\n"
              << "        seenFrames = int(Time_GetFrameCount());\n"
              << "    }\n"
              << "}\n";
        }

        // The script clock is process-wide, and every test here shares the
        // process: start each one from zero.
        ResetBindingsTime();

        engine.Initialize();
        RegisterAllBindings(engine.GetASEngine());
        engine.SetScriptDirectory(dir.string());
        engine.CompileScript((dir / "Clock.as").string());

        system.SetScriptEngine(&engine);
        system.SetCoroutineScheduler(&scheduler);
        system.SetWorld(&world);
        system.SetScriptRoot(dir.string());
        system.SetEnabled(true);

        entity = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(entity);
        ECS::ScriptComponent sc;
        ECS::ScriptAttachment att;
        att.scriptPath = "Clock.as";
        att.className = "Clock";
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

    template <typename T>
    T Read(const char* member) {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(entity);
        if (!sc || sc->scripts.empty() || !sc->scripts[0].instance) return T(-1);
        auto* obj = static_cast<asIScriptObject*>(sc->scripts[0].instance);
        for (asUINT i = 0; i < obj->GetPropertyCount(); ++i) {
            const char* name = obj->GetPropertyName(i);
            if (name && std::string(name) == member) return *reinterpret_cast<T*>(obj->GetAddressOfProperty(i));
        }
        return T(-1);
    }
};

} // namespace

// THE ONE THAT MATTERS. A tenth of a second holds six fixed steps of 1/60.
ENJIN_TEST(ScriptClock, OnUpdateSeesTheFrameDeltaOnAFrameWithFixedSteps) {
    Fixture fx;
    fx.system.Update(1.0f / 60.0f);   // OnStart frame
    fx.system.Update(0.1f);

    const f32 dt = fx.Read<f32>("seenDt");
    std::printf("    Time_GetDeltaTime() in OnUpdate: %.4f (frame was 0.1000)\n", dt);
    ENJIN_EXPECT_FLOAT_NEAR(dt, 0.1f, 0.0001f);
}

// A fixed step is not a frame: six of them must not count as six frames.
ENJIN_TEST(ScriptClock, FixedStepsDoNotCountAsFrames) {
    Fixture fx;
    fx.system.Update(1.0f / 60.0f);
    fx.system.Update(0.1f);

    const int frames = fx.Read<int>("seenFrames");
    std::printf("    Time_GetFrameCount() after two frames: %d\n", frames);
    ENJIN_EXPECT_EQ(frames, 2);
}

// Inside OnFixedUpdate the delta is the step; OnUpdate on the same frame still
// gets the frame's.
ENJIN_TEST(ScriptClock, OnFixedUpdateSeesTheStepAndOnUpdateTheFrame) {
    Fixture fx;
    fx.system.Update(1.0f / 60.0f);
    fx.system.Update(0.1f);

    const f32 fixedDt = fx.Read<f32>("seenFixedDt");
    const f32 frameDt = fx.Read<f32>("seenDt");
    std::printf("    in OnFixedUpdate: %.4f   in OnUpdate: %.4f\n", fixedDt, frameDt);
    ENJIN_EXPECT_FLOAT_NEAR(fixedDt, fx.Read<f32>("seenFixed"), 0.0001f);
    ENJIN_EXPECT_FLOAT_NEAR(frameDt, 0.1f, 0.0001f);
}

// What FixedUpdate is still there to do: report the step it ran.
ENJIN_TEST(ScriptClock, FixedDeltaStillReportsTheStep) {
    Fixture fx;
    fx.system.Update(1.0f / 60.0f);
    fx.system.Update(0.1f);

    ENJIN_EXPECT_TRUE(fx.Read<f32>("seenFixed") > 0.0f);
}

ENJIN_TEST_MAIN()
