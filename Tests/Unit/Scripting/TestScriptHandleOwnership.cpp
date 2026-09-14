// A native binding that takes an object HANDLE owns that reference.
//
// AngelScript hands a plain `Type@` parameter to native code as a reference the
// callee now owns and must Release. `Type@+` (auto handle) is the form where
// the engine releases it for you. Three bindings took a plain handle and none
// of them released it:
//
//   uint Events_Listen(const string &in, EventCallback@)
//   void Events_Send(const string &in, EventData@)
//   void Events_Broadcast(EventData@)
//
// Events_Listen also AddRef'd a second reference into the event bus, so every
// subscription left its delegate at a count of two; teardown released one and
// the other survived the engine, held by nothing the collector could enumerate.
// That is the shutdown line this engine printed once per play cycle for as long
// as the binding existed: "GC cannot destroy an object of type '$func' as it
// can't see all references. Current ref count is 1." Measured on BiscuitBird:
// 1 leak on 2 cycles, 3 on 4, 7 on 8 -- exactly one per restart. It also kept
// the whole script module alive ("There is an external reference to an object in
// module 'scripts_BirdFlight', preventing it from being deleted").
//
// Send and Broadcast were WORSE and nobody had noticed, because they are called
// once per event FIRED rather than once per subscription, and EventData is
// registered asOBJ_REF with no asOBJ_GC flag -- so a leaked one produces no
// message at all. The bug that announced itself got found; the silent one next
// to it did not.
//
// These two tests are the shape of the whole class: subscribe, fire, tear down,
// and assert nothing survived.

#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/Scripting/ScriptSystem.h"
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

fs::path MakeDir(const char* leaf) {
    fs::path dir = fs::temp_directory_path() / "enjin_handle_ownership" / leaf;
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

// Subscribes with a DELEGATE and fires an event carrying an EventData, which is
// all three leaking bindings in four lines of script.
void WriteProbe(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    f << "class Probe : TegeBehavior {\n"
      << "    int seen = 0;\n"
      << "    void OnPhase(const string &in payload) { seen += 1; }\n"
      << "    void OnStart() {\n"
      << "        Events_Listen(\"probe_evt\", EventCallback(this.OnPhase));\n"
      << "        for (int i = 0; i < 5; i++) {\n"
      << "            EventData@ d = EventData();\n"
      << "            d.SetInt(\"n\", i);\n"
      << "            Events_Send(\"probe_evt\", d);\n"
      << "            Events_Broadcast(d);\n"
      << "        }\n"
      << "    }\n"
      << "}\n";
}

struct Fixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    ScriptEventBus bus;
    fs::path dir;
    ECS::Entity entity = ECS::INVALID_ENTITY;

    explicit Fixture(const char* leaf) {
        dir = MakeDir(leaf);
        WriteProbe(dir / "Probe.as");

        engine.Initialize();
        RegisterAllBindings(engine.GetASEngine());
        SetBindingsEventBus(&bus);
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
        system.Update(1.0f / 60.0f);   // OnStart runs here, not on the init frame
    }

    ~Fixture() {
        system.SetWorld(nullptr);
        SetBindingsEventBus(nullptr);
        engine.Shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

}  // namespace

// The delegate. Its leak was visible -- one GC complaint per play cycle -- and
// the collector's own accounting is therefore the right thing to assert.
ENJIN_TEST(ScriptHandleOwnership, SubscribingWithADelegateLeavesNothingForTheCollector) {
    Fixture fx("delegate");

    // Tear the scripts down the way every runtime does at Stop.
    fx.system.ShutdownAllScripts();

    asIScriptEngine* as = fx.engine.GetASEngine();
    ENJIN_ASSERT_NOT_NULL(as);
    as->GarbageCollect(asGC_FULL_CYCLE);

    asUINT live = 0;
    as->GetGCStatistics(&live, nullptr, nullptr, nullptr, nullptr);
    std::printf("    garbage-collected objects still alive after teardown: %u\n", live);

    // Before the fix this was 1: the delegate, at refcount 1, referenced only by
    // the native handle parameter nobody released.
    ENJIN_EXPECT_EQ(live, asUINT(0));
}

// The EventData. Its leak was silent, so the test has to supply the evidence
// the engine does not.
ENJIN_TEST(ScriptHandleOwnership, FiringEventsLeavesNoEventDataAlive) {
    const i32 before = LiveScriptEventDataCount();

    {
        Fixture fx("eventdata");
        // OnStart created five EventData objects and passed each to BOTH
        // Events_Send and Events_Broadcast -- ten owned handles in total.
        fx.system.ShutdownAllScripts();
        fx.engine.GetASEngine()->GarbageCollect(asGC_FULL_CYCLE);
    }

    const i32 after = LiveScriptEventDataCount();
    std::printf("    live EventData objects: %d before, %d after\n", before, after);

    // Before the fix this was +5: the script's own handle went out of scope, but
    // the two native calls each kept one, and five iterations each leaked one of
    // the two extra references they took.
    ENJIN_EXPECT_EQ(after, before);
}

ENJIN_TEST_MAIN()
