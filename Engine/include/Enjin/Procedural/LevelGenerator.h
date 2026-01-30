#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/World.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>

namespace Enjin {
namespace Procedural {

// ============================================================================
// Connection Points - Where rooms can connect
// ============================================================================

// Direction a connection faces (for matching)
enum class ConnectionDirection : u8 {
    North,   // +Z (3D) or Up (2D)
    South,   // -Z (3D) or Down (2D)
    East,    // +X (3D/2D)
    West,    // -X (3D/2D)
    Up,      // +Y (3D only, for vertical connections)
    Down     // -Y (3D only)
};

// Get opposite direction for matching
inline ConnectionDirection GetOppositeDirection(ConnectionDirection dir) {
    switch (dir) {
        case ConnectionDirection::North: return ConnectionDirection::South;
        case ConnectionDirection::South: return ConnectionDirection::North;
        case ConnectionDirection::East: return ConnectionDirection::West;
        case ConnectionDirection::West: return ConnectionDirection::East;
        case ConnectionDirection::Up: return ConnectionDirection::Down;
        case ConnectionDirection::Down: return ConnectionDirection::Up;
    }
    return ConnectionDirection::North;
}

// Connection point on a prefab
struct ConnectionPoint {
    std::string id;                     // Unique identifier within prefab
    Math::Vector3 localPosition;        // Position relative to prefab origin
    ConnectionDirection direction;       // Which way the connection faces
    Math::Vector2 size;                 // Width x Height of the opening
    std::string type = "default";       // For type matching (door, corridor, etc.)
    bool required = false;              // Must this connection be used?

    // Tags for conditional matching
    std::vector<std::string> tags;
    std::vector<std::string> excludeTags;  // Won't connect if other has these
};

// ============================================================================
// Room Prefab - Template for a room
// ============================================================================

struct RoomPrefab {
    std::string id;                     // Unique prefab identifier
    std::string name;                   // Display name
    std::string category;               // Room category (spawn, boss, treasure, etc.)

    // Bounds
    Math::Vector3 size;                 // Bounding box dimensions
    Math::Vector3 origin;               // Local origin offset

    // Connection points
    std::vector<ConnectionPoint> connections;

    // Visual/gameplay data
    std::string meshPath;               // Path to 3D mesh (optional)
    std::string scenePath;              // Path to scene file to instantiate
    std::vector<std::string> tags;      // Tags for filtering

    // Generation rules
    f32 weight = 1.0f;                  // Selection probability weight
    u32 minCount = 0;                   // Minimum times this must appear
    u32 maxCount = UINT32_MAX;          // Maximum times this can appear
    bool allowRotation = true;          // Can be rotated during placement
    bool allowMirror = false;           // Can be mirrored

    // Placement constraints
    std::vector<std::string> mustConnectTo;     // Must connect to these categories
    std::vector<std::string> cannotConnectTo;   // Cannot connect to these categories
    u32 minDistanceFromStart = 0;               // Minimum rooms from start
    u32 maxDistanceFromStart = UINT32_MAX;      // Maximum rooms from start
};

// ============================================================================
// Placed Room - A room instance in the generated level
// ============================================================================

struct PlacedRoom {
    const RoomPrefab* prefab;           // Source prefab
    Math::Vector3 position;             // World position
    f32 rotation = 0.0f;                // Rotation in degrees (Y-axis for 3D)
    bool mirrored = false;

    // Instance data
    u32 instanceId;                     // Unique instance ID
    u32 depth;                          // Distance from start room
    std::vector<u32> connectedRooms;    // IDs of connected room instances
    std::vector<std::pair<std::string, std::string>> usedConnections;  // (ourConn, theirConn)

    // Entity created from this room
    ECS::Entity rootEntity = ECS::INVALID_ENTITY;
};

// ============================================================================
// Generation Settings
// ============================================================================

struct LevelGenSettings {
    // Size constraints
    u32 minRooms = 5;
    u32 maxRooms = 20;
    u32 targetRooms = 10;

