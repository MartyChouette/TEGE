#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Platform/Paths.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace Enjin {
namespace Scene {

StreamingManager::StreamingManager() = default;

StreamingManager::~StreamingManager()
{
    ClearChunks();
}

void StreamingManager::AddChunk(const StreamingChunk& chunk)
{
    // Cap total registered chunks to prevent unbounded growth
    static constexpr usize kMaxChunks = 4096;
    if (m_Chunks.size() >= kMaxChunks) {
        ENJIN_LOG_WARN(Game, "Max chunk count (%zu) reached, cannot add '%s'", kMaxChunks, chunk.chunkId.c_str());
        return;
    }

    // Check for duplicate chunk IDs
    for (const auto& existing : m_Chunks) {
        if (existing.chunkId == chunk.chunkId) {
            ENJIN_LOG_WARN(Game, "Chunk '%s' already registered, ignoring duplicate", chunk.chunkId.c_str());
            return;
        }
    }

    m_Chunks.push_back(chunk);
    ENJIN_LOG_INFO(Game, "Registered streaming chunk '%s' at (%.1f, %.1f, %.1f) with load distance %.1f",
        chunk.chunkId.c_str(),
        chunk.center.x, chunk.center.y, chunk.center.z,
        chunk.loadDistance);
}

void StreamingManager::RemoveChunk(const std::string& chunkId)
{
    auto it = std::find_if(m_Chunks.begin(), m_Chunks.end(),
        [&](const StreamingChunk& c) { return c.chunkId == chunkId; });

    if (it != m_Chunks.end()) {
        if (it->state == ChunkState::Loaded) {
            UnloadChunkEntities(*it);
        }
        m_Chunks.erase(it);
        ENJIN_LOG_INFO(Game, "Removed streaming chunk '%s'", chunkId.c_str());
    }
}

void StreamingManager::ClearChunks()
{
    for (auto& chunk : m_Chunks) {
        if (chunk.state == ChunkState::Loaded) {
            UnloadChunkEntities(chunk);
        }
    }
    m_Chunks.clear();
    m_LoadQueue.clear();
    m_UnloadQueue.clear();
    m_LoadQueueSet.clear();
    m_UnloadQueueSet.clear();
    m_ResidentBytes = 0;
    ENJIN_LOG_INFO(Game, "Cleared all streaming chunks");
}

u32 StreamingManager::RegisterChunksFromWorld(const std::string& baseDir)
{
    if (!m_World) {
        ENJIN_LOG_WARN(Game, "RegisterChunksFromWorld: no world set");
        return 0;
    }

    u32 registered = 0;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<StreamingVolumeComponent>()) {
        const auto* vol = m_World->GetComponent<StreamingVolumeComponent>(e);
        if (!vol) continue;
        if (vol->chunkId.empty() || vol->scenePath.empty()) {
            ENJIN_LOG_WARN(Game, "Streaming volume on entity %llu has empty chunkId/scenePath, skipping",
                static_cast<unsigned long long>(e));
            continue;
        }

        StreamingChunk chunk;
        chunk.chunkId = vol->chunkId;
        // Prefix the authored (relative) scene path with the scene/asset root so the
        // chunk resolves independently of the process CWD. Kept relative so the
        // loader's path-traversal / absolute-path guards still apply.
        if (!baseDir.empty()) {
            std::string sep = (baseDir.back() == '/' || baseDir.back() == '\\') ? "" : "/";
            chunk.scenePath = baseDir + sep + vol->scenePath;
        } else {
            chunk.scenePath = vol->scenePath;
        }

        // Chunk center = the volume entity's world position (its transform).
        chunk.center = Math::Vector3(0.0f);
        if (const auto* tf = m_World->GetComponent<ECS::TransformComponent>(e)) {
            chunk.center = tf->position;
        }
        chunk.halfExtents = vol->halfExtents;
        chunk.loadDistance = vol->loadDistance;
        chunk.unloadDistance = vol->unloadDistance;
        chunk.priority = vol->priority;

        usize before = m_Chunks.size();
        AddChunk(chunk);  // dedups by chunkId + enforces the max-chunk cap
        if (m_Chunks.size() > before) registered++;
    }

