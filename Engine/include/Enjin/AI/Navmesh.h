#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <functional>

namespace Enjin {
namespace AI {

// ============================================================================
// Navmesh Polygon
// ============================================================================

/**
 * @brief A convex polygon in the navigation mesh
 */
struct NavPolygon {
    u32 id = 0;
    std::vector<Math::Vector3> vertices;
    Math::Vector3 center;
    f32 area = 0.0f;

    // Connectivity
    std::vector<u32> neighbors;           // Adjacent polygon IDs
    std::vector<std::pair<u32, u32>> edges; // Edge indices for each neighbor

    // Flags
    bool walkable = true;
    u32 areaType = 0;                     // Custom area type for costs
    f32 cost = 1.0f;                      // Traversal cost multiplier
};

/**
 * @brief A node in the A* pathfinding algorithm
 */
struct PathNode {
    u32 polygonId;
    f32 gCost;    // Cost from start
    f32 hCost;    // Heuristic cost to end
    f32 fCost() const { return gCost + hCost; }
    u32 parentId;
    Math::Vector3 position;

    bool operator>(const PathNode& other) const {
        return fCost() > other.fCost();
    }
};

// ============================================================================
// Navmesh
// ============================================================================

/**
 * @brief Navigation mesh for pathfinding
 */
class ENJIN_API Navmesh {
public:
    Navmesh() = default;
    ~Navmesh() = default;

    /// Add a polygon to the navmesh
    u32 AddPolygon(const NavPolygon& polygon);

    /// Remove a polygon
    void RemovePolygon(u32 id);

    /// Get polygon by ID
    NavPolygon* GetPolygon(u32 id);
    const NavPolygon* GetPolygon(u32 id) const;

    /// Get all polygons
    const std::vector<NavPolygon>& GetPolygons() const { return m_Polygons; }

    /// Find polygon containing point
    u32 FindPolygonAtPoint(const Math::Vector3& point) const;

    /// Find nearest polygon to point
    u32 FindNearestPolygon(const Math::Vector3& point, f32 maxDistance = 100.0f) const;

    /// Get nearest point on navmesh
    Math::Vector3 GetNearestPoint(const Math::Vector3& point) const;

    /// Check if point is on navmesh
    bool IsPointOnNavmesh(const Math::Vector3& point) const;

    /// Connect adjacent polygons
    void BuildConnectivity();

    /// Clear the navmesh
    void Clear();

    /// Get bounds
    Math::Vector3 GetMin() const { return m_BoundsMin; }
    Math::Vector3 GetMax() const { return m_BoundsMax; }

    /// Raycast against navmesh
    bool Raycast(const Math::Vector3& start, const Math::Vector3& end,
                Math::Vector3& hitPoint, u32& hitPolygonId) const;

private:
    void UpdateBounds();
    bool PointInPolygon(const Math::Vector3& point, const NavPolygon& polygon) const;
    f32 DistanceToPolygon(const Math::Vector3& point, const NavPolygon& polygon) const;

    std::vector<NavPolygon> m_Polygons;
    std::unordered_map<u32, usize> m_PolygonIndex;
    u32 m_NextId = 1;

    Math::Vector3 m_BoundsMin;
    Math::Vector3 m_BoundsMax;
};

// ============================================================================
// Path Result
// ============================================================================

struct PathResult {
    bool success = false;
    std::vector<Math::Vector3> waypoints;
    f32 totalCost = 0.0f;
    u32 nodesExplored = 0;

    // Polygon path (for debugging)
    std::vector<u32> polygonPath;
};

// ============================================================================
// Pathfinder
// ============================================================================

/**
 * @brief A* pathfinder for navigation meshes
 */
class ENJIN_API Pathfinder {
public:
    Pathfinder() = default;
    Pathfinder(const Navmesh* navmesh);
    ~Pathfinder() = default;

    /// Set the navmesh to use
    void SetNavmesh(const Navmesh* navmesh) { m_Navmesh = navmesh; }

    /// Find path between two points
    PathResult FindPath(const Math::Vector3& start, const Math::Vector3& end);

    /// Find path with custom heuristic
    using HeuristicFunc = std::function<f32(const Math::Vector3&, const Math::Vector3&)>;
    PathResult FindPath(const Math::Vector3& start, const Math::Vector3& end,
                       HeuristicFunc heuristic);

    /// Check if path exists (faster than full pathfinding)
    bool PathExists(const Math::Vector3& start, const Math::Vector3& end);

    /// Get partial path (if full path not possible)
    PathResult FindPartialPath(const Math::Vector3& start, const Math::Vector3& end,
                              u32 maxNodes = 100);

    /// Settings
    void SetMaxIterations(u32 max) { m_MaxIterations = max; }
    void SetSmoothPath(bool smooth) { m_SmoothPath = smooth; }
    void SetStringPulling(bool enabled) { m_StringPulling = enabled; }

    /// Area costs
    void SetAreaCost(u32 areaType, f32 cost);
    f32 GetAreaCost(u32 areaType) const;

private:
    PathResult ReconstructPath(const std::unordered_map<u32, PathNode>& nodes,
                               u32 endId, const Math::Vector3& start,
                               const Math::Vector3& end);
    void SmoothPath(PathResult& path);
    void StringPull(PathResult& path);
    f32 DefaultHeuristic(const Math::Vector3& a, const Math::Vector3& b) const;