    // Room placement
    f32 roomSpacing = 0.0f;             // Gap between rooms
    bool allowOverlap = false;          // Allow rooms to overlap (for 2D overlays)
    bool snapToGrid = true;
    f32 gridSize = 1.0f;

    // Generation style
    bool linearPath = false;            // Force linear progression (no branches)
    f32 branchChance = 0.3f;            // Chance to branch off main path
    u32 maxBranchLength = 3;            // Maximum branch depth
    u32 maxDeadEnds = 5;                // Maximum dead-end rooms

    // Required rooms
    bool requireBossRoom = true;
    bool requireTreasureRoom = true;
    u32 minTreasureRooms = 1;

    // Seed for reproducibility
    u32 seed = 0;                       // 0 = random seed
};

// ============================================================================
// Level Generator
// ============================================================================

class ENJIN_API LevelGenerator {
public:
    LevelGenerator();
    ~LevelGenerator();

    // Prefab management
    void AddPrefab(const RoomPrefab& prefab);
    void AddPrefabs(const std::vector<RoomPrefab>& prefabs);
    void RemovePrefab(const std::string& id);
    void ClearPrefabs();
    const RoomPrefab* GetPrefab(const std::string& id) const;
    const std::vector<RoomPrefab>& GetPrefabs() const { return m_Prefabs; }

    // Load prefabs from JSON file
    bool LoadPrefabsFromFile(const std::string& filepath);
    bool SavePrefabsToFile(const std::string& filepath) const;

    // Generation
    bool Generate(const LevelGenSettings& settings);
    void Clear();

    // Get results
    const std::vector<PlacedRoom>& GetPlacedRooms() const { return m_PlacedRooms; }
    const PlacedRoom* GetStartRoom() const;
    const PlacedRoom* GetEndRoom() const;
    u32 GetRoomCount() const { return static_cast<u32>(m_PlacedRooms.size()); }

    // Instantiate in world
    void InstantiateInWorld(ECS::World* world);
    void DestroyInWorld(ECS::World* world);

    // Validation
    bool ValidateLevel() const;
    std::vector<std::string> GetValidationErrors() const;

    // Callbacks
    using RoomPlacedCallback = std::function<void(const PlacedRoom&)>;
    using GenerationCompleteCallback = std::function<void(bool success)>;

    void SetOnRoomPlaced(RoomPlacedCallback callback) { m_OnRoomPlaced = callback; }
    void SetOnGenerationComplete(GenerationCompleteCallback callback) { m_OnComplete = callback; }

    // Get generation stats
    struct GenerationStats {
        u32 totalAttempts = 0;
        u32 roomsPlaced = 0;
        u32 connectionsUsed = 0;
        u32 deadEnds = 0;
        f32 generationTimeMs = 0.0f;
    };
    const GenerationStats& GetStats() const { return m_Stats; }

private:
    // Generation helpers
    bool PlaceStartRoom();
    bool PlaceNextRoom(u32 fromRoomIdx, const std::string& fromConnectionId);
    bool CanPlaceRoom(const RoomPrefab& prefab, const Math::Vector3& position, f32 rotation) const;
    bool ConnectionsMatch(const ConnectionPoint& a, const ConnectionPoint& b) const;
    Math::Vector3 CalculateRoomPosition(const PlacedRoom& from, const ConnectionPoint& fromConn,
                                         const RoomPrefab& toPrefab, const ConnectionPoint& toConn,
                                         f32 rotation) const;
    std::vector<const RoomPrefab*> GetValidPrefabs(const ConnectionPoint& conn, u32 depth) const;
    const RoomPrefab* SelectPrefab(const std::vector<const RoomPrefab*>& valid) const;
    ConnectionPoint TransformConnection(const ConnectionPoint& conn, f32 rotation, bool mirror) const;

    // Random helpers
    void InitRandom(u32 seed);
    f32 RandomFloat() const;
    u32 RandomUint(u32 max) const;
    bool RandomChance(f32 probability) const;

