// A script must be able to put a component on an entity it just made.
//
// Scene_InstantiateNamed handed back an entity carrying only a Transform, and
// nothing could be added to it: three dozen HasComponent_* queries existed and
// not one AddComponent_*. So every quad a game draws had to exist in the scene
// file before play -- one project generates 62 sprite entities from a Python
// tool for exactly this reason, and needs a second tool to check the generated
// positions still agree with the script's hit-test constants, because those
// two now live in different files and drift silently.
//
// The bindings themselves need a live AngelScript engine, so what is pinned
// here is the behaviour underneath them: the add-or-keep semantics they rely
// on, and the ECS guarantees that make calling this from a script safe at all.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Text.h"

using namespace Enjin;
using namespace Enjin::ECS;

namespace {

// What AddComponent_* does, with the World calls the binding makes. Kept in
// step with the binding by the tests below rather than by hope: if the binding
// stopped being idempotent, AddingTwiceKeepsTheFirstValues would still pass
// here, so that test also asserts the ECS behaviour the binding leans on.
template <typename T>
bool AddIfAbsent(World& w, Entity e) {
    if (!w.IsValid(e)) return false;
    if (w.HasComponent<T>(e)) return true;
    w.AddComponent<T>(e, T{});
    return true;
}

} // namespace

ENJIN_TEST(AddComponent, AnEntityWithOnlyATransformCanGainOne) {
    // Arrange: exactly what Scene_InstantiateNamed returns.
    World w;
    const Entity e = w.CreateEntity();
    w.AddComponent<TransformComponent>(e, TransformComponent{});
    ENJIN_ASSERT_TRUE(!w.HasComponent<MaterialComponent>(e));

    // Act
    const bool ok = AddIfAbsent<MaterialComponent>(w, e);

    // Assert
    ENJIN_EXPECT_TRUE(ok);
    ENJIN_EXPECT_TRUE(w.HasComponent<MaterialComponent>(e));
    ENJIN_EXPECT_TRUE(w.GetComponent<MaterialComponent>(e) != nullptr);
}

ENJIN_TEST(AddComponent, AddingTwiceKeepsTheFirstValues) {
    // Arrange: these are called from OnUpdate, so a second call must not wipe
    // what the first one set. A plain AddComponent would overwrite.
    World w;
    const Entity e = w.CreateEntity();
    AddIfAbsent<MaterialComponent>(w, e);
    // Every channel must differ from the DEFAULT, or an overwrite would leave
    // the assertion passing: World::AddComponent assigns over an existing
    // component, and the default base colour is white.
    const Math::Vector3 defaultColor = MaterialComponent{}.baseColor;
    w.GetComponent<MaterialComponent>(e)->baseColor = Math::Vector3(0.1f, 0.2f, 0.3f);
    ENJIN_ASSERT_TRUE(defaultColor.x != 0.1f);

    // Act
    const bool ok = AddIfAbsent<MaterialComponent>(w, e);

    // Assert: reports success, and the authored colour survived intact.
    ENJIN_EXPECT_TRUE(ok);
    const Math::Vector3 after = w.GetComponent<MaterialComponent>(e)->baseColor;
    ENJIN_EXPECT_TRUE(after.x == 0.1f);
    ENJIN_EXPECT_TRUE(after.y == 0.2f);
    ENJIN_EXPECT_TRUE(after.z == 0.3f);
}

ENJIN_TEST(AddComponent, AnInvalidEntityIsRefusedRatherThanCreated) {
    // Arrange: a script holds a stale handle after a destroy. Adding to it
    // must fail, not resurrect a slot.
    World w;
    const Entity e = w.CreateEntity();
    w.DestroyEntity(e);
    w.Update(0.016f);                   // destroys are deferred to here

    // Act / Assert
    ENJIN_EXPECT_TRUE(!w.IsValid(e));
    ENJIN_EXPECT_TRUE(!AddIfAbsent<MaterialComponent>(w, e));
}

ENJIN_TEST(AddComponent, ANewComponentTypeDoesNotMoveExistingStorages) {
    // Arrange: this is the guarantee that makes adding from a script safe at
    // all. Systems cache raw ComponentStorage pointers; if creating a storage
    // for a type nobody had yet could rehash the map and move the others, a
    // cached pointer would dangle the moment a script added something new.
    World w;
    const Entity a = w.CreateEntity();
    w.AddComponent<TransformComponent>(a, TransformComponent{});
    ComponentStorage<TransformComponent>* before = w.GetComponentStorage<TransformComponent>();
    ENJIN_ASSERT_TRUE(before != nullptr);

    // Act: introduce several types the world has never seen.
    const Entity b = w.CreateEntity();
    AddIfAbsent<MaterialComponent>(w, b);
    AddIfAbsent<TextComponent>(w, b);

    // Assert: same storage object, still holding the same component.
    ENJIN_EXPECT_TRUE(w.GetComponentStorage<TransformComponent>() == before);
    ENJIN_EXPECT_TRUE(w.GetComponent<TransformComponent>(a) != nullptr);
}

ENJIN_TEST(AddComponent, AddedComponentsCarryTheirAuthoredDefaults) {
    // Arrange: the binding adds a default-constructed component, so a script
    // that adds one and sets nothing must get the same thing the inspector
    // would have given it -- not a zeroed struct.
    World w;
    const Entity e = w.CreateEntity();

    // Act
    AddIfAbsent<TextComponent>(w, e);

    // Assert: TextComponent's own defaults, not zero.
    const TextComponent* tc = w.GetComponent<TextComponent>(e);
    ENJIN_ASSERT_TRUE(tc != nullptr);
    ENJIN_EXPECT_TRUE(tc->sdfText);              // the SDF path, as authored
    ENJIN_EXPECT_TRUE(tc->fontSize > 0.0f);
    ENJIN_EXPECT_TRUE(tc->dirty);                // so it bakes on the next frame
}

ENJIN_TEST(AddComponent, TheEntityKeepsWhatItAlreadyHad) {
    // Arrange: adding one component must not disturb another.
    World w;
    const Entity e = w.CreateEntity();
    TransformComponent xf;
    xf.position = Math::Vector3(3.0f, -2.0f, 1.0f);
    w.AddComponent<TransformComponent>(e, xf);

    // Act
    AddIfAbsent<MaterialComponent>(w, e);
    AddIfAbsent<TextComponent>(w, e);

    // Assert
    const TransformComponent* got = w.GetComponent<TransformComponent>(e);
    ENJIN_ASSERT_TRUE(got != nullptr);
    ENJIN_EXPECT_TRUE(got->position.x == 3.0f);
    ENJIN_EXPECT_TRUE(got->position.y == -2.0f);
}

ENJIN_TEST_MAIN()
