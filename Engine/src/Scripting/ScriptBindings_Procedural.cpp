#include "Enjin/Scripting/ScriptBindings.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include "Enjin/Procedural/LevelGenerator.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Name.h"
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
// AngelScript Registration
// ============================================================================
namespace Enjin {
namespace Scripting {

void RegisterProceduralBindings(asIScriptEngine* engine) {
    // Grid generation algorithms
    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_CellularAutomata(uint width, uint height, uint fillPct, uint smoothPasses, uint seed)",
        asFUNCTION(ProceduralGen_CellularAutomata), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_RandomWalker(uint width, uint height, uint steps, uint seed)",
        asFUNCTION(ProceduralGen_RandomWalker), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_BSP(uint width, uint height, uint minRoomSize, uint maxRoomSize, uint seed)",
        asFUNCTION(ProceduralGen_BSP), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_DiamondSquare(uint size, float roughness, uint seed)",
        asFUNCTION(ProceduralGen_DiamondSquare), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string ProceduralGen_LSystem(const string &in axiom, const string &in rules, uint iterations)",
        asFUNCTION(ProceduralGen_LSystem), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_Voronoi(uint width, uint height, uint numPoints, uint seed)",
        asFUNCTION(ProceduralGen_Voronoi), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_WFC(uint width, uint height, uint tileSetSize, uint seed)",
        asFUNCTION(ProceduralGen_WFC), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "string ProceduralGen_Grammar(const string &in rules, const string &in startSymbol, uint seed)",
        asFUNCTION(ProceduralGen_Grammar), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_PrefabAssemble(int maxRooms, uint seed)",
        asFUNCTION(ProceduralGen_PrefabAssemble), asCALL_CDECL));

    // Result query functions
    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_GetCell(int x, int y)",
        asFUNCTION(ProceduralGen_GetCell), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "float ProceduralGen_GetHeight(int x, int y)",
        asFUNCTION(ProceduralGen_GetHeight), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_GetWidth()",
        asFUNCTION(ProceduralGen_GetWidth), asCALL_CDECL));

    AS_CHECK(engine->RegisterGlobalFunction(
        "int ProceduralGen_GetGridHeight()",
        asFUNCTION(ProceduralGen_GetHeight_Size), asCALL_CDECL));

    // Entity spawning
    AS_CHECK(engine->RegisterGlobalFunction(
        "void ProceduralGen_SpawnGrid(float cellSize, int wallValue, int floorValue)",
        asFUNCTION(ProceduralGen_SpawnGrid), asCALL_CDECL));
}

} // namespace Scripting
} // namespace Enjin
