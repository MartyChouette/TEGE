#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include "Enjin/Procedural/LevelGenerator.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Math/Noise.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stack>

namespace Enjin {
namespace Procedural {

// ============================================================================
// Simple LCG random number generator
// ============================================================================

struct LCGRandom {
    mutable u32 state;

    explicit LCGRandom(u32 seed) : state(seed == 0 ? 12345u : seed) {}

    u32 Next() const {
        state = state * 1103515245u + 12345u;
        return (state >> 16) & 0x7FFF;
    }

    // Random float in [0, 1)
    f32 Float() const {
        return static_cast<f32>(Next()) / 32768.0f;
    }

    // Random u32 in [0, max)
    u32 Uint(u32 max) const {
        if (max == 0) return 0;
        return Next() % max;
    }

    // Random i32 in [min, max]
    i32 Int(i32 min, i32 max) const {
        if (max <= min) return min;
        return min + static_cast<i32>(Uint(static_cast<u32>(max - min + 1)));
    }

    // Random float in [min, max]
    f32 FloatRange(f32 min, f32 max) const {
        return min + Float() * (max - min);
    }

    bool Chance(f32 probability) const {
        return Float() < probability;
    }
};

// ============================================================================
// CellularAutomata
// ============================================================================

CellularAutomata::Result CellularAutomata::Generate(const Params& params) {
    Result result;
    result.width = params.width;
    result.height = params.height;

    if (params.width == 0 || params.height == 0) {
        return result;
    }

    LCGRandom rng(params.seed);

    // Initialize grid randomly based on fillPercent
    result.grid.resize(params.height);
    for (u32 y = 0; y < params.height; ++y) {
        result.grid[y].resize(params.width, 0);
        for (u32 x = 0; x < params.width; ++x) {
            // Border cells are always walls
            if (x == 0 || x == params.width - 1 || y == 0 || y == params.height - 1) {
                result.grid[y][x] = 1;
            } else {
                result.grid[y][x] = (rng.Uint(100) < params.fillPercent) ? 1 : 0;
            }
        }
    }

    // Temporary buffer for double-buffered iteration
    std::vector<std::vector<u8>> buffer(params.height, std::vector<u8>(params.width, 0));

    // Run cellular automata iterations
    for (u32 iter = 0; iter < params.iterations; ++iter) {
        for (u32 y = 0; y < params.height; ++y) {
            for (u32 x = 0; x < params.width; ++x) {
                // Border cells stay as walls
                if (x == 0 || x == params.width - 1 || y == 0 || y == params.height - 1) {
                    buffer[y][x] = 1;
                    continue;
                }

                // Count alive neighbors in 8-direction Moore neighborhood
                u32 neighbors = 0;
                for (i32 dy = -1; dy <= 1; ++dy) {
                    for (i32 dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        i32 nx = static_cast<i32>(x) + dx;
                        i32 ny = static_cast<i32>(y) + dy;
                        if (nx >= 0 && nx < static_cast<i32>(params.width) &&
                            ny >= 0 && ny < static_cast<i32>(params.height)) {
                            neighbors += result.grid[ny][nx];
                        } else {
                            // Out of bounds counts as wall
                            neighbors++;
                        }
                    }
                }

                // Apply birth/death rules
                if (result.grid[y][x] == 1) {
                    // Cell is alive: it stays alive if enough neighbors, otherwise dies
                    buffer[y][x] = (neighbors >= params.deathLimit) ? 1 : 0;
                } else {
                    // Cell is dead: it comes alive if enough neighbors
                    buffer[y][x] = (neighbors >= params.birthLimit) ? 1 : 0;
                }
            }
        }

        // Swap buffer into result grid
        std::swap(result.grid, buffer);
    }

    return result;
}

// ============================================================================
// RandomWalker
// ============================================================================

RandomWalker::Result RandomWalker::Generate(const Params& params) {
    Result result;

    if (params.width == 0 || params.height == 0) {
        return result;
    }

    LCGRandom rng(params.seed);

    // Initialize grid to all walls (1)
    result.grid.resize(params.height, std::vector<u8>(params.width, 1));

    // Start at center
    i32 cx = static_cast<i32>(params.width / 2);
    i32 cy = static_cast<i32>(params.height / 2);
    result.startPos = Math::Vector2(static_cast<f32>(cx), static_cast<f32>(cy));

    // Mark starting position as floor
    result.grid[cy][cx] = 0;

    // Direction vectors: N, E, S, W
    const i32 dx[4] = { 0, 1, 0, -1 };
    const i32 dy[4] = { -1, 0, 1, 0 };

    i32 currentDir = static_cast<i32>(rng.Uint(4));

    for (u32 step = 0; step < params.steps; ++step) {
        // Decide whether to change direction
        if (rng.Chance(params.turnChance)) {
            // Pick new direction, possibly influenced by directionalBias
            f32 biasX = params.directionalBias.x;
            f32 biasY = params.directionalBias.y;

            // Weight each direction
            f32 weights[4];
            weights[0] = 1.0f + Math::Max(0.0f, -biasY);  // North (negative Y)
            weights[1] = 1.0f + Math::Max(0.0f,  biasX);   // East (positive X)
            weights[2] = 1.0f + Math::Max(0.0f,  biasY);   // South (positive Y)
            weights[3] = 1.0f + Math::Max(0.0f, -biasX);   // West (negative X)

            f32 total = weights[0] + weights[1] + weights[2] + weights[3];
            f32 roll = rng.Float() * total;
            f32 cumulative = 0.0f;
            for (i32 d = 0; d < 4; ++d) {
                cumulative += weights[d];
                if (roll < cumulative) {
                    currentDir = d;
                    break;
                }
            }
        }

        // Move in current direction
        i32 nx = cx + dx[currentDir];
        i32 ny = cy + dy[currentDir];

        // Clamp to grid bounds (leave 1-cell border)
        if (nx < 1) nx = 1;
        if (ny < 1) ny = 1;
        if (nx >= static_cast<i32>(params.width) - 1)  nx = static_cast<i32>(params.width) - 2;
        if (ny >= static_cast<i32>(params.height) - 1) ny = static_cast<i32>(params.height) - 2;

        cx = nx;
        cy = ny;
        result.grid[cy][cx] = 0;
    }

    result.endPos = Math::Vector2(static_cast<f32>(cx), static_cast<f32>(cy));
    return result;
}

// ============================================================================
// BSPGenerator
// ============================================================================

namespace {

struct BSPNode {
    u32 x, y, w, h;
    BSPNode* left  = nullptr;
    BSPNode* right = nullptr;
    BSPGenerator::Rect room;
    bool hasRoom = false;
};

void SplitBSP(BSPNode* node, u32 depth, u32 maxDepth, u32 minRoomSize, const LCGRandom& rng) {
    if (depth >= maxDepth) return;

    // Decide split direction: prefer splitting the longer dimension
    bool splitHorizontal;
    if (node->w > node->h) {
        splitHorizontal = false;  // Split vertically (along width)
    } else if (node->h > node->w) {
        splitHorizontal = true;   // Split horizontally (along height)
    } else {
        splitHorizontal = rng.Chance(0.5f);
    }

    u32 maxSplit, minSplit;
    if (splitHorizontal) {
        // Need room for two children of at least minRoomSize height
        if (node->h < minRoomSize * 2) return;
        minSplit = minRoomSize;
        maxSplit = node->h - minRoomSize;
    } else {
        if (node->w < minRoomSize * 2) return;
        minSplit = minRoomSize;
        maxSplit = node->w - minRoomSize;
    }

    if (maxSplit <= minSplit) return;

    u32 splitPos = minSplit + rng.Uint(maxSplit - minSplit + 1);

    node->left  = new BSPNode();
    node->right = new BSPNode();

    if (splitHorizontal) {
        node->left->x  = node->x;
        node->left->y  = node->y;
        node->left->w  = node->w;
        node->left->h  = splitPos;

        node->right->x = node->x;
        node->right->y = node->y + splitPos;
        node->right->w = node->w;
        node->right->h = node->h - splitPos;
    } else {
        node->left->x  = node->x;
        node->left->y  = node->y;
        node->left->w  = splitPos;
        node->left->h  = node->h;

        node->right->x = node->x + splitPos;
        node->right->y = node->y;
        node->right->w = node->w - splitPos;
        node->right->h = node->h;
    }

    SplitBSP(node->left, depth + 1, maxDepth, minRoomSize, rng);
    SplitBSP(node->right, depth + 1, maxDepth, minRoomSize, rng);
}

void PlaceBSPRooms(BSPNode* node, u32 minRoomSize, u32 maxRoomSize,
                    std::vector<BSPGenerator::Rect>& rooms,
                    std::vector<std::vector<u8>>& grid,
                    const LCGRandom& rng) {
    if (!node) return;

    // Leaf node: place a room
    if (!node->left && !node->right) {
        u32 capW = (maxRoomSize < node->w) ? maxRoomSize : node->w;
        u32 capH = (maxRoomSize < node->h) ? maxRoomSize : node->h;
        u32 roomW = minRoomSize + rng.Uint(capW - minRoomSize + 1);
        u32 roomH = minRoomSize + rng.Uint(capH - minRoomSize + 1);

        if (roomW > node->w) roomW = node->w;
        if (roomH > node->h) roomH = node->h;
        if (roomW < minRoomSize) roomW = minRoomSize;
        if (roomH < minRoomSize) roomH = minRoomSize;

        u32 roomX = node->x;
        u32 roomY = node->y;
        if (node->w > roomW) {
            roomX += rng.Uint(node->w - roomW);
        }
        if (node->h > roomH) {
            roomY += rng.Uint(node->h - roomH);
        }

        node->room = { roomX, roomY, roomW, roomH };
        node->hasRoom = true;
        rooms.push_back(node->room);

        // Carve room into grid
        u32 gridH = static_cast<u32>(grid.size());
        u32 gridW = gridH > 0 ? static_cast<u32>(grid[0].size()) : 0;
        for (u32 ry = roomY; ry < roomY + roomH && ry < gridH; ++ry) {
            for (u32 rx = roomX; rx < roomX + roomW && rx < gridW; ++rx) {
                grid[ry][rx] = 1;
            }
        }
        return;
    }

    PlaceBSPRooms(node->left, minRoomSize, maxRoomSize, rooms, grid, rng);
    PlaceBSPRooms(node->right, minRoomSize, maxRoomSize, rooms, grid, rng);
}

// Get a room from the deepest leaf of a subtree
BSPGenerator::Rect GetLeafRoom(BSPNode* node) {
    if (!node) return {0, 0, 0, 0};
    if (node->hasRoom) return node->room;
    if (node->left) {
        auto r = GetLeafRoom(node->left);
        if (r.w > 0) return r;
    }
    if (node->right) {
        auto r = GetLeafRoom(node->right);
        if (r.w > 0) return r;
    }
    return {0, 0, 0, 0};
}

void ConnectBSPRooms(BSPNode* node, u32 corridorWidth,
                     std::vector<std::vector<u8>>& grid) {
    if (!node || !node->left || !node->right) return;

    // Recursively connect children first
    ConnectBSPRooms(node->left, corridorWidth, grid);
    ConnectBSPRooms(node->right, corridorWidth, grid);

    // Connect a room from left subtree to a room from right subtree
    auto roomA = GetLeafRoom(node->left);
    auto roomB = GetLeafRoom(node->right);

    if (roomA.w == 0 || roomB.w == 0) return;

    // Center points of each room
    i32 ax = static_cast<i32>(roomA.x + roomA.w / 2);
    i32 ay = static_cast<i32>(roomA.y + roomA.h / 2);
    i32 bx = static_cast<i32>(roomB.x + roomB.w / 2);
    i32 by = static_cast<i32>(roomB.y + roomB.h / 2);

    u32 gridH = static_cast<u32>(grid.size());
    u32 gridW = gridH > 0 ? static_cast<u32>(grid[0].size()) : 0;
    u32 halfCorridor = corridorWidth / 2;
    if (halfCorridor == 0) halfCorridor = 1;

    // L-shaped corridor: horizontal first, then vertical
    // Horizontal segment from ax to bx at row ay
    i32 startX = (ax < bx) ? ax : bx;
    i32 endX   = (ax < bx) ? bx : ax;
    for (i32 x = startX; x <= endX; ++x) {
        for (u32 cw = 0; cw < corridorWidth; ++cw) {
            i32 cy = ay + static_cast<i32>(cw) - static_cast<i32>(halfCorridor);
            if (cy >= 0 && cy < static_cast<i32>(gridH) &&
                x >= 0 && x < static_cast<i32>(gridW)) {
                grid[cy][x] = 1;
            }
        }
    }

    // Vertical segment from ay to by at column bx
    i32 startY = (ay < by) ? ay : by;
    i32 endY   = (ay < by) ? by : ay;
    for (i32 y = startY; y <= endY; ++y) {
        for (u32 cw = 0; cw < corridorWidth; ++cw) {
            i32 cx = bx + static_cast<i32>(cw) - static_cast<i32>(halfCorridor);
            if (y >= 0 && y < static_cast<i32>(gridH) &&
                cx >= 0 && cx < static_cast<i32>(gridW)) {
                grid[y][cx] = 1;
            }
        }
    }
}

void FreeBSPTree(BSPNode* node) {
    if (!node) return;
    FreeBSPTree(node->left);
    FreeBSPTree(node->right);
    delete node;
}

} // anonymous namespace

BSPGenerator::Result BSPGenerator::Generate(const Params& params) {
    Result result;

    if (params.width == 0 || params.height == 0) {
        return result;
    }

    LCGRandom rng(params.seed);

    // Initialize grid to all empty (0)
    result.grid.resize(params.height, std::vector<u8>(params.width, 0));

    // Create root BSP node
    BSPNode* root = new BSPNode();
    root->x = 0;
    root->y = 0;
    root->w = params.width;
    root->h = params.height;

    // Recursively split
    SplitBSP(root, 0, params.splitDepth, params.minRoomSize, rng);

    // Place rooms in leaf nodes
    PlaceBSPRooms(root, params.minRoomSize, params.maxRoomSize, result.rooms, result.grid, rng);

    // Connect rooms with corridors
    ConnectBSPRooms(root, params.corridorWidth, result.grid);

    // Clean up tree
    FreeBSPTree(root);

    return result;
}

// ============================================================================
// DiamondSquare
// ============================================================================

DiamondSquare::Result DiamondSquare::Generate(const Params& params) {
    Result result;

    // Validate size is 2^n + 1
    u32 size = params.size;
    if (size < 3) size = 3;

    // Round up to nearest 2^n + 1
    u32 power = 1;
    while (power + 1 < size) {
        power *= 2;
    }
    size = power + 1;
    result.size = size;

    LCGRandom rng(params.seed);

    // Initialize heightmap
    result.heightmap.resize(size, std::vector<f32>(size, 0.0f));

    // Set corner values
    result.heightmap[0][0]               = params.minHeight + (params.maxHeight - params.minHeight) * 0.5f;
    result.heightmap[0][size - 1]        = params.minHeight + (params.maxHeight - params.minHeight) * 0.5f;
    result.heightmap[size - 1][0]        = params.minHeight + (params.maxHeight - params.minHeight) * 0.5f;
    result.heightmap[size - 1][size - 1] = params.minHeight + (params.maxHeight - params.minHeight) * 0.5f;

    f32 roughness = params.roughness;
    f32 range = params.maxHeight - params.minHeight;

    u32 stepSize = size - 1;

    while (stepSize > 1) {
        u32 halfStep = stepSize / 2;

        // Diamond step: set center of each square to average of corners + random offset
        for (u32 y = 0; y < size - 1; y += stepSize) {
            for (u32 x = 0; x < size - 1; x += stepSize) {
                f32 avg = (result.heightmap[y][x] +
                           result.heightmap[y][x + stepSize] +
                           result.heightmap[y + stepSize][x] +
                           result.heightmap[y + stepSize][x + stepSize]) * 0.25f;

                f32 offset = (rng.Float() * 2.0f - 1.0f) * roughness * range;
                result.heightmap[y + halfStep][x + halfStep] = avg + offset;
            }
        }

        // Square step: set edge midpoints to average of adjacent diamond centers + random
        for (u32 y = 0; y < size; y += halfStep) {
            // Offset x start for alternating rows
            u32 xStart = ((y / halfStep) % 2 == 0) ? halfStep : 0;
            for (u32 x = xStart; x < size; x += stepSize) {
                f32 sum = 0.0f;
                u32 count = 0;

                // Up
                if (y >= halfStep) {
                    sum += result.heightmap[y - halfStep][x];
                    count++;
                }
                // Down
                if (y + halfStep < size) {
                    sum += result.heightmap[y + halfStep][x];
                    count++;
                }
                // Left
                if (x >= halfStep) {
                    sum += result.heightmap[y][x - halfStep];
                    count++;
                }
                // Right
                if (x + halfStep < size) {
                    sum += result.heightmap[y][x + halfStep];
                    count++;
                }

                f32 avg = (count > 0) ? sum / static_cast<f32>(count) : 0.0f;
                f32 offset = (rng.Float() * 2.0f - 1.0f) * roughness * range;
                result.heightmap[y][x] = avg + offset;
            }
        }

        // Reduce roughness each iteration
        roughness *= 0.5f;
        stepSize = halfStep;
    }

    // Clamp values to [minHeight, maxHeight]
    for (u32 y = 0; y < size; ++y) {
        for (u32 x = 0; x < size; ++x) {
            if (result.heightmap[y][x] < params.minHeight) result.heightmap[y][x] = params.minHeight;
            if (result.heightmap[y][x] > params.maxHeight) result.heightmap[y][x] = params.maxHeight;
        }
    }

    return result;
}

// ============================================================================
// LSystemGenerator
// ============================================================================

LSystemGenerator::Result LSystemGenerator::Generate(const Params& params) {
    Result result;
    result.boundsMin = Math::Vector2(Math::FLOAT_MAX, Math::FLOAT_MAX);
    result.boundsMax = Math::Vector2(Math::FLOAT_MIN, Math::FLOAT_MIN);

    if (params.axiom.empty()) {
        result.boundsMin = Math::Vector2(0.0f, 0.0f);
        result.boundsMax = Math::Vector2(0.0f, 0.0f);
        return result;
    }

    // String rewriting: apply rules for given iterations
    std::string current = params.axiom;
    for (u32 iter = 0; iter < params.iterations; ++iter) {
        std::string next;
        next.reserve(current.size() * 2);

        for (char c : current) {
            auto it = params.rules.find(c);
            if (it != params.rules.end()) {
                next += it->second;
            } else {
                next += c;
            }
        }
        current = std::move(next);
    }

    // Turtle graphics interpretation
    struct TurtleState {
        Math::Vector2 pos;
        f32 angle;  // In radians
    };

    TurtleState turtle;
    turtle.pos = Math::Vector2(0.0f, 0.0f);
    turtle.angle = Math::PI * 0.5f;  // Start facing up (90 degrees)

    std::stack<TurtleState> stateStack;

    f32 turnAngle = Math::Radians(params.angle);
    f32 step = params.stepLength;

    auto updateBounds = [&](const Math::Vector2& p) {
        if (p.x < result.boundsMin.x) result.boundsMin.x = p.x;
        if (p.y < result.boundsMin.y) result.boundsMin.y = p.y;
        if (p.x > result.boundsMax.x) result.boundsMax.x = p.x;
        if (p.y > result.boundsMax.y) result.boundsMax.y = p.y;
    };

    updateBounds(turtle.pos);

    for (char c : current) {
        switch (c) {
            case 'F': {
                // Move forward and draw
                Math::Vector2 start = turtle.pos;
                turtle.pos.x += Math::Cos(turtle.angle) * step;
                turtle.pos.y += Math::Sin(turtle.angle) * step;

                LineSegment seg;
                seg.start = start;
                seg.end = turtle.pos;
                result.segments.push_back(seg);

                updateBounds(turtle.pos);
                break;
            }
            case 'f': {
                // Move forward without drawing
                turtle.pos.x += Math::Cos(turtle.angle) * step;
                turtle.pos.y += Math::Sin(turtle.angle) * step;
                updateBounds(turtle.pos);
                break;
            }
            case '+': {
                // Turn left by angle
                turtle.angle += turnAngle;
                break;
            }
            case '-': {
                // Turn right by angle
                turtle.angle -= turnAngle;
                break;
            }
            case '[': {
                // Push state
                stateStack.push(turtle);
                break;
            }
            case ']': {
                // Pop state
                if (!stateStack.empty()) {
                    turtle = stateStack.top();
                    stateStack.pop();
                }
                break;
            }
            default:
                // Other symbols are ignored during interpretation
                break;
        }
    }

    // Handle case where no segments were generated
    if (result.segments.empty()) {
        result.boundsMin = Math::Vector2(0.0f, 0.0f);
        result.boundsMax = Math::Vector2(0.0f, 0.0f);
    }

    return result;
}

// ============================================================================
// WaveFunctionCollapse
// ============================================================================

WaveFunctionCollapse::Result WaveFunctionCollapse::Generate(const Params& params) {
    Result result;
    result.success = false;

    if (params.width == 0 || params.height == 0 || params.tiles.empty()) {
        return result;
    }

    LCGRandom rng(params.seed);

    u32 numTiles = static_cast<u32>(params.tiles.size());
    u32 totalCells = params.width * params.height;

    // Each cell has a set of possible tile IDs.
    // Represent as a vector of bools per cell (one entry per tile type).
    struct CellState {
        std::vector<bool> possible;  // Which tiles are still possible
        bool collapsed = false;
        u32 tileId = 0;

        u32 Entropy() const {
            u32 count = 0;
            for (bool b : possible) {
                if (b) count++;
            }
            return count;
        }
    };

    std::vector<CellState> cells(totalCells);
    for (auto& cell : cells) {
        cell.possible.resize(numTiles, true);
    }

    // Build a lookup: tile index by tile id
    std::unordered_map<u32, u32> tileIndexById;
    for (u32 i = 0; i < numTiles; ++i) {
        tileIndexById[params.tiles[i].id] = i;
    }

    // Backtracking state
    struct Snapshot {
        std::vector<CellState> cells;
        u32 collapsedCell;
        u32 chosenTile;
    };

    std::vector<Snapshot> backtrackStack;
    u32 backtracksUsed = 0;

    // Direction offsets: [0]=North(y-1), [1]=South(y+1), [2]=East(x+1), [3]=West(x-1)
    const i32 dxArr[4] = { 0,  0,  1, -1 };
    const i32 dyArr[4] = { -1, 1,  0,  0 };

    // Propagation function
    auto propagate = [&](u32 startIdx) -> bool {
        std::vector<u32> propagationStack;
        propagationStack.push_back(startIdx);

        while (!propagationStack.empty()) {
            u32 cellIdx = propagationStack.back();
            propagationStack.pop_back();

            i32 cx = static_cast<i32>(cellIdx % params.width);
            i32 cy = static_cast<i32>(cellIdx / params.width);

            for (u32 dir = 0; dir < 4; ++dir) {
                i32 nx = cx + dxArr[dir];
                i32 ny = cy + dyArr[dir];

                if (nx < 0 || nx >= static_cast<i32>(params.width) ||
                    ny < 0 || ny >= static_cast<i32>(params.height)) {
                    continue;
                }

                u32 neighborIdx = static_cast<u32>(ny) * params.width + static_cast<u32>(nx);
                CellState& neighbor = cells[neighborIdx];
                if (neighbor.collapsed) continue;

                bool changed = false;

                // For each possible tile in the neighbor, check if any possible
                // tile in the current cell allows it
                for (u32 nt = 0; nt < numTiles; ++nt) {
                    if (!neighbor.possible[nt]) continue;

                    // Check if any possible tile in current cell allows this neighbor tile
                    bool allowed = false;
                    for (u32 ct = 0; ct < numTiles; ++ct) {
                        if (!cells[cellIdx].possible[ct]) continue;

                        // Check if tile ct allows tile nt in direction dir
                        const auto& allowedList = params.tiles[ct].allowedNeighbors[dir];
                        for (u32 allowedId : allowedList) {
                            auto it = tileIndexById.find(allowedId);
                            if (it != tileIndexById.end() && it->second == nt) {
                                allowed = true;
                                break;
                            }
                        }
                        if (allowed) break;
                    }

                    if (!allowed) {
                        neighbor.possible[nt] = false;
                        changed = true;
                    }
                }

                if (changed) {
                    // Check for contradiction
                    if (neighbor.Entropy() == 0) {
                        return false;  // Contradiction
                    }
                    propagationStack.push_back(neighborIdx);
                }
            }
        }
        return true;
    };

    // Main collapse loop
    u32 collapsedCount = 0;
    bool failed = false;

    while (collapsedCount < totalCells && !failed) {
        // Find cell with lowest entropy (fewest possibilities) that isn't collapsed
        u32 bestIdx = UINT32_MAX;
        u32 bestEntropy = UINT32_MAX;

        for (u32 i = 0; i < totalCells; ++i) {
            if (cells[i].collapsed) continue;
            u32 entropy = cells[i].Entropy();
            if (entropy == 0) {
                // Contradiction found
                failed = true;
                break;
            }
            if (entropy < bestEntropy) {
                bestEntropy = entropy;
                bestIdx = i;
            }
        }

        if (failed || bestIdx == UINT32_MAX) break;

        // Save state for backtracking
        if (backtracksUsed < params.maxBacktracks) {
            Snapshot snap;
            snap.cells = cells;
            snap.collapsedCell = bestIdx;
            snap.chosenTile = 0;
            backtrackStack.push_back(snap);
        }

        // Collapse: pick a random tile from possible options
        CellState& cell = cells[bestIdx];
        std::vector<u32> options;
        for (u32 t = 0; t < numTiles; ++t) {
            if (cell.possible[t]) {
                options.push_back(t);
            }
        }

        u32 chosen = options[rng.Uint(static_cast<u32>(options.size()))];
        cell.collapsed = true;
        cell.tileId = params.tiles[chosen].id;
        for (u32 t = 0; t < numTiles; ++t) {
            cell.possible[t] = (t == chosen);
        }
        collapsedCount++;

        // Propagate constraints
        if (!propagate(bestIdx)) {
            // Contradiction: try to backtrack
            if (!backtrackStack.empty() && backtracksUsed < params.maxBacktracks) {
                backtracksUsed++;
                auto& snap = backtrackStack.back();
                cells = snap.cells;

                // Remove the chosen tile from options so we don't pick it again
                cells[snap.collapsedCell].possible[chosen] = false;
                if (cells[snap.collapsedCell].Entropy() == 0) {
                    // No options left, remove this snapshot too
                    backtrackStack.pop_back();
                }

                // Recount collapsed
                collapsedCount = 0;
                for (u32 i = 0; i < totalCells; ++i) {
                    if (cells[i].collapsed) collapsedCount++;
                }
                failed = false;
            } else {
                failed = true;
            }
        }
    }

    // Build result grid
    result.grid.resize(params.height, std::vector<u32>(params.width, 0));
    for (u32 y = 0; y < params.height; ++y) {
        for (u32 x = 0; x < params.width; ++x) {
            u32 idx = y * params.width + x;
            if (cells[idx].collapsed) {
                result.grid[y][x] = cells[idx].tileId;
            } else {
                // Uncollapsed cell: pick first possible tile
                for (u32 t = 0; t < numTiles; ++t) {
                    if (cells[idx].possible[t]) {
                        result.grid[y][x] = params.tiles[t].id;
                        break;
                    }
                }
            }
        }
    }

    result.success = (collapsedCount == totalCells) && !failed;
    return result;
}

// ============================================================================
// VoronoiGenerator
// ============================================================================

VoronoiGenerator::Result VoronoiGenerator::Generate(const Params& params) {
    Result result;

    if (params.width == 0 || params.height == 0 || params.numSeeds == 0) {
        return result;
    }

    LCGRandom rng(params.seed);

    // Place random seed points
    result.seedPoints.reserve(params.numSeeds);
    for (u32 i = 0; i < params.numSeeds; ++i) {
        f32 sx = rng.Float() * static_cast<f32>(params.width);
        f32 sy = rng.Float() * static_cast<f32>(params.height);
        result.seedPoints.push_back(Math::Vector2(sx, sy));
    }

    // For each cell, find closest seed
    result.grid.resize(params.height, std::vector<u32>(params.width, 0));

    for (u32 y = 0; y < params.height; ++y) {
        for (u32 x = 0; x < params.width; ++x) {
            f32 px = static_cast<f32>(x) + 0.5f;
            f32 py = static_cast<f32>(y) + 0.5f;

            f32 bestDist = Math::FLOAT_MAX;
            u32 bestSeed = 0;

            for (u32 s = 0; s < params.numSeeds; ++s) {
                f32 dx = px - result.seedPoints[s].x;
                f32 dy = py - result.seedPoints[s].y;

                f32 dist;
                switch (params.distanceType) {
                    case DistanceType::Euclidean:
                        dist = dx * dx + dy * dy;  // Compare squared to avoid sqrt
                        break;
                    case DistanceType::Manhattan:
                        dist = Math::Abs(dx) + Math::Abs(dy);
                        break;
                    case DistanceType::Chebyshev:
                        dist = Math::Max(Math::Abs(dx), Math::Abs(dy));
                        break;
                    default:
                        dist = dx * dx + dy * dy;
                        break;
                }

                if (dist < bestDist) {
                    bestDist = dist;
                    bestSeed = s;
                }
            }

            result.grid[y][x] = bestSeed;
        }
    }

    return result;
}

// ============================================================================
// GrammarGenerator
// ============================================================================

GrammarGenerator::Result GrammarGenerator::Generate(const Params& params) {
    Result result;

    if (params.startSymbol.empty()) {
        return result;
    }

    LCGRandom rng(params.seed);

    // Start with the start symbol as a list of tokens
    // Symbols are separated by spaces
    std::vector<std::string> symbols;
    symbols.push_back(params.startSymbol);

    // Build a lookup from symbol name to matching rules
    std::unordered_map<std::string, std::vector<usize>> rulesBySymbol;
    for (usize i = 0; i < params.rules.size(); ++i) {
        rulesBySymbol[params.rules[i].symbol].push_back(i);
    }

    // String rewriting iterations
    for (u32 iter = 0; iter < params.iterations; ++iter) {
        std::vector<std::string> next;
        next.reserve(symbols.size() * 2);

        for (const auto& sym : symbols) {
            auto it = rulesBySymbol.find(sym);
            if (it == rulesBySymbol.end() || it->second.empty()) {
                // No rule for this symbol: keep it as-is (terminal)
                next.push_back(sym);
                continue;
            }

            // Weighted random rule selection
            const auto& matchingIndices = it->second;
            f32 totalWeight = 0.0f;
            for (usize idx : matchingIndices) {
                totalWeight += params.rules[idx].weight;
            }

            f32 roll = rng.Float() * totalWeight;
            f32 cumulative = 0.0f;
            usize chosenRule = matchingIndices[0];
            for (usize idx : matchingIndices) {
                cumulative += params.rules[idx].weight;
                if (roll < cumulative) {
                    chosenRule = idx;
                    break;
                }
            }

            // Parse replacement string: space-separated symbols
            const std::string& replacement = params.rules[chosenRule].replacements.empty()
                ? params.rules[chosenRule].symbol
                : params.rules[chosenRule].replacements[0];

            // If the rule has multiple replacements, pick one at random
            const auto& repls = params.rules[chosenRule].replacements;
            const std::string* chosen = &replacement;
            if (repls.size() > 1) {
                u32 pickIdx = rng.Uint(static_cast<u32>(repls.size()));
                chosen = &repls[pickIdx];
            }

            // Split chosen replacement by spaces
            std::string token;
            for (char c : *chosen) {
                if (c == ' ') {
                    if (!token.empty()) {
                        next.push_back(token);
                        token.clear();
                    }
                } else {
                    token += c;
                }
            }
            if (!token.empty()) {
                next.push_back(token);
            }
        }

        symbols = std::move(next);
    }

    // Convert final symbols to shapes with position/size
    // Simple stacking rules: boxes stack vertically, roofs go on top
    f32 currentY = 0.0f;
    f32 baseWidth = 4.0f;
    f32 baseDepth = 4.0f;

    for (const auto& sym : symbols) {
        GrammarShape shape;
        shape.position = Math::Vector3(0.0f, currentY, 0.0f);
        shape.rotation = 0.0f;

        if (sym == "Box" || sym == "box") {
            shape.type = ShapeType::Box;
            shape.size = Math::Vector3(baseWidth, 3.0f, baseDepth);
            currentY += shape.size.y;
        } else if (sym == "Floor" || sym == "floor") {
            shape.type = ShapeType::Floor;
            shape.size = Math::Vector3(baseWidth, 0.2f, baseDepth);
            currentY += shape.size.y;
        } else if (sym == "Wall" || sym == "wall") {
            shape.type = ShapeType::Wall;
            shape.size = Math::Vector3(baseWidth, 3.0f, 0.3f);
            currentY += shape.size.y;
        } else if (sym == "Roof" || sym == "roof") {
            shape.type = ShapeType::Roof;
            shape.size = Math::Vector3(baseWidth + 1.0f, 2.0f, baseDepth + 1.0f);
            currentY += shape.size.y;
        } else if (sym == "Cylinder" || sym == "cylinder" || sym == "Tower" || sym == "tower") {
            shape.type = ShapeType::Cylinder;
            shape.size = Math::Vector3(2.0f, 5.0f, 2.0f);
            currentY += shape.size.y;
        } else if (sym == "Window" || sym == "window") {
            shape.type = ShapeType::Window;
            shape.size = Math::Vector3(1.0f, 1.5f, 0.1f);
            // Windows don't stack; placed on the current level
        } else if (sym == "Door" || sym == "door") {
            shape.type = ShapeType::Door;
            shape.size = Math::Vector3(1.5f, 2.5f, 0.2f);
            shape.position.y = 0.0f;  // Doors always at ground level
        } else {
            // Unknown terminal: treat as a small box
            shape.type = ShapeType::Box;
            shape.size = Math::Vector3(baseWidth * 0.5f, 1.0f, baseDepth * 0.5f);
            currentY += shape.size.y;
        }

        result.shapes.push_back(shape);
    }

    return result;
}

// ============================================================================
// PrefabAssembler
// ============================================================================

PrefabAssembler::Result PrefabAssembler::Generate(const Params& params) {
    Result result;
    result.placements.clear();

    if (params.slots.empty() || params.maxRooms == 0) {
        return result;
    }

    LCGRandom rng(params.seed);

    // Track how many times each slot has been used
    std::vector<u32> useCounts(params.slots.size(), 0);

    // Pick a random starting slot and place it at origin
    u32 startIdx = rng.Uint(static_cast<u32>(params.slots.size()));
    Placement startPlacement;
    startPlacement.slotIndex = startIdx;
    startPlacement.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    startPlacement.rotation = 0.0f;

    result.placements.push_back(startPlacement);
    useCounts[startIdx]++;

    // Track unconnected connection points: (placementIndex, connectionIndex into slot.connections)
    struct OpenConnection {
        u32 placementIdx;
        u32 connectionIdx;
    };
    std::vector<OpenConnection> openConnections;

    // Add starting slot's connections as open
    for (u32 ci = 0; ci < static_cast<u32>(params.slots[startIdx].connections.size()); ++ci) {
        openConnections.push_back({0, ci});
    }

    // Main assembly loop
    u32 maxAttempts = params.maxRooms * 10;
    u32 attempts = 0;

    while (result.placements.size() < params.maxRooms && !openConnections.empty() && attempts < maxAttempts) {
        attempts++;

        // Pick a random open connection
        u32 openIdx = rng.Uint(static_cast<u32>(openConnections.size()));
        OpenConnection open = openConnections[openIdx];

        const Placement& fromPlacement = result.placements[open.placementIdx];
        const PrefabSlot& fromSlot = params.slots[fromPlacement.slotIndex];
        ConnectionDir fromDir = fromSlot.connections[open.connectionIdx];

        // We need the opposite direction on the new slot
        ConnectionDir neededDir = GetOppositeDirection(fromDir);

        // Find a compatible slot to place (one that has a connection facing neededDir)
        struct Candidate {
            u32 slotIdx;
            u32 connIdx;
        };
        std::vector<Candidate> candidates;

        for (u32 si = 0; si < static_cast<u32>(params.slots.size()); ++si) {
            const PrefabSlot& candidateSlot = params.slots[si];

            for (u32 ci = 0; ci < static_cast<u32>(candidateSlot.connections.size()); ++ci) {
                ConnectionDir candidateDir = candidateSlot.connections[ci];

                // Check direction match
                if (static_cast<u8>(candidateDir) != static_cast<u8>(neededDir)) continue;

                candidates.push_back({si, ci});
            }
        }

        if (candidates.empty()) {
            // Remove this open connection, it can't be satisfied
            openConnections.erase(openConnections.begin() + openIdx);
            continue;
        }

        // Pick a random candidate
        u32 candIdx = rng.Uint(static_cast<u32>(candidates.size()));
        const Candidate& cand = candidates[candIdx];
        const PrefabSlot& newSlot = params.slots[cand.slotIdx];

        // Calculate position offset based on the from-connection direction
        Math::Vector3 offset(0.0f, 0.0f, 0.0f);
        f32 fromHalfX = fromSlot.size.x * 0.5f;
        f32 fromHalfZ = fromSlot.size.z * 0.5f;
        f32 newHalfX  = newSlot.size.x * 0.5f;
        f32 newHalfZ  = newSlot.size.z * 0.5f;

        switch (fromDir) {
            case PrefabAssembler::ConnectionDir::North:
                offset = Math::Vector3(0.0f, 0.0f, fromHalfZ + newHalfZ);
                break;
            case PrefabAssembler::ConnectionDir::South:
                offset = Math::Vector3(0.0f, 0.0f, -(fromHalfZ + newHalfZ));
                break;
            case PrefabAssembler::ConnectionDir::East:
                offset = Math::Vector3(fromHalfX + newHalfX, 0.0f, 0.0f);
                break;
            case PrefabAssembler::ConnectionDir::West:
                offset = Math::Vector3(-(fromHalfX + newHalfX), 0.0f, 0.0f);
                break;
            case PrefabAssembler::ConnectionDir::Up:
                offset = Math::Vector3(0.0f, fromSlot.size.y * 0.5f + newSlot.size.y * 0.5f, 0.0f);
                break;
            case PrefabAssembler::ConnectionDir::Down:
                offset = Math::Vector3(0.0f, -(fromSlot.size.y * 0.5f + newSlot.size.y * 0.5f), 0.0f);
                break;
        }

        Math::Vector3 newPos = fromPlacement.position + offset;

        // Check for overlap with existing placements (simple AABB check)
        bool overlaps = false;
        for (const auto& existing : result.placements) {
            const PrefabSlot& existSlot = params.slots[existing.slotIndex];
            Math::Vector3 halfA = existSlot.size * 0.5f;
            Math::Vector3 halfB = newSlot.size * 0.5f;

            if (Math::Abs(existing.position.x - newPos.x) < halfA.x + halfB.x - 0.01f &&
                Math::Abs(existing.position.y - newPos.y) < halfA.y + halfB.y - 0.01f &&
                Math::Abs(existing.position.z - newPos.z) < halfA.z + halfB.z - 0.01f) {
                overlaps = true;
                break;
            }
        }

        if (overlaps) {
            // Remove this open connection
            openConnections.erase(openConnections.begin() + openIdx);
            continue;
        }

        // Place the new slot
        Placement newPlacement;
        newPlacement.slotIndex = cand.slotIdx;
        newPlacement.position = newPos;
        newPlacement.rotation = 0.0f;

        u32 newPlacementIdx = static_cast<u32>(result.placements.size());
        result.placements.push_back(newPlacement);
        useCounts[cand.slotIdx]++;

        // Remove the used open connection
        openConnections.erase(openConnections.begin() + openIdx);

        // Add new slot's other connections as open (skip the one we just used)
        for (u32 ci = 0; ci < static_cast<u32>(newSlot.connections.size()); ++ci) {
            if (ci == cand.connIdx) continue;
            openConnections.push_back({newPlacementIdx, ci});
        }
    }

    result.placements.shrink_to_fit();
    return result;
}

// ============================================================================
// FBMTerrain
// ============================================================================

FBMTerrain::Result FBMTerrain::Generate(const Params& params) {
    Result result;

    if (params.width == 0 || params.height == 0) {
        return result;
    }

    result.width = params.width;
    result.height = params.height;

    u32 totalCells = params.width * params.height;
    result.heightmap.resize(totalCells, 0.0f);

    // Clamp octaves to valid range
    u32 octaves = params.octaves;
    if (octaves < 1) octaves = 1;
    if (octaves > 12) octaves = 12;

    u32 seed = params.seed;
    if (seed == 0) {
        // Use a deterministic default so results are reproducible
        seed = 48271u;
    }

    f32 rawMin =  Math::FLOAT_MAX;
    f32 rawMax =  Math::FLOAT_MIN;

    // Generate the heightmap
    for (u32 y = 0; y < params.height; ++y) {
        for (u32 x = 0; x < params.width; ++x) {
            // Normalize coordinates to [0, 1] range, then scale by frequency
            f32 nx = static_cast<f32>(x) / static_cast<f32>(params.width);
            f32 ny = static_cast<f32>(y) / static_cast<f32>(params.height);

            f32 value = 0.0f;

            if (params.mode == NoiseMode::Standard) {
                // Classic fBm: sum of octaved Perlin noise
                f32 amplitude = 1.0f;
                f32 freq = params.frequency;
                f32 maxAmplitude = 0.0f;

                for (u32 o = 0; o < octaves; ++o) {
                    value += amplitude * Math::PerlinNoise2D(
                        nx * freq, ny * freq, seed + o);
                    maxAmplitude += amplitude;
                    amplitude *= params.gain;
                    freq *= params.lacunarity;
                }

                // Normalize to [-1, 1] range
                if (maxAmplitude > 0.0f) {
                    value /= maxAmplitude;
                }
            } else {
                // Ridged multifractal: abs(noise) inverted, with power term
                f32 amplitude = 1.0f;
                f32 freq = params.frequency;
                f32 weight = 1.0f;

                for (u32 o = 0; o < octaves; ++o) {
                    f32 signal = Math::PerlinNoise2D(
                        nx * freq, ny * freq, seed + o);

                    // Create ridges: invert the absolute value
                    signal = params.ridgedOffset - Math::Abs(signal);

                    // Sharpen ridges with power term
                    signal = Math::Pow(Math::Abs(signal), params.ridgedPower);

                    // Weight by previous octave's contribution for detail clustering
                    signal *= weight;
                    weight = Math::Clamp(signal * params.gain * 2.0f, 0.0f, 1.0f);

                    value += signal * amplitude;
                    freq *= params.lacunarity;
                    amplitude *= params.gain;
                }
            }

            result.heightmap[y * params.width + x] = value;

            if (value < rawMin) rawMin = value;
            if (value > rawMax) rawMax = value;
        }
    }

    result.minValue = rawMin;
    result.maxValue = rawMax;

    // Normalize heightmap to [0, 1]
    f32 range = rawMax - rawMin;
    if (range > Math::EPSILON) {
        f32 invRange = 1.0f / range;
        for (u32 i = 0; i < totalCells; ++i) {
            result.heightmap[i] = (result.heightmap[i] - rawMin) * invRange;
        }
    } else {
        // Flat terrain
        for (u32 i = 0; i < totalCells; ++i) {
            result.heightmap[i] = 0.5f;
        }
    }

    return result;
}

// ============================================================================
// HydraulicErosion
// ============================================================================

namespace {

// Bilinear interpolation of heightmap at fractional position
f32 SampleHeight(const std::vector<f32>& heightmap, u32 width, u32 height, f32 x, f32 y) {
    // Clamp to valid range
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x >= static_cast<f32>(width) - 1.001f) x = static_cast<f32>(width) - 1.001f;
    if (y >= static_cast<f32>(height) - 1.001f) y = static_cast<f32>(height) - 1.001f;