    ENJIN_LOG_INFO(Game, "RegisterChunksFromWorld: registered %u streaming chunk(s)", registered);
    return registered;
}

void StreamingManager::Update(const Math::Vector3& cameraPosition, f32 deltaTime)
{
    if (!m_Enabled || !m_World) return;
    m_Clock += static_cast<f64>(deltaTime);

    for (auto& chunk : m_Chunks) {
        // Calculate distance from camera to chunk center
        f32 dx = cameraPosition.x - chunk.center.x;
        f32 dy = cameraPosition.y - chunk.center.y;
        f32 dz = cameraPosition.z - chunk.center.z;
        f32 distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance <= chunk.loadDistance) chunk.lastNearTime = m_Clock;   // LRU key for the budget

        switch (chunk.state) {
            case ChunkState::Unloaded:
                // Queue for loading if within load distance (O(1) duplicate check)
                if (distance <= chunk.loadDistance && m_LoadQueue.size() < 256) {
                    if (m_LoadQueueSet.insert(chunk.chunkId).second) {
                        m_LoadQueue.push_back(chunk.chunkId);
                        ENJIN_LOG_INFO(Game, "Queuing chunk '%s' for load (distance: %.1f)",
                            chunk.chunkId.c_str(), distance);
                    }
                }
                break;

            case ChunkState::Loaded:
                // Queue for unloading if beyond unload distance (O(1) duplicate check)
                if (distance > chunk.unloadDistance && m_UnloadQueue.size() < 256) {
                    if (m_UnloadQueueSet.insert(chunk.chunkId).second) {
                        m_UnloadQueue.push_back(chunk.chunkId);
                        ENJIN_LOG_INFO(Game, "Queuing chunk '%s' for unload (distance: %.1f)",
                            chunk.chunkId.c_str(), distance);
                    }
                }
                break;

            case ChunkState::Loading:
            case ChunkState::Unloading:
                // In progress, skip
                break;
        }
    }

    // Sort load queue by priority — pre-build priority map for O(1) lookups in comparator
    if (m_LoadQueue.size() > 1) {
        std::unordered_map<std::string, StreamPriority> priorityMap;
        priorityMap.reserve(m_LoadQueue.size());
        for (const auto& c : m_Chunks) {
            priorityMap[c.chunkId] = c.priority;
        }
        std::sort(m_LoadQueue.begin(), m_LoadQueue.end(),
            [&priorityMap](const std::string& a, const std::string& b) {
                auto itA = priorityMap.find(a);
                auto itB = priorityMap.find(b);
                u8 pa = (itA != priorityMap.end()) ? static_cast<u8>(itA->second) : static_cast<u8>(StreamPriority::Normal);
                u8 pb = (itB != priorityMap.end()) ? static_cast<u8>(itB->second) : static_cast<u8>(StreamPriority::Normal);
                return pa < pb;
            });
    }

    ProcessLoadQueue();
    ProcessStagedIntegration(); // Time-sliced: create entities within 2ms budget
    ProcessUnloadQueue();
    EnforceMemoryBudget(cameraPosition);
}

void StreamingManager::ProcessLoadQueue()
{
    while (!m_LoadQueue.empty() && m_ActiveLoads.load() < m_MaxConcurrentLoads) {
        std::string chunkId = m_LoadQueue.front();
        m_LoadQueue.erase(m_LoadQueue.begin());
        m_LoadQueueSet.erase(chunkId);

        // Find the chunk
        StreamingChunk* chunk = nullptr;
        for (auto& c : m_Chunks) {
            if (c.chunkId == chunkId) { chunk = &c; break; }
        }

        if (!chunk || chunk->state != ChunkState::Unloaded) continue;

        LoadChunkAsync(*chunk);
    }
}

void StreamingManager::ProcessUnloadQueue()
{
    while (!m_UnloadQueue.empty()) {
        std::string chunkId = m_UnloadQueue.front();
        m_UnloadQueue.erase(m_UnloadQueue.begin());
        m_UnloadQueueSet.erase(chunkId);

        // Find the chunk
        StreamingChunk* chunk = nullptr;
        for (auto& c : m_Chunks) {
            if (c.chunkId == chunkId) { chunk = &c; break; }
        }

        if (!chunk || chunk->state != ChunkState::Loaded) continue;

        chunk->state = ChunkState::Unloading;
        UnloadChunkEntities(*chunk);
        chunk->state = ChunkState::Unloaded;

        ENJIN_LOG_INFO(Game, "Unloaded chunk '%s'", chunkId.c_str());

        if (m_OnChunkUnloaded) {
            m_OnChunkUnloaded(chunkId);
        }
    }
}

