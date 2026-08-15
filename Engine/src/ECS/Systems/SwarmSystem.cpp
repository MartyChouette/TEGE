#include "Enjin/ECS/Systems/SwarmSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Swarm.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Renderer/MeshFactory.h"

namespace Enjin {
namespace ECS {

void SwarmSystem::Update(World* world, f32 deltaTime) {
    if (!m_Enabled || !world) return;

    // Copy the entity list: spawning proxies below mutates component storage,
    // and we must not iterate the SwarmComponent set while adding entities.
    std::vector<Entity> swarms = world->GetEntitiesWithComponent<SwarmComponent>();

    for (Entity e : swarms) {
        auto* sc = world->GetComponent<SwarmComponent>(e);
        if (!sc) continue;

        if (!sc->started) {
            Sim::SwarmConfig cfg;
            cfg.count       = sc->count;
            cfg.spawnRadius = sc->spawnRadius;
            cfg.maxSpeed    = sc->maxSpeed;
            cfg.pull        = sc->pull;
            cfg.damping     = sc->damping;
            if (auto* t = world->GetComponent<TransformComponent>(e))
                cfg.center = t->position;
            sc->sim.Init(cfg, 1337ull + static_cast<u64>(e));

            const u32 n = sc->count < sc->renderCap ? sc->count : sc->renderCap;
            // One shared cube mesh -> RenderSystem instances all proxies.
            const MeshComponent cube = Renderer::MeshFactory::CreateCube(sc->cubeSize);
            sc->proxies.reserve(n);
            for (u32 i = 0; i < n; ++i) {
                Entity p = world->CreateEntity();
                world->AddComponent<TransformComponent>(p);
                world->AddComponent<MeshComponent>(p, cube);
                auto& mat = world->AddComponent<MaterialComponent>(p);
                mat.baseColor = sc->color;
                sc->proxies.push_back(p);
            }
            sc->started = true;
        }

        sc->sim.Update(deltaTime);

        const f32* px = sc->sim.PosX();
        const f32* py = sc->sim.PosY();
        const f32* pz = sc->sim.PosZ();
        const usize m = sc->proxies.size();
        for (usize i = 0; i < m; ++i) {
            Entity p = sc->proxies[i];
            if (!world->IsValid(p)) continue;
            if (auto* t = world->GetComponent<TransformComponent>(p))
                t->position = Math::Vector3(px[i], py[i], pz[i]);
        }
    }
}

void SwarmSystem::Reset(World* world) {
    if (!world) return;
    for (Entity e : world->GetEntitiesWithComponent<SwarmComponent>()) {
        auto* sc = world->GetComponent<SwarmComponent>(e);
        if (!sc) continue;
        for (Entity p : sc->proxies)
            if (world->IsValid(p)) world->DestroyEntity(p);
        sc->proxies.clear();
        sc->started = false;
    }
}

} // namespace ECS
} // namespace Enjin
