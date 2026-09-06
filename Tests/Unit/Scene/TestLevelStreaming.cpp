#include "EnjinTest.h"
#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

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

// ===========================================================================
// Chunk load path: SetSceneRoot resolution, staged integration, rejection
// ===========================================================================

namespace {

const char* kChunkSceneJson =
    "{\"version\":\"1.0\",\"entities\":[{\"id\":1,\"name\":{\"name\":\"StreamedProp\"},"
    "\"transform\":{\"position\":[1,2,3],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true}}]}";

std::filesystem::path MakeChunkRoot(const char* tag) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root = fs::temp_directory_path(ec) / (std::string("enjin_streaming_") + tag);
    fs::remove_all(root, ec);
    fs::create_directories(root / "scenes", ec);
    std::ofstream f((root / "scenes" / "chunk_a.enjin").string(), std::ios::binary);
    f << kChunkSceneJson;
    return root;
}

// Drive Update until the chunk leaves Loading (worker read + main-thread staged
// integration both happen inside Update's pump), bounded by a timeout.
void PumpUntilSettled(StreamingManager& sm, const std::string& id) {
    for (int i = 0; i < 300; ++i) {
        sm.Update(Vector3(0.0f), 0.016f);
        if (sm.GetChunkState(id) != ChunkState::Loading) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace

ENJIN_TEST(ChunkLoad, SubSceneLoadsFromSceneRoot) {
    // Arrange: a project-relative sub-scene under a temp scene root
    auto root = MakeChunkRoot("root");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    StreamingChunk c;
    c.chunkId = "a";
    c.scenePath = "scenes/chunk_a.enjin";   // as authored: relative, never absolute
    sm.AddChunk(c);
    usize before = world.GetAllEntities().size();

    // Act
    sm.ForceLoadChunk("a");
    PumpUntilSettled(sm, "a");

    // Assert: loaded from the root, entities integrated + tracked on the chunk
    ENJIN_EXPECT_TRUE(sm.GetChunkState("a") == ChunkState::Loaded);
    ENJIN_EXPECT_EQ(world.GetAllEntities().size(), before + 1);
    ENJIN_EXPECT_TRUE(world.FindEntityByName("StreamedProp") != ECS::INVALID_ENTITY);
    ENJIN_ASSERT_TRUE(sm.GetChunks().size() == (size_t)1);
    ENJIN_EXPECT_EQ(sm.GetChunks()[0].entities.size(), (size_t)1);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(ChunkLoad, ClearChunksDestroysStreamedEntities) {
    // Arrange: a loaded chunk
    auto root = MakeChunkRoot("clear");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    StreamingChunk c;
    c.chunkId = "a";
    c.scenePath = "scenes/chunk_a.enjin";
    sm.AddChunk(c);
    sm.ForceLoadChunk("a");
    PumpUntilSettled(sm, "a");
    ECS::Entity streamed = world.FindEntityByName("StreamedProp");
    ENJIN_ASSERT_TRUE(streamed != ECS::INVALID_ENTITY);

    // Act: scene teardown / play stop path
    sm.ClearChunks();

    // Assert: the streamed entity is gone (pending destruction reads as invalid)
    ENJIN_EXPECT_FALSE(world.IsValid(streamed));
    ENJIN_EXPECT_TRUE(sm.GetChunks().empty());

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(ChunkLoad, PathEscapingRootIsRejected) {
    // Arrange: a traversal path that would resolve outside the root
    auto root = MakeChunkRoot("escape");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot((root / "scenes").string());
    StreamingChunk c;
    c.chunkId = "evil";
    c.scenePath = "../scenes/chunk_a.enjin";   // exists on disk, but escapes the root
    sm.AddChunk(c);
    usize before = world.GetAllEntities().size();

    // Act
    sm.ForceLoadChunk("evil");
    PumpUntilSettled(sm, "evil");

    // Assert: rejected, nothing integrated, slot released (state back to Unloaded)
    ENJIN_EXPECT_TRUE(sm.GetChunkState("evil") == ChunkState::Unloaded);
    ENJIN_EXPECT_EQ(world.GetAllEntities().size(), before);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(ChunkLoad, AbsolutePathIsRejected) {
    // Arrange: an absolute path to a real file (must still be refused)
    auto root = MakeChunkRoot("abs");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    StreamingChunk c;
    c.chunkId = "abs";
    c.scenePath = (root / "scenes" / "chunk_a.enjin").string();
    sm.AddChunk(c);
    usize before = world.GetAllEntities().size();

    // Act
    sm.ForceLoadChunk("abs");
    PumpUntilSettled(sm, "abs");

    // Assert
    ENJIN_EXPECT_TRUE(sm.GetChunkState("abs") == ChunkState::Unloaded);
    ENJIN_EXPECT_EQ(world.GetAllEntities().size(), before);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

// ===========================================================================
// Memory budget + LRU eviction
// ===========================================================================

namespace {

// A chunk file whose entity carries inline geometry (3 verts / 3 indices) so
// the built-in estimate has something to count.
const char* kGeoChunkJson =
    "{\"version\":\"1.0\",\"entities\":[{\"id\":1,\"name\":{\"name\":\"GeoProp\"},"
    "\"transform\":{\"position\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1],\"visible\":true},"
    "\"mesh\":{\"vertexCount\":3,\"indexCount\":3,\"vertices\":["
    "{\"position\":[0,0,0],\"normal\":[0,1,0],\"uv\":[0,0]},"
    "{\"position\":[1,0,0],\"normal\":[0,1,0],\"uv\":[1,0]},"
    "{\"position\":[0,0,1],\"normal\":[0,1,0],\"uv\":[0,1]}],\"indices\":[0,1,2]}}]}";

std::filesystem::path MakeBudgetRoot(const char* tag) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path root = fs::temp_directory_path(ec) / (std::string("enjin_streambudget_") + tag);
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    std::ofstream f((root / "geo.enjin").string(), std::ios::binary);
    f << kGeoChunkJson;
    return root;
}

StreamingChunk MakeChunk(const char* id, f32 x, f32 loadDist, f32 unloadDist) {
    StreamingChunk c;
    c.chunkId = id;
    c.scenePath = "geo.enjin";
    c.center = Vector3(x, 0.0f, 0.0f);
    c.loadDistance = loadDist;
    c.unloadDistance = unloadDist;
    return c;
}

void PumpAt(StreamingManager& sm, const Vector3& cam, int frames) {
    for (int i = 0; i < frames; ++i) {
        sm.Update(cam, 0.016f);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // namespace

ENJIN_TEST(Budget, EstimateCountsInlineGeometryTwice) {
    // Arrange: one chunk with the 3-vertex mesh
    auto root = MakeBudgetRoot("estimate");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    sm.AddChunk(MakeChunk("a", 0.0f, 100.0f, 150.0f));

    // Act
    PumpAt(sm, Vector3(0.0f), 60);

    // Assert: resident = per-entity overhead + geometry x2 (ECS copy + GPU copy)
    ENJIN_ASSERT_TRUE(sm.GetChunkState("a") == ChunkState::Loaded);
    const u64 geo = 3ull * sizeof(ECS::Vertex) + 3ull * sizeof(u32);
    ENJIN_EXPECT_EQ(sm.GetResidentBytes(), 256ull + geo * 2ull);
    ENJIN_EXPECT_EQ(sm.GetChunks()[0].residentBytes, sm.GetResidentBytes());

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(Budget, EvictsLeastRecentlyNearChunkOutsideLoadDistance) {
    // Arrange: two chunks, each costing 1000 via the cost hook, budget for one.
    // Wide unload distance keeps A loaded (hysteresis band) once the camera moves on.
    auto root = MakeBudgetRoot("evict");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    sm.SetChunkCostFn([](const StreamingChunk&) { return 1000ull; });
    sm.AddChunk(MakeChunk("a", 0.0f, 50.0f, 10000.0f));
    sm.AddChunk(MakeChunk("b", 500.0f, 50.0f, 10000.0f));
    const u64 perChunk = 256ull + (3ull * sizeof(ECS::Vertex) + 12ull) * 2ull + 1000ull;
    sm.SetMemoryBudgetBytes(perChunk + perChunk / 2);   // room for one, not two

    // Act: load A at the origin, then walk to B
    PumpAt(sm, Vector3(0.0f), 60);
    ENJIN_ASSERT_TRUE(sm.GetChunkState("a") == ChunkState::Loaded);
    PumpAt(sm, Vector3(500.0f, 0.0f, 0.0f), 60);

    // Assert: B loaded, A (outside its load distance, least recently near) evicted
    ENJIN_EXPECT_TRUE(sm.GetChunkState("b") == ChunkState::Loaded);
    ENJIN_EXPECT_TRUE(sm.GetChunkState("a") == ChunkState::Unloaded);
    ENJIN_EXPECT_EQ(sm.GetBudgetEvictionCount(), 1u);
    ENJIN_EXPECT_EQ(sm.GetResidentBytes(), perChunk);
    ENJIN_EXPECT_TRUE(world.FindEntityByName("GeoProp") != ECS::INVALID_ENTITY);   // B's copy survives

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(Budget, NeverEvictsChunkWithinLoadDistance) {
    // Arrange: a single in-range chunk that alone blows the budget
    auto root = MakeBudgetRoot("inrange");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    sm.SetChunkCostFn([](const StreamingChunk&) { return 1000ull; });
    sm.AddChunk(MakeChunk("a", 0.0f, 100.0f, 150.0f));
    sm.SetMemoryBudgetBytes(10);

    // Act
    PumpAt(sm, Vector3(0.0f), 60);

    // Assert: over budget, but content under the player is never pulled
    ENJIN_EXPECT_TRUE(sm.GetChunkState("a") == ChunkState::Loaded);
    ENJIN_EXPECT_EQ(sm.GetBudgetEvictionCount(), 0u);
    ENJIN_EXPECT_TRUE(sm.GetResidentBytes() > sm.GetMemoryBudgetBytes());

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(Budget, ZeroBudgetMeansUnlimited) {
    auto root = MakeBudgetRoot("unlimited");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    sm.SetChunkCostFn([](const StreamingChunk&) { return 1ull << 40; });   // 1 TB each
    sm.AddChunk(MakeChunk("a", 0.0f, 50.0f, 10000.0f));
    sm.AddChunk(MakeChunk("b", 500.0f, 50.0f, 10000.0f));

    PumpAt(sm, Vector3(0.0f), 60);
    PumpAt(sm, Vector3(500.0f, 0.0f, 0.0f), 60);

    ENJIN_EXPECT_TRUE(sm.GetChunkState("a") == ChunkState::Loaded);
    ENJIN_EXPECT_TRUE(sm.GetChunkState("b") == ChunkState::Loaded);
    ENJIN_EXPECT_EQ(sm.GetBudgetEvictionCount(), 0u);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(Budget, ClearChunksResetsResident) {
    auto root = MakeBudgetRoot("clear");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());
    sm.AddChunk(MakeChunk("a", 0.0f, 100.0f, 150.0f));
    PumpAt(sm, Vector3(0.0f), 60);
    ENJIN_ASSERT_TRUE(sm.GetResidentBytes() > 0);

    sm.ClearChunks();

    ENJIN_EXPECT_EQ(sm.GetResidentBytes(), 0ull);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

// ===========================================================================
// In-flight chunk reads vs teardown
//
// Regression: chunk reads were std::thread::detach and the destructor only
// called ClearChunks(). A worker that finished after teardown took a freed
// m_StagedMutex and pushed into a freed m_StagedChunks. The same gap leaked a
// load slot: ProcessStagedIntegration releases m_ActiveLoads by finding the
// chunk by id, so anything staged for a chunk ClearChunks had already dropped
// matched nothing, and after m_MaxConcurrentLoads of those the budget was
// permanently full and streaming silently stopped loading.
// ===========================================================================

ENJIN_TEST(StreamingTeardown, DestructorWithLoadInFlightDoesNotFault) {
    auto root = MakeBudgetRoot("teardown");
    ECS::World world;
    {
        StreamingManager sm;
        sm.SetWorld(&world);
        sm.SetSceneRoot(root.string());
        sm.AddChunk(MakeChunk("a", 0.0f, 100.0f, 150.0f));
        // One Update queues and starts the read; destruct immediately, without
        // ever calling ProcessStagedIntegration, so a read is still in flight.
        sm.Update(Vector3(0.0f), 0.016f);
    }
    // Reaching here without a crash or a hang is the assertion.
    ENJIN_EXPECT_TRUE(true);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST(StreamingTeardown, ClearChunksMidLoadKeepsLoadSlotsAvailable) {
    auto root = MakeBudgetRoot("slotleak");
    ECS::World world;
    StreamingManager sm;
    sm.SetWorld(&world);
    sm.SetSceneRoot(root.string());

    // Repeatedly start a load and clear before integrating. Each round used to
    // strand one m_ActiveLoads slot permanently.
    for (int i = 0; i < 6; ++i) {
        sm.AddChunk(MakeChunk("a", 0.0f, 100.0f, 150.0f));
        sm.Update(Vector3(0.0f), 0.016f);
        sm.ClearChunks();
    }

    // Nothing should be left staged, and a fresh chunk must still be able to
    // claim a slot and load.
    ENJIN_EXPECT_EQ(sm.GetPendingIntegrationCount(), (u32)0);
    sm.AddChunk(MakeChunk("a", 0.0f, 100.0f, 150.0f));
    PumpAt(sm, Vector3(0.0f), 60);
    ENJIN_EXPECT_TRUE(sm.GetChunkState("a") == ChunkState::Loaded);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
}

ENJIN_TEST_MAIN()
