#include "EnjinTest.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/ECS/Components/Transform.h"

using namespace Enjin;
using namespace Enjin::Scene;
using namespace Enjin::Math;

// ===========================================================================
// StreamingChunk Defaults
// ===========================================================================

ENJIN_TEST(ChunkDefaults, State) {
    StreamingChunk chunk;
    ENJIN_EXPECT_EQ((int)chunk.state, (int)ChunkState::Unloaded);
}

ENJIN_TEST(ChunkDefaults, Distances) {
    StreamingChunk chunk;
    ENJIN_EXPECT_FLOAT_EQ(chunk.loadDistance, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(chunk.unloadDistance, 150.0f);
}

ENJIN_TEST(ChunkDefaults, Priority) {
    StreamingChunk chunk;
    ENJIN_EXPECT_EQ((int)chunk.priority, (int)StreamPriority::Normal);
}

ENJIN_TEST(ChunkDefaults, LOD) {
    StreamingChunk chunk;
    ENJIN_EXPECT_EQ(chunk.lodLevel, 0u);
}

ENJIN_TEST(ChunkDefaults, EmptyEntities) {
    StreamingChunk chunk;
    ENJIN_EXPECT_EQ(chunk.entities.size(), (size_t)0);
}

// ===========================================================================
// StreamingVolumeComponent
// ===========================================================================

ENJIN_TEST(StreamingVolume, Defaults) {
    StreamingVolumeComponent vol;
    ENJIN_EXPECT_FLOAT_EQ(vol.halfExtents.x, 50.0f);
    ENJIN_EXPECT_FLOAT_EQ(vol.loadDistance, 100.0f);
    ENJIN_EXPECT_FLOAT_EQ(vol.unloadDistance, 150.0f);
    ENJIN_EXPECT_EQ((int)vol.priority, (int)StreamPriority::Normal);
}

// ===========================================================================
// StreamingPortalComponent
// ===========================================================================

ENJIN_TEST(StreamingPortal, Defaults) {
    StreamingPortalComponent portal;
    ENJIN_EXPECT_TRUE(portal.bidirectional);
    ENJIN_EXPECT_FLOAT_EQ(portal.halfExtents.x, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(portal.halfExtents.y, 3.0f);
}

// ===========================================================================
// StreamingManager
// ===========================================================================

ENJIN_TEST(StreamingManager, InitialState) {
    StreamingManager sm;
    ENJIN_EXPECT_EQ(sm.GetChunks().size(), (size_t)0);
    ENJIN_EXPECT_EQ(sm.GetLoadedChunkCount(), 0u);
    ENJIN_EXPECT_EQ(sm.GetLoadingChunkCount(), 0u);
}

ENJIN_TEST(StreamingManager, AddChunk) {
    StreamingManager sm;
    StreamingChunk chunk;
    chunk.chunkId = "chunk_01";
    chunk.center = Vector3(0.0f, 0.0f, 0.0f);
    chunk.halfExtents = Vector3(50.0f, 50.0f, 50.0f);
    sm.AddChunk(chunk);
    ENJIN_EXPECT_EQ(sm.GetChunks().size(), (size_t)1);
}

ENJIN_TEST(StreamingManager, RemoveChunk) {
    StreamingManager sm;
    StreamingChunk chunk;
    chunk.chunkId = "chunk_01";
    sm.AddChunk(chunk);
    sm.RemoveChunk("chunk_01");
    ENJIN_EXPECT_EQ(sm.GetChunks().size(), (size_t)0);
}

ENJIN_TEST(StreamingManager, ClearChunks) {
    StreamingManager sm;
    StreamingChunk c1, c2;
    c1.chunkId = "a";
    c2.chunkId = "b";
    sm.AddChunk(c1);
    sm.AddChunk(c2);
    sm.ClearChunks();
    ENJIN_EXPECT_EQ(sm.GetChunks().size(), (size_t)0);
}

ENJIN_TEST(StreamingManager, GetChunkState) {
    StreamingManager sm;
    StreamingChunk chunk;
    chunk.chunkId = "test";
    sm.AddChunk(chunk);
    ENJIN_EXPECT_EQ((int)sm.GetChunkState("test"), (int)ChunkState::Unloaded);
}

ENJIN_TEST(StreamingManager, NonexistentChunkState) {
    StreamingManager sm;
    // Getting state of non-existent chunk
    ChunkState state = sm.GetChunkState("nonexistent");
    ENJIN_EXPECT_EQ((int)state, (int)ChunkState::Unloaded);
}

// ===========================================================================
// ChunkState Enum
// ===========================================================================

ENJIN_TEST(ChunkStateEnum, Values) {
    ENJIN_EXPECT_EQ((int)ChunkState::Unloaded, 0);
    ENJIN_EXPECT_EQ((int)ChunkState::Loading, 1);
    ENJIN_EXPECT_EQ((int)ChunkState::Loaded, 2);
    ENJIN_EXPECT_EQ((int)ChunkState::Unloading, 3);
}

// ===========================================================================
// StreamPriority Enum
// ===========================================================================

ENJIN_TEST(StreamPriorityEnum, Values) {
    ENJIN_EXPECT_EQ((int)StreamPriority::Critical, 0);
    ENJIN_EXPECT_EQ((int)StreamPriority::High, 1);
    ENJIN_EXPECT_EQ((int)StreamPriority::Normal, 2);
    ENJIN_EXPECT_EQ((int)StreamPriority::Low, 3);
}

// ===========================================================================
// Hysteresis Design
// ===========================================================================

ENJIN_TEST(Hysteresis, LoadUnloadGap) {
    StreamingChunk chunk;
    chunk.loadDistance = 100.0f;
    chunk.unloadDistance = 150.0f;
    // There should be a gap between load and unload distances
    ENJIN_EXPECT_TRUE(chunk.unloadDistance > chunk.loadDistance);
    f32 gap = chunk.unloadDistance - chunk.loadDistance;
    ENJIN_EXPECT_FLOAT_EQ(gap, 50.0f);
}

// ===========================================================================
// RegisterChunksFromWorld — the authoring bridge (volumes -> live chunks)
// ===========================================================================

ENJIN_TEST(RegisterChunks, VolumeBecomesChunkAtTransformCenter) {
    // Arrange: one entity with a streaming volume + a transform
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    ECS::Entity e = world.CreateEntity();
    StreamingVolumeComponent vol;
    vol.chunkId = "zone_a";
    vol.scenePath = "chunks/zone_a.enjin";
    vol.halfExtents = Vector3(80.0f, 40.0f, 80.0f);
    vol.loadDistance = 120.0f;
    world.AddComponent<StreamingVolumeComponent>(e, vol);
    ECS::TransformComponent tf;
    tf.position = Vector3(10.0f, 20.0f, 30.0f);
    world.AddComponent<ECS::TransformComponent>(e, tf);

    // Act
    u32 n = sm.RegisterChunksFromWorld();

    // Assert: one chunk, centered on the volume entity's transform
    ENJIN_EXPECT_EQ(n, 1u);
    ENJIN_ASSERT_TRUE(sm.GetChunks().size() == (size_t)1);
    const auto& c = sm.GetChunks()[0];
    ENJIN_EXPECT_TRUE(c.chunkId == "zone_a");
    ENJIN_EXPECT_TRUE(c.scenePath == "chunks/zone_a.enjin");
    ENJIN_EXPECT_FLOAT_EQ(c.center.x, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(c.center.y, 20.0f);
    ENJIN_EXPECT_FLOAT_EQ(c.center.z, 30.0f);
    ENJIN_EXPECT_FLOAT_EQ(c.loadDistance, 120.0f);
    ENJIN_EXPECT_FLOAT_EQ(c.halfExtents.x, 80.0f);
}

ENJIN_TEST(RegisterChunks, EmptyScenePathIsSkipped) {
    // Arrange: a volume with a chunkId but no scenePath
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    ECS::Entity e = world.CreateEntity();
    StreamingVolumeComponent vol;
    vol.chunkId = "no_scene";
    world.AddComponent<StreamingVolumeComponent>(e, vol);

    // Act
    u32 n = sm.RegisterChunksFromWorld();

    // Assert: nothing registered
    ENJIN_EXPECT_EQ(n, 0u);
    ENJIN_EXPECT_EQ(sm.GetChunks().size(), (size_t)0);
}

ENJIN_TEST(RegisterChunks, BaseDirIsPrefixed) {
    // Arrange
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    ECS::Entity e = world.CreateEntity();
    StreamingVolumeComponent vol;
    vol.chunkId = "zone_b";
    vol.scenePath = "zone_b.enjin";
    world.AddComponent<StreamingVolumeComponent>(e, vol);

    // Act
    sm.RegisterChunksFromWorld("scenes/chunks");

    // Assert: baseDir is prefixed onto the relative scene path
    ENJIN_ASSERT_TRUE(sm.GetChunks().size() == (size_t)1);
    ENJIN_EXPECT_TRUE(sm.GetChunks()[0].scenePath == "scenes/chunks/zone_b.enjin");
}

ENJIN_TEST(RegisterChunks, DuplicateChunkIdRegisteredOnce) {
    // Arrange: two volumes sharing one chunkId
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    for (int i = 0; i < 2; ++i) {
        ECS::Entity e = world.CreateEntity();
        StreamingVolumeComponent vol;
        vol.chunkId = "dup";
        vol.scenePath = "dup.enjin";
        world.AddComponent<StreamingVolumeComponent>(e, vol);
    }

    // Act
    u32 n = sm.RegisterChunksFromWorld();

    // Assert: AddChunk dedups by chunkId
    ENJIN_EXPECT_EQ(n, 1u);
    ENJIN_EXPECT_EQ(sm.GetChunks().size(), (size_t)1);
}

ENJIN_TEST(RegisterChunks, NoWorldReturnsZero) {
    // Arrange: manager with no world set
    StreamingManager sm;

    // Act
    u32 n = sm.RegisterChunksFromWorld();

    // Assert
    ENJIN_EXPECT_EQ(n, 0u);
}

ENJIN_TEST_MAIN()