    std::vector<RoomPrefab> m_Prefabs;
    std::vector<PlacedRoom> m_PlacedRooms;
    std::unordered_map<std::string, u32> m_PrefabUseCounts;

    LevelGenSettings m_Settings;
    GenerationStats m_Stats;

    RoomPlacedCallback m_OnRoomPlaced;
    GenerationCompleteCallback m_OnComplete;

    mutable u32 m_RandomState = 12345;  // Simple LCG state

    u32 m_StartRoomIdx = UINT32_MAX;
    u32 m_EndRoomIdx = UINT32_MAX;
    u32 m_NextInstanceId = 0;
};

// ============================================================================
// 2D Level Generator (Specialized for platformers/top-down)
// ============================================================================

struct Room2D {
    std::string id;
    Math::Vector2 size;                 // Width x Height in tiles
    std::vector<std::vector<u8>> tiles; // Tile data (0 = empty, 1+ = tile types)

    // Connection points (in tile coordinates)
    struct Connection2D {
        std::string id;
        Math::Vector2 position;         // Tile position
        ConnectionDirection direction;
        u32 width = 1;                  // Width in tiles
        std::string type = "default";
    };
    std::vector<Connection2D> connections;

    // Properties
    std::string category;
    f32 weight = 1.0f;
    std::vector<std::string> tags;
};

struct LevelGen2DSettings {
    u32 minRooms = 5;
    u32 maxRooms = 15;
    u32 tileSize = 16;                  // Pixels per tile (for positioning)

    bool horizontalBias = true;         // Prefer horizontal connections
    f32 verticalChance = 0.3f;          // Chance of vertical connection

    u32 seed = 0;
};

class ENJIN_API LevelGenerator2D {
public:
    void AddRoom(const Room2D& room);
    void ClearRooms();

    bool Generate(const LevelGen2DSettings& settings);
    void Clear();

    // Get combined tilemap of entire level
    struct GeneratedLevel {
        std::vector<std::vector<u8>> tiles;
        Math::Vector2 size;             // Total size in tiles
        Math::Vector2 startPosition;    // Start room position
        Math::Vector2 endPosition;      // End room position
        std::vector<Math::Vector2> roomPositions;
    };
    const GeneratedLevel& GetLevel() const { return m_GeneratedLevel; }

    // Get as tilemap component data
    void GetTilemapData(std::vector<u32>& outTiles, u32& outWidth, u32& outHeight) const;

private:
    std::vector<Room2D> m_Rooms;
    GeneratedLevel m_GeneratedLevel;
};

// ============================================================================
// Prefab Creator Helpers
// ============================================================================

namespace PrefabHelpers {
    // Create a simple rectangular room prefab
    ENJIN_API RoomPrefab CreateRectangularRoom(
        const std::string& id,
        const Math::Vector3& size,
        bool northDoor = true,
        bool southDoor = true,
        bool eastDoor = false,
        bool westDoor = false
    );

    // Create an L-shaped room
    ENJIN_API RoomPrefab CreateLShapedRoom(
        const std::string& id,
        f32 width, f32 depth, f32 height,
        ConnectionDirection cornerDirection
    );

    // Create a corridor prefab
    ENJIN_API RoomPrefab CreateCorridor(
        const std::string& id,
        f32 length, f32 width, f32 height,
        bool horizontal = true
    );

    // Create a T-junction
    ENJIN_API RoomPrefab CreateTJunction(
        const std::string& id,
        f32 size, f32 height
    );

    // Create a 4-way intersection
    ENJIN_API RoomPrefab CreateCrossroads(
        const std::string& id,
        f32 size, f32 height
    );

    // 2D prefab helpers
    ENJIN_API Room2D CreateRoom2D(
        const std::string& id,
        u32 width, u32 height,
        bool leftDoor = true,
        bool rightDoor = true,
        bool topDoor = false,
        bool bottomDoor = false
    );

    ENJIN_API Room2D CreatePlatform2D(
        const std::string& id,
        u32 width,
        bool leftConnection = true,
        bool rightConnection = true
    );
}

} // namespace Procedural
} // namespace Enjin