bool StreamingManager::ReadChunkSource(const std::string& relPath, std::string& outJson) const
{
    // Chunk scene paths are authored project-root-relative. Validate lexically
    // (no absolute path, no drive, no "..") before touching any source.
    if (!Platform::IsSafeRelativePath(relPath)) {
        ENJIN_LOG_ERROR(Game, "Streaming: unsafe chunk scene path rejected: %s", relPath.c_str());
        return false;
    }

    // Packed source first (player .enjpak / web pak): sub-scenes live only there.
    if (m_AssetReader && m_AssetReader->HasFile(relPath)) {
        std::vector<u8> data = m_AssetReader->ReadFile(relPath);
        if (data.empty()) return false;
        outJson.assign(data.begin(), data.end());
        return true;
    }

    // Disk: resolve against the scene root (project dir / loose game dir). The
    // process CWD is never reliable; the bare-path fallback exists only for
    // callers that never set a root (tests, tools).
    std::string fullPath;
    if (!m_SceneRoot.empty()) {
        fullPath = Platform::ResolveWithinRoot(m_SceneRoot, relPath);
        if (fullPath.empty()) {
            ENJIN_LOG_ERROR(Game, "Streaming: chunk scene path escapes the scene root: %s", relPath.c_str());
            return false;
        }
    } else {
        ENJIN_LOG_WARN(Game, "Streaming: no scene root set, resolving '%s' against the process CWD", relPath.c_str());
        fullPath = relPath;
    }

    std::ifstream file(fullPath, std::ios::binary);
    if (!file.is_open()) {
        ENJIN_LOG_ERROR(Game, "Streaming: cannot open chunk scene: %s", fullPath.c_str());
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    outJson = ss.str();
    return !outJson.empty();
}

void StreamingManager::LoadChunkAsync(StreamingChunk& chunk)
{
    // Atomically claim a load slot to prevent races
    u32 prev = m_ActiveLoads.fetch_add(1);
    if (prev >= m_MaxConcurrentLoads) {
        m_ActiveLoads.fetch_sub(1);
        return; // Slot was taken between check and claim
    }
    chunk.state = ChunkState::Loading;

    ENJIN_LOG_INFO(Game, "Loading chunk '%s' from '%s'", chunk.chunkId.c_str(), chunk.scenePath.c_str());

    // The pak/disk read (the expensive part) runs off the main thread; entity
    // creation is deferred to ProcessStagedIntegration(), which runs on the main
    // thread within the integration budget. The staged JSON is what gets
    // integrated: nothing re-reads the path later.
    std::string chunkId = chunk.chunkId;
    std::string scenePath = chunk.scenePath;
    auto readAndStage = [this, chunkId, scenePath]() {
        StagedChunkData staged;
        staged.chunkId = chunkId;
        staged.jsonReady = ReadChunkSource(scenePath, staged.sceneJson);
        std::lock_guard<std::mutex> lock(m_StagedMutex);
        m_StagedChunks.push(std::move(staged));
    };

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    // Web builds link without pthreads: std::thread construction throws. Read
    // inline (pak reads are in-memory) and stage through the same path.
    readAndStage();
#else
    std::thread worker(readAndStage);
    worker.detach();
#endif
}

void StreamingManager::ProcessStagedIntegration()
{
    auto frameStart = std::chrono::high_resolution_clock::now();
    u32 budgetUs = m_IntegrationBudgetUs;

    while (true) {
        StagedChunkData staged;

        // Pop next staged chunk (if any)
        {
            std::lock_guard<std::mutex> lock(m_StagedMutex);
            if (m_StagedChunks.empty()) break;
            staged = std::move(m_StagedChunks.front());
            m_StagedChunks.pop();
        }

        if (!staged.jsonReady || staged.sceneJson.empty()) {
            // File read failed or empty — mark chunk as failed
            for (auto& chunk : m_Chunks) {
                if (chunk.chunkId == staged.chunkId) {
                    chunk.state = ChunkState::Unloaded;
                    m_ActiveLoads.fetch_sub(1);
                    ENJIN_LOG_WARN(Game, "Chunk '%s' staged data empty, skipping", staged.chunkId.c_str());
                    break;
                }
            }
            continue;
        }

        // Find the chunk
        StreamingChunk* chunk = nullptr;
        for (auto& c : m_Chunks) {
            if (c.chunkId == staged.chunkId) { chunk = &c; break; }
        }
        if (!chunk) {
            m_ActiveLoads.fetch_sub(1);
            continue;
        }

        // Integrate via SceneSerializer (this is the main-thread-bound part).
        // For now, the full integration happens here. A further optimization would
        // split the entity creation loop itself and resume across frames.
        IntegrateLoadedChunk(*chunk, staged.sceneJson);

        // Check time budget
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - frameStart).count();
        m_LastIntegrationTimeMs = static_cast<f32>(elapsedUs) / 1000.0f;

        if (static_cast<u32>(elapsedUs) >= budgetUs) {
            // Budget exhausted — remaining staged chunks will be processed next frame
            break;
        }
    }
}

