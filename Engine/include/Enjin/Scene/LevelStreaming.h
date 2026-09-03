#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>

namespace Enjin {
namespace Build { class AssetReader; }
namespace Scene {

// Load state for a streaming chunk
enum class ChunkState : u8 {
    Unloaded,
    Loading,
    Loaded,
    Unloading
};

// Priority level for entity streaming
enum class StreamPriority : u8 {
    Critical,    // Always load first (gameplay-essential)
    High,        // Load early (important NPCs, triggers)
    Normal,      // Standard loading
    Low          // Load last (decoration, particles)
};

// A spatial chunk of the world
struct StreamingChunk {
    std::string chunkId;
    std::string scenePath;          // Path to the chunk's scene file
    Math::Vector3 center;
    Math::Vector3 halfExtents;      // AABB half-size
    f32 loadDistance = 100.0f;       // Distance to camera for loading
    f32 unloadDistance = 150.0f;     // Distance for unloading (hysteresis)
    ChunkState state = ChunkState::Unloaded;
    std::vector<ECS::Entity> entities;  // Entities in this chunk when loaded
    StreamPriority priority = StreamPriority::Normal;
    u32 lodLevel = 0;               // 0 = full detail, 1+ = lower detail
};

// Component marking an entity as a streaming volume boundary. The entity's world
// position is the chunk center; `scenePath` names the (relative) .enjin sub-scene
// that gets loaded/unloaded as the camera nears/leaves the volume. This is the
// authoring hook the runtime scans to register chunks (RegisterChunksFromWorld).
struct StreamingVolumeComponent {
    std::string chunkId;
    std::string scenePath;          // Relative path to the chunk's .enjin sub-scene
    Math::Vector3 halfExtents = Math::Vector3(50.0f);
    f32 loadDistance = 100.0f;
    f32 unloadDistance = 150.0f;
    StreamPriority priority = StreamPriority::Normal;
};

// Portal connecting two chunks (doorways, corridors)
struct StreamingPortalComponent {
    std::string chunkA;
    std::string chunkB;
    Math::Vector3 halfExtents = Math::Vector3(2.0f, 3.0f, 0.5f);
    bool bidirectional = true;
};

// Streaming manager — tracks camera, loads/unloads chunks by distance
class ENJIN_API StreamingManager {
public:
    StreamingManager();
    ~StreamingManager();

    void SetWorld(ECS::World* world) { m_World = world; }

    // Register chunks
    void AddChunk(const StreamingChunk& chunk);
    void RemoveChunk(const std::string& chunkId);
    void ClearChunks();

    // Scan the world for StreamingVolumeComponent entities and register a chunk for
    // each (center = the volume entity's world position). This is the bridge that
    // turns authored volumes into live streaming chunks; call it once at scene
    // runtime init AFTER the scene is loaded. Volumes with an empty chunkId or
    // scenePath are skipped. Returns the number of chunks registered.
    // `baseDir`, if non-empty, is prefixed to each volume's relative scenePath so
    // the chunk resolves against the scene/asset root rather than the process CWD.
    u32 RegisterChunksFromWorld(const std::string& baseDir = "");

    // Where relative chunk scenePaths resolve on disk: the absolute project dir
    // (editor) or loose game dir (player). The process CWD is never reliable, so
    // a chunk with neither a root nor an asset reader only falls back to the
    // path as given (tests/tools) with a warning.
    void SetSceneRoot(const std::string& absRoot) { m_SceneRoot = absRoot; }
    const std::string& GetSceneRoot() const { return m_SceneRoot; }

    // Packed source (player .enjpak / web pak). Tried before disk: sub-scenes
    // are never extracted to disk in pak mode.
    void SetAssetReader(const Build::AssetReader* reader) { m_AssetReader = reader; }

    // Update streaming based on camera position (call each frame)
    void Update(const Math::Vector3& cameraPosition, f32 deltaTime);

    // Force load/unload
    void ForceLoadChunk(const std::string& chunkId);
    void ForceUnloadChunk(const std::string& chunkId);

    // Query
    const std::vector<StreamingChunk>& GetChunks() const { return m_Chunks; }
    ChunkState GetChunkState(const std::string& chunkId) const;
    u32 GetLoadedChunkCount() const;
    u32 GetLoadingChunkCount() const;

    // Configuration
    void SetMaxConcurrentLoads(u32 max) { m_MaxConcurrentLoads = max; }
    void SetEnabled(bool enabled) { m_Enabled = enabled; }

    // Time-sliced integration budget (microseconds per frame for main-thread entity creation)
    // Default 2000us = 2ms at 60fps. Glacier-inspired: no single call exceeds this budget.
    void SetIntegrationBudgetUs(u32 budgetUs) { m_IntegrationBudgetUs = budgetUs; }
    u32 GetIntegrationBudgetUs() const { return m_IntegrationBudgetUs; }

    // Stats
    u32 GetPendingIntegrationCount() const { return static_cast<u32>(m_StagedChunks.size()); }
    f32 GetLastIntegrationTimeMs() const { return m_LastIntegrationTimeMs; }

    // Callbacks
    using ChunkCallback = std::function<void(const std::string& chunkId)>;
    void SetOnChunkLoaded(ChunkCallback cb) { m_OnChunkLoaded = cb; }
    void SetOnChunkUnloaded(ChunkCallback cb) { m_OnChunkUnloaded = cb; }

    // Debug visualization
    void DrawDebugOverlay();

private:
    void ProcessLoadQueue();
    void ProcessUnloadQueue();
    void LoadChunkAsync(StreamingChunk& chunk);
    void IntegrateLoadedChunk(StreamingChunk& chunk, const std::string& sceneJson);
    // Validates + reads a chunk's (relative) scene JSON from the pak or the scene
    // root. Safe to call from a worker thread (read-only members).
    bool ReadChunkSource(const std::string& relPath, std::string& outJson) const;
    void UnloadChunkEntities(StreamingChunk& chunk);

    // Time-sliced integration: process staged chunks within budget
    void ProcessStagedIntegration();

    // Staged chunk data — scene JSON parsed on worker thread, entities created on main thread
    struct StagedChunkData {
        std::string chunkId;
        std::string sceneJson;     // Raw JSON string (parsed off main thread)
        bool jsonReady = false;    // Set by worker thread when JSON is loaded
        u32 entitiesCreated = 0;   // How many entities have been integrated so far
    };

    ECS::World* m_World = nullptr;
    std::string m_SceneRoot;                          // absolute; see SetSceneRoot
    const Build::AssetReader* m_AssetReader = nullptr; // optional pak source
    std::vector<StreamingChunk> m_Chunks;
    std::vector<std::string> m_LoadQueue;
    std::vector<std::string> m_UnloadQueue;
    std::unordered_set<std::string> m_LoadQueueSet;    // O(1) duplicate check
    std::unordered_set<std::string> m_UnloadQueueSet;  // O(1) duplicate check

    // Staged chunks waiting for time-sliced integration on main thread
    std::queue<StagedChunkData> m_StagedChunks;
    std::mutex m_StagedMutex;

    u32 m_MaxConcurrentLoads = 2;
    u32 m_IntegrationBudgetUs = 2000; // 2ms default (Glacier-inspired)
    f32 m_LastIntegrationTimeMs = 0.0f;
    bool m_Enabled = true;
    std::atomic<u32> m_ActiveLoads{0};
    std::mutex m_LoadMutex;

    ChunkCallback m_OnChunkLoaded;
    ChunkCallback m_OnChunkUnloaded;
};

} // namespace Scene
} // namespace Enjin
