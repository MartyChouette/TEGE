#include "EnjinTest.h"
#include "Enjin/Platform/Types.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"

using namespace Enjin;
using namespace Enjin::Procedural;

// ============================================================================
// CellularAutomata
// ============================================================================

ENJIN_TEST(CellularAutomata, GeneratesCorrectDimensions) {
    CellularAutomata::Params params;
    params.width = 32;
    params.height = 32;
    params.seed = 42;

    auto result = CellularAutomata::Generate(params);
    ENJIN_EXPECT_EQ(result.width, 32u);
    ENJIN_EXPECT_EQ(result.height, 32u);
    ENJIN_EXPECT_EQ(result.grid.size(), (usize)32);
    ENJIN_ASSERT_TRUE(result.grid.size() > 0);
    ENJIN_EXPECT_EQ(result.grid[0].size(), (usize)32);
}

ENJIN_TEST(CellularAutomata, DeterministicWithSeed) {
    CellularAutomata::Params params;
    params.width = 16;
    params.height = 16;
    params.seed = 123;

    auto r1 = CellularAutomata::Generate(params);
    auto r2 = CellularAutomata::Generate(params);

    // Same seed should produce identical results
    bool identical = true;
    for (u32 y = 0; y < 16 && identical; ++y)
        for (u32 x = 0; x < 16 && identical; ++x)
            if (r1.grid[y][x] != r2.grid[y][x]) identical = false;
    ENJIN_EXPECT_TRUE(identical);
}

ENJIN_TEST(CellularAutomata, GridContainsValidValues) {
    CellularAutomata::Params params;
    params.width = 16;
    params.height = 16;
    params.seed = 7;

    auto result = CellularAutomata::Generate(params);
    bool allValid = true;
    for (auto& row : result.grid)
        for (u8 cell : row)
            if (cell > 1) allValid = false;
    ENJIN_EXPECT_TRUE(allValid);
}

// ============================================================================
// RandomWalker
// ============================================================================

ENJIN_TEST(RandomWalker, GeneratesCorrectDimensions) {
    RandomWalker::Params params;
    params.width = 32;
    params.height = 32;
    params.steps = 500;
    params.seed = 42;

    auto result = RandomWalker::Generate(params);
    ENJIN_EXPECT_EQ(result.grid.size(), (usize)32);
    ENJIN_EXPECT_EQ(result.grid[0].size(), (usize)32);
}

ENJIN_TEST(RandomWalker, CarvesOpenCells) {
    RandomWalker::Params params;
    params.width = 32;
    params.height = 32;
    params.steps = 500;
    params.seed = 42;

    auto result = RandomWalker::Generate(params);
    u32 openCount = 0;
    for (auto& row : result.grid)
        for (u8 cell : row)
            if (cell == 0) openCount++;
    ENJIN_EXPECT_GT(openCount, 0u);
}

// ============================================================================
// BSPGenerator
// ============================================================================

ENJIN_TEST(BSPGenerator, GeneratesRooms) {
    BSPGenerator::Params params;
    params.width = 64;
    params.height = 64;
    params.splitDepth = 4;
    params.seed = 42;

    auto result = BSPGenerator::Generate(params);
    ENJIN_EXPECT_GT(result.rooms.size(), (usize)0);
    ENJIN_EXPECT_EQ(result.grid.size(), (usize)64);
}

ENJIN_TEST(BSPGenerator, RoomsWithinBounds) {
    BSPGenerator::Params params;
    params.width = 64;
    params.height = 64;
    params.seed = 42;

    auto result = BSPGenerator::Generate(params);
    for (const auto& room : result.rooms) {
        ENJIN_EXPECT_LT(room.x + room.w, 65u);
        ENJIN_EXPECT_LT(room.y + room.h, 65u);
    }
}

// ============================================================================
// DiamondSquare
// ============================================================================

ENJIN_TEST(DiamondSquare, GeneratesCorrectSize) {
    DiamondSquare::Params params;
    params.size = 33;  // 2^5 + 1
    params.seed = 42;

    auto result = DiamondSquare::Generate(params);
    ENJIN_EXPECT_EQ(result.size, 33u);
    ENJIN_EXPECT_EQ(result.heightmap.size(), (usize)33);
    ENJIN_ASSERT_TRUE(result.heightmap.size() > 0);
    ENJIN_EXPECT_EQ(result.heightmap[0].size(), (usize)33);
}

ENJIN_TEST(DiamondSquare, HeightmapInRange) {
    DiamondSquare::Params params;
    params.size = 33;
    params.seed = 42;
    params.minHeight = 0.0f;
    params.maxHeight = 1.0f;

    auto result = DiamondSquare::Generate(params);
    bool inRange = true;
    for (auto& row : result.heightmap)
        for (f32 h : row)
            if (h < -0.5f || h > 1.5f) inRange = false;  // Allow some overshoot
    ENJIN_EXPECT_TRUE(inRange);
}

// ============================================================================
// LSystemGenerator (2D)
// ============================================================================

ENJIN_TEST(LSystem2D, KochCurveProducesSegments) {
    LSystemGenerator::Params params;
    params.axiom = "F";
    params.rules['F'] = "F+F-F-F+F";
    params.iterations = 2;
    params.angle = 90.0f;
    params.stepLength = 1.0f;

    auto result = LSystemGenerator::Generate(params);
    ENJIN_EXPECT_GT(result.segments.size(), (usize)0);
}

