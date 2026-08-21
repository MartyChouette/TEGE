#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Scripting/ASCallConv.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include "Enjin/Procedural/LevelGenerator.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/RandomBag.h"
#include "Enjin/ECS/Systems/RandomBagSystem.h"
#include "Enjin/ECS/Components/Scatter.h"
#include "Enjin/ECS/Systems/ScatterSystem.h"
#include "Enjin/ECS/Components/TerrainGenerator.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Systems/TerrainGeneratorSystem.h"
#include "Enjin/ECS/Components/WFC.h"
#include "Enjin/ECS/Components/Gameplay.h"   // TilemapComponent
#include "Enjin/ECS/Systems/WFCSystem.h"
#include <angelscript.h>
#include <cassert>

using namespace Enjin;

#define AS_CHECK(expr) \
    do { int _r = (expr); if (_r < 0) { ENJIN_LOG_ERROR(Script, "AS registration failed (code %d) at %s:%d", _r, __FILE__, __LINE__); } } while(0)

extern ECS::World* s_BindingsWorld;

static Procedural::LevelGenerator* s_BindingsLevelGenerator = nullptr;

namespace Enjin {
namespace Scripting {
void SetBindingsProcedural(Procedural::LevelGenerator* generator) {
    s_BindingsLevelGenerator = generator;
}
} // namespace Scripting
} // namespace Enjin

// ---------------------------------------------------------------------------
// Cached result grids (latest generation output)
// ---------------------------------------------------------------------------
static std::vector<std::vector<u8>> s_GridResultU8;
static std::vector<std::vector<u32>> s_GridResultU32;
static std::vector<std::vector<f32>> s_HeightmapResult;
static u32 s_ResultWidth = 0;
static u32 s_ResultHeight = 0;

// ============================================================================
// Cellular Automata
// ============================================================================
static void ProceduralGen_CellularAutomata(u32 width, u32 height, u32 fillPct, u32 smoothPasses, u32 seed) {
    Procedural::CellularAutomata::Params p;
    p.width = width;
    p.height = height;
    p.fillPercent = fillPct;
    p.iterations = smoothPasses;
    p.seed = seed;
    auto result = Procedural::CellularAutomata::Generate(p);
    s_GridResultU8 = std::move(result.grid);
    s_GridResultU32.clear();
    s_HeightmapResult.clear();
    s_ResultWidth = result.width;
    s_ResultHeight = result.height;
}

// ============================================================================
// Random Walker
// ============================================================================
static void ProceduralGen_RandomWalker(u32 width, u32 height, u32 steps, u32 seed) {
    Procedural::RandomWalker::Params p;
    p.width = width;
    p.height = height;
    p.steps = steps;
    p.seed = seed;
    auto result = Procedural::RandomWalker::Generate(p);
    s_GridResultU8 = std::move(result.grid);
    s_GridResultU32.clear();
    s_HeightmapResult.clear();
    s_ResultWidth = width;
    s_ResultHeight = height;
}

// ============================================================================
// BSP
// ============================================================================
static void ProceduralGen_BSP(u32 width, u32 height, u32 minRoom, u32 maxRoom, u32 seed) {
    Procedural::BSPGenerator::Params p;
    p.width = width;
    p.height = height;
    p.minRoomSize = minRoom;
    p.maxRoomSize = maxRoom;
    p.seed = seed;
    auto result = Procedural::BSPGenerator::Generate(p);
    s_GridResultU8 = std::move(result.grid);
    s_GridResultU32.clear();
    s_HeightmapResult.clear();
    s_ResultWidth = width;
    s_ResultHeight = height;
}

// ============================================================================
// Diamond-Square (heightmap)
// ============================================================================
static void ProceduralGen_DiamondSquare(u32 size, float roughness, u32 seed) {
    Procedural::DiamondSquare::Params p;
    p.size = size;
    p.roughness = roughness;
    p.seed = seed;
    auto result = Procedural::DiamondSquare::Generate(p);
    s_HeightmapResult = std::move(result.heightmap);
    s_GridResultU8.clear();
    s_GridResultU32.clear();
    s_ResultWidth = result.size;
    s_ResultHeight = result.size;
}

