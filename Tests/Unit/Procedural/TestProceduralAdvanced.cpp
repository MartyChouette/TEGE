#include "EnjinTest.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include "Enjin/Procedural/LevelGenerator.h"

using namespace Enjin;
using namespace Enjin::Procedural;

// ===========================================================================
// GrammarGenerator
// ===========================================================================

ENJIN_TEST(Grammar, ParamsDefaults) {
    GrammarGenerator::Params p;
    ENJIN_EXPECT_EQ(p.iterations, 4u);
    ENJIN_EXPECT_EQ(p.seed, 0u);
    ENJIN_EXPECT_TRUE(p.startSymbol.empty());
    ENJIN_EXPECT_EQ(p.rules.size(), (size_t)0);
}

ENJIN_TEST(Grammar, RuleDefaults) {
    GrammarGenerator::GrammarRule r;
    ENJIN_EXPECT_FLOAT_EQ(r.weight, 1.0f);
    ENJIN_EXPECT_TRUE(r.symbol.empty());
    ENJIN_EXPECT_EQ(r.replacements.size(), (size_t)0);
}

ENJIN_TEST(Grammar, ShapeDefaults) {
    GrammarGenerator::GrammarShape s;
    ENJIN_EXPECT_EQ((int)s.type, (int)GrammarGenerator::ShapeType::Box);
    ENJIN_EXPECT_FLOAT_EQ(s.rotation, 0.0f);
}

ENJIN_TEST(Grammar, GenerateWithStartSymbol) {
    GrammarGenerator::Params p;
    p.startSymbol = "S";
    auto result = GrammarGenerator::Generate(p);
    // Even with no rules, the start symbol may produce a default shape
}

ENJIN_TEST(Grammar, GenerateWithRules) {
    GrammarGenerator::Params p;
    p.startSymbol = "House";
    p.iterations = 1;
    p.seed = 42;

    GrammarGenerator::GrammarRule rule;
    rule.symbol = "House";
    rule.replacements.push_back("Wall Floor Roof");
    p.rules.push_back(rule);

    auto result = GrammarGenerator::Generate(p);
    // Should produce at least some shapes from expansion
    // (exact count depends on implementation)
}

// ===========================================================================
// HydraulicErosion
// ===========================================================================

ENJIN_TEST(HydraulicErosion, ParamsDefaults) {
    HydraulicErosion::Params p;
    ENJIN_EXPECT_EQ(p.iterations, 50000u);
    ENJIN_EXPECT_EQ(p.dropLifetime, 64u);
    ENJIN_EXPECT_FLOAT_EQ(p.sedimentCapacity, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.depositRate, 0.3f);
    ENJIN_EXPECT_FLOAT_EQ(p.erosionRate, 0.3f);
    ENJIN_EXPECT_FLOAT_NEAR(p.evaporateRate, 0.01f, 0.001f);
    ENJIN_EXPECT_FLOAT_EQ(p.gravity, 4.0f);
    ENJIN_EXPECT_FLOAT_NEAR(p.minSlope, 0.01f, 0.001f);
    ENJIN_EXPECT_EQ(p.erosionRadius, 3u);
    ENJIN_EXPECT_FLOAT_NEAR(p.inertia, 0.05f, 0.001f);
    ENJIN_EXPECT_EQ(p.seed, 0u);
}

ENJIN_TEST(HydraulicErosion, ModifiesHeightmap) {
    const u32 size = 32;
    std::vector<f32> heightmap(size * size);
    // Create a peaked heightmap
    for (u32 y = 0; y < size; ++y)
        for (u32 x = 0; x < size; ++x) {
            f32 dx = (f32)x / size - 0.5f;
            f32 dy = (f32)y / size - 0.5f;
            heightmap[y * size + x] = 1.0f - (dx * dx + dy * dy) * 4.0f;
        }

    std::vector<f32> original = heightmap;

    HydraulicErosion::Params p;
    p.iterations = 500;  // Small for test speed
    p.seed = 42;
    HydraulicErosion::Erode(heightmap, size, size, p);

    // At least some cells should change
    bool changed = false;
    for (size_t i = 0; i < heightmap.size(); ++i) {
        if (heightmap[i] != original[i]) { changed = true; break; }
    }
    ENJIN_EXPECT_TRUE(changed);
}

// ===========================================================================
// ThermalErosion
// ===========================================================================

