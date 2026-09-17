// What a 3D fluid step actually costs, measured rather than reasoned about.
//
// The 3D grid is capped at 48 and the solver clamps iterations to 10 above 32,
// and nobody had numbers for why or for what it would take to lift either. The
// arithmetic said a 128 grid is about 18x the work of 48; this says what the
// wall clock is.
//
// NO ENGINE CHANGES. The phases (Diffuse3D, Advect3D, Project3D) are private, so
// rather than instrument them this varies the ONE knob that changes how many
// relaxation sweeps run -- solverIterations -- and reads the split off the
// slope. Advection, boundaries and buoyancy run once per step whatever the
// iteration count, so:
//
//     time(iters) = fixed + iters * per_iteration
//
// Two measurements give both terms, and a third checks the line is straight.
// Instrumenting the solver would have measured the instrumentation too.
//
//     build/bin/Tests/Release/FluidBench.exe
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/FluidVolume.h"
#include "Enjin/Effects/FluidSimulation.h"

#include <chrono>
#include <cstdio>
#include <vector>

using namespace Enjin;

namespace {

struct Result {
    u32 grid = 0;          // asked for
    u32 actualGrid = 0;    // actually simulated, after the solver's clamp
    i32 iterations = 0;
    f64 msPerStep = 0.0;
    f64 megaCells = 0.0;
};

Result Measure(u32 gridSize, i32 iterations, int steps) {
    ECS::World world;
    ECS::Entity e = world.CreateEntity();
    world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});

    ECS::FluidVolumeComponent vol;
    vol.dimension = ECS::FluidDimension::Mode3D;
    vol.fluidType = ECS::FluidType::Smoke;
    vol.gridSize = gridSize;
    vol.solverIterations = iterations;
    vol.buoyancy = 1.0f;          // the smoke case, so the buoyancy pass is included
    vol.isActive = true;
    world.AddComponent<ECS::FluidVolumeComponent>(e, vol);

    Effects::FluidSimulation sim;

    // One step to allocate and to get the source plume going, so the timed
    // steps measure a grid with something in it. An empty grid advects nothing
    // and would flatter the result.
    sim.Update(1.0f / 60.0f, &world);
    for (int i = 0; i < 10; ++i) sim.Update(1.0f / 60.0f, &world);

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < steps; ++i) sim.Update(1.0f / 60.0f, &world);
    const auto t1 = std::chrono::high_resolution_clock::now();

    Result r;
    r.grid = gridSize;
    r.iterations = iterations;
    r.msPerStep = std::chrono::duration<f64, std::milli>(t1 - t0).count() / steps;

    // The SIMULATED size, not the requested one. FluidSimulation clamps 3D to 48
    // (FluidSimulation.cpp: `if (is3D) clampedSize = std::min(clampedSize, 48u)`),
    // silently -- so asking for 128 gets 48 and the first run of this benchmark
    // printed 2.197M cells for a grid that held 0.125M, alongside a timing
    // identical to the 48 case. Reading the real N back off the grid is what
    // turned "why is 128 free" into "128 does not exist".
    const Effects::FluidGridData* g = sim.GetGridData(e);
    r.actualGrid = g ? g->N : 0;
    const f64 n = f64(r.actualGrid) + 2.0;
    r.megaCells = (n * n * n) / 1.0e6;
    return r;
}

} // namespace

