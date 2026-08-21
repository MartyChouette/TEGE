#include "Enjin/ECS/Systems/TerrainGeneratorSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include <random>
#include <algorithm>

namespace Enjin {
namespace ECS {

u32 TerrainGeneratorSystem::Generate(TerrainGeneratorComponent& gen, TerrainComponent& terrain) {
    u32 w = std::clamp(gen.gridWidth,  4u, 1024u);
    u32 h = std::clamp(gen.gridHeight, 4u, 1024u);

    // seed 0 = fresh non-deterministic seed; otherwise reproducible.
    u32 seed = gen.seed;
    if (seed == 0) {
        std::random_device rd;
        seed = rd();
        if (seed == 0) seed = 1;
    }
    gen.lastSeed = seed;

    // Base shape: fractional Brownian motion, normalized to [0,1].
    Procedural::FBMTerrain::Params p;
    p.width = w; p.height = h;
    p.octaves = std::clamp(gen.octaves, 1u, 12u);
    p.lacunarity = gen.lacunarity;
    p.gain = gen.gain;
    p.frequency = gen.frequency;
    p.ridgedPower = gen.ridgedPower;
    p.mode = gen.ridged ? Procedural::FBMTerrain::NoiseMode::RidgedMultifractal
                        : Procedural::FBMTerrain::NoiseMode::Standard;
    p.seed = seed;
    auto result = Procedural::FBMTerrain::Generate(p);
    std::vector<f32> hm = std::move(result.heightmap);
    if (hm.size() != static_cast<usize>(w) * h) hm.assign(static_cast<usize>(w) * h, 0.0f);

    // Weather it. Both erosions run in-place on the normalized field.
    if (gen.thermal) {
        Procedural::ThermalErosion::Params tp;
        tp.iterations = std::clamp(gen.thermalIterations, 1u, 1000u);
        tp.talusAngle = gen.talusAngle;
        Procedural::ThermalErosion::Erode(hm, w, h, tp);
    }
    if (gen.hydraulic) {
        Procedural::HydraulicErosion::Params hp;
        hp.iterations = std::clamp(gen.hydraulicDroplets, 1000u, 500000u);
        hp.seed = seed;
        Procedural::HydraulicErosion::Erode(hm, w, h, hp);
    }

    // Erosion can push values slightly out of [0,1]; renormalize before scaling so
    // the terrain always fills [0, maxHeight] regardless of the erosion settings.
    f32 mn = hm.empty() ? 0.0f : hm[0];
    f32 mx = mn;
    for (f32 v : hm) { mn = std::min(mn, v); mx = std::max(mx, v); }
    f32 range = (mx - mn) > 1e-6f ? (mx - mn) : 1.0f;

    // Write into the terrain. Heightmap holds world-space Y directly (CreateTerrain
    // does not multiply by maxHeight), so scale here.
    terrain.gridWidth = w;
    terrain.gridHeight = h;
    terrain.cellSize = gen.cellSize;
    terrain.maxHeight = gen.maxHeight;
    terrain.heightmap.resize(static_cast<usize>(w) * h);
    for (usize i = 0; i < hm.size(); ++i)
        terrain.heightmap[i] = ((hm[i] - mn) / range) * gen.maxHeight;

    // First splat layer covers everything (matches TerrainComponent::InitializeFlat).
    terrain.splatmap.assign(static_cast<usize>(w) * h * 4, 0.0f);
    for (usize i = 0; i < static_cast<usize>(w) * h; ++i)
        terrain.splatmap[i * 4 + 0] = 1.0f;

    terrain.meshDirty = true;
    return seed;
}

void TerrainGeneratorSystem::GenerateAll(World* world) {
    if (!world) return;
    for (Entity e : world->GetEntitiesWithComponent<TerrainGeneratorComponent>()) {
        auto* gen = world->GetComponent<TerrainGeneratorComponent>(e);
        if (!gen || !gen->generateOnStart) continue;
        auto* terrain = world->GetComponent<TerrainComponent>(e);
        if (!terrain) terrain = &world->AddComponent<TerrainComponent>(e);
        Generate(*gen, *terrain);
    }
}

} // namespace ECS
} // namespace Enjin