ENJIN_TEST(ThermalErosion, ParamsDefaults) {
    ThermalErosion::Params p;
    ENJIN_EXPECT_EQ(p.iterations, 50u);
    ENJIN_EXPECT_FLOAT_NEAR(p.talusAngle, 0.8f, 0.01f);
    ENJIN_EXPECT_FLOAT_EQ(p.erosionRate, 0.5f);
}

ENJIN_TEST(ThermalErosion, SmoothsSharpEdges) {
    const u32 size = 16;
    std::vector<f32> heightmap(size * size, 0.0f);
    // Create a sharp peak in center
    heightmap[8 * size + 8] = 10.0f;

    f32 peakBefore = heightmap[8 * size + 8];

    ThermalErosion::Params p;
    p.iterations = 20;
    p.talusAngle = 0.5f;
    ThermalErosion::Erode(heightmap, size, size, p);

    // Peak should be lower after thermal erosion
    ENJIN_EXPECT_TRUE(heightmap[8 * size + 8] < peakBefore);
}

// ===========================================================================
// FBMTerrain
// ===========================================================================

ENJIN_TEST(FBMTerrain, ParamsDefaults) {
    FBMTerrain::Params p;
    ENJIN_EXPECT_EQ(p.width, 256u);
    ENJIN_EXPECT_EQ(p.height, 256u);
    ENJIN_EXPECT_EQ(p.octaves, 6u);
    ENJIN_EXPECT_FLOAT_EQ(p.lacunarity, 2.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.gain, 0.5f);
    ENJIN_EXPECT_FLOAT_EQ(p.frequency, 1.0f);
    ENJIN_EXPECT_EQ(p.seed, 0u);
}

ENJIN_TEST(FBMTerrain, GenerateProducesHeightmap) {
    FBMTerrain::Params p;
    p.width = 32;
    p.height = 32;
    p.seed = 42;
    auto result = FBMTerrain::Generate(p);
    ENJIN_EXPECT_EQ(result.heightmap.size(), (size_t)(32 * 32));
}

ENJIN_TEST(FBMTerrain, DeterministicWithSeed) {
    FBMTerrain::Params p;
    p.width = 16;
    p.height = 16;
    p.seed = 123;
    auto r1 = FBMTerrain::Generate(p);
    auto r2 = FBMTerrain::Generate(p);
    ENJIN_EXPECT_EQ(r1.heightmap.size(), r2.heightmap.size());
    bool same = true;
    for (size_t i = 0; i < r1.heightmap.size(); ++i) {
        if (r1.heightmap[i] != r2.heightmap[i]) { same = false; break; }
    }
    ENJIN_EXPECT_TRUE(same);
}

// ===========================================================================
// LSystem3D
// ===========================================================================

ENJIN_TEST(LSystem3D, ParamsDefaults) {
    LSystem3D::Params p;
    ENJIN_EXPECT_EQ(p.iterations, 4u);
    ENJIN_EXPECT_FLOAT_NEAR(p.angle, 25.7f, 0.1f);
    ENJIN_EXPECT_FLOAT_EQ(p.stepLength, 1.0f);
    ENJIN_EXPECT_FLOAT_EQ(p.initialRadius, 1.0f);
    ENJIN_EXPECT_FLOAT_NEAR(p.radiusDecay, 0.7f, 0.01f);
    ENJIN_EXPECT_EQ(p.seed, 0u);
}

ENJIN_TEST(LSystem3D, GenerateSimpleTree) {
    LSystem3D::Params p;
    p.axiom = "F";
    p.rules['F'] = "F[+F][-F]";
    p.iterations = 2;
    p.seed = 42;
    auto result = LSystem3D::Generate(p);
    ENJIN_EXPECT_TRUE(result.segments.size() > (size_t)0);
}

// ===========================================================================
// LevelGenSettings Defaults
// ===========================================================================