void StreamingManager::IntegrateLoadedChunk(StreamingChunk& chunk, const std::string& sceneJson)
{
    if (!m_World) {
        ENJIN_LOG_ERROR(Game, "Cannot load chunk '%s': no world set", chunk.chunkId.c_str());
        chunk.state = ChunkState::Unloaded;
        m_ActiveLoads.fetch_sub(1);
        return;
    }

    // Integrate the JSON the worker already read and validated (ReadChunkSource:
    // pak or scene root) additively into the live world. Re-reading the path here
    // would resolve against the process CWD and miss pak-only sub-scenes.
    if (!sceneJson.empty()) {
        SceneSerializer serializer(m_World);

        // Track entities before load to identify new ones
        // P5: Use unordered_set for O(1) lookup instead of O(N*M) nested loop
        auto entitiesBefore = m_World->GetAllEntities();
        std::unordered_set<ECS::Entity> beforeSet(entitiesBefore.begin(), entitiesBefore.end());

        auto result = serializer.LoadFromString(sceneJson, /*clearExisting=*/false);

        if (result.success) {
            chunk.entities.clear();
            auto entitiesAfter = m_World->GetAllEntities();
            for (auto e : entitiesAfter) {
                if (beforeSet.find(e) == beforeSet.end()) {
                    chunk.entities.push_back(e);
                }
            }

            chunk.residentBytes = EstimateChunkBytes(chunk);
            m_ResidentBytes += chunk.residentBytes;
            chunk.lastNearTime = m_Clock;

            ENJIN_LOG_INFO(Game, "Chunk '%s' loaded with %u entities (~%.1f KB resident, %.1f KB total)",
                chunk.chunkId.c_str(), static_cast<u32>(chunk.entities.size()),
                chunk.residentBytes / 1024.0, m_ResidentBytes / 1024.0);
        } else {
            ENJIN_LOG_ERROR(Game, "Failed to load scene for chunk '%s' (%s): %s",
                chunk.chunkId.c_str(), chunk.scenePath.c_str(), result.error.c_str());
        }
    }

    chunk.state = ChunkState::Loaded;
    m_ActiveLoads.fetch_sub(1);

    if (m_OnChunkLoaded) {
        m_OnChunkLoaded(chunk.chunkId);
    }
}

void StreamingManager::UnloadChunkEntities(StreamingChunk& chunk)
{
    if (!m_World) return;

    for (auto entity : chunk.entities) {
        m_World->DestroyEntity(entity);
    }

    ENJIN_LOG_INFO(Game, "Destroyed %u entities from chunk '%s'",
        static_cast<u32>(chunk.entities.size()), chunk.chunkId.c_str());

    chunk.entities.clear();
    m_ResidentBytes = (m_ResidentBytes >= chunk.residentBytes) ? m_ResidentBytes - chunk.residentBytes : 0;
    chunk.residentBytes = 0;
}