    const Navmesh* m_Navmesh = nullptr;
    u32 m_MaxIterations = 10000;
    bool m_SmoothPath = true;
    bool m_StringPulling = true;
    std::unordered_map<u32, f32> m_AreaCosts;
};

// ============================================================================
// Navmesh Generator
// ============================================================================

struct NavmeshGenSettings {
    f32 cellSize = 0.3f;          // Voxel size for rasterization
    f32 cellHeight = 0.2f;        // Voxel height
    f32 agentHeight = 2.0f;       // Agent height
    f32 agentRadius = 0.6f;       // Agent radius
    f32 agentMaxClimb = 0.9f;     // Max step height
    f32 agentMaxSlope = 45.0f;    // Max walkable slope (degrees)
    f32 regionMinSize = 8.0f;     // Min region area
    f32 regionMergeSize = 20.0f;  // Merge regions smaller than this
    f32 edgeMaxLen = 12.0f;       // Max edge length
    f32 edgeMaxError = 1.3f;      // Max edge error
    u32 vertsPerPoly = 6;         // Max vertices per polygon
    f32 detailSampleDist = 6.0f;  // Detail mesh sample distance
    f32 detailSampleMaxError = 1.0f;
};

/**
 * @brief Simple navmesh generator from geometry
 */
class ENJIN_API NavmeshGenerator {
public:
    NavmeshGenerator() = default;
    ~NavmeshGenerator() = default;

    /// Generate navmesh from triangles
    bool Generate(const std::vector<Math::Vector3>& vertices,
                 const std::vector<u32>& indices,
                 const NavmeshGenSettings& settings = NavmeshGenSettings{});

    /// Generate navmesh from heightmap
    bool GenerateFromHeightmap(const std::vector<f32>& heights,
                              u32 width, u32 height,
                              f32 cellSize, f32 heightScale,
                              const NavmeshGenSettings& settings = NavmeshGenSettings{});

    /// Generate simple grid navmesh
    bool GenerateGrid(const Math::Vector3& min, const Math::Vector3& max,
                     f32 cellSize, f32 height = 0.0f);

    /// Get the generated navmesh
    const Navmesh& GetNavmesh() const { return m_Navmesh; }
    Navmesh& GetNavmesh() { return m_Navmesh; }

    /// Take ownership of the navmesh
    Navmesh TakeNavmesh() { return std::move(m_Navmesh); }

private:
    Navmesh m_Navmesh;
};

// ============================================================================
// Path Follower
// ============================================================================

/**
 * @brief Component for following a path
 */
struct PathFollowerComponent {
    PathResult currentPath;
    u32 currentWaypointIndex = 0;
    f32 speed = 5.0f;
    f32 turnSpeed = 180.0f;     // Degrees per second
    f32 arrivalRadius = 0.5f;   // Stop when this close to waypoint
    f32 slowdownRadius = 2.0f;  // Start slowing down
    bool isFollowing = false;
    bool hasArrived = false;
    bool smoothRotation = true;

    // Callbacks
    std::function<void()> onPathComplete;
    std::function<void(const Math::Vector3&)> onWaypointReached;
};

/**
 * @brief Path following helper
 */
class ENJIN_API PathFollower {
public:
    /// Update path following for an entity
    static void Update(Math::Vector3& position, Math::Vector3& forward,
                      PathFollowerComponent& follower, f32 deltaTime);

    /// Set new path to follow
    static void SetPath(PathFollowerComponent& follower, const PathResult& path);

    /// Stop following current path
    static void Stop(PathFollowerComponent& follower);

    /// Get distance to end of path
    static f32 GetRemainingDistance(const PathFollowerComponent& follower,
                                    const Math::Vector3& currentPosition);

    /// Check if near current waypoint
    static bool IsNearWaypoint(const PathFollowerComponent& follower,
                              const Math::Vector3& currentPosition);
};

// ============================================================================
// Navmesh Utilities
// ============================================================================

namespace NavmeshUtils {
    /// Calculate distance between two points (Euclidean)
    ENJIN_API f32 Distance(const Math::Vector3& a, const Math::Vector3& b);

    /// Calculate squared distance (faster, for comparisons)
    ENJIN_API f32 DistanceSquared(const Math::Vector3& a, const Math::Vector3& b);

    /// Calculate Manhattan distance
    ENJIN_API f32 ManhattanDistance(const Math::Vector3& a, const Math::Vector3& b);

    /// Calculate polygon area
    ENJIN_API f32 CalculatePolygonArea(const std::vector<Math::Vector3>& vertices);

    /// Calculate polygon center
    ENJIN_API Math::Vector3 CalculatePolygonCenter(const std::vector<Math::Vector3>& vertices);

    /// Check if polygon is convex
    ENJIN_API bool IsPolygonConvex(const std::vector<Math::Vector3>& vertices);

    /// Triangulate a convex polygon
    ENJIN_API std::vector<u32> TriangulateConvex(const std::vector<Math::Vector3>& vertices);
}

} // namespace AI
} // namespace Enjin
