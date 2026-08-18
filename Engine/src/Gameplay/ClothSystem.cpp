#include "Enjin/Gameplay/ClothSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Cloth.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/Math/Matrix.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Gameplay {

using namespace Enjin::ECS;

namespace {

inline i32 Idx(i32 x, i32 y, i32 resX) { return y * resX + x; }

inline Math::Vector3 XformPoint(const Math::Matrix4& m, const Math::Vector3& p) {
    Math::Vector4 r = m * Math::Vector4(p.x, p.y, p.z, 1.0f);
    return Math::Vector3(r.x, r.y, r.z);
}

bool IsPinned(const ClothComponent& c, i32 x, i32 y) {
    switch (c.pin) {
        case ClothPin::TopEdge:    return y == 0;
        case ClothPin::TopCorners: return y == 0 && (x == 0 || x == c.resX - 1);
        case ClothPin::LeftEdge:   return x == 0;
        case ClothPin::AllCorners: return (y == 0 || y == c.resY - 1) && (x == 0 || x == c.resX - 1);
        case ClothPin::None:       return false;
    }
    return false;
}

// Build the grid: rest positions (local, centered on X, hanging down -Y),
// constraints (structural + shear), triangles, and the render mesh.
void BuildCloth(World* world, Entity entity, ClothComponent& c) {
    c.resX = std::max(c.resX, 2);
    c.resY = std::max(c.resY, 2);
    const i32 nx = c.resX, ny = c.resY;
    const i32 count = nx * ny;
    const f32 dx = c.width / static_cast<f32>(nx - 1);
    const f32 dy = c.height / static_cast<f32>(ny - 1);

    c.restLocal.resize(count);
    c.invMass.assign(count, 1.0f);
    for (i32 y = 0; y < ny; ++y)
        for (i32 x = 0; x < nx; ++x) {
            c.restLocal[Idx(x, y, nx)] = Math::Vector3(
                -c.width * 0.5f + x * dx, -y * dy, 0.0f);
            if (IsPinned(c, x, y)) c.invMass[Idx(x, y, nx)] = 0.0f;
        }

    // World-space start = rest positions through the entity transform.
    Math::Matrix4 model = ComputeWorldMatrix(world, entity);
    c.positions.resize(count);
    for (i32 i = 0; i < count; ++i)
        c.positions[i] = XformPoint(model, c.restLocal[i]);
    c.prevPositions = c.positions;

    // Constraints: structural (right + down neighbors) and shear (diagonals).
    c.constraints.clear();
    auto addC = [&](i32 a, i32 b) {
        f32 rest = (c.restLocal[a] - c.restLocal[b]).Length();
        c.constraints.push_back({a, b, rest});
    };
    for (i32 y = 0; y < ny; ++y)
        for (i32 x = 0; x < nx; ++x) {
            if (x + 1 < nx) addC(Idx(x, y, nx), Idx(x + 1, y, nx));
            if (y + 1 < ny) addC(Idx(x, y, nx), Idx(x, y + 1, nx));
            if (x + 1 < nx && y + 1 < ny) {
                addC(Idx(x, y, nx), Idx(x + 1, y + 1, nx));
                addC(Idx(x + 1, y, nx), Idx(x, y + 1, nx));
            }
        }

    // Triangles (two per quad).
    c.tris.clear();
    for (i32 y = 0; y + 1 < ny; ++y)
        for (i32 x = 0; x + 1 < nx; ++x) {
            u32 a = static_cast<u32>(Idx(x, y, nx));
            u32 b = static_cast<u32>(Idx(x + 1, y, nx));
            u32 d = static_cast<u32>(Idx(x, y + 1, nx));
            u32 e = static_cast<u32>(Idx(x + 1, y + 1, nx));
            c.tris.push_back({a, b, e, true});
            c.tris.push_back({a, e, d, true});
        }

    // Render mesh: positions/uv now, normals per-frame.
    MeshComponent mesh;
    mesh.vertices.resize(count);
    for (i32 y = 0; y < ny; ++y)
        for (i32 x = 0; x < nx; ++x) {
            auto& v = mesh.vertices[Idx(x, y, nx)];
            v.position = c.restLocal[Idx(x, y, nx)];
            v.normal = Math::Vector3(0, 0, 1);
            v.uv = Math::Vector2(x / static_cast<f32>(nx - 1), y / static_cast<f32>(ny - 1));
        }
    mesh.indices.reserve(c.tris.size() * 3);
    for (const auto& t : c.tris) { mesh.indices.push_back(t.i0); mesh.indices.push_back(t.i1); mesh.indices.push_back(t.i2); }

    if (world->HasComponent<MeshComponent>(entity))
        *world->GetComponent<MeshComponent>(entity) = std::move(mesh);
    else
        world->AddComponent<MeshComponent>(entity, std::move(mesh));

    c.initialized = true;
    c.topologyDirty = true;   // buffers must (re)build from the fresh mesh
}

} // namespace