u64 StreamingManager::EstimateChunkBytes(const StreamingChunk& chunk) const
{
    // An estimate, not a measurement: inline geometry is what streamed content is
    // mostly made of, and it exists twice while loaded (ECS copy + GPU upload).
    // Textures/materials referenced by path are shared and cached elsewhere; the
    // runtime can add them through the cost hook if it knows their sizes.
    constexpr u64 kPerEntityOverhead = 256;
    u64 bytes = 0;
    if (m_World) {
        for (auto e : chunk.entities) {
            bytes += kPerEntityOverhead;
            if (const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(e)) {
                const u64 geo = static_cast<u64>(mesh->vertices.size()) * sizeof(ECS::Vertex)
                              + static_cast<u64>(mesh->indices.size()) * sizeof(u32);
                bytes += geo * 2;
            }
        }
    }
    if (m_ChunkCostFn) bytes += m_ChunkCostFn(chunk);
    return bytes;
}

void StreamingManager::EnforceMemoryBudget(const Math::Vector3& cameraPosition)
{
    if (m_MemoryBudgetBytes == 0) return;
    while (m_ResidentBytes > m_MemoryBudgetBytes) {
        // Victim = least-recently-near Loaded chunk the camera is outside the
        // loadDistance of (i.e. sitting in the hysteresis band). In-range chunks
        // are never evicted: they would reload next frame and thrash.
        StreamingChunk* victim = nullptr;
        for (auto& c : m_Chunks) {
            if (c.state != ChunkState::Loaded) continue;
            const f32 dx = cameraPosition.x - c.center.x;
            const f32 dy = cameraPosition.y - c.center.y;
            const f32 dz = cameraPosition.z - c.center.z;
            if (std::sqrt(dx * dx + dy * dy + dz * dz) <= c.loadDistance) continue;
            if (!victim || c.lastNearTime < victim->lastNearTime) victim = &c;
        }
        if (!victim) {
            if (!m_BudgetWarned) {
                ENJIN_LOG_WARN(Game, "Streaming memory budget exceeded (%.1f / %.1f MB) but every loaded chunk is "
                    "within its load distance; nothing evicted. Raise the budget or shrink load distances.",
                    m_ResidentBytes / (1024.0 * 1024.0), m_MemoryBudgetBytes / (1024.0 * 1024.0));
                m_BudgetWarned = true;
            }
            return;
        }
        m_BudgetWarned = false;
        const std::string id = victim->chunkId;
        victim->state = ChunkState::Unloading;
        UnloadChunkEntities(*victim);
        victim->state = ChunkState::Unloaded;
        ++m_BudgetEvictions;
        ENJIN_LOG_INFO(Game, "Budget evicted chunk '%s' (resident now %.1f / %.1f MB)",
            id.c_str(), m_ResidentBytes / (1024.0 * 1024.0), m_MemoryBudgetBytes / (1024.0 * 1024.0));
        if (m_OnChunkUnloaded) m_OnChunkUnloaded(id);
    }
}

void StreamingManager::ForceLoadChunk(const std::string& chunkId)
{
    for (auto& chunk : m_Chunks) {
        if (chunk.chunkId == chunkId) {
            if (chunk.state == ChunkState::Unloaded) {
                LoadChunkAsync(chunk);
            } else {
                ENJIN_LOG_WARN(Game, "Cannot force-load chunk '%s': state is not Unloaded", chunkId.c_str());
            }
            return;
        }
    }
    ENJIN_LOG_WARN(Game, "ForceLoadChunk: chunk '%s' not found", chunkId.c_str());
}

void StreamingManager::ForceUnloadChunk(const std::string& chunkId)
{
    for (auto& chunk : m_Chunks) {
        if (chunk.chunkId == chunkId) {
            if (chunk.state == ChunkState::Loaded) {
                chunk.state = ChunkState::Unloading;
                UnloadChunkEntities(chunk);
                chunk.state = ChunkState::Unloaded;

                if (m_OnChunkUnloaded) {
                    m_OnChunkUnloaded(chunkId);
                }
            } else {
                ENJIN_LOG_WARN(Game, "Cannot force-unload chunk '%s': state is not Loaded", chunkId.c_str());
            }
            return;
        }
    }
    ENJIN_LOG_WARN(Game, "ForceUnloadChunk: chunk '%s' not found", chunkId.c_str());
}

