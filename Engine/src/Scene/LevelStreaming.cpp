#include "Enjin/Scene/LevelStreaming.h"
#include "Enjin/Scene/SceneSerializer.h"
#include "Enjin/Logging/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
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
    ENJIN_LOG_INFO(Game, "Cleared all streaming chunks");
}

void StreamingManager::Update(const Math::Vector3& cameraPosition, f32 deltaTime)
{
    (void)deltaTime;

    if (!m_Enabled || !m_World) return;

    for (auto& chunk : m_Chunks) {
        // Calculate distance from camera to chunk center
        f32 dx = cameraPosition.x - chunk.center.x;
        f32 dy = cameraPosition.y - chunk.center.y;
        f32 dz = cameraPosition.z - chunk.center.z;
        f32 distance = std::sqrt(dx * dx + dy * dy + dz * dz);

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
    ProcessUnloadQueue();
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

    // For now, load synchronously on the main thread
    // A future improvement would spawn a thread for file I/O and
    // defer entity creation to the main thread
    IntegrateLoadedChunk(chunk);
}

void StreamingManager::IntegrateLoadedChunk(StreamingChunk& chunk)
{
    if (!m_World) {
        ENJIN_LOG_ERROR(Game, "Cannot load chunk '%s': no world set", chunk.chunkId.c_str());
        chunk.state = ChunkState::Unloaded;
        m_ActiveLoads.fetch_sub(1);
        return;
    }

    // N16: Validate path to prevent directory traversal and absolute paths
    if (!chunk.scenePath.empty()) {
        auto normalPath = std::filesystem::path(chunk.scenePath).lexically_normal().string();
        if (normalPath.find("..") != std::string::npos) {
            ENJIN_LOG_ERROR(Game, "Path traversal in chunk '%s': %s", chunk.chunkId.c_str(), chunk.scenePath.c_str());
            chunk.state = ChunkState::Unloaded;
            m_ActiveLoads.fetch_sub(1);
            return;
        }
        // Reject absolute paths
        if (chunk.scenePath.size() >= 2 && chunk.scenePath[1] == ':') {
            ENJIN_LOG_ERROR(Game, "Absolute path rejected in chunk '%s': %s", chunk.chunkId.c_str(), chunk.scenePath.c_str());
            chunk.state = ChunkState::Unloaded;
            m_ActiveLoads.fetch_sub(1);
            return;
        }
        if (chunk.scenePath[0] == '/' || chunk.scenePath[0] == '\\') {
            ENJIN_LOG_ERROR(Game, "Absolute path rejected in chunk '%s': %s", chunk.chunkId.c_str(), chunk.scenePath.c_str());
            chunk.state = ChunkState::Unloaded;
            m_ActiveLoads.fetch_sub(1);
            return;
        }
    }

    // Use SceneSerializer to load the scene file additively
    if (!chunk.scenePath.empty()) {
        SceneSerializer serializer(m_World);

        // Track entities before load to identify new ones
        // P5: Use unordered_set for O(1) lookup instead of O(N*M) nested loop
        auto entitiesBefore = m_World->GetAllEntities();
        std::unordered_set<ECS::Entity> beforeSet(entitiesBefore.begin(), entitiesBefore.end());

        auto result = serializer.LoadAdditive(chunk.scenePath);

        if (result.success) {
            chunk.entities.clear();
            auto entitiesAfter = m_World->GetAllEntities();
            for (auto e : entitiesAfter) {
                if (beforeSet.find(e) == beforeSet.end()) {
                    chunk.entities.push_back(e);
                }
            }

            ENJIN_LOG_INFO(Game, "Chunk '%s' loaded with %u entities",
                chunk.chunkId.c_str(), static_cast<u32>(chunk.entities.size()));
        } else {
            ENJIN_LOG_ERROR(Game, "Failed to load scene file for chunk '%s': %s",
                chunk.chunkId.c_str(), chunk.scenePath.c_str());
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