int main() {
    std::printf("3D fluid step cost, smoke preset, single-threaded CPU\n");
    std::printf("%-6s %-7s %-6s %10s %10s %12s\n",
                "asked", "actual", "iters", "cells(M)", "ms/step", "fps budget");

    // The solver clamps iterations to 10 above grid 32, so anything asked for
    // above that is really 10. Asked for below the clamp as well, to see the
    // line without it interfering.
    const std::vector<std::pair<u32, i32>> cases = {
        {16, 4}, {16, 10}, {16, 20},
        {32, 4}, {32, 10}, {32, 20},
        {48, 4}, {48, 10},
        {64, 4}, {64, 10},
        {96, 4},
        {128, 4},
    };

    std::vector<Result> results;
    for (const auto& c : cases) {
        // Fewer steps for the big ones or this takes minutes.
        const int steps = (c.first <= 32) ? 60 : (c.first <= 64 ? 20 : 5);
        Result r = Measure(c.first, c.second, steps);
        results.push_back(r);
        std::printf("%-6u %-7u %-6d %10.3f %10.2f %12s\n",
                    r.grid, r.actualGrid, r.iterations, r.megaCells, r.msPerStep,
                    r.msPerStep <= 16.6 ? "fits 60fps"
                                        : (r.msPerStep <= 33.3 ? "fits 30fps" : "no"));
    }

    // Split fixed cost from per-iteration cost, per grid, from the pairs above.
    std::printf("\nfixed vs per-iteration, solved from the pairs:\n");
    std::printf("%-6s %14s %18s\n", "grid", "fixed(ms)", "per-iteration(ms)");
    for (u32 g : {16u, 32u, 48u, 64u}) {
        const Result* lo = nullptr;
        const Result* hi = nullptr;
        for (const auto& r : results) {
            if (r.grid != g) continue;
            if (r.iterations == 4) lo = &r;
            if (r.iterations == 10) hi = &r;
        }
        if (!lo || !hi) continue;
        const f64 perIter = (hi->msPerStep - lo->msPerStep) / f64(hi->iterations - lo->iterations);
        const f64 fixed = lo->msPerStep - perIter * lo->iterations;
        std::printf("%-6u %14.2f %18.3f\n", g, fixed, perIter);
    }

    // The scene that made this urgent: several campfires at the shipped
    // defaults. Before the frame budget, every volume solved in full every
    // frame, so N volumes cost N times one volume and three of them was 4 fps.
    //
    // Each count is measured twice: once with the budget disabled (a budget of
    // an hour is never reached, so every volume solves every frame, which is
    // what the engine did before) and once at the shipped 8ms. The disabled row
    // is the control -- without it, "the budget works" is an assertion about a
    // number with nothing to compare it to.
    std::printf("\nmany volumes at the SHIPPED default of 20 iterations:\n");
    std::printf("%-9s %-9s %10s %10s %12s\n",
                "volumes", "budget", "ms/step", "deferred", "fps budget");
    for (int n : {1, 3, 8}) {
      for (f64 budgetMs : {3600000.0, 8.0}) {
        ECS::World world;
        Effects::FluidSimulation sim;
        sim.SetFrameBudgetMs(budgetMs);
        for (int i = 0; i < n; ++i) {
            ECS::Entity e = world.CreateEntity();
            ECS::TransformComponent xf;
            xf.position = Math::Vector3(f32(i) * 20.0f, 0.0f, 0.0f);
            world.AddComponent<ECS::TransformComponent>(e, xf);
            ECS::FluidVolumeComponent v;
            v.dimension = ECS::FluidDimension::Mode3D;
            v.fluidType = ECS::FluidType::Smoke;
            v.gridSize = 48;
            v.buoyancy = 1.0f;
            world.AddComponent<ECS::FluidVolumeComponent>(e, v);
        }
        for (int i = 0; i < 3; ++i) sim.Update(1.0f / 60.0f, &world);
        const auto a = std::chrono::high_resolution_clock::now();
        const int steps = 10;
        for (int i = 0; i < steps; ++i) sim.Update(1.0f / 60.0f, &world);
        const auto b = std::chrono::high_resolution_clock::now();
        const f64 ms = std::chrono::duration<f64, std::milli>(b - a).count() / steps;
        std::printf("%-9d %-9s %10.2f %10u %12s\n",
                    n, budgetMs > 1000.0 ? "off" : "8ms", ms,
                    sim.GetDeferredVolumeCount(),
                    ms <= 16.6 ? "fits 60fps" : (ms <= 33.3 ? "fits 30fps" : "no"));
      }
    }

    // How many cells actually want to be drawn, against the renderer's cap.
    // "Too scarce" reads like a density problem and is not one: FluidRenderer
    // stops at MAX_FLUID_CELLS (16384) shared across EVERY volume, and its 3D
    // loop walks k outermost, so the cut is a flat plane in z rather than a
    // thinning -- and any volume after the one that fills the cache draws
    // nothing at all.
    {
        ECS::World world;
        ECS::Entity e = world.CreateEntity();
        world.AddComponent<ECS::TransformComponent>(e, ECS::TransformComponent{});
        ECS::FluidVolumeComponent v;
        v.dimension = ECS::FluidDimension::Mode3D;
        v.fluidType = ECS::FluidType::Smoke;
        v.gridSize = 48;
        v.buoyancy = 1.0f;
        world.AddComponent<ECS::FluidVolumeComponent>(e, v);

        Effects::FluidSimulation sim;
        std::printf("\ncells above densityThreshold %.3f, one 48 smoke volume:\n",
                    v.densityThreshold);
        std::printf("%-8s %12s %12s %10s\n", "step", "visible", "cap 16384", "drawn %");
        for (int step = 1; step <= 300; ++step) {
            sim.Update(1.0f / 60.0f, &world);
            if (step != 30 && step != 60 && step != 120 && step != 300) continue;
            const Effects::FluidGridData* g = sim.GetGridData(e);
            if (!g || g->N == 0) continue;
            const u32 N = g->N;
            u32 visible = 0;
            for (u32 k = 1; k <= N; ++k)
            for (u32 j = 1; j <= N; ++j)
            for (u32 i = 1; i <= N; ++i)
                if (g->density[g->IX3(i, j, k)] >= v.densityThreshold) ++visible;
            const f64 pct = visible ? 100.0 * f64(std::min(visible, 16384u)) / f64(visible) : 0.0;
            std::printf("%-8d %12u %12s %9.1f%%\n",
                        step, visible, visible > 16384 ? "EXCEEDED" : "ok", pct);
        }
    }

    std::printf("\nRelaxation is the per-iteration column: diffuse on three velocity\n");
    std::printf("components plus density, and two pressure projections. That is the\n");
    std::printf("work red-black parallelism and multigrid go after. The fixed column is\n");
    std::printf("advection, boundaries and buoyancy, which sparse bricks go after.\n");
    return 0;
}