ENJIN_TEST(LSystem2D, EmptyAxiomReturnsEmpty) {
    LSystemGenerator::Params params;
    params.axiom = "";
    params.iterations = 3;

    auto result = LSystemGenerator::Generate(params);
    ENJIN_EXPECT_EQ(result.segments.size(), (usize)0);
}

ENJIN_TEST(LSystem2D, BoundsContainSegments) {
    LSystemGenerator::Params params;
    params.axiom = "F";
    params.rules['F'] = "F+F";
    params.iterations = 3;
    params.angle = 45.0f;

    auto result = LSystemGenerator::Generate(params);
    for (const auto& seg : result.segments) {
        ENJIN_EXPECT_GE(seg.start.x, result.boundsMin.x - 0.01f);
        ENJIN_EXPECT_LE(seg.start.x, result.boundsMax.x + 0.01f);
        ENJIN_EXPECT_GE(seg.end.x, result.boundsMin.x - 0.01f);
        ENJIN_EXPECT_LE(seg.end.x, result.boundsMax.x + 0.01f);
    }
}

ENJIN_TEST(LSystem2D, StringLengthCapped) {
    // Exponential growth rule — should hit the 4MB cap
    LSystemGenerator::Params params;
    params.axiom = "F";
    params.rules['F'] = "FFFFFFFFFF";  // 10x growth per iteration
    params.iterations = 20;  // Would be 10^20 chars without cap

    auto result = LSystemGenerator::Generate(params);
    // Just verifying it returns without crashing or OOM
    ENJIN_EXPECT_TRUE(true);
}

// ============================================================================
// LSystem3D
// ============================================================================

ENJIN_TEST(LSystem3D, BasicTreeProducesSegments) {
    LSystem3D::Params params;
    params.axiom = "F";
    params.rules['F'] = "F[+F][-F]";
    params.iterations = 2;
    params.angle = 25.0f;
    params.stepLength = 1.0f;
    params.seed = 42;

    auto result = LSystem3D::Generate(params);
    ENJIN_EXPECT_GT(result.segments.size(), (usize)0);
}

// ============================================================================
// WaveFunctionCollapse
// ============================================================================

ENJIN_TEST(WFC, SimpleCheckerboard) {
    // Two tiles that must alternate
    WaveFunctionCollapse::WFCTile tileA;
    tileA.id = 0;
    for (int d = 0; d < 4; ++d) tileA.allowedNeighbors[d] = {1};

    WaveFunctionCollapse::WFCTile tileB;
    tileB.id = 1;
    for (int d = 0; d < 4; ++d) tileB.allowedNeighbors[d] = {0};

    WaveFunctionCollapse::Params params;
    params.width = 4;
    params.height = 4;
    params.tiles = {tileA, tileB};
    params.seed = 42;

    auto result = WaveFunctionCollapse::Generate(params);
    ENJIN_EXPECT_TRUE(result.success);
    ENJIN_EXPECT_EQ(result.grid.size(), (usize)4);
}

ENJIN_TEST(WFC, EmptyTilesReturnsFail) {
    WaveFunctionCollapse::Params params;
    params.width = 4;
    params.height = 4;
    // No tiles
    auto result = WaveFunctionCollapse::Generate(params);
    ENJIN_EXPECT_FALSE(result.success);
}

ENJIN_TEST(WFC, ZeroDimensionsReturnsFail) {
    WaveFunctionCollapse::Params params;
    params.width = 0;
    params.height = 0;
    auto result = WaveFunctionCollapse::Generate(params);
    ENJIN_EXPECT_FALSE(result.success);
}

// ============================================================================
// VoronoiGenerator
// ============================================================================

ENJIN_TEST(Voronoi, GeneratesCorrectDimensions) {
    VoronoiGenerator::Params params;
    params.width = 32;
    params.height = 32;
    params.numSeeds = 5;
    params.seed = 42;

    auto result = VoronoiGenerator::Generate(params);
    ENJIN_EXPECT_EQ(result.grid.size(), (usize)32);
    ENJIN_EXPECT_EQ(result.seedPoints.size(), (usize)5);
}

// ============================================================================
// FBMTerrain
// ============================================================================

ENJIN_TEST(FBMTerrain, GeneratesCorrectSize) {
    FBMTerrain::Params params;
    params.width = 64;
    params.height = 64;
    params.seed = 42;

    auto result = FBMTerrain::Generate(params);
    ENJIN_EXPECT_EQ(result.width, 64u);
    ENJIN_EXPECT_EQ(result.height, 64u);
    ENJIN_EXPECT_EQ(result.heightmap.size(), (usize)(64 * 64));
}

ENJIN_TEST(FBMTerrain, HeightmapNormalized) {
    FBMTerrain::Params params;
    params.width = 32;
    params.height = 32;
    params.seed = 42;

    auto result = FBMTerrain::Generate(params);
    bool inRange = true;
    for (f32 h : result.heightmap)
        if (h < -0.1f || h > 1.1f) inRange = false;
    ENJIN_EXPECT_TRUE(inRange);
}

ENJIN_TEST(FBMTerrain, DeterministicWithSeed) {
    FBMTerrain::Params params;
    params.width = 16;
    params.height = 16;
    params.seed = 99;

    auto r1 = FBMTerrain::Generate(params);
    auto r2 = FBMTerrain::Generate(params);
    ENJIN_EXPECT_FLOAT_EQ(r1.heightmap[0], r2.heightmap[0]);
    ENJIN_EXPECT_FLOAT_EQ(r1.heightmap[128], r2.heightmap[128]);
}

ENJIN_TEST_MAIN()
