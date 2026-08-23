#include "Enjin/ECS/Systems/TerrainGeneratorSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/Procedural/ProceduralAlgorithms.h"
#include <random>
#include <algorithm>
#include <cmath>

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

    terrain.splatmap.assign(static_cast<usize>(w) * h * 4, 0.0f);
    if (!gen.autoSplat) {
        // First splat layer covers everything (matches TerrainComponent::InitializeFlat).
        for (usize i = 0; i < static_cast<usize>(w) * h; ++i)
            terrain.splatmap[i * 4 + 0] = 1.0f;
    } else {
        // Auto-splat: weight the 4 layers from slope + height.
        //   layer 1 (rock)  - slope above rockSlopeDeg
        //   layer 2 (snow)  - above snowHeightFrac, suppressed on steeps
        //   layer 3 (shore) - below shoreHeightFrac, suppressed on steeps
        //   layer 0 (base)  - whatever weight remains
        auto smoothstep = [](f32 lo, f32 hi, f32 x) {
            if (hi <= lo) return x >= hi ? 1.0f : 0.0f;
            f32 t = std::clamp((x - lo) / (hi - lo), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        };
        const f32 cell = gen.cellSize > 1e-4f ? gen.cellSize : 1.0f;
        const f32 blendSlope = std::max(1.0f, gen.rockSlopeDeg * gen.splatBlend);
        const f32 blendH = std::max(0.01f, gen.splatBlend);
        auto heightAt = [&](i32 x, i32 z) {
            x = std::clamp(x, 0, static_cast<i32>(w) - 1);
            z = std::clamp(z, 0, static_cast<i32>(h) - 1);
            return terrain.heightmap[static_cast<usize>(z) * w + x];
        };
        for (u32 z = 0; z < h; ++z) {
            for (u32 x = 0; x < w; ++x) {
                // Central-difference slope in world units -> angle in degrees.
                f32 dx = (heightAt(static_cast<i32>(x) + 1, z) - heightAt(static_cast<i32>(x) - 1, z)) / (2.0f * cell);
                f32 dz = (heightAt(x, static_cast<i32>(z) + 1) - heightAt(x, static_cast<i32>(z) - 1)) / (2.0f * cell);
                f32 slopeDeg = std::atan(std::sqrt(dx * dx + dz * dz)) * 57.2957795f;
                f32 hFrac = gen.maxHeight > 1e-4f
                              ? terrain.heightmap[static_cast<usize>(z) * w + x] / gen.maxHeight : 0.0f;

                f32 rock = smoothstep(gen.rockSlopeDeg - blendSlope, gen.rockSlopeDeg + blendSlope, slopeDeg);
                f32 flat = 1.0f - rock;
                f32 snow = smoothstep(gen.snowHeightFrac - blendH, gen.snowHeightFrac + blendH, hFrac) * flat;
                f32 shore = (1.0f - smoothstep(gen.shoreHeightFrac - blendH, gen.shoreHeightFrac + blendH, hFrac)) * flat;
                f32 base = std::max(0.0f, 1.0f - rock - snow - shore);

                f32 sum = base + rock + snow + shore;
                if (sum < 1e-4f) { base = 1.0f; sum = 1.0f; }
                usize idx = (static_cast<usize>(z) * w + x) * 4;
                terrain.splatmap[idx + 0] = base / sum;
                terrain.splatmap[idx + 1] = rock / sum;
                terrain.splatmap[idx + 2] = snow / sum;
                terrain.splatmap[idx + 3] = shore / sum;
            }
        }
    }

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