// ============================================================================
// L-System (returns generated string)
// ============================================================================
static std::string ProceduralGen_LSystem(const std::string& axiom, const std::string& rulesStr, u32 iterations) {
    Procedural::LSystemGenerator::Params p;
    p.axiom = axiom;
    p.iterations = iterations;

    // Parse rules from "F=F+F-F;G=GG" format
    size_t pos = 0;
    std::string rules = rulesStr;
    while (pos < rules.size()) {
        size_t eq = rules.find('=', pos);
        if (eq == std::string::npos || eq == pos) break;
        char symbol = rules[pos];
        size_t semi = rules.find(';', eq);
        std::string replacement = rules.substr(eq + 1, (semi == std::string::npos) ? std::string::npos : semi - eq - 1);
        p.rules[symbol] = replacement;
        pos = (semi == std::string::npos) ? rules.size() : semi + 1;
    }

    auto result = Procedural::LSystemGenerator::Generate(p);

    // Reconstruct the generated string from segments (return axiom after iteration)
    // The actual expanded string is obtained by iterating axiom through rules
    std::string expanded = axiom;
    for (u32 i = 0; i < iterations; i++) {
        std::string next;
        next.reserve(expanded.size() * 2);
        for (char c : expanded) {
            auto it = p.rules.find(c);
            if (it != p.rules.end())
                next += it->second;
            else
                next += c;
        }
        expanded = std::move(next);
    }
    return expanded;
}

// ============================================================================
// Voronoi
// ============================================================================
static void ProceduralGen_Voronoi(u32 width, u32 height, u32 numPoints, u32 seed) {
    Procedural::VoronoiGenerator::Params p;
    p.width = width;
    p.height = height;
    p.numSeeds = numPoints;
    p.seed = seed;
    auto result = Procedural::VoronoiGenerator::Generate(p);
    s_GridResultU32 = std::move(result.grid);
    s_GridResultU8.clear();
    s_HeightmapResult.clear();
    s_ResultWidth = width;
    s_ResultHeight = height;
}

// ============================================================================
// Wave Function Collapse (WFC / "Wang Tiles")
// ============================================================================
static void ProceduralGen_WFC(u32 width, u32 height, u32 tileSetSize, u32 seed) {
    Procedural::WaveFunctionCollapse::Params p;
    p.width = width;
    p.height = height;
    p.seed = seed;

    // Create a simple tile set where each tile can neighbor any tile
    for (u32 i = 0; i < tileSetSize; i++) {
        Procedural::WaveFunctionCollapse::WFCTile tile;
        tile.id = i;
        for (int dir = 0; dir < 4; dir++) {
            for (u32 j = 0; j < tileSetSize; j++)
                tile.allowedNeighbors[dir].push_back(j);
        }
        p.tiles.push_back(tile);
    }

    auto result = Procedural::WaveFunctionCollapse::Generate(p);
    s_GridResultU32 = std::move(result.grid);
    s_GridResultU8.clear();
    s_HeightmapResult.clear();
    s_ResultWidth = width;
    s_ResultHeight = height;
}

// ============================================================================
// Grammar
// ============================================================================
static std::string ProceduralGen_Grammar(const std::string& rulesStr, const std::string& startSymbol, u32 seed) {
    Procedural::GrammarGenerator::Params p;
    p.startSymbol = startSymbol;
    p.seed = seed;
    p.iterations = 4;

    // Parse rules from "symbol=replacement1|replacement2;..." format
    size_t pos = 0;
    std::string rules = rulesStr;
    while (pos < rules.size()) {
        size_t eq = rules.find('=', pos);
        if (eq == std::string::npos) break;
        std::string symbol = rules.substr(pos, eq - pos);
        size_t semi = rules.find(';', eq);
        std::string replacementStr = rules.substr(eq + 1, (semi == std::string::npos) ? std::string::npos : semi - eq - 1);

        // Split by '|' for alternatives
        size_t rpos = 0;
        while (rpos < replacementStr.size()) {
            size_t pipe = replacementStr.find('|', rpos);
            std::string rep = replacementStr.substr(rpos, (pipe == std::string::npos) ? std::string::npos : pipe - rpos);
            Procedural::GrammarGenerator::GrammarRule rule;
            rule.symbol = symbol;
            rule.replacements.push_back(rep);
            p.rules.push_back(rule);
            rpos = (pipe == std::string::npos) ? replacementStr.size() : pipe + 1;
        }

        pos = (semi == std::string::npos) ? rules.size() : semi + 1;
    }

    auto result = Procedural::GrammarGenerator::Generate(p);

    // Return a summary of generated shapes
    std::string output;
    output.reserve(result.shapes.size() * 40);
    for (const auto& shape : result.shapes) {
        const char* typeNames[] = {"Box", "Cylinder", "Roof", "Wall", "Floor", "Window", "Door"};
        u32 typeIdx = static_cast<u32>(shape.type);
        output += (typeIdx < 7 ? typeNames[typeIdx] : "Unknown");
        output += " at (" + std::to_string(shape.position.x) + "," +
                  std::to_string(shape.position.y) + "," +
                  std::to_string(shape.position.z) + ")\n";
    }
    return output;
}