    i32 ix = static_cast<i32>(x);
    i32 iy = static_cast<i32>(y);
    f32 fx = x - static_cast<f32>(ix);
    f32 fy = y - static_cast<f32>(iy);

    i32 ix1 = ix + 1;
    i32 iy1 = iy + 1;
    if (ix1 >= static_cast<i32>(width))  ix1 = static_cast<i32>(width) - 1;
    if (iy1 >= static_cast<i32>(height)) iy1 = static_cast<i32>(height) - 1;

    f32 h00 = heightmap[iy  * width + ix];
    f32 h10 = heightmap[iy  * width + ix1];
    f32 h01 = heightmap[iy1 * width + ix];
    f32 h11 = heightmap[iy1 * width + ix1];

    f32 h0 = h00 * (1.0f - fx) + h10 * fx;
    f32 h1 = h01 * (1.0f - fx) + h11 * fx;

    return h0 * (1.0f - fy) + h1 * fy;
}

// Compute gradient at fractional position using bilinear interpolation
void ComputeGradient(const std::vector<f32>& heightmap, u32 width, u32 height,
                     f32 x, f32 y, f32& gradX, f32& gradY) {
    i32 ix = static_cast<i32>(x);
    i32 iy = static_cast<i32>(y);
    f32 fx = x - static_cast<f32>(ix);
    f32 fy = y - static_cast<f32>(iy);

    // Clamp indices
    if (ix < 0) ix = 0;
    if (iy < 0) iy = 0;
    if (ix >= static_cast<i32>(width) - 1)  ix = static_cast<i32>(width) - 2;
    if (iy >= static_cast<i32>(height) - 1) iy = static_cast<i32>(height) - 2;

    i32 ix1 = ix + 1;
    i32 iy1 = iy + 1;

    f32 h00 = heightmap[iy  * width + ix];
    f32 h10 = heightmap[iy  * width + ix1];
    f32 h01 = heightmap[iy1 * width + ix];
    f32 h11 = heightmap[iy1 * width + ix1];

    // Gradient from bilinear interpolation partial derivatives
    gradX = (h10 - h00) * (1.0f - fy) + (h11 - h01) * fy;
    gradY = (h01 - h00) * (1.0f - fx) + (h11 - h10) * fx;
}

} // anonymous namespace

void HydraulicErosion::Erode(std::vector<f32>& heightmap, u32 width, u32 height,
                              const Params& params) {
    if (width < 3 || height < 3) return;
    if (heightmap.size() != static_cast<usize>(width) * height) return;

    LCGRandom rng(params.seed);

    // Precompute erosion brush weights for the given radius
    u32 erosionRadius = params.erosionRadius;
    if (erosionRadius < 1) erosionRadius = 1;
    if (erosionRadius > 5) erosionRadius = 5;

    // Build erosion brush: list of (dx, dy, weight) offsets
    struct BrushEntry {
        i32 dx, dy;
        f32 weight;
    };
    std::vector<BrushEntry> brush;
    f32 totalBrushWeight = 0.0f;

    for (i32 by = -static_cast<i32>(erosionRadius); by <= static_cast<i32>(erosionRadius); ++by) {
        for (i32 bx = -static_cast<i32>(erosionRadius); bx <= static_cast<i32>(erosionRadius); ++bx) {
            f32 dist = Math::Sqrt(static_cast<f32>(bx * bx + by * by));
            if (dist <= static_cast<f32>(erosionRadius)) {
                f32 w = Math::Max(0.0f, static_cast<f32>(erosionRadius) - dist);
                brush.push_back({bx, by, w});
                totalBrushWeight += w;
            }
        }
    }

    // Normalize brush weights
    if (totalBrushWeight > 0.0f) {
        for (auto& b : brush) {
            b.weight /= totalBrushWeight;
        }
    }

    // Simulate water droplets
    for (u32 iter = 0; iter < params.iterations; ++iter) {
        // Spawn droplet at random position
        f32 posX = rng.FloatRange(1.0f, static_cast<f32>(width) - 2.0f);
        f32 posY = rng.FloatRange(1.0f, static_cast<f32>(height) - 2.0f);
        f32 dirX = 0.0f;
        f32 dirY = 0.0f;
        f32 speed = 1.0f;
        f32 water = 1.0f;
        f32 sediment = 0.0f;

        for (u32 step = 0; step < params.dropLifetime; ++step) {
            i32 cellX = static_cast<i32>(posX);
            i32 cellY = static_cast<i32>(posY);

            // Check bounds (leave 1-cell border)
            if (cellX < 1 || cellX >= static_cast<i32>(width) - 1 ||
                cellY < 1 || cellY >= static_cast<i32>(height) - 1) {
                break;
            }

            // Sample current height
            f32 currentHeight = SampleHeight(heightmap, width, height, posX, posY);

            // Compute gradient for flow direction
            f32 gradX, gradY;
            ComputeGradient(heightmap, width, height, posX, posY, gradX, gradY);

            // Update direction with inertia
            dirX = dirX * params.inertia - gradX * (1.0f - params.inertia);
            dirY = dirY * params.inertia - gradY * (1.0f - params.inertia);

            // Normalize direction
            f32 dirLen = Math::Sqrt(dirX * dirX + dirY * dirY);
            if (dirLen < Math::EPSILON) {
                // Pick random direction if on flat area
                f32 angle = rng.Float() * Math::PI_2;
                dirX = Math::Cos(angle);
                dirY = Math::Sin(angle);
            } else {
                dirX /= dirLen;
                dirY /= dirLen;
            }

            // Move to new position
            f32 newPosX = posX + dirX;
            f32 newPosY = posY + dirY;

            // Check bounds
            if (newPosX < 1.0f || newPosX >= static_cast<f32>(width) - 2.0f ||
                newPosY < 1.0f || newPosY >= static_cast<f32>(height) - 2.0f) {
                break;
            }

            f32 newHeight = SampleHeight(heightmap, width, height, newPosX, newPosY);
            f32 heightDiff = newHeight - currentHeight;

            // Calculate sediment capacity based on slope, speed and water
            f32 slopeAngle = Math::Max(-heightDiff, params.minSlope);
            f32 capacity = slopeAngle * speed * water * params.sedimentCapacity;

            if (sediment > capacity || heightDiff > 0.0f) {
                // Deposit sediment
                f32 depositAmount;
                if (heightDiff > 0.0f) {
                    // Moving uphill: deposit at most the height difference
                    depositAmount = Math::Min(sediment, heightDiff);
                } else {
                    // Moving downhill but over capacity: deposit fraction of excess
                    depositAmount = (sediment - capacity) * params.depositRate;
                }

                sediment -= depositAmount;

                // Distribute deposit to nearby cells using erosion brush
                for (const auto& b : brush) {
                    i32 bx = cellX + b.dx;
                    i32 by = cellY + b.dy;
                    if (bx >= 0 && bx < static_cast<i32>(width) &&
                        by >= 0 && by < static_cast<i32>(height)) {
                        heightmap[by * width + bx] += depositAmount * b.weight;
                    }
                }
            } else {
                // Erode terrain
                f32 erodeAmount = Math::Min((capacity - sediment) * params.erosionRate,
                                            -heightDiff);
                erodeAmount = Math::Max(erodeAmount, 0.0f);

                sediment += erodeAmount;

                // Distribute erosion to nearby cells using erosion brush
                for (const auto& b : brush) {
                    i32 bx = cellX + b.dx;
                    i32 by = cellY + b.dy;
                    if (bx >= 0 && bx < static_cast<i32>(width) &&
                        by >= 0 && by < static_cast<i32>(height)) {
                        heightmap[by * width + bx] -= erodeAmount * b.weight;
                    }
                }
            }

            // Update speed: accelerate downhill, decelerate uphill
            speed = Math::Sqrt(Math::Max(speed * speed - heightDiff * params.gravity, 0.01f));

            // Evaporate water
            water *= (1.0f - params.evaporateRate);

            posX = newPosX;
            posY = newPosY;

            // Stop if water is too low
            if (water < 0.001f) break;
        }
    }
}

// ============================================================================
// ThermalErosion
// ============================================================================

void ThermalErosion::Erode(std::vector<f32>& heightmap, u32 width, u32 height,
                            const Params& params) {
    if (width < 3 || height < 3) return;
    if (heightmap.size() != static_cast<usize>(width) * height) return;

    f32 talusThreshold = params.talusAngle;
    if (talusThreshold < 0.001f) talusThreshold = 0.001f;

    f32 rate = Math::Clamp(params.erosionRate, 0.0f, 0.5f);

    // 4-connected neighbors (Von Neumann neighborhood)
    const i32 dx[4] = { 0, 1, 0, -1 };
    const i32 dy[4] = { -1, 0, 1, 0 };

    // Temporary buffer to accumulate changes per iteration
    std::vector<f32> delta(static_cast<usize>(width) * height, 0.0f);

    for (u32 iter = 0; iter < params.iterations; ++iter) {
        // Reset delta buffer
        std::memset(delta.data(), 0, delta.size() * sizeof(f32));

        for (u32 y = 1; y < height - 1; ++y) {
            for (u32 x = 1; x < width - 1; ++x) {
                u32 idx = y * width + x;
                f32 h = heightmap[idx];

                // Calculate total excess height difference to neighbors below talus
                f32 totalDiff = 0.0f;
                f32 maxDiff = 0.0f;

                // First pass: find total and max height differences exceeding talus
                f32 diffs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                for (u32 n = 0; n < 4; ++n) {
                    u32 nx = static_cast<u32>(static_cast<i32>(x) + dx[n]);
                    u32 ny = static_cast<u32>(static_cast<i32>(y) + dy[n]);
                    u32 nIdx = ny * width + nx;
                    f32 diff = h - heightmap[nIdx];

                    if (diff > talusThreshold) {
                        diffs[n] = diff - talusThreshold;
                        totalDiff += diffs[n];
                        if (diff > maxDiff) maxDiff = diff;
                    }
                }

                // Second pass: distribute material proportionally
                if (totalDiff > 0.0f) {
                    // Amount to remove from this cell, proportional to max excess
                    f32 removeAmount = (maxDiff - talusThreshold) * rate * 0.5f;

                    for (u32 n = 0; n < 4; ++n) {
                        if (diffs[n] > 0.0f) {
                            f32 fraction = diffs[n] / totalDiff;
                            f32 transfer = removeAmount * fraction;

                            u32 nx = static_cast<u32>(static_cast<i32>(x) + dx[n]);
                            u32 ny = static_cast<u32>(static_cast<i32>(y) + dy[n]);
                            u32 nIdx = ny * width + nx;

                            delta[idx] -= transfer;
                            delta[nIdx] += transfer;
                        }
                    }
                }
            }
        }

        // Apply accumulated changes
        for (u32 i = 0; i < static_cast<u32>(heightmap.size()); ++i) {
            heightmap[i] += delta[i];
        }
    }
}

// ============================================================================
// LSystem3D
// ============================================================================

LSystem3D::Result LSystem3D::Generate(const Params& params) {
    Result result;
    result.boundsMin = Math::Vector3(Math::FLOAT_MAX, Math::FLOAT_MAX, Math::FLOAT_MAX);
    result.boundsMax = Math::Vector3(Math::FLOAT_MIN, Math::FLOAT_MIN, Math::FLOAT_MIN);

    if (params.axiom.empty()) {
        result.boundsMin = Math::Vector3(0.0f, 0.0f, 0.0f);
        result.boundsMax = Math::Vector3(0.0f, 0.0f, 0.0f);
        return result;
    }

    LCGRandom rng(params.seed);

    // ---- String rewriting phase ----
    std::string current = params.axiom;
    for (u32 iter = 0; iter < params.iterations; ++iter) {
        std::string next;
        next.reserve(current.size() * 2);

        for (char c : current) {
            // Check stochastic rules first (they override deterministic ones)
            auto stochIt = params.stochasticRules.find(c);
            if (stochIt != params.stochasticRules.end() && !stochIt->second.options.empty()) {
                const auto& options = stochIt->second.options;

                // Weighted random selection
                f32 totalWeight = 0.0f;
                for (const auto& opt : options) {
                    totalWeight += opt.second;
                }

                f32 roll = rng.Float() * totalWeight;
                f32 cumulative = 0.0f;
                const std::string* chosen = &options[0].first;
                for (const auto& opt : options) {
                    cumulative += opt.second;
                    if (roll < cumulative) {
                        chosen = &opt.first;
                        break;
                    }
                }
                next += *chosen;
            } else {
                // Check deterministic rules
                auto detIt = params.rules.find(c);
                if (detIt != params.rules.end()) {
                    next += detIt->second;
                } else {
                    next += c;
                }
            }
        }
        current = std::move(next);
    }

    // ---- 3D turtle interpretation phase ----
    struct TurtleState {
        Math::Vector3 position;
        Math::Vector3 heading;    // Forward direction
        Math::Vector3 up;         // Up direction
        Math::Vector3 left;       // Left direction
        f32 radius;
    };

    TurtleState turtle;
    turtle.position = Math::Vector3(0.0f, 0.0f, 0.0f);
    turtle.heading  = Math::Vector3(0.0f, 1.0f, 0.0f);  // Start growing upward (+Y)
    turtle.up       = Math::Vector3(0.0f, 0.0f, -1.0f);  // Backward Z is "up" in screen sense
    turtle.left     = Math::Vector3(-1.0f, 0.0f, 0.0f);  // Left
    turtle.radius   = params.initialRadius;

    std::stack<TurtleState> stateStack;

    f32 angleRad = Math::Radians(params.angle);
    f32 step = params.stepLength;

    // Helper: rotate vector around an arbitrary axis by angle (Rodrigues' formula)
    auto rotateAroundAxis = [](const Math::Vector3& v, const Math::Vector3& axis, f32 angle) -> Math::Vector3 {
        f32 cosA = Math::Cos(angle);
        f32 sinA = Math::Sin(angle);
        // v * cos(a) + (axis x v) * sin(a) + axis * (axis . v) * (1 - cos(a))
        Math::Vector3 cross = axis.Cross(v);
        f32 dot = axis.Dot(v);
        return v * cosA + cross * sinA + axis * (dot * (1.0f - cosA));
    };

    auto updateBounds = [&](const Math::Vector3& p) {
        if (p.x < result.boundsMin.x) result.boundsMin.x = p.x;
        if (p.y < result.boundsMin.y) result.boundsMin.y = p.y;
        if (p.z < result.boundsMin.z) result.boundsMin.z = p.z;
        if (p.x > result.boundsMax.x) result.boundsMax.x = p.x;
        if (p.y > result.boundsMax.y) result.boundsMax.y = p.y;
        if (p.z > result.boundsMax.z) result.boundsMax.z = p.z;
    };

    updateBounds(turtle.position);

    for (usize ci = 0; ci < current.size(); ++ci) {
        char c = current[ci];

        switch (c) {
            case 'F': {
                // Move forward and draw
                Math::Vector3 start = turtle.position;
                turtle.position = turtle.position + turtle.heading * step;

                LineSegment3D seg;
                seg.start = start;
                seg.end = turtle.position;
                seg.radius = turtle.radius;
                result.segments.push_back(seg);

                updateBounds(turtle.position);
                break;
            }
            case 'f': {
                // Move forward without drawing
                turtle.position = turtle.position + turtle.heading * step;
                updateBounds(turtle.position);
                break;
            }
            case '+': {
                // Yaw left: rotate heading and left around up axis
                turtle.heading = rotateAroundAxis(turtle.heading, turtle.up, angleRad);
                turtle.left    = rotateAroundAxis(turtle.left,    turtle.up, angleRad);
                break;
            }
            case '-': {
                // Yaw right: rotate heading and left around up axis (negative angle)
                turtle.heading = rotateAroundAxis(turtle.heading, turtle.up, -angleRad);
                turtle.left    = rotateAroundAxis(turtle.left,    turtle.up, -angleRad);
                break;
            }
            case '^': {
                // Pitch up: rotate heading and up around left axis
                turtle.heading = rotateAroundAxis(turtle.heading, turtle.left, angleRad);
                turtle.up      = rotateAroundAxis(turtle.up,      turtle.left, angleRad);
                break;
            }
            case '&': {
                // Pitch down: rotate heading and up around left axis (negative angle)
                turtle.heading = rotateAroundAxis(turtle.heading, turtle.left, -angleRad);
                turtle.up      = rotateAroundAxis(turtle.up,      turtle.left, -angleRad);
                break;
            }
            case '/': {
                // Roll left: rotate up and left around heading axis
                turtle.up   = rotateAroundAxis(turtle.up,   turtle.heading, angleRad);
                turtle.left = rotateAroundAxis(turtle.left,  turtle.heading, angleRad);
                break;
            }
            case '\\': {
                // Roll right: rotate up and left around heading axis (negative angle)
                turtle.up   = rotateAroundAxis(turtle.up,   turtle.heading, -angleRad);
                turtle.left = rotateAroundAxis(turtle.left,  turtle.heading, -angleRad);
                break;
            }
            case '[': {
                // Push state and decay radius
                stateStack.push(turtle);
                turtle.radius *= params.radiusDecay;
                break;
            }
            case ']': {
                // Pop state
                if (!stateStack.empty()) {
                    turtle = stateStack.top();
                    stateStack.pop();
                }
                break;
            }
            default:
                // Other symbols are ignored during interpretation
                break;
        }
    }

    // Handle case where no segments were generated
    if (result.segments.empty()) {
        result.boundsMin = Math::Vector3(0.0f, 0.0f, 0.0f);
        result.boundsMax = Math::Vector3(0.0f, 0.0f, 0.0f);
    }

    return result;
}

} // namespace Procedural
} // namespace Enjin
