#include "Enjin/AI/Navmesh.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Enjin {
namespace AI {

// ============================================================================
// Navmesh Implementation
// ============================================================================

u32 Navmesh::AddPolygon(const NavPolygon& polygon) {
    NavPolygon poly = polygon;
    poly.id = m_NextId++;

    // Calculate center and area
    poly.center = NavmeshUtils::CalculatePolygonCenter(poly.vertices);
    poly.area = NavmeshUtils::CalculatePolygonArea(poly.vertices);

    m_PolygonIndex[poly.id] = m_Polygons.size();
    m_Polygons.push_back(poly);

    UpdateBounds();
    return poly.id;
}

void Navmesh::RemovePolygon(u32 id) {
    auto it = m_PolygonIndex.find(id);
    if (it != m_PolygonIndex.end()) {
        usize index = it->second;
        m_Polygons.erase(m_Polygons.begin() + index);
        m_PolygonIndex.erase(it);

        // Update indices
        for (auto& [key, idx] : m_PolygonIndex) {
            if (idx > index) idx--;
        }

        UpdateBounds();
    }
}

NavPolygon* Navmesh::GetPolygon(u32 id) {
    auto it = m_PolygonIndex.find(id);
    if (it != m_PolygonIndex.end()) {
        return &m_Polygons[it->second];
    }
    return nullptr;
}

const NavPolygon* Navmesh::GetPolygon(u32 id) const {
    auto it = m_PolygonIndex.find(id);
    if (it != m_PolygonIndex.end()) {
        return &m_Polygons[it->second];
    }
    return nullptr;
}

u32 Navmesh::FindPolygonAtPoint(const Math::Vector3& point) const {
    for (const auto& poly : m_Polygons) {
        if (poly.walkable && PointInPolygon(point, poly)) {
            return poly.id;
        }
    }
    return 0;
}

u32 Navmesh::FindNearestPolygon(const Math::Vector3& point, f32 maxDistance) const {
    u32 nearestId = 0;
    f32 nearestDist = maxDistance * maxDistance;

    for (const auto& poly : m_Polygons) {
        if (!poly.walkable) continue;

        f32 dist = DistanceToPolygon(point, poly);
        if (dist < nearestDist) {
            nearestDist = dist;
            nearestId = poly.id;
        }
    }

    return nearestId;
}

Math::Vector3 Navmesh::GetNearestPoint(const Math::Vector3& point) const {
    u32 polyId = FindNearestPolygon(point);
    if (polyId == 0) return point;

    const NavPolygon* poly = GetPolygon(polyId);
    if (!poly) return point;

    // Project point onto polygon plane and clamp to edges
    // Simplified: return center for now
    // TODO: Proper projection
    return poly->center;
}

bool Navmesh::IsPointOnNavmesh(const Math::Vector3& point) const {
    return FindPolygonAtPoint(point) != 0;
}

void Navmesh::BuildConnectivity() {
    // Clear existing connectivity
    for (auto& poly : m_Polygons) {
        poly.neighbors.clear();
        poly.edges.clear();
    }

    // Find shared edges between polygons
    for (usize i = 0; i < m_Polygons.size(); ++i) {
        for (usize j = i + 1; j < m_Polygons.size(); ++j) {
            NavPolygon& polyA = m_Polygons[i];
            NavPolygon& polyB = m_Polygons[j];

            // Check each edge of polyA against each edge of polyB
            for (u32 ai = 0; ai < polyA.vertices.size(); ++ai) {
                u32 ai2 = (ai + 1) % polyA.vertices.size();
                const Math::Vector3& a1 = polyA.vertices[ai];
                const Math::Vector3& a2 = polyA.vertices[ai2];

                for (u32 bi = 0; bi < polyB.vertices.size(); ++bi) {
                    u32 bi2 = (bi + 1) % polyB.vertices.size();
                    const Math::Vector3& b1 = polyB.vertices[bi];
                    const Math::Vector3& b2 = polyB.vertices[bi2];

                    // Check if edges are shared (reversed direction)
                    f32 epsilon = 0.01f;
                    if ((NavmeshUtils::DistanceSquared(a1, b2) < epsilon &&
                         NavmeshUtils::DistanceSquared(a2, b1) < epsilon) ||
                        (NavmeshUtils::DistanceSquared(a1, b1) < epsilon &&
                         NavmeshUtils::DistanceSquared(a2, b2) < epsilon)) {

                        polyA.neighbors.push_back(polyB.id);
                        polyA.edges.push_back({ai, ai2});

                        polyB.neighbors.push_back(polyA.id);
                        polyB.edges.push_back({bi, bi2});
                        break;
                    }
                }
            }
        }
    }

    ENJIN_LOG_INFO(AI, "Built navmesh connectivity: %zu polygons", m_Polygons.size());
}

void Navmesh::Clear() {
    m_Polygons.clear();
    m_PolygonIndex.clear();
    m_NextId = 1;
}

bool Navmesh::Raycast(const Math::Vector3& start, const Math::Vector3& end,
                     Math::Vector3& hitPoint, u32& hitPolygonId) const {
    // Simple raycast - check intersection with each polygon
    // TODO: Use spatial acceleration structure

    f32 closestT = std::numeric_limits<f32>::max();
    bool hit = false;

    Math::Vector3 dir = end - start;
    f32 length = Math::Sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (length < 0.0001f) return false;
    dir = dir * (1.0f / length);

    for (const auto& poly : m_Polygons) {
        // Check ray-polygon intersection
        // Simplified: just check if ray crosses the polygon's bounding area
        // TODO: Proper ray-polygon intersection
    }

    if (hit) {
        hitPoint = start + dir * closestT;
    }

    return hit;
}

void Navmesh::UpdateBounds() {
    if (m_Polygons.empty()) {
        m_BoundsMin = Math::Vector3(0, 0, 0);
        m_BoundsMax = Math::Vector3(0, 0, 0);
        return;
    }

    m_BoundsMin = Math::Vector3(std::numeric_limits<f32>::max(),
                                std::numeric_limits<f32>::max(),
                                std::numeric_limits<f32>::max());
    m_BoundsMax = Math::Vector3(std::numeric_limits<f32>::lowest(),
                                std::numeric_limits<f32>::lowest(),
                                std::numeric_limits<f32>::lowest());

    for (const auto& poly : m_Polygons) {
        for (const auto& v : poly.vertices) {
            m_BoundsMin.x = Math::Min(m_BoundsMin.x, v.x);
            m_BoundsMin.y = Math::Min(m_BoundsMin.y, v.y);
            m_BoundsMin.z = Math::Min(m_BoundsMin.z, v.z);
            m_BoundsMax.x = Math::Max(m_BoundsMax.x, v.x);
            m_BoundsMax.y = Math::Max(m_BoundsMax.y, v.y);
            m_BoundsMax.z = Math::Max(m_BoundsMax.z, v.z);
        }
    }
}

bool Navmesh::PointInPolygon(const Math::Vector3& point, const NavPolygon& polygon) const {
    // 2D point-in-polygon test (XZ plane)
    usize n = polygon.vertices.size();
    if (n < 3) return false;

    bool inside = false;
    for (usize i = 0, j = n - 1; i < n; j = i++) {
        f32 xi = polygon.vertices[i].x, zi = polygon.vertices[i].z;
        f32 xj = polygon.vertices[j].x, zj = polygon.vertices[j].z;

        if (((zi > point.z) != (zj > point.z)) &&
            (point.x < (xj - xi) * (point.z - zi) / (zj - zi) + xi)) {
            inside = !inside;
        }
    }

    return inside;
}

f32 Navmesh::DistanceToPolygon(const Math::Vector3& point, const NavPolygon& polygon) const {
    // Distance to center (simplified)
    return NavmeshUtils::DistanceSquared(point, polygon.center);
}

// ============================================================================
// Pathfinder Implementation
// ============================================================================

Pathfinder::Pathfinder(const Navmesh* navmesh)
    : m_Navmesh(navmesh) {
}

PathResult Pathfinder::FindPath(const Math::Vector3& start, const Math::Vector3& end) {
    return FindPath(start, end, [this](const Math::Vector3& a, const Math::Vector3& b) {
        return DefaultHeuristic(a, b);
    });
}

PathResult Pathfinder::FindPath(const Math::Vector3& start, const Math::Vector3& end,
                                HeuristicFunc heuristic) {
    PathResult result;
    result.success = false;

    if (!m_Navmesh) {
        ENJIN_LOG_WARN(AI, "Pathfinder: No navmesh set");
        return result;
    }

    // Find start and end polygons
    u32 startPolyId = m_Navmesh->FindPolygonAtPoint(start);
    u32 endPolyId = m_Navmesh->FindPolygonAtPoint(end);

    if (startPolyId == 0) {
        startPolyId = m_Navmesh->FindNearestPolygon(start);
    }
    if (endPolyId == 0) {
        endPolyId = m_Navmesh->FindNearestPolygon(end);
    }

    if (startPolyId == 0 || endPolyId == 0) {
        ENJIN_LOG_WARN(AI, "Pathfinder: Start or end point not on navmesh");
        return result;
    }

    // Same polygon - direct path
    if (startPolyId == endPolyId) {
        result.success = true;
        result.waypoints.push_back(start);
        result.waypoints.push_back(end);
        result.totalCost = heuristic(start, end);
        result.polygonPath.push_back(startPolyId);
        return result;
    }

    // A* algorithm
    std::priority_queue<PathNode, std::vector<PathNode>, std::greater<PathNode>> openSet;
    std::unordered_map<u32, PathNode> allNodes;
    std::unordered_map<u32, bool> closedSet;

    const NavPolygon* endPoly = m_Navmesh->GetPolygon(endPolyId);

    // Start node
    PathNode startNode;
    startNode.polygonId = startPolyId;
    startNode.gCost = 0;
    startNode.hCost = heuristic(start, endPoly->center);
    startNode.parentId = 0;
    startNode.position = start;

    openSet.push(startNode);
    allNodes[startPolyId] = startNode;

    u32 iterations = 0;
    while (!openSet.empty() && iterations < m_MaxIterations) {
        iterations++;
        result.nodesExplored++;

        PathNode current = openSet.top();
        openSet.pop();

        // Skip if already processed
        if (closedSet[current.polygonId]) continue;
        closedSet[current.polygonId] = true;

        // Found the end
        if (current.polygonId == endPolyId) {
            return ReconstructPath(allNodes, endPolyId, start, end);
        }

        // Explore neighbors
        const NavPolygon* currentPoly = m_Navmesh->GetPolygon(current.polygonId);
        if (!currentPoly) continue;

        for (u32 neighborId : currentPoly->neighbors) {
            if (closedSet[neighborId]) continue;

            const NavPolygon* neighborPoly = m_Navmesh->GetPolygon(neighborId);
            if (!neighborPoly || !neighborPoly->walkable) continue;

            f32 moveCost = heuristic(currentPoly->center, neighborPoly->center);
            f32 areaCost = GetAreaCost(neighborPoly->areaType) * neighborPoly->cost;
            f32 newGCost = current.gCost + moveCost * areaCost;

            auto it = allNodes.find(neighborId);
            if (it == allNodes.end() || newGCost < it->second.gCost) {
                PathNode neighbor;
                neighbor.polygonId = neighborId;
                neighbor.gCost = newGCost;
                neighbor.hCost = heuristic(neighborPoly->center, endPoly->center);
                neighbor.parentId = current.polygonId;
                neighbor.position = neighborPoly->center;

                allNodes[neighborId] = neighbor;
                openSet.push(neighbor);
            }
        }
    }

    ENJIN_LOG_WARN(AI, "Pathfinder: No path found after %u iterations", iterations);
    return result;
}

bool Pathfinder::PathExists(const Math::Vector3& start, const Math::Vector3& end) {
    PathResult result = FindPath(start, end);
    return result.success;
}

PathResult Pathfinder::FindPartialPath(const Math::Vector3& start, const Math::Vector3& end,
                                       u32 maxNodes) {
    u32 originalMax = m_MaxIterations;
    m_MaxIterations = maxNodes;
    PathResult result = FindPath(start, end);
    m_MaxIterations = originalMax;
    return result;
}

void Pathfinder::SetAreaCost(u32 areaType, f32 cost) {
    m_AreaCosts[areaType] = cost;
}

f32 Pathfinder::GetAreaCost(u32 areaType) const {
    auto it = m_AreaCosts.find(areaType);
    return (it != m_AreaCosts.end()) ? it->second : 1.0f;
}

PathResult Pathfinder::ReconstructPath(const std::unordered_map<u32, PathNode>& nodes,
                                       u32 endId, const Math::Vector3& start,
                                       const Math::Vector3& end) {
    PathResult result;
    result.success = true;

    // Build polygon path backwards
    std::vector<u32> polyPath;
    u32 currentId = endId;
    while (currentId != 0) {
        polyPath.push_back(currentId);
        auto it = nodes.find(currentId);
        if (it == nodes.end()) break;
        currentId = it->second.parentId;
    }

    std::reverse(polyPath.begin(), polyPath.end());
    result.polygonPath = polyPath;

    // Build waypoints
    result.waypoints.push_back(start);
    for (usize i = 1; i < polyPath.size() - 1; ++i) {
        const NavPolygon* poly = m_Navmesh->GetPolygon(polyPath[i]);
        if (poly) {
            result.waypoints.push_back(poly->center);
        }
    }
    result.waypoints.push_back(end);

    // Calculate total cost
    auto endIt = nodes.find(endId);
    if (endIt != nodes.end()) {
        result.totalCost = endIt->second.gCost;
    }

    // Post-processing
    if (m_SmoothPath) {
        SmoothPath(result);
    }
    if (m_StringPulling) {
        StringPull(result);
    }

    return result;
}

void Pathfinder::SmoothPath(PathResult& path) {
    // Simple path smoothing - average adjacent waypoints
    if (path.waypoints.size() < 3) return;

    std::vector<Math::Vector3> smoothed;
    smoothed.push_back(path.waypoints.front());

    for (usize i = 1; i < path.waypoints.size() - 1; ++i) {
        Math::Vector3 avg = (path.waypoints[i - 1] +
                            path.waypoints[i] * 2.0f +
                            path.waypoints[i + 1]) * 0.25f;
        smoothed.push_back(avg);
    }

    smoothed.push_back(path.waypoints.back());
    path.waypoints = smoothed;
}

void Pathfinder::StringPull(PathResult& path) {
    // Simple string pulling - remove unnecessary waypoints
    if (path.waypoints.size() < 3) return;

    std::vector<Math::Vector3> pulled;
    pulled.push_back(path.waypoints.front());

    usize current = 0;
    while (current < path.waypoints.size() - 1) {
        usize farthest = current + 1;

        // Find farthest visible waypoint
        for (usize i = current + 2; i < path.waypoints.size(); ++i) {
            // TODO: Proper visibility check using raycast
            // For now, just check if distance is reasonable
            f32 dist = NavmeshUtils::Distance(path.waypoints[current], path.waypoints[i]);
            if (dist < 20.0f) {
                farthest = i;
            }
        }

        pulled.push_back(path.waypoints[farthest]);
        current = farthest;
    }

    path.waypoints = pulled;
}

f32 Pathfinder::DefaultHeuristic(const Math::Vector3& a, const Math::Vector3& b) const {
    return NavmeshUtils::Distance(a, b);
}

// ============================================================================
// NavmeshGenerator Implementation
// ============================================================================

bool NavmeshGenerator::Generate(const std::vector<Math::Vector3>& vertices,
                               const std::vector<u32>& indices,
                               const NavmeshGenSettings& settings) {
    m_Navmesh.Clear();

    // Simple implementation: create polygons from triangles
    // A proper implementation would use Recast/Detour algorithms

    if (indices.size() % 3 != 0) {
        ENJIN_LOG_ERROR(AI, "NavmeshGenerator: Invalid index count");
        return false;
    }

    for (usize i = 0; i < indices.size(); i += 3) {
        NavPolygon poly;
        poly.vertices.push_back(vertices[indices[i]]);
        poly.vertices.push_back(vertices[indices[i + 1]]);
        poly.vertices.push_back(vertices[indices[i + 2]]);

        // Check slope
        Math::Vector3 v0 = poly.vertices[1] - poly.vertices[0];
        Math::Vector3 v1 = poly.vertices[2] - poly.vertices[0];
        Math::Vector3 normal = v0.Cross(v1);
        f32 len = Math::Sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (len > 0.0001f) {
            normal = normal * (1.0f / len);
            f32 slope = Math::Acos(Math::Abs(normal.y)) * 180.0f / 3.14159f;
            poly.walkable = (slope <= settings.agentMaxSlope);
        }

        m_Navmesh.AddPolygon(poly);
    }

    m_Navmesh.BuildConnectivity();

    ENJIN_LOG_INFO(AI, "Generated navmesh with %u polygons", m_Navmesh.GetPolygons().size());
    return true;
}

bool NavmeshGenerator::GenerateFromHeightmap(const std::vector<f32>& heights,
                                            u32 width, u32 height,
                                            f32 cellSize, f32 heightScale,
                                            const NavmeshGenSettings& settings) {
    m_Navmesh.Clear();

    if (heights.size() != static_cast<usize>(width) * height) {
        ENJIN_LOG_ERROR(AI, "NavmeshGenerator: Invalid heightmap size");
        return false;
    }

    // Generate triangles from heightmap
    std::vector<Math::Vector3> vertices;
    std::vector<u32> indices;

    for (u32 z = 0; z < height; ++z) {
        for (u32 x = 0; x < width; ++x) {
            f32 h = heights[z * width + x] * heightScale;
            vertices.push_back(Math::Vector3(x * cellSize, h, z * cellSize));
        }
    }

    for (u32 z = 0; z < height - 1; ++z) {
        for (u32 x = 0; x < width - 1; ++x) {
            u32 i00 = z * width + x;
            u32 i10 = z * width + x + 1;
            u32 i01 = (z + 1) * width + x;
            u32 i11 = (z + 1) * width + x + 1;

            // Two triangles per quad
            indices.push_back(i00);
            indices.push_back(i01);
            indices.push_back(i10);

            indices.push_back(i10);
            indices.push_back(i01);
            indices.push_back(i11);
        }
    }

    return Generate(vertices, indices, settings);
}

bool NavmeshGenerator::GenerateGrid(const Math::Vector3& min, const Math::Vector3& max,
                                   f32 cellSize, f32 height) {
    m_Navmesh.Clear();

    f32 width = max.x - min.x;
    f32 depth = max.z - min.z;
    u32 numX = static_cast<u32>(width / cellSize);
    u32 numZ = static_cast<u32>(depth / cellSize);

    for (u32 z = 0; z < numZ; ++z) {
        for (u32 x = 0; x < numX; ++x) {
            NavPolygon poly;
            f32 x0 = min.x + x * cellSize;
            f32 z0 = min.z + z * cellSize;
            f32 x1 = x0 + cellSize;
            f32 z1 = z0 + cellSize;

            poly.vertices.push_back(Math::Vector3(x0, height, z0));
            poly.vertices.push_back(Math::Vector3(x1, height, z0));
            poly.vertices.push_back(Math::Vector3(x1, height, z1));
            poly.vertices.push_back(Math::Vector3(x0, height, z1));

            m_Navmesh.AddPolygon(poly);
        }
    }

    m_Navmesh.BuildConnectivity();

    ENJIN_LOG_INFO(AI, "Generated grid navmesh: %ux%u cells", numX, numZ);
    return true;
}

// ============================================================================
// PathFollower Implementation
// ============================================================================

void PathFollower::Update(Math::Vector3& position, Math::Vector3& forward,
                         PathFollowerComponent& follower, f32 deltaTime) {
    if (!follower.isFollowing || follower.hasArrived) return;
    if (follower.currentWaypointIndex >= follower.currentPath.waypoints.size()) {
        follower.hasArrived = true;
        follower.isFollowing = false;
        if (follower.onPathComplete) follower.onPathComplete();
        return;
    }

    const Math::Vector3& target = follower.currentPath.waypoints[follower.currentWaypointIndex];
    Math::Vector3 toTarget = target - position;
    toTarget.y = 0; // Stay on plane

    f32 distance = Math::Sqrt(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

    // Reached waypoint?
    if (distance < follower.arrivalRadius) {
        if (follower.onWaypointReached) {
            follower.onWaypointReached(target);
        }

        follower.currentWaypointIndex++;
        if (follower.currentWaypointIndex >= follower.currentPath.waypoints.size()) {
            follower.hasArrived = true;
            follower.isFollowing = false;
            if (follower.onPathComplete) follower.onPathComplete();
        }
        return;
    }

    // Calculate speed (slow down near waypoint)
    f32 speed = follower.speed;
    if (distance < follower.slowdownRadius) {
        speed *= distance / follower.slowdownRadius;
    }

    // Move towards target
    Math::Vector3 direction = toTarget * (1.0f / distance);
    position = position + direction * speed * deltaTime;

    // Rotate towards target
    if (follower.smoothRotation) {
        f32 targetAngle = Math::Atan2(direction.x, direction.z);
        f32 currentAngle = Math::Atan2(forward.x, forward.z);

        f32 angleDiff = targetAngle - currentAngle;
        while (angleDiff > 3.14159f) angleDiff -= 6.28318f;
        while (angleDiff < -3.14159f) angleDiff += 6.28318f;

        f32 maxTurn = follower.turnSpeed * 3.14159f / 180.0f * deltaTime;
        if (Math::Abs(angleDiff) > maxTurn) {
            angleDiff = (angleDiff > 0) ? maxTurn : -maxTurn;
        }

        f32 newAngle = currentAngle + angleDiff;
        forward = Math::Vector3(Math::Sin(newAngle), 0, Math::Cos(newAngle));
    } else {
        forward = direction;
    }
}

void PathFollower::SetPath(PathFollowerComponent& follower, const PathResult& path) {
    follower.currentPath = path;
    follower.currentWaypointIndex = 0;
    follower.isFollowing = path.success && !path.waypoints.empty();
    follower.hasArrived = false;
}

void PathFollower::Stop(PathFollowerComponent& follower) {
    follower.isFollowing = false;
}

f32 PathFollower::GetRemainingDistance(const PathFollowerComponent& follower,
                                       const Math::Vector3& currentPosition) {
    if (!follower.isFollowing) return 0.0f;

    f32 total = 0.0f;
    Math::Vector3 pos = currentPosition;

    for (usize i = follower.currentWaypointIndex; i < follower.currentPath.waypoints.size(); ++i) {
        total += NavmeshUtils::Distance(pos, follower.currentPath.waypoints[i]);
        pos = follower.currentPath.waypoints[i];
    }

    return total;
}

bool PathFollower::IsNearWaypoint(const PathFollowerComponent& follower,
                                  const Math::Vector3& currentPosition) {
    if (follower.currentWaypointIndex >= follower.currentPath.waypoints.size()) {
        return false;
    }

    const Math::Vector3& target = follower.currentPath.waypoints[follower.currentWaypointIndex];
    f32 dist = NavmeshUtils::Distance(currentPosition, target);
    return dist < follower.arrivalRadius;
}

// ============================================================================
// NavmeshUtils
// ============================================================================

namespace NavmeshUtils {

f32 Distance(const Math::Vector3& a, const Math::Vector3& b) {
    f32 dx = b.x - a.x;
    f32 dy = b.y - a.y;
    f32 dz = b.z - a.z;
    return Math::Sqrt(dx * dx + dy * dy + dz * dz);
}

f32 DistanceSquared(const Math::Vector3& a, const Math::Vector3& b) {
    f32 dx = b.x - a.x;
    f32 dy = b.y - a.y;
    f32 dz = b.z - a.z;
    return dx * dx + dy * dy + dz * dz;
}

f32 ManhattanDistance(const Math::Vector3& a, const Math::Vector3& b) {
    return Math::Abs(b.x - a.x) + Math::Abs(b.y - a.y) + Math::Abs(b.z - a.z);
}

f32 CalculatePolygonArea(const std::vector<Math::Vector3>& vertices) {
    if (vertices.size() < 3) return 0.0f;

    // Shoelace formula on XZ plane
    f32 area = 0.0f;
    for (usize i = 0; i < vertices.size(); ++i) {
        usize j = (i + 1) % vertices.size();
        area += vertices[i].x * vertices[j].z;
        area -= vertices[j].x * vertices[i].z;
    }
    return Math::Abs(area) * 0.5f;
}

Math::Vector3 CalculatePolygonCenter(const std::vector<Math::Vector3>& vertices) {
    if (vertices.empty()) return Math::Vector3(0, 0, 0);

    Math::Vector3 sum(0, 0, 0);
    for (const auto& v : vertices) {
        sum = sum + v;
    }
    return sum * (1.0f / static_cast<f32>(vertices.size()));
}

bool IsPolygonConvex(const std::vector<Math::Vector3>& vertices) {
    if (vertices.size() < 3) return false;

    bool sign = false;
    bool hasSign = false;

    for (usize i = 0; i < vertices.size(); ++i) {
        usize j = (i + 1) % vertices.size();
        usize k = (i + 2) % vertices.size();

        f32 cross = (vertices[j].x - vertices[i].x) * (vertices[k].z - vertices[j].z) -
                   (vertices[j].z - vertices[i].z) * (vertices[k].x - vertices[j].x);

        if (cross != 0) {
            if (!hasSign) {
                sign = (cross > 0);
                hasSign = true;
            } else if ((cross > 0) != sign) {
                return false;
            }
        }
    }

    return true;
}

std::vector<u32> TriangulateConvex(const std::vector<Math::Vector3>& vertices) {
    std::vector<u32> indices;
    if (vertices.size() < 3) return indices;

    // Fan triangulation
    for (u32 i = 1; i < vertices.size() - 1; ++i) {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    return indices;
}

} // namespace NavmeshUtils

} // namespace AI
} // namespace Enjin
