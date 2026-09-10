// A prefab is the default way to reuse work in this editor, and it knew seven
// component types.
//
// `PrefabManager::RegisterBuiltInComponents` registered Transform, Name,
// Material, Light, Mesh, Camera and Notes. The scene serializer's registry knows
// 189. `Instantiate` looked its type name up in a map and skipped anything it did
// not find, with no log at either end, and `SavePrefab`'s return value was
// discarded by its caller. So right-clicking an entity that had a rigidbody, a
// collider, a script or an audio source and choosing "Create Prefab" wrote a file
// that looked fine and instantiated as a bare Transform -- and the loss only
// surfaced later, when someone placed the prefab and wondered why it fell through
// the floor.
//
// The Pixel Editor made it sharper: its "Export as Prefab" wrote the type names
// "Sprite2D", "AnimatedSprite2D" and "BoxCollider", none of which were among the
// seven, so exported pixel art was guaranteed to come back empty.
//
// Prefabs now carry the scene serializer's own per-component JSON and go through
// the same registry, so a component is covered the day it is registered rather
// than the day someone hand-writes a callback pair for it.
#include "EnjinTest.h"
#include "Enjin/Assets/Prefab.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Door.h"
#include "Enjin/ECS/Components/Notes.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/StableId.h"
#include "Enjin/Scene/SceneSerializer.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace Enjin;
using namespace Enjin::ECS;
using namespace Enjin::Math;

namespace {

bool Near(f32 a, f32 b, f32 eps = 0.001f) { return std::fabs(a - b) < eps; }

// An entity of the kind someone actually turns into a prefab: not one of the
// seven privileged types among them except Transform and Name.
Entity AuthoredCrate(World& w) {
    Entity e = w.CreateEntity();
    w.AddComponent<NameComponent>(e, NameComponent{"Crate"});

    TransformComponent t;
    t.position = Vector3(3.0f, 1.5f, -2.0f);
    t.scale = Vector3(2.0f, 2.0f, 2.0f);
    w.AddComponent<TransformComponent>(e, t);

    RigidbodyComponent rb;
    rb.mass = 12.5f;
    rb.useGravity = true;
    w.AddComponent<RigidbodyComponent>(e, rb);

    BoxColliderComponent box;
    box.size = Vector3(2.0f, 2.0f, 2.0f);
    w.AddComponent<BoxColliderComponent>(e, box);

    HealthComponent hp;
    hp.maxHealth = 40.0f;
    hp.currentHealth = 40.0f;
    w.AddComponent<HealthComponent>(e, hp);

    DoorComponent door;
    door.openAngle = 88.0f;
    door.autoCloseDelay = 2.5f;
    w.AddComponent<DoorComponent>(e, door);

    w.AddComponent<NotesComponent>(e, NotesComponent{});
    return e;
}

} // namespace

ENJIN_TEST(PrefabCoverage, EveryComponentSurvivesTheRoundTrip) {
    // Arrange: an entity with four components that were outside the old seven.
    World src;
    Entity e = AuthoredCrate(src);
    const usize authored = Scene::SceneSerializer::ComponentKeysOn(&src, e).size();
    ENJIN_ASSERT_TRUE(authored >= 7);

    // Act: prefab it, then instantiate into a fresh world.
    auto& mgr = Assets::PrefabManager::Get();
    auto prefab = mgr.CreateFromEntity(&src, e, "CratePrefab");
    ENJIN_ASSERT_TRUE(prefab != nullptr);

    World dst;
    Entity made = mgr.Instantiate(&dst, *prefab, Vector3(0.0f), Vector3(0.0f), Vector3(1.0f));
    ENJIN_ASSERT_TRUE(dst.IsValid(made));

    // Assert: the component SET came across. This is the assertion that fails
    // against the old seven-type registry, whatever the field values are.
    ENJIN_EXPECT_TRUE(dst.HasComponent<RigidbodyComponent>(made));
    ENJIN_EXPECT_TRUE(dst.HasComponent<BoxColliderComponent>(made));
    ENJIN_EXPECT_TRUE(dst.HasComponent<HealthComponent>(made));
    ENJIN_EXPECT_TRUE(dst.HasComponent<DoorComponent>(made));
}

ENJIN_TEST(PrefabCoverage, ValuesComeBackNotJustPresence) {
    World src;
    Entity e = AuthoredCrate(src);
    auto& mgr = Assets::PrefabManager::Get();
    auto prefab = mgr.CreateFromEntity(&src, e, "CrateValues");

    World dst;
    Entity made = mgr.Instantiate(&dst, *prefab, Vector3(0.0f), Vector3(0.0f), Vector3(1.0f));
    ENJIN_ASSERT_TRUE(dst.IsValid(made));

    ENJIN_EXPECT_TRUE(Near(dst.GetComponent<RigidbodyComponent>(made)->mass, 12.5f));
    ENJIN_EXPECT_TRUE(dst.GetComponent<RigidbodyComponent>(made)->useGravity);
    ENJIN_EXPECT_TRUE(Near(dst.GetComponent<BoxColliderComponent>(made)->size.y, 2.0f));
    ENJIN_EXPECT_TRUE(Near(dst.GetComponent<HealthComponent>(made)->maxHealth, 40.0f));
    ENJIN_EXPECT_TRUE(Near(dst.GetComponent<DoorComponent>(made)->openAngle, 88.0f));
    ENJIN_EXPECT_EQ(dst.GetComponent<NameComponent>(made)->name, std::string("Crate"));
}

