#include "Enjin/ECS/Systems/DungeonGeneratorSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include <random>

namespace Enjin {
namespace ECS {

u32 DungeonGeneratorSystem::Generate(DungeonGeneratorComponent& gen, TilemapComponent& tilemap) {
    u32 w = gen.width  < 4u ? 4u : (gen.width  > 512u ? 512u : gen.width);
    u32 h = gen.height < 4u ? 4u : (gen.height > 512u ? 512u : gen.height);

    // seed 0 = pick a fresh non-deterministic seed; otherwise use the authored
    // one so the same seed always reproduces the same dungeon.
    u32 seed = gen.seed;
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
        if (seed == 0) seed = 1;
    }
    gen.lastSeed = seed;

    // Run the chosen algorithm into a shared 0/1 grid (1 = solid wall).
    std::vector<std::vector<u8>> grid;
    switch (gen.algorithm) {
        case DungeonGeneratorComponent::Algorithm::RandomWalk: {
            Procedural::RandomWalker::Params p;
            p.width = w; p.height = h; p.steps = gen.walkSteps;
            p.turnChance = gen.walkTurnChance; p.seed = seed;
            grid = Procedural::RandomWalker::Generate(p).grid;   // 0 = carved, 1 = solid
            break;
        }
        case DungeonGeneratorComponent::Algorithm::Cellular: {
            Procedural::CellularAutomata::Params p;
            p.width = w; p.height = h; p.fillPercent = gen.fillPercent;
            p.iterations = gen.iterations; p.seed = seed;
            grid = Procedural::CellularAutomata::Generate(p).grid; // 0 = empty, 1 = solid
            break;
        }
        case DungeonGeneratorComponent::Algorithm::BSPRooms: {
            Procedural::BSPGenerator::Params p;
            p.width = w; p.height = h; p.minRoomSize = gen.minRoomSize;
            p.maxRoomSize = gen.maxRoomSize; p.splitDepth = gen.splitDepth;
            p.corridorWidth = gen.corridorWidth; p.seed = seed;
            // BSP grid: 1 = room/corridor (walkable), 0 = empty. Invert so 1 = solid
            // like the other two, so the floor/wall mapping below is uniform.
            auto r = Procedural::BSPGenerator::Generate(p);
            grid.assign(h, std::vector<u8>(w, 1));
            for (u32 y = 0; y < h && y < r.grid.size(); ++y)
                for (u32 x = 0; x < w && x < r.grid[y].size(); ++x)
                    grid[y][x] = r.grid[y][x] ? 0u : 1u;
            break;
        }
    }

    // Paint the tilemap: solid -> wallTile, carved -> floorTile.
    tilemap.width = w;
    tilemap.height = h;
    tilemap.tiles.assign(static_cast<usize>(w) * h, -1);
    if (gen.fillCollision) tilemap.collisionMask.assign(static_cast<usize>(w) * h, false);

    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            bool solid = (y < grid.size() && x < grid[y].size()) ? (grid[y][x] != 0) : true;
            i32 tile = solid ? gen.wallTile : gen.floorTile;
            tilemap.tiles[static_cast<usize>(y) * w + x] = tile;
            if (gen.fillCollision) tilemap.collisionMask[static_cast<usize>(y) * w + x] = solid;
        }
    }
    if (gen.fillCollision) tilemap.hasCollision = true;
    tilemap.meshDirty = true;
    return seed;
}

void DungeonGeneratorSystem::GenerateAll(World* world) {
    if (!world) return;
    for (Entity e : world->GetEntitiesWithComponent<DungeonGeneratorComponent>()) {
        auto* gen = world->GetComponent<DungeonGeneratorComponent>(e);
        if (!gen || !gen->generateOnStart) continue;
        auto* tm = world->GetComponent<TilemapComponent>(e);
        if (!tm) tm = &world->AddComponent<TilemapComponent>(e);
        Generate(*gen, *tm);
    }
}

} // namespace ECS
} // namespace Enjin
