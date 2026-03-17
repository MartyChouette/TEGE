#include "EnjinTest.h"
#include "Enjin/Scene/LevelStreaming.h"

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

ENJIN_TEST_MAIN()
