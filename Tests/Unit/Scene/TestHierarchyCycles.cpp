// A parent cycle must not kill the process, and a scene carrying one must load.
//
// Marty, 2026-09-13: two entities in a scene shared a name, a tool keyed its
// lookup on the name and picked the wrong one, and the file came out with entity
// 4 parented to 5 while 5 was parented to 4. The engine walked that chain until
// the stack ran out. A segfault, no message, nothing naming the two entities.
//
// ComputeWorldMatrix's comment had promised "Depth-capped at 64 to prevent
// infinite loops" for as long as the function existed. There was no cap of any
// kind. That is the exact shape this rebuild is about: a comment describing code
// nobody wrote, and a person losing a day to it.
//
// Two separate defences, because either alone leaves half the hole:
//   - the walk is bounded, so ANY cycle from any source is survivable
//   - the loader breaks a cycle on the way in, so a bad file loads degraded and
//     says which entities disagreed, rather than arriving intact and lethal
//
// SetParent has always refused to create a cycle. It does not help here: the
// scene loader writes ParentComponent directly and never calls it.

#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Scene/SceneSerializer.h"

#include <cstdio>
#include <sstream>
#include <string>

using namespace Enjin;

namespace {

// Parent two entities to each other, the way a bad scene file does it: straight
// into the component, with no guard in the way.
void MakeCycle(ECS::World& world, ECS::Entity a, ECS::Entity b) {
    world.AddComponent<ECS::ParentComponent>(a).parent = b;
    world.AddComponent<ECS::ParentComponent>(b).parent = a;
}

}  // namespace

// The headline: this used to be a stack overflow.
ENJIN_TEST(HierarchyCycles, AParentCycleReturnsInsteadOfExhaustingTheStack) {
    ECS::World world;

    const ECS::Entity a = world.CreateEntity();
    const ECS::Entity b = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(a).position = Math::Vector3(1.0f, 0.0f, 0.0f);
    world.AddComponent<ECS::TransformComponent>(b).position = Math::Vector3(0.0f, 2.0f, 0.0f);
    MakeCycle(world, a, b);

    // Reaching this line at all is the assertion. Before the cap it did not
    // return; the process died inside the recursion.
    const Math::Matrix4 m = ECS::ComputeWorldMatrix(&world, a);
    std::printf("    survived; translation (%.2f, %.2f, %.2f)\n", m.m[12], m.m[13], m.m[14]);
    ENJIN_SURVIVED("a parent cycle, without exhausting the stack");
}

// A chain deeper than the cap is treated the same way, because it is either a
// cycle wearing a disguise or a mistake, and neither should be drawn at a
// confidently wrong place.
ENJIN_TEST(HierarchyCycles, AChainDeeperThanTheCapIsBounded) {
    ECS::World world;

    ECS::Entity previous = ECS::INVALID_ENTITY;
    for (u32 i = 0; i < ECS::kMaxHierarchyDepth + 20; ++i) {
        const ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e).position = Math::Vector3(1.0f, 0.0f, 0.0f);
        if (previous != ECS::INVALID_ENTITY) {
            world.AddComponent<ECS::ParentComponent>(e).parent = previous;
        }
        previous = e;
    }

    const Math::Matrix4 m = ECS::ComputeWorldMatrix(&world, previous);
    std::printf("    deep chain survived; x = %.1f\n", m.m[12]);
    ENJIN_SURVIVED("a chain past the depth cap, without exhausting the stack");
}

// SetParent's guard is correct and was silent. Keep it correct.
ENJIN_TEST(HierarchyCycles, SetParentStillRefusesToMakeOne) {
    ECS::World world;
    const ECS::Entity parent = world.CreateEntity();
    const ECS::Entity child = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(parent);
    world.AddComponent<ECS::TransformComponent>(child);

    ECS::SetParent(&world, child, parent);
    ENJIN_EXPECT_TRUE(ECS::GetParent(&world, child) == parent);

    // The other way round would close the loop, and must not take.
    ECS::SetParent(&world, parent, child);
    ENJIN_EXPECT_TRUE(ECS::GetParent(&world, parent) == ECS::INVALID_ENTITY);
}

// A scene file with a cycle in it. The loader writes ParentComponent directly,
// so this is the path that actually let one in.
ENJIN_TEST(HierarchyCycles, ASceneCarryingACycleLoadsWithTheLoopBroken) {
    ECS::World world;
    Scene::SceneSerializer serializer(&world);

    // Two entities parented to each other, and a third that is fine, so the test
    // can tell "broke the cycle" from "refused the whole file".
    const std::string scene =
        "{\n"
        "  \"version\": \"1.0\",\n"
        "  \"entities\": [\n"
        "    { \"id\": 4, \"name\": { \"name\": \"Car\" },\n"
        "      \"transform\": { \"position\": [0,0,0], \"rotation\": [0,0,0,1], \"scale\": [1,1,1] },\n"
        "      \"parent\": 5 },\n"
        "    { \"id\": 5, \"name\": { \"name\": \"Car\" },\n"
        "      \"transform\": { \"position\": [0,0,0], \"rotation\": [0,0,0,1], \"scale\": [1,1,1] },\n"
        "      \"parent\": 4 },\n"
        "    { \"id\": 6, \"name\": { \"name\": \"Ground\" },\n"
        "      \"transform\": { \"position\": [0,0,0], \"rotation\": [0,0,0,1], \"scale\": [1,1,1] } }\n"
        "  ]\n"
        "}\n";

    const auto result = serializer.LoadFromString(scene);
    std::printf("    loaded=%d entities=%zu\n", (int)result.success, result.entities.size());
    ENJIN_ASSERT_TRUE(result.success);

    // Every entity is present -- a cycle is not a reason to lose the scene.
    ENJIN_EXPECT_EQ(result.entities.size(), usize(3));

    // And walking any of them terminates. This is the line that used to be a
    // segfault on Marty's machine, via the exact same file shape.
    for (ECS::Entity e : result.entities) {
        const Math::Matrix4 m = ECS::ComputeWorldMatrix(&world, e);
        (void)m;
    }

    // At least one of the two has been detached, or the loop is still there.
    u32 stillParented = 0;
    for (ECS::Entity e : result.entities) {
        if (world.HasComponent<ECS::ParentComponent>(e)) ++stillParented;
    }
    std::printf("    entities still carrying a parent: %u\n", stillParented);
    ENJIN_EXPECT_TRUE(stillParented < 2);
}

ENJIN_TEST_MAIN()
