#pragma once
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Entity.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>

namespace Enjin {
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

// Component marking an entity as a streaming volume boundary
struct StreamingVolumeComponent {
    std::string chunkId;
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
    void IntegrateLoadedChunk(StreamingChunk& chunk);
    void UnloadChunkEntities(StreamingChunk& chunk);

    ECS::World* m_World = nullptr;
    std::vector<StreamingChunk> m_Chunks;
    std::vector<std::string> m_LoadQueue;
    std::vector<std::string> m_UnloadQueue;

    u32 m_MaxConcurrentLoads = 2;
    bool m_Enabled = true;
    std::atomic<u32> m_ActiveLoads{0};
    std::mutex m_LoadMutex;

    ChunkCallback m_OnChunkLoaded;
    ChunkCallback m_OnChunkUnloaded;
};

} // namespace Scene
} // namespace Enjin