// ============================================================================
// Result Query Functions
// ============================================================================
static int ProceduralGen_GetCell(int x, int y) {
    if (x < 0 || y < 0) return 0;
    u32 ux = static_cast<u32>(x);
    u32 uy = static_cast<u32>(y);

    // u8 grid
    if (!s_GridResultU8.empty()) {
        if (uy < s_GridResultU8.size() && ux < (s_GridResultU8.empty() ? 0 : s_GridResultU8[0].size()))
            return static_cast<int>(s_GridResultU8[uy][ux]);
        return 0;
    }
    // u32 grid
    if (!s_GridResultU32.empty()) {
        if (uy < s_GridResultU32.size() && ux < (s_GridResultU32.empty() ? 0 : s_GridResultU32[0].size()))
            return static_cast<int>(s_GridResultU32[uy][ux]);
        return 0;
    }
    return 0;
}

static float ProceduralGen_GetHeight(int x, int y) {
    if (x < 0 || y < 0) return 0.0f;
    u32 ux = static_cast<u32>(x);
    u32 uy = static_cast<u32>(y);
    if (!s_HeightmapResult.empty()) {
        if (uy < s_HeightmapResult.size() && ux < (s_HeightmapResult.empty() ? 0 : s_HeightmapResult[0].size()))
            return s_HeightmapResult[uy][ux];
    }
    return 0.0f;
}

static int ProceduralGen_GetWidth() { return static_cast<int>(s_ResultWidth); }
static int ProceduralGen_GetHeight_Size() { return static_cast<int>(s_ResultHeight); }

// ============================================================================
// SpawnGrid — Creates entities from the latest u8 grid result
// ============================================================================
static void ProceduralGen_SpawnGrid(float cellSize, int wallValue, int floorValue) {
    if (!s_BindingsWorld || s_GridResultU8.empty()) return;

    for (u32 y = 0; y < s_GridResultU8.size(); y++) {
        for (u32 x = 0; x < s_GridResultU8[y].size(); x++) {
            u8 cell = s_GridResultU8[y][x];
            if (cell != static_cast<u8>(wallValue) && cell != static_cast<u8>(floorValue))
                continue;

            auto entity = s_BindingsWorld->CreateEntity();
            auto& transform = s_BindingsWorld->AddComponent<ECS::TransformComponent>(entity);
            transform.position = Math::Vector3(
                static_cast<f32>(x) * cellSize,
                0.0f,
                static_cast<f32>(y) * cellSize
            );
            transform.scale = Math::Vector3(cellSize, cellSize, cellSize);

            auto& name = s_BindingsWorld->AddComponent<ECS::NameComponent>(entity);
            name.name = (cell == static_cast<u8>(wallValue)) ? "Wall" : "Floor";
        }
    }
}

// ============================================================================
// Prefab Assembler (simplified binding)
// ============================================================================
static int ProceduralGen_PrefabAssemble(int maxRooms, u32 seed) {
    Procedural::PrefabAssembler::Params p;
    p.maxRooms = static_cast<u32>(maxRooms);
    p.seed = seed;
    auto result = Procedural::PrefabAssembler::Generate(p);
    return static_cast<int>(result.placements.size());
}

