#include "EnjinTest.h"
#include "Enjin/Gameplay/ObjectPool.h"
#include "Enjin/ECS/World.h"

using namespace Enjin;
using namespace Enjin::Gameplay;

// ===========================================================================
// Pool Creation & Acquire/Release
// ===========================================================================

ENJIN_TEST(Pool, CreateAndAcquire) {
    ECS::World world;
    ObjectPool pool;
    pool.CreatePool("bullets", &world, 5, [](ECS::Entity) {});

    ECS::Entity e = pool.Acquire("bullets");
    ENJIN_EXPECT_TRUE(world.IsValid(e));
}

ENJIN_TEST(Pool, AcquireMultiple) {
    ECS::World world;
    ObjectPool pool;
    pool.CreatePool("bullets", &world, 10, [](ECS::Entity) {});

    ECS::Entity e1 = pool.Acquire("bullets");
    ECS::Entity e2 = pool.Acquire("bullets");
    ECS::Entity e3 = pool.Acquire("bullets");
    ENJIN_EXPECT_TRUE(e1 != e2);
    ENJIN_EXPECT_TRUE(e2 != e3);
}

ENJIN_TEST(Pool, ReleaseAndReacquire) {
    ECS::World world;
    ObjectPool pool;
    pool.CreatePool("bullets", &world, 3, [](ECS::Entity) {});

    ECS::Entity e1 = pool.Acquire("bullets");
    pool.Release("bullets", e1);
    ECS::Entity e2 = pool.Acquire("bullets");
    // After release, the entity should be available again
    ENJIN_EXPECT_TRUE(world.IsValid(e2));
}

ENJIN_TEST(Pool, DestroyPool) {
    ECS::World world;
    ObjectPool pool;
    pool.CreatePool("fx", &world, 5, [](ECS::Entity) {});
    pool.Acquire("fx");
    pool.DestroyPool("fx", &world);
    // Pool destroyed — acquiring should return invalid entity
    ECS::Entity e = pool.Acquire("fx");
    ENJIN_EXPECT_EQ(e, (ECS::Entity)0);
}

ENJIN_TEST(Pool, DestroyAll) {
    ECS::World world;
    ObjectPool pool;
    pool.CreatePool("a", &world, 3, [](ECS::Entity) {});
    pool.CreatePool("b", &world, 3, [](ECS::Entity) {});
    pool.DestroyAll(&world);
    ENJIN_EXPECT_EQ(pool.Acquire("a"), (ECS::Entity)0);
    ENJIN_EXPECT_EQ(pool.Acquire("b"), (ECS::Entity)0);
}

ENJIN_TEST(Pool, AcquireFromNonexistentPool) {
    ObjectPool pool;
    ECS::Entity e = pool.Acquire("nope");
    ENJIN_EXPECT_EQ(e, (ECS::Entity)0);
}

ENJIN_TEST_MAIN()
