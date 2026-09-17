// What a CPU particle update actually costs, per particle and per emitter.
//
// "Audit the CPU path for correctness and perf" has been an open backlog item
// with no numbers attached. This gets the numbers, so an optimisation can be
// argued for by what it saves rather than by how clever it is.
//
// The two costs are separated by measuring the same particle budget arranged
// two ways: a few big emitters, and many small ones. The difference between
// them is per-emitter overhead -- the component lookups, the transform resolve,
// the pool checks -- and it is the part a campfire-and-chimney scene pays most,
// because that scene is dozens of small emitters rather than one big one.
//
//     build/bin/Tests/Release/ParticleBench.exe
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/Effects/ParticleSystem.h"

#include <chrono>
#include <cstdio>
#include <vector>

using namespace Enjin;

namespace {

struct Result {
    int emitters = 0;
    u32 perEmitter = 0;
    u32 liveParticles = 0;
    f64 msPerStep = 0.0;
};

Result Measure(int emitterCount, u32 particlesEach, int steps, bool wind) {
    ECS::World world;
    Effects::ParticleSystem sim;
    if (wind) sim.SetSceneWind(Math::Vector3(3.0f, 0.0f, 1.0f));

    for (int i = 0; i < emitterCount; ++i) {
        ECS::Entity e = world.CreateEntity();
        ECS::TransformComponent xf;
        xf.position = Math::Vector3(f32(i) * 2.0f, 0.0f, 0.0f);
        world.AddComponent<ECS::TransformComponent>(e, xf);

        ECS::ParticleEmitterComponent em;
        em.maxParticles = particlesEach;
        // Enough rate to fill the pool and hold it full, so the timed steps
        // measure a saturated emitter rather than one still filling up.
        em.emissionRate = 5000.0f;
        em.lifetime = 5.0f;
        em.startSize = 0.5f;
        em.endSize = 0.1f;
        em.gravity = Math::Vector3(0.0f, -1.0f, 0.0f);
        em.drag = 0.1f;
        em.useSceneWind = wind;
        em.windInfluence = 1.0f;
        em.loop = true;
        em.playOnAwake = true;
        em.isPlaying = true;
        world.AddComponent<ECS::ParticleEmitterComponent>(e, em);
    }

    // Fill the pools before timing.
    for (int i = 0; i < 240; ++i) sim.Update(1.0f / 60.0f, &world);

    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < steps; ++i) sim.Update(1.0f / 60.0f, &world);
    const auto t1 = std::chrono::high_resolution_clock::now();

    Result r;
    r.emitters = emitterCount;
    r.perEmitter = particlesEach;
    r.liveParticles = sim.GetTotalActiveParticles();
    r.msPerStep = std::chrono::duration<f64, std::milli>(t1 - t0).count() / steps;
    return r;
}

void Row(const Result& r) {
    const f64 nsPerParticle = r.liveParticles
        ? (r.msPerStep * 1.0e6) / f64(r.liveParticles) : 0.0;
    std::printf("%-9d %-11u %-10u %10.3f %14.1f\n",
                r.emitters, r.perEmitter, r.liveParticles, r.msPerStep, nsPerParticle);
}

} // namespace

int main() {
    std::printf("CPU particle update, single-threaded\n\n");
    std::printf("%-9s %-11s %-10s %10s %14s\n",
                "emitters", "per emitter", "live", "ms/step", "ns/particle");

    // Same total budget, arranged differently. If per-particle cost is all
    // there is, every row in a group reads the same ns/particle.
    std::printf("-- 16k particles total, split three ways --\n");
    Row(Measure(1, 16384, 200, false));
    Row(Measure(16, 1024, 200, false));
    Row(Measure(128, 128, 200, false));

    std::printf("-- scaling with count --\n");
    Row(Measure(1, 1024, 400, false));
    Row(Measure(1, 4096, 300, false));
    Row(Measure(1, 16384, 200, false));

    std::printf("-- what scene wind costs --\n");
    Result dry = Measure(64, 256, 300, false);
    Result wet = Measure(64, 256, 300, true);
    Row(dry);
    Row(wet);
    std::printf("   wind adds %.1f%% to the same particle count\n",
                dry.msPerStep > 0.0 ? (wet.msPerStep / dry.msPerStep - 1.0) * 100.0 : 0.0);

    std::printf("\nA campfire-and-chimney scene is the THIRD row of the first group:\n");
    std::printf("many small emitters, not one big one. If that row costs more per\n");
    std::printf("particle than the first, the overhead is per-EMITTER and no amount of\n");
    std::printf("tightening the inner loop will touch it.\n");
    return 0;
}