ChunkState StreamingManager::GetChunkState(const std::string& chunkId) const
{
    for (const auto& chunk : m_Chunks) {
        if (chunk.chunkId == chunkId) {
            return chunk.state;
        }
    }
    return ChunkState::Unloaded;
}

u32 StreamingManager::GetLoadedChunkCount() const
{
    u32 count = 0;
    for (const auto& chunk : m_Chunks) {
        if (chunk.state == ChunkState::Loaded) count++;
    }
    return count;
}

u32 StreamingManager::GetLoadingChunkCount() const
{
    u32 count = 0;
    for (const auto& chunk : m_Chunks) {
        if (chunk.state == ChunkState::Loading) count++;
    }
    return count;
}

void StreamingManager::DrawDebugOverlay()
{
    if (!ImGui::Begin("Level Streaming Debug")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Streaming Manager");
    ImGui::Separator();

    ImGui::Text("Enabled: %s", m_Enabled ? "Yes" : "No");
    ImGui::Text("Total Chunks: %u", static_cast<u32>(m_Chunks.size()));
    ImGui::Text("Loaded: %u", GetLoadedChunkCount());
    ImGui::Text("Loading: %u", GetLoadingChunkCount());
    ImGui::Text("Load Queue: %u", static_cast<u32>(m_LoadQueue.size()));
    ImGui::Text("Unload Queue: %u", static_cast<u32>(m_UnloadQueue.size()));
    ImGui::Text("Max Concurrent Loads: %u", m_MaxConcurrentLoads);
    if (m_MemoryBudgetBytes > 0) {
        ImGui::Text("Memory: %.1f / %.1f MB resident (%u budget evictions)",
            m_ResidentBytes / (1024.0 * 1024.0), m_MemoryBudgetBytes / (1024.0 * 1024.0), m_BudgetEvictions);
    } else {
        ImGui::Text("Memory: %.1f MB resident (no budget)", m_ResidentBytes / (1024.0 * 1024.0));
    }

    ImGui::Separator();
    ImGui::Text("Chunks:");

    if (ImGui::BeginTable("ChunksTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Chunk ID");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Priority");
        ImGui::TableSetupColumn("Entities");
        ImGui::TableSetupColumn("Actions");
        ImGui::TableHeadersRow();

        for (auto& chunk : m_Chunks) {
            ImGui::TableNextRow();

            ImGui::TableNextColumn();
            ImGui::Text("%s", chunk.chunkId.c_str());

            ImGui::TableNextColumn();
            const char* stateStr = "Unknown";
            ImVec4 stateColor = ImVec4(1, 1, 1, 1);
            switch (chunk.state) {
                case ChunkState::Unloaded:
                    stateStr = "Unloaded";
                    stateColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                    break;
                case ChunkState::Loading:
                    stateStr = "Loading";
                    stateColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
                    break;
                case ChunkState::Loaded:
                    stateStr = "Loaded";
                    stateColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
                    break;
                case ChunkState::Unloading:
                    stateStr = "Unloading";
                    stateColor = ImVec4(1.0f, 0.5f, 0.0f, 1.0f);
                    break;
            }
            ImGui::TextColored(stateColor, "%s", stateStr);

            ImGui::TableNextColumn();
            const char* prioStr = "Normal";
            switch (chunk.priority) {
                case StreamPriority::Critical: prioStr = "Critical"; break;
                case StreamPriority::High:     prioStr = "High"; break;
                case StreamPriority::Normal:   prioStr = "Normal"; break;
                case StreamPriority::Low:      prioStr = "Low"; break;
            }
            ImGui::Text("%s", prioStr);

            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<u32>(chunk.entities.size()));

            ImGui::TableNextColumn();
            ImGui::PushID(chunk.chunkId.c_str());
            if (chunk.state == ChunkState::Unloaded) {
                if (ImGui::SmallButton("Load")) {
                    ForceLoadChunk(chunk.chunkId);
                }
            } else if (chunk.state == ChunkState::Loaded) {
                if (ImGui::SmallButton("Unload")) {
                    ForceUnloadChunk(chunk.chunkId);
                }
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace Scene
} // namespace Enjin