void ClothSystem::Update(World* world, f32 deltaTime) {
    if (!world || deltaTime <= 0.0f) return;
    f32 dt = std::min(deltaTime, 1.0f / 30.0f);   // clamp spiral-of-death steps

    for (Entity entity : world->GetEntitiesWithComponent<ClothComponent>()) {
        auto* c = world->GetComponent<ClothComponent>(entity);
        if (!c) continue;
        if (!c->initialized) BuildCloth(world, entity, *c);
        const i32 count = static_cast<i32>(c->positions.size());
        if (count == 0) continue;

        Math::Matrix4 model = ComputeWorldMatrix(world, entity);

        // Verlet integrate free points; pinned points snap to the transform.
        Math::Vector3 accel = Math::Vector3(0.0f, -9.81f * c->gravityScale, 0.0f) + c->wind;
        const f32 keep = 1.0f - std::clamp(c->damping, 0.0f, 0.5f);
        for (i32 i = 0; i < count; ++i) {
            if (c->invMass[i] == 0.0f) {
                Math::Vector3 target = XformPoint(model, c->restLocal[i]);
                c->prevPositions[i] = c->positions[i] = target;
                continue;
            }
            Math::Vector3 pos = c->positions[i];
            Math::Vector3 vel = (pos - c->prevPositions[i]) * keep;
            c->prevPositions[i] = pos;
            c->positions[i] = pos + vel + accel * (dt * dt);
        }

        // Satisfy distance constraints; tear the overstretched when allowed.
        bool tore = false;
        for (i32 it = 0; it < std::max(c->iterations, 1); ++it) {
            for (usize ci = 0; ci < c->constraints.size(); ++ci) {
                auto& con = c->constraints[ci];
                Math::Vector3 delta = c->positions[con.b] - c->positions[con.a];
                f32 dist = delta.Length();
                if (dist < 1e-6f) continue;

                if (c->tearable && it == 0 && dist > con.rest * c->tearThreshold) {
                    // Snap: drop the constraint and every triangle on that edge.
                    for (auto& t : c->tris) {
                        if (!t.alive) continue;
                        u32 a = static_cast<u32>(con.a), b = static_cast<u32>(con.b);
                        bool hasA = (t.i0 == a || t.i1 == a || t.i2 == a);
                        bool hasB = (t.i0 == b || t.i1 == b || t.i2 == b);
                        if (hasA && hasB) t.alive = false;
                    }
                    c->constraints[ci] = c->constraints.back();
                    c->constraints.pop_back();
                    --ci;
                    tore = true;
                    continue;
                }

                f32 wA = c->invMass[con.a], wB = c->invMass[con.b];
                f32 wSum = wA + wB;
                if (wSum <= 0.0f) continue;
                Math::Vector3 corr = delta * ((dist - con.rest) / (dist * wSum));
                c->positions[con.a] = c->positions[con.a] + corr * wA;
                c->positions[con.b] = c->positions[con.b] - corr * wB;
            }
        }

        // Write back into the mesh (local space) + recompute normals.
        auto* mesh = world->GetComponent<MeshComponent>(entity);
        if (!mesh || mesh->vertices.size() != static_cast<usize>(count)) continue;
        Math::Matrix4 inv = model.Inverse();
        for (i32 i = 0; i < count; ++i) {
            mesh->vertices[i].position = XformPoint(inv, c->positions[i]);
            mesh->vertices[i].normal = Math::Vector3(0, 0, 0);
        }
        for (const auto& t : c->tris) {
            if (!t.alive) continue;
            const Math::Vector3& p0 = mesh->vertices[t.i0].position;
            Math::Vector3 n = (mesh->vertices[t.i1].position - p0)
                                  .Cross(mesh->vertices[t.i2].position - p0);
            mesh->vertices[t.i0].normal = mesh->vertices[t.i0].normal + n;
            mesh->vertices[t.i1].normal = mesh->vertices[t.i1].normal + n;
            mesh->vertices[t.i2].normal = mesh->vertices[t.i2].normal + n;
        }
        for (i32 i = 0; i < count; ++i) {
            f32 len = mesh->vertices[i].normal.Length();
            mesh->vertices[i].normal = (len > 1e-6f)
                ? mesh->vertices[i].normal * (1.0f / len) : Math::Vector3(0, 0, 1);
        }

        if (tore) {
            mesh->indices.clear();
            for (const auto& t : c->tris) {
                if (!t.alive) continue;
                mesh->indices.push_back(t.i0); mesh->indices.push_back(t.i1); mesh->indices.push_back(t.i2);
            }
            c->topologyDirty = true;
        }
        c->meshDirty = true;
    }
}

} // namespace Gameplay
} // namespace Enjin
