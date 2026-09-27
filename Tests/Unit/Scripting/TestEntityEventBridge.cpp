// Engine events reach scripts and visual scripts.
//
// Components send on the ECS EntityEventBus (ActionTrigger's Emit Event,
// dialogue, water_enter). Scripts subscribe on the ScriptEventBus. Nothing
// joined the two, and the ECS bus had no listeners anywhere in shipped code, so
// every engine event was sent to nobody. These tests fire an event on the ECS
// bus and check a real script heard it with its payload.

#include "EnjinTest.h"
#include "Enjin/Scripting/ScriptEngine.h"
#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ScriptEvents.h"
#include "Enjin/Scripting/ScriptSystem.h"
#include "Enjin/Scripting/CoroutineScheduler.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/EntityEventBus.h"
#include "Enjin/ECS/EntityEventBridge.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Transform.h"

#include <cmath>
#include <filesystem>
#include <fstream>

using namespace Enjin;
using namespace Enjin::Scripting;

namespace {

namespace fs = std::filesystem;

// On "door_open" the script writes what it read into its own position, which
// the test can see: x = the "n" int, y = 1 if "sender" matched, z = hit count.
void WriteListener(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    f << "class Listener : TegeBehavior {\n"
      << "    float hits = 0;\n"
      << "    void OnDoor(const string &in name) {\n"
      << "        hits += 1;\n"
      << "        float sent = (Events_CurrentEntity(\"sender\") == 77) ? 1.0f : 0.0f;\n"
      << "        Entity_SetPosition(_entityId, Vector3(float(Events_CurrentInt(\"n\")), sent, hits));\n"
      << "    }\n"
      << "    void OnStart() { Events_Listen(\"door_open\", EventCallback(this.OnDoor)); }\n"
      << "}\n";
}

struct Fixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    ScriptEventBus scriptBus;
    ECS::EntityEventBus entityBus;
    fs::path dir;
    ECS::Entity entity = ECS::INVALID_ENTITY;

    Fixture() {
        dir = fs::temp_directory_path() / "enjin_entity_event_bridge";
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
        WriteListener(dir / "Listener.as");

        engine.Initialize();
        RegisterAllBindings(engine.GetASEngine());
        scriptBus.SetScriptEngine(&engine);
        SetBindingsEventBus(&scriptBus);
        SetBindingsWorld(&world);
        engine.SetScriptDirectory(dir.string());
        engine.CompileScript((dir / "Listener.as").string());

        system.SetScriptEngine(&engine);
        system.SetCoroutineScheduler(&scheduler);
        system.SetWorld(&world);
        system.SetScriptRoot(dir.string());
        system.SetEnabled(true);

        entity = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(entity);
        ECS::ScriptComponent sc;
        ECS::ScriptAttachment att;
        att.scriptPath = "Listener.as";
        att.className = "Listener";
        att.enabled = true;
        sc.scripts.push_back(att);
        world.AddComponent<ECS::ScriptComponent>(entity, sc);

        system.InitializeAllScripts();
        system.Update(1.0f / 60.0f);   // OnStart subscribes here
    }

    ~Fixture() {
        system.ShutdownAllScripts();
        system.SetWorld(nullptr);
        SetBindingsEventBus(nullptr);
        SetBindingsWorld(nullptr);
        engine.Shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    Math::Vector3 Seen() const { return world.GetComponent<ECS::TransformComponent>(entity)->position; }

    void Fire() {
        ECS::EntityEvent ev;
        ev.name = "door_open";
        ev.sender = static_cast<ECS::Entity>(77);
        ev.ints["n"] = 5;
        entityBus.Send("door_open", ev);
    }
};

bool Near(f32 a, f32 b) { return std::abs(a - b) < 1e-4f; }

}  // namespace

ENJIN_TEST(EntityEventBridge, AScriptHearsAnEngineEventWithItsPayload) {
    Fixture fx;
    ECS::ForwardEntityEventsToScripts(fx.entityBus, &fx.scriptBus, nullptr);
    fx.Fire();
    const Math::Vector3 seen = fx.Seen();
    ENJIN_EXPECT_TRUE(Near(seen.x, 5.0f));   // the int payload
    ENJIN_EXPECT_TRUE(Near(seen.y, 1.0f));   // "sender" as an entity
    ENJIN_EXPECT_TRUE(Near(seen.z, 1.0f));   // exactly once
}

ENJIN_TEST(EntityEventBridge, DeferredEventsArriveWhenTheBusIsFlushed) {
    Fixture fx;
    ECS::ForwardEntityEventsToScripts(fx.entityBus, &fx.scriptBus, nullptr);
    ECS::EntityEvent ev;
    ev.ints["n"] = 3;
    fx.entityBus.SendDeferred("door_open", ev);
    ENJIN_EXPECT_TRUE(Near(fx.Seen().z, 0.0f));
    fx.entityBus.ProcessDeferred();
    ENJIN_EXPECT_TRUE(Near(fx.Seen().x, 3.0f));
    ENJIN_EXPECT_TRUE(Near(fx.Seen().y, 0.0f));   // no sender given
}

ENJIN_TEST(EntityEventBridge, TheForwarderSurvivesClear) {
    // Editor Stop clears the bus; the next Play must still reach scripts
    Fixture fx;
    ECS::ForwardEntityEventsToScripts(fx.entityBus, &fx.scriptBus, nullptr);
    fx.entityBus.Clear();
    fx.Fire();
    ENJIN_EXPECT_TRUE(Near(fx.Seen().z, 1.0f));
}

ENJIN_TEST(EntityEventBridge, WithoutTheBridgeNothingHears) {
    // The state every runtime was in: an event on the ECS bus reaches no script
    Fixture fx;
    fx.Fire();
    ENJIN_EXPECT_TRUE(Near(fx.Seen().z, 0.0f));
}

ENJIN_TEST(EntityEventBridge, PayloadKeepsASenderKeyTheEventSetItself) {
    ECS::EntityEvent ev;
    ev.sender = static_cast<ECS::Entity>(9);
    ev.target = static_cast<ECS::Entity>(10);
    ev.entities["sender"] = static_cast<ECS::Entity>(42);
    ev.strings["event"] = "x";
    const EventData d = ECS::ToScriptEventData(ev);
    ENJIN_EXPECT_EQ(d.GetEntity("sender"), u64(42));
    ENJIN_EXPECT_EQ(d.GetEntity("target"), u64(10));
    ENJIN_EXPECT_EQ(d.GetString("event"), std::string("x"));
}

ENJIN_TEST_MAIN()
