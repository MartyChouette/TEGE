// AngelScript scripts never received a collision (SD-1, 2026-09-26).
//
// ScriptSystem::OnCollisionEnter/Exit and OnTriggerEnter/Exit were implemented
// and had no caller anywhere: GameplayLoop::DispatchCollisionEvents3D and
// Wire2DCollisionCallbacks delivered physics events to VisualScriptSystem only.
// A TegeBehavior overriding OnCollisionEnter(uint64 other) -- a hook the
// embedded TegeBehavior.as documents -- was silent in every runtime.
//
// Both entry points now take the ScriptSystem. These tests drive them with fake
// backends holding canned events, so no physics engine has to produce a contact.
#include "EnjinTest.h"
#include "Enjin/Gameplay/GameplayLoop.h"
#include "Enjin/Physics/IPhysicsBackend.h"
#include "Enjin/Physics/IPhysicsBackend2D.h"
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

// Every pure virtual stubbed; only the event queue does anything.
class FakeBackend3D : public Physics::IPhysicsBackend {
public:
    std::vector<Physics::CollisionEvent> events;

    void SetWorld(ECS::World*) override {}
    void Update(f32) override {}
    void SetGravity(const Math::Vector3&) override {}
    Math::Vector3 GetGravity() const override { return {}; }
    bool CheckAABBCollision(const Physics::AABB&, const Physics::AABB&, Physics::CollisionResult&) override { return false; }
    bool CheckSphereCollision(const Math::Vector3&, f32, const Math::Vector3&, f32, Physics::CollisionResult&) override { return false; }
    Physics::RaycastHit Raycast(const Physics::Ray&, f32, u32) override { return {}; }
    std::vector<Physics::RaycastHit> RaycastAll(const Physics::Ray&, f32, u32) override { return {}; }
    bool CheckGround(const Math::Vector3&, f32, Physics::RaycastHit&, u32, ECS::Entity) override { return false; }
    Math::Vector3 MoveAndSlide(const Math::Vector3& p, const Math::Vector3&, const Physics::AABB&, f32, u32) override { return p; }
    std::vector<ECS::Entity> GetCollidersInRadius(const Math::Vector3&, f32, u32) override { return {}; }
    std::vector<ECS::Entity> OverlapBox(const Math::Vector3&, const Math::Vector3&, u32) override { return {}; }
    const std::vector<Physics::CollisionEvent>& GetPendingCollisionEvents() const override { return events; }
    void ClearPendingCollisionEvents() override { events.clear(); }
    Physics::ConstraintSolver* GetConstraintSolver() override { return nullptr; }
    const char* GetName() const override { return "Fake3D"; }
};

// Keeps the callbacks GameplayLoop installs so the test can fire them.
class FakeBackend2D : public Physics::IPhysicsBackend2D {
public:
    CollisionCallback onEnter, onExit, onSensorEnter, onSensorExit;

    void Initialize(ECS::World*) override {}
    void Update(f32) override {}
    void Shutdown() override {}
    void SetGravity(const Math::Vector2&) override {}
    Math::Vector2 GetGravity() const override { return {}; }
    void SetVelocityIterations(u32) override {}
    void SetPositionIterations(u32) override {}
    bool Raycast(const Math::Vector2&, const Math::Vector2&, f32, Physics::RayHit2D&, u32) const override { return false; }
    std::vector<Physics::RayHit2D> RaycastAll(const Math::Vector2&, const Math::Vector2&, f32, u32) const override { return {}; }
    bool OverlapCircle(const Math::Vector2&, f32, std::vector<ECS::Entity>&, u32) const override { return false; }
    bool OverlapBox(const Math::Vector2&, const Math::Vector2&, std::vector<ECS::Entity>&, u32) const override { return false; }
    void SetOnCollisionEnter(CollisionCallback cb) override { onEnter = std::move(cb); }
    void SetOnCollisionExit(CollisionCallback cb) override { onExit = std::move(cb); }
    void SetOnSensorEnter(CollisionCallback cb) override { onSensorEnter = std::move(cb); }
    void SetOnSensorExit(CollisionCallback cb) override { onSensorExit = std::move(cb); }
    void SetCCDEnabled(bool) override {}
    const char* GetName() const override { return "Fake2D"; }
};

// Counts each hook and remembers the last `other` it was handed.
void WriteProbe(const fs::path& p) {
    std::ofstream f(p, std::ios::trunc);
    f << "class Probe : TegeBehavior {\n"
      << "    int collisionEnter = 0;\n"
      << "    int collisionExit = 0;\n"
      << "    int triggerEnter = 0;\n"
      << "    int triggerExit = 0;\n"
      << "    uint64 lastOther = 0;\n"
      << "    void OnCollisionEnter(uint64 other) { collisionEnter += 1; lastOther = other; }\n"
      << "    void OnCollisionExit(uint64 other)  { collisionExit += 1;  lastOther = other; }\n"
      << "    void OnTriggerEnter(uint64 other)   { triggerEnter += 1;   lastOther = other; }\n"
      << "    void OnTriggerExit(uint64 other)    { triggerExit += 1;    lastOther = other; }\n"
      << "}\n";
}

struct Fixture {
    ECS::World world;
    ScriptEngine engine;
    CoroutineScheduler scheduler;
    ScriptSystem system;
    fs::path dir;
    ECS::Entity a = ECS::INVALID_ENTITY;
    ECS::Entity b = ECS::INVALID_ENTITY;
    std::vector<ECS::Entity> deferred;

    explicit Fixture(const char* leaf) {
        dir = fs::temp_directory_path() / "enjin_collision_dispatch_test" / leaf;
        std::error_code ec;
        fs::remove_all(dir, ec);
        fs::create_directories(dir, ec);
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

        a = MakeProbeEntity();
        b = MakeProbeEntity();
        system.InitializeAllScripts();
        system.Update(1.0f / 60.0f);   // OnStart, so both are fully live
    }