ENJIN_TEST(LevelGen, SettingsDefaults) {
    LevelGenSettings s;
    ENJIN_EXPECT_EQ(s.minRooms, 5u);
    ENJIN_EXPECT_EQ(s.maxRooms, 20u);
    ENJIN_EXPECT_EQ(s.targetRooms, 10u);
    ENJIN_EXPECT_FLOAT_EQ(s.roomSpacing, 0.0f);
    ENJIN_EXPECT_FALSE(s.allowOverlap);
    ENJIN_EXPECT_TRUE(s.snapToGrid);
    ENJIN_EXPECT_FLOAT_EQ(s.gridSize, 1.0f);
    ENJIN_EXPECT_FALSE(s.linearPath);
    ENJIN_EXPECT_FLOAT_NEAR(s.branchChance, 0.3f, 0.01f);
    ENJIN_EXPECT_EQ(s.maxBranchLength, 3u);
    ENJIN_EXPECT_EQ(s.maxDeadEnds, 5u);
    ENJIN_EXPECT_TRUE(s.requireBossRoom);
    ENJIN_EXPECT_TRUE(s.requireTreasureRoom);
    ENJIN_EXPECT_EQ(s.minTreasureRooms, 1u);
    ENJIN_EXPECT_EQ(s.seed, 0u);
}

ENJIN_TEST(LevelGen, ConnectionDirection) {
    ENJIN_EXPECT_EQ((int)GetOppositeDirection(ConnectionDirection::North),
                    (int)ConnectionDirection::South);
    ENJIN_EXPECT_EQ((int)GetOppositeDirection(ConnectionDirection::East),
                    (int)ConnectionDirection::West);
    ENJIN_EXPECT_EQ((int)GetOppositeDirection(ConnectionDirection::Up),
                    (int)ConnectionDirection::Down);
}

ENJIN_TEST(LevelGen, RoomPrefabDefaults) {
    RoomPrefab rp;
    ENJIN_EXPECT_FLOAT_EQ(rp.weight, 1.0f);
    ENJIN_EXPECT_EQ(rp.minCount, 0u);
    ENJIN_EXPECT_TRUE(rp.allowRotation);
    ENJIN_EXPECT_FALSE(rp.allowMirror);
    ENJIN_EXPECT_EQ(rp.minDistanceFromStart, 0u);
}

ENJIN_TEST(LevelGen, ConnectionPointDefaults) {
    ConnectionPoint cp;
    ENJIN_EXPECT_FALSE(cp.required);
}

// ===========================================================================
// LevelGenerator Instance
// ===========================================================================

ENJIN_TEST(LevelGen, AddAndGetPrefab) {
    LevelGenerator gen;
    RoomPrefab rp;
    rp.id = "test_room";
    rp.size = Math::Vector3(5, 3, 5);
    gen.AddPrefab(rp);

    auto* found = gen.GetPrefab("test_room");
    ENJIN_EXPECT_NOT_NULL(found);

    auto* notFound = gen.GetPrefab("nonexistent");
    ENJIN_EXPECT_TRUE(notFound == nullptr);
}

ENJIN_TEST(LevelGen, RemoveAndClearPrefabs) {
    LevelGenerator gen;
    RoomPrefab a, b;
    a.id = "a"; b.id = "b";
    gen.AddPrefab(a);
    gen.AddPrefab(b);
    ENJIN_EXPECT_EQ(gen.GetPrefabs().size(), (size_t)2);

    gen.RemovePrefab("a");
    ENJIN_EXPECT_TRUE(gen.GetPrefab("a") == nullptr);
    ENJIN_EXPECT_EQ(gen.GetPrefabs().size(), (size_t)1);

    gen.ClearPrefabs();
    ENJIN_EXPECT_EQ(gen.GetPrefabs().size(), (size_t)0);
}

ENJIN_TEST(LevelGen, GenerationStatsDefaults) {
    LevelGenerator gen;
    auto& stats = gen.GetStats();
    ENJIN_EXPECT_EQ(stats.totalAttempts, 0u);
    ENJIN_EXPECT_EQ(stats.roomsPlaced, 0u);
    ENJIN_EXPECT_EQ(stats.connectionsUsed, 0u);
    ENJIN_EXPECT_EQ(stats.deadEnds, 0u);
    ENJIN_EXPECT_FLOAT_EQ(stats.generationTimeMs, 0.0f);
}

ENJIN_TEST(LevelGen, InitialRoomCount) {
    LevelGenerator gen;
    ENJIN_EXPECT_EQ(gen.GetRoomCount(), 0u);
    ENJIN_EXPECT_TRUE(gen.GetPlacedRooms().empty());
    ENJIN_EXPECT_TRUE(gen.GetStartRoom() == nullptr);
    ENJIN_EXPECT_TRUE(gen.GetEndRoom() == nullptr);
}