// ============================================================================
// RandomBag — stream-feeder procgen. Pulls one authored item per call from the
// entity's RandomBagComponent. Stateful (no-replacement modes draw a shuffled
// queue). Missing component = safe empty/no-op.
// ============================================================================
static std::string RandomBag_Draw(u64 self) {
    if (!s_BindingsWorld) return std::string();
    if (auto* b = s_BindingsWorld->GetComponent<ECS::RandomBagComponent>(self))
        return ECS::RandomBagSystem::Draw(*b);
    return std::string();
}
static int RandomBag_DrawIndex(u64 self) {
    if (!s_BindingsWorld) return -1;
    if (auto* b = s_BindingsWorld->GetComponent<ECS::RandomBagComponent>(self))
        return static_cast<int>(ECS::RandomBagSystem::DrawIndex(*b));
    return -1;
}
static void RandomBag_Reset(u64 self) {
    if (!s_BindingsWorld) return;
    if (auto* b = s_BindingsWorld->GetComponent<ECS::RandomBagComponent>(self))
        ECS::RandomBagSystem::Reset(*b);
}
static int RandomBag_Remaining(u64 self) {
    if (!s_BindingsWorld) return 0;
    if (auto* b = s_BindingsWorld->GetComponent<ECS::RandomBagComponent>(self))
        return static_cast<int>(ECS::RandomBagSystem::Remaining(*b));
    return 0;
}
static int RandomBag_Count(u64 self) {
    if (!s_BindingsWorld) return 0;
    if (auto* b = s_BindingsWorld->GetComponent<ECS::RandomBagComponent>(self))
        return static_cast<int>(b->items.size());
    return 0;
}

// ============================================================================
// Scatter — space-builder procgen. Re-rolls the entity's ScatterComponent batch
// (destroys the previous instances, places a fresh set). Missing component = no-op.
// ============================================================================
static int Scatter_Generate(u64 self) {
    if (!s_BindingsWorld) return 0;
    if (auto* sc = s_BindingsWorld->GetComponent<ECS::ScatterComponent>(self))
        return static_cast<int>(ECS::ScatterSystem::Generate(s_BindingsWorld, self, *sc));
    return 0;
}
static int Scatter_Clear(u64 self) {
    if (!s_BindingsWorld) return 0;
    return static_cast<int>(ECS::ScatterSystem::Clear(s_BindingsWorld, self));
}
static int Scatter_Count(u64 self) {
    if (!s_BindingsWorld) return 0;
    if (auto* sc = s_BindingsWorld->GetComponent<ECS::ScatterComponent>(self))
        return static_cast<int>(sc->lastCount);
    return 0;
}

// ============================================================================
// TerrainGenerator — space-builder procgen. Rebakes the entity's terrain heightmap
// from its TerrainGeneratorComponent (FBM + erosion). Creates a TerrainComponent if
// the entity lacks one. Missing generator component = no-op returning its seed 0.
// ============================================================================
static int TerrainGen_Generate(u64 self) {
    if (!s_BindingsWorld) return 0;
    auto* gen = s_BindingsWorld->GetComponent<ECS::TerrainGeneratorComponent>(self);
    if (!gen) return 0;
    auto* terrain = s_BindingsWorld->GetComponent<ECS::TerrainComponent>(self);
    if (!terrain) terrain = &s_BindingsWorld->AddComponent<ECS::TerrainComponent>(self);
    return static_cast<int>(ECS::TerrainGeneratorSystem::Generate(*gen, *terrain));
}

// ============================================================================
// WFC — constraint-based space-builder. Resolves the entity's WFCComponent into
// its TilemapComponent (creating one if absent). Returns 1 on a full collapse, 0
// on contradiction / missing component.
// ============================================================================
static int WFC_Generate(u64 self) {
    if (!s_BindingsWorld) return 0;
    auto* gen = s_BindingsWorld->GetComponent<ECS::WFCComponent>(self);
    if (!gen) return 0;
    // Dispatches on mode: 2D paints the Tilemap, 3D places prefab modules.
    return ECS::WFCSystem::Generate(s_BindingsWorld, self, *gen) ? 1 : 0;
}

