// FindEntityByName must see entities that were created after the cache was built.
//
// The name cache was invalidated by hand, and it was maintained at 3 of the 210
// sites that add a NameComponent. So once anything had looked an entity up, the
// cache was built, and everything spawned afterwards -- a prefab instance, a
// script spawn, a visual-script Spawn node -- was invisible to
// FindEntityByName. A rename left a stale entry pointing at the old name.
//
// It read as intermittent because any entity destroyed in the same frame set
// the dirty flag and masked it.
//
// These tests never call InvalidateNameCache. That is the point: the cache now
// derives its own validity from the NameComponent storage, so no caller has to
// remember anything.
#include "EnjinTest.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Transform.h"

#include <string>

using namespace Enjin;

namespace {

ECS::Entity MakeNamed(ECS::World& w, const std::string& name) {
    const ECS::Entity e = w.CreateEntity();
    w.AddComponent<ECS::NameComponent>(e, ECS::NameComponent(name));
    return e;
}

} // namespace

ENJIN_TEST(EntityNameCache, AnEntityAddedAfterTheCacheIsBuiltIsFound) {
    // Arrange: build the cache by looking something up first. This is the step
    // that armed the bug -- before it, every lookup rebuilt and appeared to work.
    ECS::World w;
    const ECS::Entity first = MakeNamed(w, "First");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("First") == first);

    // Act: spawn after the cache exists, the way a prefab or script spawn does.
    const ECS::Entity late = MakeNamed(w, "SpawnedLater");

    // Assert
    ENJIN_EXPECT_TRUE(w.FindEntityByName("SpawnedLater") == late);
}

ENJIN_TEST(EntityNameCache, ManySpawnsAfterTheCacheIsBuiltAreAllFound) {
    // Arrange
    ECS::World w;
    MakeNamed(w, "Seed");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("Seed") != ECS::INVALID_ENTITY);

    // Act
    ECS::Entity spawned[16];
    for (int i = 0; i < 16; ++i) {
        spawned[i] = MakeNamed(w, "Spawn" + std::to_string(i));
    }

    // Assert
    for (int i = 0; i < 16; ++i) {
        ENJIN_EXPECT_TRUE(w.FindEntityByName("Spawn" + std::to_string(i)) == spawned[i]);
    }
}

ENJIN_TEST(EntityNameCache, RenamingMovesTheEntityToTheNewName) {
    // Arrange
    ECS::World w;
    const ECS::Entity e = MakeNamed(w, "OldName");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("OldName") == e);

    // Act
    w.SetEntityName(e, "NewName");

    // Assert: both halves. A stale entry under the old name is just as wrong as
    // a miss under the new one.
    ENJIN_EXPECT_TRUE(w.FindEntityByName("NewName") == e);
    ENJIN_EXPECT_TRUE(w.FindEntityByName("OldName") == ECS::INVALID_ENTITY);
}

ENJIN_TEST(EntityNameCache, SetEntityNameAddsTheComponentWhenThereIsNone) {
    // Arrange: an entity with no NameComponent at all.
    ECS::World w;
    const ECS::Entity e = w.CreateEntity();
    MakeNamed(w, "Seed");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("Seed") != ECS::INVALID_ENTITY);

    // Act
    w.SetEntityName(e, "GivenAName");

    // Assert
    ENJIN_ASSERT_TRUE(w.HasComponent<ECS::NameComponent>(e));
    ENJIN_EXPECT_TRUE(w.FindEntityByName("GivenAName") == e);
}

ENJIN_TEST(EntityNameCache, ADestroyedEntityIsNoLongerFound) {
    // Arrange
    ECS::World w;
    const ECS::Entity e = MakeNamed(w, "Doomed");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("Doomed") == e);

    // Act: DestroyEntity is deferred, flushed at the start of Update.
    w.DestroyEntity(e);
    w.Update(0.016f);

    // Assert
    ENJIN_EXPECT_TRUE(w.FindEntityByName("Doomed") == ECS::INVALID_ENTITY);
}

ENJIN_TEST(EntityNameCache, RemovingTheNameComponentRemovesTheEntry) {
    // Arrange
    ECS::World w;
    const ECS::Entity e = MakeNamed(w, "Temporary");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("Temporary") == e);

    // Act
    w.RemoveComponent<ECS::NameComponent>(e);

    // Assert
    ENJIN_EXPECT_TRUE(w.FindEntityByName("Temporary") == ECS::INVALID_ENTITY);
}

ENJIN_TEST(EntityNameCache, ClearEmptiesTheCache) {
    // Arrange
    ECS::World w;
    MakeNamed(w, "BeforeClear");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("BeforeClear") != ECS::INVALID_ENTITY);

    // Act
    w.Clear();

    // Assert
    ENJIN_EXPECT_TRUE(w.FindEntityByName("BeforeClear") == ECS::INVALID_ENTITY);
}

ENJIN_TEST(EntityNameCache, ADestroyDoesNotMaskAMissingSpawn) {
    // Arrange: the combination that made the original bug look intermittent.
    // A destroy in the same frame used to set the dirty flag and force a
    // rebuild, so the spawn was found -- by accident, and only sometimes.
    ECS::World w;
    const ECS::Entity doomed = MakeNamed(w, "Doomed");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("Doomed") == doomed);

    // Act: spawn WITHOUT any accompanying destroy.
    const ECS::Entity spawned = MakeNamed(w, "Unmasked");

    // Assert
    ENJIN_EXPECT_TRUE(w.FindEntityByName("Unmasked") == spawned);
    ENJIN_EXPECT_TRUE(w.FindEntityByName("Doomed") == doomed);
}

ENJIN_TEST(EntityNameCache, AWrittenThroughRenameStillDoesNotReturnTheWrongEntity) {
    // Arrange: someone writes NameComponent::name directly instead of calling
    // SetEntityName. The cache cannot see that, so the old key still maps to
    // this entity -- but it must not hand back an entity that no longer holds
    // the name the caller asked for.
    ECS::World w;
    const ECS::Entity e = MakeNamed(w, "Original");
    ENJIN_ASSERT_TRUE(w.FindEntityByName("Original") == e);

    // Act
    w.GetComponent<ECS::NameComponent>(e)->name = "WrittenDirectly";

    // Assert
    ENJIN_EXPECT_TRUE(w.FindEntityByName("Original") == ECS::INVALID_ENTITY);
}

ENJIN_TEST(EntityNameCache, LookingUpANameThatWasNeverUsedReturnsInvalid) {
    // Arrange
    ECS::World w;
    MakeNamed(w, "Present");

    // Act / Assert
    ENJIN_EXPECT_TRUE(w.FindEntityByName("Absent") == ECS::INVALID_ENTITY);
}

ENJIN_TEST_MAIN()