ENJIN_TEST(PrefabCoverage, TheRootOffsetStillApplies) {
    // The one thing Instantiate does on top of a plain restore: the placement
    // offset has to compose with the authored transform, not replace it.
    World src;
    Entity e = AuthoredCrate(src);   // authored at (3, 1.5, -2), scale 2
    auto& mgr = Assets::PrefabManager::Get();
    auto prefab = mgr.CreateFromEntity(&src, e, "CrateOffset");

    World dst;
    Entity made = mgr.Instantiate(&dst, *prefab,
                                  Vector3(10.0f, 0.0f, 0.0f), Vector3(0.0f), Vector3(0.5f, 0.5f, 0.5f));
    ENJIN_ASSERT_TRUE(dst.IsValid(made));

    const auto* t = dst.GetComponent<TransformComponent>(made);
    ENJIN_EXPECT_TRUE(Near(t->position.x, 13.0f));
    ENJIN_EXPECT_TRUE(Near(t->position.y, 1.5f));
    ENJIN_EXPECT_TRUE(Near(t->scale.x, 1.0f));
}

ENJIN_TEST(PrefabCoverage, ChildrenKeepTheirComponentsToo) {
    // A prefab is usually a hierarchy, and the child path is a separate loop.
    World src;
    Entity parent = AuthoredCrate(src);
    Entity child = src.CreateEntity();
    src.AddComponent<NameComponent>(child, NameComponent{"Lid"});
    src.AddComponent<TransformComponent>(child, TransformComponent{});
    HealthComponent lidHp;
    lidHp.maxHealth = 5.0f;
    src.AddComponent<HealthComponent>(child, lidHp);
    SetParent(&src, child, parent);

    auto& mgr = Assets::PrefabManager::Get();
    auto prefab = mgr.CreateFromEntity(&src, parent, "CrateWithLid");

    World dst;
    Entity made = mgr.Instantiate(&dst, *prefab, Vector3(0.0f), Vector3(0.0f), Vector3(1.0f));
    ENJIN_ASSERT_TRUE(dst.IsValid(made));
    ENJIN_ASSERT_TRUE(dst.HasComponent<ChildrenComponent>(made));

    const auto& kids = dst.GetComponent<ChildrenComponent>(made)->children;
    ENJIN_ASSERT_EQ(kids.size(), static_cast<usize>(1));
    ENJIN_EXPECT_TRUE(dst.HasComponent<HealthComponent>(kids[0]));
    ENJIN_EXPECT_TRUE(Near(dst.GetComponent<HealthComponent>(kids[0])->maxHealth, 5.0f));
}

ENJIN_TEST(PrefabCoverage, StableIdIsNotCopiedIntoEveryInstance) {
    // A stable id is a per-entity authoring identity. Carrying it through a
    // prefab would give every instance the same one, which is worse than having
    // none -- the override-layer system keys on it.
    World src;
    Entity e = AuthoredCrate(src);
    src.AddComponent<StableIdComponent>(e, StableIdComponent{ 12345u });

    auto& mgr = Assets::PrefabManager::Get();
    auto prefab = mgr.CreateFromEntity(&src, e, "CrateStable");

    World dst;
    Entity a = mgr.Instantiate(&dst, *prefab, Vector3(0.0f), Vector3(0.0f), Vector3(1.0f));
    Entity b = mgr.Instantiate(&dst, *prefab, Vector3(5.0f, 0.0f, 0.0f), Vector3(0.0f), Vector3(1.0f));
    ENJIN_ASSERT_TRUE(dst.IsValid(a) && dst.IsValid(b));

    const bool aHas = dst.HasComponent<StableIdComponent>(a);
    const bool bHas = dst.HasComponent<StableIdComponent>(b);
    if (aHas && bHas) {
        ENJIN_EXPECT_TRUE(dst.GetComponent<StableIdComponent>(a)->id !=
                          dst.GetComponent<StableIdComponent>(b)->id);
    } else {
        ENJIN_EXPECT_FALSE(aHas);
        ENJIN_EXPECT_FALSE(bHas);
    }
}

ENJIN_TEST(PrefabCoverage, CoverageIsTheRegistryNotAHandWrittenList) {
    // The assertion that scales. Whatever the seven were, the prefab path and the
    // scene path must know the same components -- otherwise the next component
    // added to the engine is silently outside prefabs again.
    World src;
    Entity e = AuthoredCrate(src);
    auto& mgr = Assets::PrefabManager::Get();
    auto prefab = mgr.CreateFromEntity(&src, e, "CrateCoverage");

    World dst;
    Entity made = mgr.Instantiate(&dst, *prefab, Vector3(0.0f), Vector3(0.0f), Vector3(1.0f));
    ENJIN_ASSERT_TRUE(dst.IsValid(made));

    // Everything the source had, the instance has. Compared as joined strings so
    // a failure names the components that went missing rather than a count.
    auto join = [](std::vector<std::string> keys) {
        std::sort(keys.begin(), keys.end());
        std::string out;
        for (const auto& k : keys) {
            if (k == "stableId" || k == "prefabInstance") continue;   // per-instance
            if (!out.empty()) out += ",";
            out += k;
        }
        return out;
    };
    ENJIN_EXPECT_EQ(join(Scene::SceneSerializer::ComponentKeysOn(&dst, made)),
                    join(Scene::SceneSerializer::ComponentKeysOn(&src, e)));
}

ENJIN_TEST_MAIN()