    ~Fixture() {
        system.SetWorld(nullptr);
        engine.Shutdown();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    ECS::Entity MakeProbeEntity() {
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e);
        ECS::ScriptComponent sc;
        ECS::ScriptAttachment att;
        att.scriptPath = "Probe.as";
        att.className = "Probe";
        att.enabled = true;
        sc.scripts.push_back(att);
        world.AddComponent<ECS::ScriptComponent>(e, sc);
        return e;
    }

    template <typename T>
    T Read(ECS::Entity e, const char* member) {
        auto* sc = world.GetComponent<ECS::ScriptComponent>(e);
        if (!sc || sc->scripts.empty() || !sc->scripts[0].instance) return T(-1);
        auto* obj = static_cast<asIScriptObject*>(sc->scripts[0].instance);
        for (asUINT i = 0; i < obj->GetPropertyCount(); ++i) {
            const char* name = obj->GetPropertyName(i);
            if (name && std::string(name) == member) {
                return *reinterpret_cast<T*>(obj->GetAddressOfProperty(i));
            }
        }
        return T(-1);
    }
    int Count(ECS::Entity e, const char* member) { return Read<int>(e, member); }
    u64 LastOther(ECS::Entity e) { return Read<u64>(e, "lastOther"); }

    Physics::CollisionEvent Event(Physics::CollisionEvent::Type type, bool trigger) const {
        Physics::CollisionEvent evt;
        evt.entityA = a;
        evt.entityB = b;
        evt.type = type;
        evt.isTrigger = trigger;
        return evt;
    }
};

using Type = Physics::CollisionEvent::Type;

} // namespace

// The regression: both counts were 0 because nothing called ScriptSystem.
ENJIN_TEST(ScriptCollisionDispatch, Collision3DReachesBothScripts) {
    Fixture fx("c3d_enter");
    FakeBackend3D physics;
    physics.events.push_back(fx.Event(Type::Enter, false));

    Gameplay::GameplayLoop::DispatchCollisionEvents3D(
        &fx.world, &physics, nullptr, 0.0f, fx.deferred, &fx.system);

    ENJIN_EXPECT_EQ(fx.Count(fx.a, "collisionEnter"), 1);
    ENJIN_EXPECT_EQ(fx.Count(fx.b, "collisionEnter"), 1);
    // Each side is told about the OTHER entity, not itself.
    ENJIN_EXPECT_EQ(fx.LastOther(fx.a), static_cast<u64>(fx.b));
    ENJIN_EXPECT_EQ(fx.LastOther(fx.b), static_cast<u64>(fx.a));
    ENJIN_EXPECT_TRUE(physics.events.empty());
}

ENJIN_TEST(ScriptCollisionDispatch, Collision3DExitAndTriggers) {
    Fixture fx("c3d_rest");
    FakeBackend3D physics;
    physics.events.push_back(fx.Event(Type::Exit, false));
    physics.events.push_back(fx.Event(Type::Enter, true));
    physics.events.push_back(fx.Event(Type::Exit, true));

    Gameplay::GameplayLoop::DispatchCollisionEvents3D(
        &fx.world, &physics, nullptr, 0.0f, fx.deferred, &fx.system);

    for (ECS::Entity e : {fx.a, fx.b}) {
        ENJIN_EXPECT_EQ(fx.Count(e, "collisionEnter"), 0);
        ENJIN_EXPECT_EQ(fx.Count(e, "collisionExit"), 1);
        ENJIN_EXPECT_EQ(fx.Count(e, "triggerEnter"), 1);
        ENJIN_EXPECT_EQ(fx.Count(e, "triggerExit"), 1);
    }
}

// A caller with no scripting passes nullptr and nothing is delivered.
ENJIN_TEST(ScriptCollisionDispatch, NullScriptSystemIsSkipped) {
    Fixture fx("c3d_null");
    FakeBackend3D physics;
    physics.events.push_back(fx.Event(Type::Enter, false));

    Gameplay::GameplayLoop::DispatchCollisionEvents3D(
        &fx.world, &physics, nullptr, 0.0f, fx.deferred);

    ENJIN_EXPECT_EQ(fx.Count(fx.a, "collisionEnter"), 0);
    ENJIN_EXPECT_TRUE(physics.events.empty());
}

ENJIN_TEST(ScriptCollisionDispatch, Collision2DCallbacksReachScripts) {
    Fixture fx("c2d");
    FakeBackend2D physics2D;
    Gameplay::GameplayLoop::Wire2DCollisionCallbacks(
        &physics2D, &fx.world, nullptr, fx.deferred, &fx.system);

    Physics::Contact2D c{};
    c.entityA = fx.a;
    c.entityB = fx.b;
    ENJIN_ASSERT_TRUE(physics2D.onEnter && physics2D.onExit
                      && physics2D.onSensorEnter && physics2D.onSensorExit);
    physics2D.onEnter(c);
    physics2D.onExit(c);
    physics2D.onSensorEnter(c);
    physics2D.onSensorExit(c);

    for (ECS::Entity e : {fx.a, fx.b}) {
        ENJIN_EXPECT_EQ(fx.Count(e, "collisionEnter"), 1);
        ENJIN_EXPECT_EQ(fx.Count(e, "collisionExit"), 1);
        ENJIN_EXPECT_EQ(fx.Count(e, "triggerEnter"), 1);
        ENJIN_EXPECT_EQ(fx.Count(e, "triggerExit"), 1);
    }
    ENJIN_EXPECT_EQ(fx.LastOther(fx.a), static_cast<u64>(fx.b));
}

ENJIN_TEST_MAIN()