// ===========================================================================
// LevelGen2D
// ===========================================================================

ENJIN_TEST(LevelGen2D, SettingsDefaults) {
    LevelGen2DSettings s;
    ENJIN_EXPECT_EQ(s.minRooms, 5u);
    ENJIN_EXPECT_EQ(s.maxRooms, 15u);
    ENJIN_EXPECT_EQ(s.tileSize, 16u);
    ENJIN_EXPECT_TRUE(s.horizontalBias);
    ENJIN_EXPECT_FLOAT_NEAR(s.verticalChance, 0.3f, 0.01f);
    ENJIN_EXPECT_EQ(s.seed, 0u);
}

ENJIN_TEST(LevelGen2D, Room2DDefaults) {
    Room2D r;
    ENJIN_EXPECT_FLOAT_EQ(r.weight, 1.0f);
    ENJIN_EXPECT_TRUE(r.id.empty());
    ENJIN_EXPECT_EQ(r.tiles.size(), (size_t)0);
}

// ===========================================================================
// PrefabHelpers
// ===========================================================================

ENJIN_TEST(PrefabHelpers, CreateRectangularRoom) {
    auto room = PrefabHelpers::CreateRectangularRoom("rect1",
        Math::Vector3(10, 4, 8), true, true, false, false);
    ENJIN_EXPECT_TRUE(room.id == "rect1");
    ENJIN_EXPECT_FLOAT_EQ(room.size.x, 10.0f);
    ENJIN_EXPECT_FLOAT_EQ(room.size.y, 4.0f);
    ENJIN_EXPECT_FLOAT_EQ(room.size.z, 8.0f);
    // North+South doors = at least 2 connections
    ENJIN_EXPECT_TRUE(room.connections.size() >= (size_t)2);
}

ENJIN_TEST(PrefabHelpers, CreateCorridor) {
    auto corridor = PrefabHelpers::CreateCorridor("hall1", 10.0f, 3.0f, 3.0f, true);
    ENJIN_EXPECT_TRUE(corridor.id == "hall1");
    ENJIN_EXPECT_TRUE(corridor.connections.size() >= (size_t)2);
}

ENJIN_TEST(PrefabHelpers, CreateTJunction) {
    auto tj = PrefabHelpers::CreateTJunction("tj1", 5.0f, 3.0f);
    ENJIN_EXPECT_TRUE(tj.id == "tj1");
    // T-junction should have 3 connections
    ENJIN_EXPECT_TRUE(tj.connections.size() >= (size_t)3);
}

ENJIN_TEST(PrefabHelpers, CreateCrossroads) {
    auto cross = PrefabHelpers::CreateCrossroads("cross1", 5.0f, 3.0f);
    ENJIN_EXPECT_TRUE(cross.id == "cross1");
    // Crossroads should have 4 connections
    ENJIN_EXPECT_TRUE(cross.connections.size() >= (size_t)4);
}

ENJIN_TEST(PrefabHelpers, CreateRoom2D) {
    auto room = PrefabHelpers::CreateRoom2D("room2d", 8, 6, true, true, false, false);
    ENJIN_EXPECT_TRUE(room.id == "room2d");
    ENJIN_EXPECT_TRUE(room.connections.size() >= (size_t)2);
}

ENJIN_TEST(PrefabHelpers, CreatePlatform2D) {
    auto plat = PrefabHelpers::CreatePlatform2D("plat1", 10, true, true);
    ENJIN_EXPECT_TRUE(plat.id == "plat1");
    ENJIN_EXPECT_TRUE(plat.connections.size() >= (size_t)2);
}

// ===========================================================================
// PrefabAssembler
// ===========================================================================

ENJIN_TEST(PrefabAssembler, ParamsDefaults) {
    PrefabAssembler::Params p;
    ENJIN_EXPECT_EQ(p.maxRooms, 10u);
    ENJIN_EXPECT_EQ(p.seed, 0u);
    ENJIN_EXPECT_EQ(p.slots.size(), (size_t)0);
}

ENJIN_TEST(PrefabAssembler, PlacementDefaults) {
    PrefabAssembler::Placement pl;
    ENJIN_EXPECT_EQ(pl.slotIndex, 0u);
    ENJIN_EXPECT_FLOAT_EQ(pl.rotation, 0.0f);
}

ENJIN_TEST_MAIN()
