#include "Enjin/Procedural/LevelGenerator.h"
#include "Enjin/Math/Math.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <chrono>

namespace Enjin {
namespace Procedural {

// ============================================================================
// LevelGenerator Implementation
// ============================================================================

LevelGenerator::LevelGenerator() {
}

LevelGenerator::~LevelGenerator() {
    Clear();
}

void LevelGenerator::AddPrefab(const RoomPrefab& prefab) {
    // Check for duplicate ID
    for (const auto& p : m_Prefabs) {
        if (p.id == prefab.id) {
            ENJIN_LOG_WARN(Procedural, "Prefab with ID '%s' already exists, replacing", prefab.id.c_str());
            RemovePrefab(prefab.id);
            break;
        }
    }
    m_Prefabs.push_back(prefab);
}

void LevelGenerator::AddPrefabs(const std::vector<RoomPrefab>& prefabs) {
    for (const auto& p : prefabs) {
        AddPrefab(p);
    }
}

void LevelGenerator::RemovePrefab(const std::string& id) {
    m_Prefabs.erase(
        std::remove_if(m_Prefabs.begin(), m_Prefabs.end(),
            [&id](const RoomPrefab& p) { return p.id == id; }),
        m_Prefabs.end()
    );
}

void LevelGenerator::ClearPrefabs() {
    m_Prefabs.clear();
}

const RoomPrefab* LevelGenerator::GetPrefab(const std::string& id) const {
    for (const auto& p : m_Prefabs) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

void LevelGenerator::Clear() {
    m_PlacedRooms.clear();
    m_PrefabUseCounts.clear();
    m_StartRoomIdx = UINT32_MAX;
    m_EndRoomIdx = UINT32_MAX;
    m_NextInstanceId = 0;
    m_Stats = GenerationStats{};
}

void LevelGenerator::InitRandom(u32 seed) {
    m_RandomState = (seed == 0) ?
        static_cast<u32>(std::chrono::steady_clock::now().time_since_epoch().count()) :
        seed;
}

f32 LevelGenerator::RandomFloat() const {
    // Simple LCG
    m_RandomState = m_RandomState * 1103515245 + 12345;
    return static_cast<f32>((m_RandomState >> 16) & 0x7FFF) / 32767.0f;
}

u32 LevelGenerator::RandomUint(u32 max) const {
    if (max == 0) return 0;
    return static_cast<u32>(RandomFloat() * max);
}

bool LevelGenerator::RandomChance(f32 probability) const {
    return RandomFloat() < probability;
}

bool LevelGenerator::Generate(const LevelGenSettings& settings) {
    auto startTime = std::chrono::high_resolution_clock::now();

    Clear();
    m_Settings = settings;
    InitRandom(settings.seed);

    if (m_Prefabs.empty()) {
        ENJIN_LOG_ERROR(Procedural, "No prefabs loaded for level generation");
        if (m_OnComplete) m_OnComplete(false);
        return false;
    }

    ENJIN_LOG_INFO(Procedural, "Starting level generation (target: %u rooms)", settings.targetRooms);

    // Place start room
    if (!PlaceStartRoom()) {
        ENJIN_LOG_ERROR(Procedural, "Failed to place start room");
        if (m_OnComplete) m_OnComplete(false);
        return false;
    }

    // Main generation loop
    u32 attempts = 0;
    const u32 maxAttempts = settings.maxRooms * 100;

    while (m_PlacedRooms.size() < settings.targetRooms && attempts < maxAttempts) {
        attempts++;
        m_Stats.totalAttempts = attempts;

        // Find a room with unused connections
        bool placed = false;
        for (u32 i = 0; i < m_PlacedRooms.size() && !placed; ++i) {
            PlacedRoom& room = m_PlacedRooms[i];
            const RoomPrefab* prefab = room.prefab;

            for (const auto& conn : prefab->connections) {
                // Check if this connection is already used
                bool used = false;
                for (const auto& usedConn : room.usedConnections) {
                    if (usedConn.first == conn.id) {
                        used = true;
                        break;
                    }
                }

                if (!used) {
                    // Check branch chance
                    if (room.usedConnections.size() > 0 && !RandomChance(m_Settings.branchChance)) {
                        continue;
                    }

                    if (PlaceNextRoom(i, conn.id)) {
                        placed = true;
                        break;
                    }
                }
            }
        }

        // If no rooms could be placed, we might be done
        if (!placed && m_PlacedRooms.size() >= settings.minRooms) {
            break;
        }
    }

    // Find end room (furthest from start)
    u32 maxDepth = 0;
    for (u32 i = 0; i < m_PlacedRooms.size(); ++i) {
        if (m_PlacedRooms[i].depth > maxDepth) {
            maxDepth = m_PlacedRooms[i].depth;
            m_EndRoomIdx = i;
        }
    }

    // Calculate stats
    auto endTime = std::chrono::high_resolution_clock::now();
    m_Stats.generationTimeMs = std::chrono::duration<f32, std::milli>(endTime - startTime).count();
    m_Stats.roomsPlaced = static_cast<u32>(m_PlacedRooms.size());

    u32 deadEnds = 0;
    for (const auto& room : m_PlacedRooms) {
        m_Stats.connectionsUsed += static_cast<u32>(room.usedConnections.size());
        if (room.connectedRooms.size() <= 1 && room.instanceId != m_StartRoomIdx) {
            deadEnds++;
        }
    }
    m_Stats.deadEnds = deadEnds;

    bool success = m_PlacedRooms.size() >= settings.minRooms;
    ENJIN_LOG_INFO(Procedural, "Level generation %s: %u rooms in %.2fms",
                   success ? "complete" : "failed",
                   m_Stats.roomsPlaced, m_Stats.generationTimeMs);

    if (m_OnComplete) m_OnComplete(success);
    return success;
}

bool LevelGenerator::PlaceStartRoom() {
    // Find start room prefab
    std::vector<const RoomPrefab*> startCandidates;
    for (const auto& p : m_Prefabs) {
        if (p.category == "start" || p.category == "spawn") {
            startCandidates.push_back(&p);
        }
    }

    // If no specific start room, use any room
    if (startCandidates.empty()) {
        for (const auto& p : m_Prefabs) {
            startCandidates.push_back(&p);
        }
    }

    if (startCandidates.empty()) return false;

    const RoomPrefab* startPrefab = startCandidates[RandomUint(static_cast<u32>(startCandidates.size()))];

    PlacedRoom room;
    room.prefab = startPrefab;
    room.position = Math::Vector3(0, 0, 0);
    room.rotation = 0.0f;
    room.instanceId = m_NextInstanceId++;
    room.depth = 0;

    m_PlacedRooms.push_back(room);
    m_PrefabUseCounts[startPrefab->id]++;
    m_StartRoomIdx = 0;

    if (m_OnRoomPlaced) m_OnRoomPlaced(room);
    return true;
}

bool LevelGenerator::PlaceNextRoom(u32 fromRoomIdx, const std::string& fromConnectionId) {
    PlacedRoom& fromRoom = m_PlacedRooms[fromRoomIdx];
    const RoomPrefab* fromPrefab = fromRoom.prefab;

    // Find the connection point
    const ConnectionPoint* fromConn = nullptr;
    for (const auto& conn : fromPrefab->connections) {
        if (conn.id == fromConnectionId) {
            fromConn = &conn;
            break;
        }
    }
    if (!fromConn) return false;

    // Transform connection to world space
    ConnectionPoint worldConn = TransformConnection(*fromConn, fromRoom.rotation, fromRoom.mirrored);

    // Get valid prefabs for this connection
    std::vector<const RoomPrefab*> validPrefabs = GetValidPrefabs(worldConn, fromRoom.depth + 1);
    if (validPrefabs.empty()) return false;

    // Try each valid prefab
    std::vector<u32> shuffledIndices(validPrefabs.size());
    for (u32 i = 0; i < validPrefabs.size(); ++i) shuffledIndices[i] = i;

    // Shuffle
    for (u32 i = static_cast<u32>(shuffledIndices.size()) - 1; i > 0; --i) {
        u32 j = RandomUint(i + 1);
        std::swap(shuffledIndices[i], shuffledIndices[j]);
    }

    for (u32 idx : shuffledIndices) {
        const RoomPrefab* toPrefab = validPrefabs[idx];

        // Try each connection on the target prefab
        for (const auto& toConn : toPrefab->connections) {
            if (!ConnectionsMatch(worldConn, toConn)) continue;

            // Try different rotations
            std::vector<f32> rotations = {0.0f};
            if (toPrefab->allowRotation) {
                rotations = {0.0f, 90.0f, 180.0f, 270.0f};
            }

            for (f32 rotation : rotations) {
                ConnectionPoint transformedToConn = TransformConnection(toConn, rotation, false);

                // Check if directions are opposite (required for connection)
                if (GetOppositeDirection(worldConn.direction) != transformedToConn.direction) {
                    continue;
                }

                // Calculate position
                Math::Vector3 newPos = CalculateRoomPosition(fromRoom, worldConn, *toPrefab, transformedToConn, rotation);

                // Check for collisions
                if (!CanPlaceRoom(*toPrefab, newPos, rotation)) {
                    continue;
                }

                // Place the room!
                PlacedRoom newRoom;
                newRoom.prefab = toPrefab;
                newRoom.position = newPos;
                newRoom.rotation = rotation;
                newRoom.instanceId = m_NextInstanceId++;
                newRoom.depth = fromRoom.depth + 1;
                newRoom.connectedRooms.push_back(fromRoom.instanceId);
                newRoom.usedConnections.push_back({toConn.id, fromConnectionId});

                // Update from room
                fromRoom.connectedRooms.push_back(newRoom.instanceId);
                fromRoom.usedConnections.push_back({fromConnectionId, toConn.id});

                m_PlacedRooms.push_back(newRoom);
                m_PrefabUseCounts[toPrefab->id]++;

                if (m_OnRoomPlaced) m_OnRoomPlaced(newRoom);
                return true;
            }
        }
    }

    return false;
}

bool LevelGenerator::CanPlaceRoom(const RoomPrefab& prefab, const Math::Vector3& position, f32 rotation) const {
    if (m_Settings.allowOverlap) return true;

    // Calculate rotated bounds
    Math::Vector3 halfSize = prefab.size * 0.5f;

    // Simple AABB collision for now (ignoring rotation for simplicity)
    for (const auto& placed : m_PlacedRooms) {
        Math::Vector3 placedHalfSize = placed.prefab->size * 0.5f;

        Math::Vector3 minA = position - halfSize;
        Math::Vector3 maxA = position + halfSize;
        Math::Vector3 minB = placed.position - placedHalfSize;
        Math::Vector3 maxB = placed.position + placedHalfSize;

        // AABB overlap test
        if (minA.x < maxB.x && maxA.x > minB.x &&
            minA.y < maxB.y && maxA.y > minB.y &&
            minA.z < maxB.z && maxA.z > minB.z) {
            return false;
        }
    }

    (void)rotation; // TODO: Proper rotated collision
    return true;
}

bool LevelGenerator::ConnectionsMatch(const ConnectionPoint& a, const ConnectionPoint& b) const {
    // Check type compatibility
    if (a.type != b.type && a.type != "any" && b.type != "any") {
        return false;
    }

    // Check size compatibility (within tolerance)
    f32 sizeTolerance = 0.5f;
    if (Math::Abs(a.size.x - b.size.x) > sizeTolerance ||
        Math::Abs(a.size.y - b.size.y) > sizeTolerance) {
        return false;
    }

    // Check tag exclusions
    for (const auto& excludeTag : a.excludeTags) {
        for (const auto& bTag : b.tags) {
            if (excludeTag == bTag) return false;
        }
    }
    for (const auto& excludeTag : b.excludeTags) {
        for (const auto& aTag : a.tags) {
            if (excludeTag == aTag) return false;
        }
    }

    return true;
}

Math::Vector3 LevelGenerator::CalculateRoomPosition(
    const PlacedRoom& from, const ConnectionPoint& fromConn,
    const RoomPrefab& toPrefab, const ConnectionPoint& toConn,
    f32 rotation) const {

    // Get world position of from connection
    Math::Vector3 fromConnWorld = from.position + fromConn.localPosition;

    // Get local position of to connection (after rotation)
    f32 rotRad = Math::Radians(rotation);
    f32 cosR = Math::Cos(rotRad);
    f32 sinR = Math::Sin(rotRad);

    Math::Vector3 toConnLocal = toConn.localPosition;
    Math::Vector3 toConnRotated(
        toConnLocal.x * cosR - toConnLocal.z * sinR,
        toConnLocal.y,
        toConnLocal.x * sinR + toConnLocal.z * cosR
    );

    // New room position: fromConnWorld - toConnRotated
    Math::Vector3 newPos = fromConnWorld - toConnRotated;

    // Add spacing
    if (m_Settings.roomSpacing > 0.0f) {
        Math::Vector3 dir(0, 0, 0);
        switch (fromConn.direction) {
            case ConnectionDirection::North: dir.z = 1; break;
            case ConnectionDirection::South: dir.z = -1; break;
            case ConnectionDirection::East: dir.x = 1; break;
            case ConnectionDirection::West: dir.x = -1; break;
            case ConnectionDirection::Up: dir.y = 1; break;
            case ConnectionDirection::Down: dir.y = -1; break;
        }
        newPos = newPos + dir * m_Settings.roomSpacing;
    }

    // Snap to grid
    if (m_Settings.snapToGrid && m_Settings.gridSize > 0.0f) {
        newPos.x = Math::Round(newPos.x / m_Settings.gridSize) * m_Settings.gridSize;
        newPos.y = Math::Round(newPos.y / m_Settings.gridSize) * m_Settings.gridSize;
        newPos.z = Math::Round(newPos.z / m_Settings.gridSize) * m_Settings.gridSize;
    }

    return newPos;
}

std::vector<const RoomPrefab*> LevelGenerator::GetValidPrefabs(const ConnectionPoint& conn, u32 depth) const {
    std::vector<const RoomPrefab*> valid;

    for (const auto& prefab : m_Prefabs) {
        // Check max count
        auto it = m_PrefabUseCounts.find(prefab.id);
        u32 count = (it != m_PrefabUseCounts.end()) ? it->second : 0;
        if (count >= prefab.maxCount) continue;

        // Check depth constraints
        if (depth < prefab.minDistanceFromStart) continue;
        if (depth > prefab.maxDistanceFromStart) continue;

        // Check if prefab has a compatible connection
        bool hasCompatible = false;
        for (const auto& prefabConn : prefab.connections) {
            if (ConnectionsMatch(conn, prefabConn)) {
                // Check direction compatibility
                ConnectionDirection needed = GetOppositeDirection(conn.direction);
                if (prefabConn.direction == needed) {
                    hasCompatible = true;
                    break;
                }
                // If rotation is allowed, any direction might work
                if (prefab.allowRotation) {
                    hasCompatible = true;
                    break;
                }
            }
        }
        if (!hasCompatible) continue;

        valid.push_back(&prefab);
    }

    return valid;
}

ConnectionPoint LevelGenerator::TransformConnection(const ConnectionPoint& conn, f32 rotation, bool mirror) const {
    ConnectionPoint result = conn;

    // Rotate position
    f32 rotRad = Math::Radians(rotation);
    f32 cosR = Math::Cos(rotRad);
    f32 sinR = Math::Sin(rotRad);

    result.localPosition = Math::Vector3(
        conn.localPosition.x * cosR - conn.localPosition.z * sinR,
        conn.localPosition.y,
        conn.localPosition.x * sinR + conn.localPosition.z * cosR
    );

    // Rotate direction
    if (rotation != 0.0f) {
        int rotSteps = static_cast<int>(rotation / 90.0f) % 4;
        for (int i = 0; i < rotSteps; ++i) {
            switch (result.direction) {
                case ConnectionDirection::North: result.direction = ConnectionDirection::East; break;
                case ConnectionDirection::East: result.direction = ConnectionDirection::South; break;
                case ConnectionDirection::South: result.direction = ConnectionDirection::West; break;
                case ConnectionDirection::West: result.direction = ConnectionDirection::North; break;
                default: break;
            }
        }
    }

    // Mirror
    if (mirror) {
        result.localPosition.x = -result.localPosition.x;
        if (result.direction == ConnectionDirection::East) {
            result.direction = ConnectionDirection::West;
        } else if (result.direction == ConnectionDirection::West) {
            result.direction = ConnectionDirection::East;
        }
    }

    return result;
}

const PlacedRoom* LevelGenerator::GetStartRoom() const {
    if (m_StartRoomIdx < m_PlacedRooms.size()) {
        return &m_PlacedRooms[m_StartRoomIdx];
    }
    return nullptr;
}

const PlacedRoom* LevelGenerator::GetEndRoom() const {
    if (m_EndRoomIdx < m_PlacedRooms.size()) {
        return &m_PlacedRooms[m_EndRoomIdx];
    }
    return nullptr;
}

void LevelGenerator::InstantiateInWorld(ECS::World* world) {
    if (!world) return;

    for (auto& room : m_PlacedRooms) {
        // Create root entity for room
        room.rootEntity = world->CreateEntity();

        // Add name
        ECS::NameComponent& name = world->AddComponent<ECS::NameComponent>(room.rootEntity);
        name.name = room.prefab->name + "_" + std::to_string(room.instanceId);

        // Add transform
        ECS::TransformComponent& transform = world->AddComponent<ECS::TransformComponent>(room.rootEntity);
        transform.position = room.position;
        // Convert Y-axis rotation (in degrees) to quaternion
        transform.rotation = Math::Quaternion::FromEuler(Math::Vector3(0, Math::Radians(room.rotation), 0));
        transform.scale = Math::Vector3(1, 1, 1);

        // TODO: Load scene/mesh from prefab paths
    }
}

void LevelGenerator::DestroyInWorld(ECS::World* world) {
    if (!world) return;

    for (auto& room : m_PlacedRooms) {
        if (room.rootEntity != ECS::INVALID_ENTITY) {
            world->DestroyEntity(room.rootEntity);
            room.rootEntity = ECS::INVALID_ENTITY;
        }
    }
}

bool LevelGenerator::ValidateLevel() const {
    // Check minimum rooms
    if (m_PlacedRooms.size() < m_Settings.minRooms) return false;

    // Check required room types
    // TODO: Implement

    return true;
}

std::vector<std::string> LevelGenerator::GetValidationErrors() const {
    std::vector<std::string> errors;

    if (m_PlacedRooms.size() < m_Settings.minRooms) {
        errors.push_back("Not enough rooms placed");
    }

    if (m_StartRoomIdx >= m_PlacedRooms.size()) {
        errors.push_back("No start room");
    }

    if (m_EndRoomIdx >= m_PlacedRooms.size()) {
        errors.push_back("No end room");
    }

    return errors;
}

// ============================================================================
// LevelGenerator2D Implementation
// ============================================================================

void LevelGenerator2D::AddRoom(const Room2D& room) {
    m_Rooms.push_back(room);
}

void LevelGenerator2D::ClearRooms() {
    m_Rooms.clear();
}

bool LevelGenerator2D::Generate(const LevelGen2DSettings& settings) {
    Clear();

    if (m_Rooms.empty()) return false;

    // Simple linear generation for 2D
    std::vector<u32> placedIndices;
    Math::Vector2 currentPos(0, 0);

    // Place rooms horizontally
    u32 roomsToPlace = Math::Min(settings.maxRooms, static_cast<u32>(m_Rooms.size()));

    for (u32 i = 0; i < roomsToPlace; ++i) {
        // Select random room
        u32 roomIdx = i % m_Rooms.size();
        const Room2D& room = m_Rooms[roomIdx];

        m_GeneratedLevel.roomPositions.push_back(currentPos);

        // Move to next position
        currentPos.x += room.size.x + 1;  // +1 for gap/corridor
    }

    // Calculate total size
    f32 maxX = 0, maxY = 0;
    for (usize i = 0; i < m_GeneratedLevel.roomPositions.size(); ++i) {
        const Math::Vector2& pos = m_GeneratedLevel.roomPositions[i];
        u32 roomIdx = static_cast<u32>(i) % m_Rooms.size();
        const Room2D& room = m_Rooms[roomIdx];

        maxX = Math::Max(maxX, pos.x + room.size.x);
        maxY = Math::Max(maxY, pos.y + room.size.y);
    }

    m_GeneratedLevel.size = Math::Vector2(maxX, maxY);
    m_GeneratedLevel.startPosition = m_GeneratedLevel.roomPositions.empty() ?
        Math::Vector2(0, 0) : m_GeneratedLevel.roomPositions[0];
    m_GeneratedLevel.endPosition = m_GeneratedLevel.roomPositions.empty() ?
        Math::Vector2(0, 0) : m_GeneratedLevel.roomPositions.back();

    // Generate combined tilemap
    u32 totalWidth = static_cast<u32>(Math::Ceil(maxX));
    u32 totalHeight = static_cast<u32>(Math::Ceil(maxY));

    m_GeneratedLevel.tiles.resize(totalHeight);
    for (auto& row : m_GeneratedLevel.tiles) {
        row.resize(totalWidth, 0);
    }

    // Copy room tiles into combined map
    for (usize i = 0; i < m_GeneratedLevel.roomPositions.size(); ++i) {
        const Math::Vector2& pos = m_GeneratedLevel.roomPositions[i];
        u32 roomIdx = static_cast<u32>(i) % m_Rooms.size();
        const Room2D& room = m_Rooms[roomIdx];

        u32 startX = static_cast<u32>(pos.x);
        u32 startY = static_cast<u32>(pos.y);

        for (u32 y = 0; y < room.tiles.size() && (startY + y) < totalHeight; ++y) {
            for (u32 x = 0; x < room.tiles[y].size() && (startX + x) < totalWidth; ++x) {
                if (room.tiles[y][x] != 0) {
                    m_GeneratedLevel.tiles[startY + y][startX + x] = room.tiles[y][x];
                }
            }
        }
    }

    (void)settings; // Use more settings in future
    return true;
}

void LevelGenerator2D::Clear() {
    m_GeneratedLevel.tiles.clear();
    m_GeneratedLevel.roomPositions.clear();
    m_GeneratedLevel.size = Math::Vector2(0, 0);
}

void LevelGenerator2D::GetTilemapData(std::vector<u32>& outTiles, u32& outWidth, u32& outHeight) const {
    outWidth = static_cast<u32>(m_GeneratedLevel.size.x);
    outHeight = static_cast<u32>(m_GeneratedLevel.size.y);

    outTiles.clear();
    outTiles.reserve(outWidth * outHeight);

    for (const auto& row : m_GeneratedLevel.tiles) {
        for (u8 tile : row) {
            outTiles.push_back(static_cast<u32>(tile));
        }
    }
}

// ============================================================================
// Prefab Helpers
// ============================================================================

namespace PrefabHelpers {

RoomPrefab CreateRectangularRoom(
    const std::string& id,
    const Math::Vector3& size,
    bool northDoor, bool southDoor, bool eastDoor, bool westDoor) {

    RoomPrefab prefab;
    prefab.id = id;
    prefab.name = "Room " + id;
    prefab.category = "room";
    prefab.size = size;

    f32 halfW = size.x * 0.5f;
    f32 halfD = size.z * 0.5f;
    Math::Vector2 doorSize(2.0f, 3.0f);  // Standard door size

    if (northDoor) {
        ConnectionPoint conn;
        conn.id = "north";
        conn.localPosition = Math::Vector3(0, 0, halfD);
        conn.direction = ConnectionDirection::North;
        conn.size = doorSize;
        prefab.connections.push_back(conn);
    }

    if (southDoor) {
        ConnectionPoint conn;
        conn.id = "south";
        conn.localPosition = Math::Vector3(0, 0, -halfD);
        conn.direction = ConnectionDirection::South;
        conn.size = doorSize;
        prefab.connections.push_back(conn);
    }

    if (eastDoor) {
        ConnectionPoint conn;
        conn.id = "east";
        conn.localPosition = Math::Vector3(halfW, 0, 0);
        conn.direction = ConnectionDirection::East;
        conn.size = doorSize;
        prefab.connections.push_back(conn);
    }

    if (westDoor) {
        ConnectionPoint conn;
        conn.id = "west";
        conn.localPosition = Math::Vector3(-halfW, 0, 0);
        conn.direction = ConnectionDirection::West;
        conn.size = doorSize;
        prefab.connections.push_back(conn);
    }

    return prefab;
}

RoomPrefab CreateCorridor(const std::string& id, f32 length, f32 width, f32 height, bool horizontal) {
    RoomPrefab prefab;
    prefab.id = id;
    prefab.name = "Corridor " + id;
    prefab.category = "corridor";
    prefab.weight = 2.0f;  // Corridors are common

    if (horizontal) {
        prefab.size = Math::Vector3(length, height, width);

        ConnectionPoint west;
        west.id = "west";
        west.localPosition = Math::Vector3(-length * 0.5f, 0, 0);
        west.direction = ConnectionDirection::West;
        west.size = Math::Vector2(width, height);
        prefab.connections.push_back(west);

        ConnectionPoint east;
        east.id = "east";
        east.localPosition = Math::Vector3(length * 0.5f, 0, 0);
        east.direction = ConnectionDirection::East;
        east.size = Math::Vector2(width, height);
        prefab.connections.push_back(east);
    } else {
        prefab.size = Math::Vector3(width, height, length);

        ConnectionPoint north;
        north.id = "north";
        north.localPosition = Math::Vector3(0, 0, length * 0.5f);
        north.direction = ConnectionDirection::North;
        north.size = Math::Vector2(width, height);
        prefab.connections.push_back(north);

        ConnectionPoint south;
        south.id = "south";
        south.localPosition = Math::Vector3(0, 0, -length * 0.5f);
        south.direction = ConnectionDirection::South;
        south.size = Math::Vector2(width, height);
        prefab.connections.push_back(south);
    }

    return prefab;
}

RoomPrefab CreateTJunction(const std::string& id, f32 size, f32 height) {
    RoomPrefab prefab;
    prefab.id = id;
    prefab.name = "T-Junction " + id;
    prefab.category = "junction";
    prefab.size = Math::Vector3(size, height, size);

    f32 half = size * 0.5f;
    Math::Vector2 doorSize(size * 0.8f, height);

    // T shape: North, East, West (no South)
    ConnectionPoint north;
    north.id = "north";
    north.localPosition = Math::Vector3(0, 0, half);
    north.direction = ConnectionDirection::North;
    north.size = doorSize;
    prefab.connections.push_back(north);

    ConnectionPoint east;
    east.id = "east";
    east.localPosition = Math::Vector3(half, 0, 0);
    east.direction = ConnectionDirection::East;
    east.size = doorSize;
    prefab.connections.push_back(east);

    ConnectionPoint west;
    west.id = "west";
    west.localPosition = Math::Vector3(-half, 0, 0);
    west.direction = ConnectionDirection::West;
    west.size = doorSize;
    prefab.connections.push_back(west);

    return prefab;
}

RoomPrefab CreateCrossroads(const std::string& id, f32 size, f32 height) {
    RoomPrefab prefab;
    prefab.id = id;
    prefab.name = "Crossroads " + id;
    prefab.category = "junction";
    prefab.size = Math::Vector3(size, height, size);

    f32 half = size * 0.5f;
    Math::Vector2 doorSize(size * 0.8f, height);

    ConnectionPoint north;
    north.id = "north";
    north.localPosition = Math::Vector3(0, 0, half);
    north.direction = ConnectionDirection::North;
    north.size = doorSize;
    prefab.connections.push_back(north);

    ConnectionPoint south;
    south.id = "south";
    south.localPosition = Math::Vector3(0, 0, -half);
    south.direction = ConnectionDirection::South;
    south.size = doorSize;
    prefab.connections.push_back(south);

    ConnectionPoint east;
    east.id = "east";
    east.localPosition = Math::Vector3(half, 0, 0);
    east.direction = ConnectionDirection::East;
    east.size = doorSize;
    prefab.connections.push_back(east);

    ConnectionPoint west;
    west.id = "west";
    west.localPosition = Math::Vector3(-half, 0, 0);
    west.direction = ConnectionDirection::West;
    west.size = doorSize;
    prefab.connections.push_back(west);

    return prefab;
}

RoomPrefab CreateLShapedRoom(const std::string& id, f32 width, f32 depth, f32 height, ConnectionDirection cornerDirection) {
    RoomPrefab prefab;
    prefab.id = id;
    prefab.name = "L-Room " + id;
    prefab.category = "room";
    prefab.size = Math::Vector3(width, height, depth);
    prefab.allowRotation = true;

    f32 halfW = width * 0.5f;
    f32 halfD = depth * 0.5f;
    Math::Vector2 doorSize(2.0f, 3.0f);

    // Add connections based on corner direction
    // For NE corner: has North and East connections
    switch (cornerDirection) {
        case ConnectionDirection::North: {
            ConnectionPoint n, e;
            n.id = "north"; n.localPosition = Math::Vector3(0, 0, halfD);
            n.direction = ConnectionDirection::North; n.size = doorSize;
            e.id = "east"; e.localPosition = Math::Vector3(halfW, 0, 0);
            e.direction = ConnectionDirection::East; e.size = doorSize;
            prefab.connections.push_back(n);
            prefab.connections.push_back(e);
            break;
        }
        case ConnectionDirection::South: {
            ConnectionPoint s, w;
            s.id = "south"; s.localPosition = Math::Vector3(0, 0, -halfD);
            s.direction = ConnectionDirection::South; s.size = doorSize;
            w.id = "west"; w.localPosition = Math::Vector3(-halfW, 0, 0);
            w.direction = ConnectionDirection::West; w.size = doorSize;
            prefab.connections.push_back(s);
            prefab.connections.push_back(w);
            break;
        }
        default:
            // Default to North-East
            {
                ConnectionPoint n, e;
                n.id = "north"; n.localPosition = Math::Vector3(0, 0, halfD);
                n.direction = ConnectionDirection::North; n.size = doorSize;
                e.id = "east"; e.localPosition = Math::Vector3(halfW, 0, 0);
                e.direction = ConnectionDirection::East; e.size = doorSize;
                prefab.connections.push_back(n);
                prefab.connections.push_back(e);
            }
            break;
    }

    return prefab;
}

Room2D CreateRoom2D(const std::string& id, u32 width, u32 height,
                     bool leftDoor, bool rightDoor, bool topDoor, bool bottomDoor) {
    Room2D room;
    room.id = id;
    room.size = Math::Vector2(static_cast<f32>(width), static_cast<f32>(height));
    room.category = "room";

    // Create tile data (1 = floor, 2 = wall)
    room.tiles.resize(height);
    for (u32 y = 0; y < height; ++y) {
        room.tiles[y].resize(width);
        for (u32 x = 0; x < width; ++x) {
            // Walls on edges
            if (x == 0 || x == width - 1 || y == 0 || y == height - 1) {
                room.tiles[y][x] = 2;  // Wall
            } else {
                room.tiles[y][x] = 1;  // Floor
            }
        }
    }

    // Add connections (clear walls at door positions)
    u32 midX = width / 2;
    u32 midY = height / 2;

    if (leftDoor) {
        Room2D::Connection2D conn;
        conn.id = "left";
        conn.position = Math::Vector2(0, static_cast<f32>(midY));
        conn.direction = ConnectionDirection::West;
        conn.width = 1;
        room.connections.push_back(conn);
        room.tiles[midY][0] = 1;  // Clear wall for door
    }

    if (rightDoor) {
        Room2D::Connection2D conn;
        conn.id = "right";
        conn.position = Math::Vector2(static_cast<f32>(width - 1), static_cast<f32>(midY));
        conn.direction = ConnectionDirection::East;
        conn.width = 1;
        room.connections.push_back(conn);
        room.tiles[midY][width - 1] = 1;
    }

    if (topDoor) {
        Room2D::Connection2D conn;
        conn.id = "top";
        conn.position = Math::Vector2(static_cast<f32>(midX), static_cast<f32>(height - 1));
        conn.direction = ConnectionDirection::North;
        conn.width = 1;
        room.connections.push_back(conn);
        room.tiles[height - 1][midX] = 1;
    }

    if (bottomDoor) {
        Room2D::Connection2D conn;
        conn.id = "bottom";
        conn.position = Math::Vector2(static_cast<f32>(midX), 0);
        conn.direction = ConnectionDirection::South;
        conn.width = 1;
        room.connections.push_back(conn);
        room.tiles[0][midX] = 1;
    }

    return room;
}

Room2D CreatePlatform2D(const std::string& id, u32 width, bool leftConnection, bool rightConnection) {
    Room2D room;
    room.id = id;
    room.size = Math::Vector2(static_cast<f32>(width), 3.0f);
    room.category = "platform";

    // Simple platform: 3 tiles high (floor, empty, empty)
    room.tiles.resize(3);
    for (u32 y = 0; y < 3; ++y) {
        room.tiles[y].resize(width);
        for (u32 x = 0; x < width; ++x) {
            if (y == 0) {
                room.tiles[y][x] = 2;  // Ground
            } else {
                room.tiles[y][x] = 0;  // Empty space above
            }
        }
    }

    if (leftConnection) {
        Room2D::Connection2D conn;
        conn.id = "left";
        conn.position = Math::Vector2(0, 1);
        conn.direction = ConnectionDirection::West;
        conn.width = 2;
        room.connections.push_back(conn);
    }

    if (rightConnection) {
        Room2D::Connection2D conn;
        conn.id = "right";
        conn.position = Math::Vector2(static_cast<f32>(width - 1), 1);
        conn.direction = ConnectionDirection::East;
        conn.width = 2;
        room.connections.push_back(conn);
    }

    return room;
}

} // namespace PrefabHelpers

} // namespace Procedural
} // namespace Enjin