// ============================================================================
// AngelScript Registration
// ============================================================================
namespace Enjin {
namespace Scripting {

void RegisterProceduralBindings(asIScriptEngine* engine) {
    // WFC constraint solver (author tiles+sockets in the editor, resolve from script)
    AS_CHECK(engine->RegisterGlobalFunction(
        "int WFC_Generate(uint64 self)",
        ENJIN_AS_FN(WFC_Generate), ENJIN_AS_CALL_CDECL));
    // Terrain space-builder (author in the editor, rebake from script)
    AS_CHECK(engine->RegisterGlobalFunction(
        "int TerrainGen_Generate(uint64 self)",
        ENJIN_AS_FN(TerrainGen_Generate), ENJIN_AS_CALL_CDECL));
    // Scatter space-builder (author in the editor, re-roll from script)
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Scatter_Generate(uint64 self)",
        ENJIN_AS_FN(Scatter_Generate), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Scatter_Clear(uint64 self)",
        ENJIN_AS_FN(Scatter_Clear), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int Scatter_Count(uint64 self)",
        ENJIN_AS_FN(Scatter_Count), ENJIN_AS_CALL_CDECL));
    // RandomBag stream-feeder (author items in the editor, pull from script)
    AS_CHECK(engine->RegisterGlobalFunction(
        "string RandomBag_Draw(uint64 self)",
        ENJIN_AS_FN(RandomBag_Draw), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int RandomBag_DrawIndex(uint64 self)",
        ENJIN_AS_FN(RandomBag_DrawIndex), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "void RandomBag_Reset(uint64 self)",
        ENJIN_AS_FN(RandomBag_Reset), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int RandomBag_Remaining(uint64 self)",
        ENJIN_AS_FN(RandomBag_Remaining), ENJIN_AS_CALL_CDECL));
    AS_CHECK(engine->RegisterGlobalFunction(
        "int RandomBag_Count(uint64 self)",
        ENJIN_AS_FN(RandomBag_Count), ENJIN_AS_CALL_CDECL));
    // Grid generation algorithms
    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_CellularAutomata(uint width, uint height, uint fillPct, uint smoothPasses, uint seed)",
        ENJIN_AS_FN(ProceduralGen_CellularAutomata), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_RandomWalker(uint width, uint height, uint steps, uint seed)",
        ENJIN_AS_FN(ProceduralGen_RandomWalker), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_BSP(uint width, uint height, uint minRoomSize, uint maxRoomSize, uint seed)",
        ENJIN_AS_FN(ProceduralGen_BSP), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_DiamondSquare(uint size, float roughness, uint seed)",
        ENJIN_AS_FN(ProceduralGen_DiamondSquare), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string ProceduralGen_LSystem(const string &in axiom, const string &in rules, uint iterations)",
        ENJIN_AS_FN(ProceduralGen_LSystem), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_Voronoi(uint width, uint height, uint numPoints, uint seed)",
        ENJIN_AS_FN(ProceduralGen_Voronoi), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_WFC(uint width, uint height, uint tileSetSize, uint seed)",
        ENJIN_AS_FN(ProceduralGen_WFC), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string ProceduralGen_Grammar(const string &in rules, const string &in startSymbol, uint seed)",
        ENJIN_AS_FN(ProceduralGen_Grammar), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_PrefabAssemble(int maxRooms, uint seed)",
        ENJIN_AS_FN(ProceduralGen_PrefabAssemble), ENJIN_AS_CALL_CDECL));

    // Result query functions
    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_GetCell(int x, int y)",
        ENJIN_AS_FN(ProceduralGen_GetCell), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float ProceduralGen_GetHeight(int x, int y)",
        ENJIN_AS_FN(ProceduralGen_GetHeight), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_GetWidth()",
        ENJIN_AS_FN(ProceduralGen_GetWidth), ENJIN_AS_CALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_GetGridHeight()",
        ENJIN_AS_FN(ProceduralGen_GetHeight_Size), ENJIN_AS_CALL_CDECL));

    // Entity spawning
    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_SpawnGrid(float cellSize, int wallValue, int floorValue)",
        ENJIN_AS_FN(ProceduralGen_SpawnGrid), ENJIN_AS_CALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
