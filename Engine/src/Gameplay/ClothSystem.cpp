#include "Enjin/Gameplay/ClothSystem.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Cloth.h"
#include "Enjin/ECS/Components/Rope.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/WeatherZone.h"
#include "Enjin/Effects/Wind.h"
#include "Enjin/Logging/Log.h"
#include "Enjin/Math/Matrix.h"
#include <algorithm>
#include <vector>
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
        case ClothPin::BottomEdge: return y == c.resY - 1;  // hair cards: anchor at the scalp, tips sway free
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
    // Per-link strength bakes the sewn-fabric layout: stitched links on seam
    // lines are stronger, links beside a seam slightly weaker (stress
    // concentrates next to stitching, so tears run along the panels), and
    // links touching pinned points scale by pinStrength.
    c.constraints.clear();
    const i32 spacing = std::max(c.seamSpacing, 2);
    auto onSeamX = [&](i32 x) {
        return (c.seams == ClothSeams::Vertical || c.seams == ClothSeams::Grid) &&
               x > 0 && x < nx - 1 && (x % spacing) == 0;
    };
    auto onSeamY = [&](i32 y) {
        return (c.seams == ClothSeams::Horizontal || c.seams == ClothSeams::Grid) &&
               y > 0 && y < ny - 1 && (y % spacing) == 0;
    };
    auto nearSeam = [&](i32 x, i32 y) {
        return onSeamX(x - 1) || onSeamX(x + 1) || onSeamY(y - 1) || onSeamY(y + 1);
    };
    auto addC = [&](i32 a, i32 b) {
        f32 rest = (c.restLocal[a] - c.restLocal[b]).Length();
        i32 ax = a % nx, ay = a / nx, bx = b % nx, by = b / nx;
        f32 strength = 1.0f;
        bool aOn = onSeamX(ax) || onSeamY(ay);
        bool bOn = onSeamX(bx) || onSeamY(by);
        if (aOn && bOn)
            strength = c.seamStrength;                    // stitched link on the seam
        else if (nearSeam(ax, ay) || nearSeam(bx, by))
            strength = 0.85f;                             // weak line beside the stitching
        if (c.invMass[a] == 0.0f || c.invMass[b] == 0.0f)
            strength *= c.pinStrength;                    // links holding onto the pins
        c.constraints.push_back({a, b, rest, strength});
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

// World-space collider shapes the cloth points push out of. Collider sizes are
// WORLD SPACE by engine convention (no transform-scale multiply).
struct ColliderShape {
    enum class Kind : u8 { Box, Sphere, Capsule } kind;
    Math::Vector3 pos;          // world center
    Math::Quaternion rot;       // entity rotation (box axes / capsule axis)
    Math::Vector3 half;         // box half extents
    f32 radius = 0.0f;          // sphere/capsule
    f32 halfHeight = 0.0f;      // capsule cylinder half-height
    u32 srcIndex = 0xFFFFFFFFu; // EntityIndex of the source entity — each
                                // cloth/rope skips its OWN shapes at resolve
                                // time, so ONE gather serves the whole frame
};

// Gathered ONCE per ClothSystem::Update (audit 2026-08-31) — previously this
// re-scanned five component types for every cloth and rope entity every frame.
// Storage pointers hoisted; per-entity self-exclusion moved to the resolve
// loop via srcIndex.
void GatherColliders(World* world, std::vector<ColliderShape>& out) {
    out.clear();
    auto* xformStore = world->GetComponentStorage<TransformComponent>();
    if (!xformStore) return;

    if (auto* boxStore = world->GetComponentStorage<BoxColliderComponent>())
        for (Entity e : boxStore->GetEntities()) {
            auto* col = boxStore->Get(e);
            auto* xf = xformStore->Get(e);
            if (!col || !xf || col->isTrigger) continue;
            ColliderShape s;
            s.kind = ColliderShape::Kind::Box;
            s.rot = xf->rotation;
            s.pos = xf->position + xf->rotation.Rotate(col->center);
            s.half = col->size * 0.5f;
            s.srcIndex = EntityIndex(e);
            out.push_back(s);
        }
    if (auto* sphereStore = world->GetComponentStorage<SphereColliderComponent>())
        for (Entity e : sphereStore->GetEntities()) {
            auto* col = sphereStore->Get(e);
            auto* xf = xformStore->Get(e);
            if (!col || !xf || col->isTrigger) continue;
            ColliderShape s;
            s.kind = ColliderShape::Kind::Sphere;
            s.pos = xf->position + xf->rotation.Rotate(col->center);
            s.radius = col->radius;
            s.srcIndex = EntityIndex(e);
            out.push_back(s);
        }
    auto* capStore = world->GetComponentStorage<CapsuleColliderComponent>();
    if (capStore)
        for (Entity e : capStore->GetEntities()) {
            auto* col = capStore->Get(e);
            auto* xf = xformStore->Get(e);
            if (!col || !xf || col->isTrigger) continue;
            ColliderShape s;
            s.kind = ColliderShape::Kind::Capsule;
            s.rot = xf->rotation;
            s.pos = xf->position + xf->rotation.Rotate(col->center);
            s.radius = col->radius;
            s.halfHeight = col->height * 0.5f;   // height = cylinder section only
            s.srcIndex = EntityIndex(e);
            out.push_back(s);
        }

    // Character controllers collide through Jolt's CharacterVirtual, not a
    // collider component - without this, cloth and ropes never feel the
    // player. Synthesize the same upright capsule ControllerSystem creates
    // (defaults, or the entity's CapsuleCollider override; transform = feet).
    auto addCharacter = [&](Entity e) {
        auto* xf = xformStore->Get(e);
        if (!xf) return;
        if (capStore && capStore->Get(e))
            return;   // already added by the capsule loop above
        const f32 radius = 0.3f, totalHalfH = 0.8f;   // ControllerSystem defaults
        ColliderShape s;
        s.kind = ColliderShape::Kind::Capsule;
        s.rot = Math::Quaternion();   // upright
        s.pos = xf->position + Math::Vector3(0.0f, totalHalfH, 0.0f);
        s.radius = radius;
        s.halfHeight = std::max(totalHalfH - radius, 0.0f);
        s.srcIndex = EntityIndex(e);
        out.push_back(s);
    };
    if (auto* tp = world->GetComponentStorage<ThirdPersonController>())
        for (Entity e : tp->GetEntities()) addCharacter(e);
    if (auto* fp = world->GetComponentStorage<FirstPersonController>())
        for (Entity e : fp->GetEntities()) addCharacter(e);
}

// Push a point out of a shape if inside (returns true on contact).
bool ResolvePoint(const ColliderShape& s, Math::Vector3& p, f32 skin) {
    switch (s.kind) {
        case ColliderShape::Kind::Sphere: {
            Math::Vector3 d = p - s.pos;
            f32 dist = d.Length();
            f32 r = s.radius + skin;
            if (dist >= r) return false;
            p = s.pos + (dist > 1e-6f ? d * (r / dist) : Math::Vector3(0, r, 0));
            return true;
        }
        case ColliderShape::Kind::Capsule: {
            Math::Vector3 axis = s.rot.Rotate(Math::Vector3(0, 1, 0));
            Math::Vector3 d = p - s.pos;
            f32 t = std::clamp(d.Dot(axis), -s.halfHeight, s.halfHeight);
            Math::Vector3 closest = s.pos + axis * t;
            Math::Vector3 rd = p - closest;
            f32 dist = rd.Length();
            f32 r = s.radius + skin;
            if (dist >= r) return false;
            p = closest + (dist > 1e-6f ? rd * (r / dist) : Math::Vector3(0, r, 0));
            return true;
        }
        case ColliderShape::Kind::Box: {
            Math::Quaternion invRot = s.rot.Inverse();
            Math::Vector3 lp = invRot.Rotate(p - s.pos);
            Math::Vector3 h = s.half + Math::Vector3(skin, skin, skin);
            if (std::fabs(lp.x) >= h.x || std::fabs(lp.y) >= h.y || std::fabs(lp.z) >= h.z)
                return false;
            // Inside: push out along the axis of least penetration.
            f32 px = h.x - std::fabs(lp.x);
            f32 py = h.y - std::fabs(lp.y);
            f32 pz = h.z - std::fabs(lp.z);
            if (px <= py && px <= pz)      lp.x = (lp.x >= 0 ? h.x : -h.x);
            else if (py <= px && py <= pz) lp.y = (lp.y >= 0 ? h.y : -h.y);
            else                           lp.z = (lp.z >= 0 ? h.z : -h.z);
            p = s.pos + s.rot.Rotate(lp);
            return true;
        }
    }
    return false;
}

// Per-frame snapshot of the POD fields the wind sampler needs (the component
// itself carries std::strings — don't copy those per frame).
struct FrameWindZone {
    Math::Vector3 center;
    Math::Vector3 halfExtents;
    Math::Vector3 windDirection;
    f32 windStrength = 0.0f;
    i32 priority = 0;
};

// Gathered ONCE per ClothSystem::Update (audit 2026-08-31) — previously every
// cloth and rope re-iterated all weather zones with two component lookups per
// zone per entity per frame.
void GatherWindZones(World* world, std::vector<FrameWindZone>& out) {
    out.clear();
    auto* zoneStore = world->GetComponentStorage<WeatherZoneComponent>();
    auto* xformStore = world->GetComponentStorage<TransformComponent>();
    if (!zoneStore || !xformStore) return;
    for (Entity ze : zoneStore->GetEntities()) {
        auto* zone = zoneStore->Get(ze);
        auto* zxf = xformStore->Get(ze);
        if (!zone || !zxf) continue;
        out.push_back({ zxf->position, zone->halfExtents, zone->windDirection,
                        zone->windStrength, zone->priority });
    }
}

// Live weather wind at a world position: the highest-priority weather zone
// covering it wins (indoor zone with windStrength 0 = calm interior); with no
// zone, the global wind field supplies gusts + turbulence. Shared by cloth
// and rope. time drives the gust phase.
Math::Vector3 SampleWeatherWind(const std::vector<FrameWindZone>& zones,
                                const Effects::WindSystem* wind,
                                const Math::Vector3& pos, f32 time) {
    bool inZone = false;
    i32 bestPriority = 0;
    Math::Vector3 zoneDir(1.0f, 0.0f, 0.0f);
    f32 zoneStrength = 0.0f;
    for (const auto& z : zones) {
        if (std::abs(pos.x - z.center.x) > z.halfExtents.x ||
            std::abs(pos.y - z.center.y) > z.halfExtents.y ||
            std::abs(pos.z - z.center.z) > z.halfExtents.z) continue;
        if (!inZone || z.priority > bestPriority) {
            inZone = true;
            bestPriority = z.priority;
            zoneDir = z.windDirection;
            zoneStrength = z.windStrength;
        }
    }
    if (inZone) {
        // Direction says WHERE, strength says HOW HARD (m/s^2 on the fabric).
        // The raw windDirection is a small rain-slant vector; multiplying by
        // it made zone wind ~5% of what anyone expects.
        f32 len = zoneDir.Length();
        Math::Vector3 d = (len > 1e-4f) ? zoneDir * (1.0f / len)
                                        : Math::Vector3(1.0f, 0.0f, 0.0f);
        // Gusts: a constant force just tilts fabric into a frozen angle;
        // varying it (time + position phase) makes it actually flap.
        f32 gust = 0.7f + 0.45f * std::sin(time * 1.9f + pos.x * 0.7f)
                                * std::sin(time * 0.83f + pos.z * 0.5f);
        return d * (zoneStrength * gust);
    }
    if (wind) return wind->GetWindAt(pos);
    return Math::Vector3(0.0f, 0.0f, 0.0f);
}

// --- Rope (G3) / Chain (G5): 1D verlet chain, tube or rigid-link render ----

constexpr i32 kRopeSides = 6;       // tube cross-section vertices per ring
constexpr i32 kLinkVerts = 8;       // box corners per chain link
constexpr f32 kLinkOverlap = 1.15f; // links slightly longer than a segment so they visually connect

inline usize RopeVertexCount(const RopeComponent& r) {
    return (r.style == RopeStyle::Chain)
        ? static_cast<usize>(r.segments) * kLinkVerts
        : static_cast<usize>(r.segments + 1) * kRopeSides;
}

// Write the tube/link geometry for the CURRENT chain positions into the
// entity's MeshComponent, local space via the inverse model (the renderer
// applies the entity transform). A frame is parallel-transported down the
// chain so the cross-section doesn't twist. Shared by BuildRope (rest pose)
// and the per-frame sim - BuildRope MUST call it too: without the ring
// expansion the mesh is all centerline points, every triangle is zero-area,
// and the rope rasterizes NOTHING (the edit-mode invisible-rope bug; the sim
// only runs in play, which is why play looked fine).
void WriteRopeMesh(World* world, Entity entity, RopeComponent& r, const Math::Matrix4& model) {
    const i32 n = static_cast<i32>(r.positions.size());
    if (n < 2) return;
    auto* mesh = world->GetComponent<MeshComponent>(entity);
    if (!mesh || mesh->vertices.size() != RopeVertexCount(r)) return;
    Math::Matrix4 inv = model.Inverse();
    Math::Vector3 carry = r.frameNormal;

    auto frameAt = [&](const Math::Vector3& rawTangent,
                       Math::Vector3& tangent, Math::Vector3& normal, Math::Vector3& binormal) {
        f32 tl = rawTangent.Length();
        tangent = (tl > 1e-6f) ? rawTangent * (1.0f / tl) : Math::Vector3(0, -1, 0);
        normal = carry - tangent * carry.Dot(tangent);
        f32 nl = normal.Length();
        if (nl < 1e-4f) {
            normal = Math::Vector3(1, 0, 0) - tangent * tangent.x;
            nl = normal.Length();
            if (nl < 1e-4f) { normal = Math::Vector3(0, 0, 1); nl = 1.0f; }
        }
        normal = normal * (1.0f / nl);
        carry = normal;
        binormal = tangent.Cross(normal);
    };

    if (r.style == RopeStyle::Chain) {
        // One rigid box per segment; odd links twist 90 degrees like real
        // chain links. Corner bit layout matches the index table in Build.
        const f32 halfLen = r.segmentRest * 0.5f * kLinkOverlap;
        const f32 hw = r.thickness;
        const f32 hd = r.thickness * 0.45f;
        for (i32 s = 0; s < r.segments; ++s) {
            Math::Vector3 t, nrm, bin;
            frameAt(r.positions[s + 1] - r.positions[s], t, nrm, bin);
            if (s == 0) r.frameNormal = nrm;
            if (s & 1) std::swap(nrm, bin);   // the alternating twist
            Math::Vector3 mid = (r.positions[s] + r.positions[s + 1]) * 0.5f;
            for (i32 k = 0; k < kLinkVerts; ++k) {
                Math::Vector3 off = nrm * ((k & 1) ? hw : -hw)
                                  + bin * ((k & 2) ? hd : -hd)
                                  + t   * ((k & 4) ? halfLen : -halfLen);
                auto& v = mesh->vertices[static_cast<usize>(s) * kLinkVerts + k];
                Math::Vector3 wp = mid + off;
                v.position = XformPoint(inv, wp);
                Math::Vector3 wn = wp + off;   // corner normal = corner direction
                v.normal = XformPoint(inv, wn) - v.position;
                f32 vnl = v.normal.Length();
                v.normal = (vnl > 1e-6f) ? v.normal * (1.0f / vnl) : Math::Vector3(0, 0, 1);
            }
        }
    } else {
        for (i32 i = 0; i < n; ++i) {
            Math::Vector3 t, nrm, bin;
            frameAt(r.positions[std::min(i + 1, n - 1)] - r.positions[std::max(i - 1, 0)],
                    t, nrm, bin);
            if (i == 0) r.frameNormal = nrm;   // seed next frame from the top
            for (i32 k = 0; k < kRopeSides; ++k) {
                f32 a = (2.0f * 3.14159265f * k) / kRopeSides;
                Math::Vector3 radial = nrm * std::cos(a) + bin * std::sin(a);
                auto& v = mesh->vertices[static_cast<usize>(i) * kRopeSides + k];
                Math::Vector3 wp = r.positions[i] + radial * r.thickness;
                v.position = XformPoint(inv, wp);
                // Rotate the radial into local space too (ignore non-uniform scale).
                Math::Vector3 wn = wp + radial;
                v.normal = (XformPoint(inv, wn) - v.position);
                f32 vnl = v.normal.Length();
                v.normal = (vnl > 1e-6f) ? v.normal * (1.0f / vnl) : Math::Vector3(0, 0, 1);
            }
        }
    }
    r.meshDirty = true;
}

// Build the chain (straight down from the entity) and the tube render mesh.
void BuildRope(World* world, Entity entity, RopeComponent& r) {
    r.segments = std::clamp(r.segments, 1, 256);
    const i32 n = r.segments + 1;
    r.segmentRest = r.length / static_cast<f32>(r.segments);

    Math::Matrix4 model = ComputeWorldMatrix(world, entity);
    r.positions.resize(n);
    r.invMass.assign(n, 1.0f);
    r.invMass[0] = 0.0f;                                     // top pinned to the entity
    if (r.pinBottom && !r.endAttachName.empty())
        r.invMass[n - 1] = 0.0f;                             // second anchor
    else if (r.endMass > 0.0f)
        r.invMass[n - 1] = 1.0f / (1.0f + r.endMass);        // tip weight
    for (i32 i = 0; i < n; ++i)
        r.positions[i] = XformPoint(model, Math::Vector3(0.0f, -r.segmentRest * i, 0.0f));
    r.prevPositions = r.positions;
    r.frameNormal = Math::Vector3(0, 0, 1);

    // Render mesh: positions/normals rewritten per frame; UVs and indices fixed.
    MeshComponent mesh;
    mesh.vertices.resize(RopeVertexCount(r));
    if (r.style == RopeStyle::Chain) {
        // One box per segment (rigid link); corners placed per frame.
        for (i32 s = 0; s < r.segments; ++s)
            for (i32 k = 0; k < kLinkVerts; ++k) {
                auto& v = mesh.vertices[static_cast<usize>(s) * kLinkVerts + k];
                v.position = Math::Vector3(0.0f, -r.segmentRest * (s + 0.5f), 0.0f);
                v.normal = Math::Vector3(0, 0, 1);
                v.uv = Math::Vector2((k & 1) ? 1.0f : 0.0f,
                                     s / static_cast<f32>(r.segments));
            }
        mesh.indices.clear();
        mesh.indices.reserve(static_cast<usize>(r.segments) * 36);
        // Corner layout per link: bit0 = +n, bit1 = +b, bit2 = +t (tip end).
        static const u32 kBoxIdx[36] = {
            0,4,6, 0,6,2,   1,3,7, 1,7,5,    // -n / +n faces
            0,1,5, 0,5,4,   2,6,7, 2,7,3,    // -b / +b faces
            0,2,3, 0,3,1,   4,5,7, 4,7,6 };  // -t / +t faces
        for (i32 s = 0; s < r.segments; ++s)
            for (u32 idx : kBoxIdx)
                mesh.indices.push_back(static_cast<u32>(s * kLinkVerts) + idx);
    } else {
        // Tube: one ring per point, u around, v along.
        const i32 nPts = r.segments + 1;
        for (i32 i = 0; i < nPts; ++i)
            for (i32 k = 0; k < kRopeSides; ++k) {
                auto& v = mesh.vertices[static_cast<usize>(i) * kRopeSides + k];
                v.position = Math::Vector3(0.0f, -r.segmentRest * i, 0.0f);
                v.normal = Math::Vector3(0, 0, 1);
                v.uv = Math::Vector2(k / static_cast<f32>(kRopeSides),
                                     i / static_cast<f32>(r.segments));
            }
        mesh.indices.clear();
        mesh.indices.reserve(static_cast<usize>(r.segments) * kRopeSides * 6);
        for (i32 s = 0; s < r.segments; ++s)
            for (i32 k = 0; k < kRopeSides; ++k) {
                u32 a = static_cast<u32>(s * kRopeSides + k);
                u32 b = static_cast<u32>(s * kRopeSides + (k + 1) % kRopeSides);
                u32 c2 = static_cast<u32>((s + 1) * kRopeSides + k);
                u32 d = static_cast<u32>((s + 1) * kRopeSides + (k + 1) % kRopeSides);
                mesh.indices.push_back(a); mesh.indices.push_back(c2); mesh.indices.push_back(d);
                mesh.indices.push_back(a); mesh.indices.push_back(d); mesh.indices.push_back(b);
            }
    }

    if (world->HasComponent<MeshComponent>(entity))
        *world->GetComponent<MeshComponent>(entity) = std::move(mesh);
    else
        world->AddComponent<MeshComponent>(entity, std::move(mesh));

    // Expand the placeholder centerline vertices into the real rest-pose
    // tube/links - see the WriteRopeMesh comment for why skipping this makes
    // the rope invisible.
    WriteRopeMesh(world, entity, r, model);

    r.initialized = true;
    r.topologyDirty = true;
}

} // namespace

void ClothSystem::ResetAll(World* world) {
    if (!world) return;
    for (Entity entity : world->GetEntitiesWithComponent<ClothComponent>()) {
        auto* c = world->GetComponent<ClothComponent>(entity);
        if (!c) continue;
        c->initialized = false;
        BuildCloth(world, entity, *c);   // fresh grid + topologyDirty -> buffers rebuild
    }
    for (Entity entity : world->GetEntitiesWithComponent<RopeComponent>()) {
        auto* r = world->GetComponent<RopeComponent>(entity);
        if (!r) continue;
        r->initialized = false;
        BuildRope(world, entity, *r);
    }
}

void ClothSystem::EnsureBuilt(World* world) {
    if (!world) return;
    for (Entity entity : world->GetEntitiesWithComponent<ClothComponent>()) {
        auto* c = world->GetComponent<ClothComponent>(entity);
        if (c && !c->initialized) BuildCloth(world, entity, *c);
    }
    for (Entity entity : world->GetEntitiesWithComponent<RopeComponent>()) {
        auto* r = world->GetComponent<RopeComponent>(entity);
        if (r && !r->initialized) BuildRope(world, entity, *r);
    }
}

void ClothSystem::Update(World* world, f32 deltaTime, const Effects::WindSystem* wind) {
    if (!world || deltaTime <= 0.0f) return;
    m_Time += deltaTime;
    f32 dt = std::min(deltaTime, 1.0f / 30.0f);   // clamp spiral-of-death steps

    // Frame caches: colliders and wind zones are gathered lazily ONCE and
    // shared by every cloth and rope this frame (self-exclusion via srcIndex).
    static thread_local std::vector<ColliderShape> s_FrameShapes;
    static thread_local std::vector<FrameWindZone> s_FrameZones;
    bool shapesGathered = false;
    bool zonesGathered = false;
    auto frameShapes = [&]() -> const std::vector<ColliderShape>& {
        if (!shapesGathered) { GatherColliders(world, s_FrameShapes); shapesGathered = true; }
        return s_FrameShapes;
    };
    auto frameZones = [&]() -> const std::vector<FrameWindZone>& {
        if (!zonesGathered) { GatherWindZones(world, s_FrameZones); zonesGathered = true; }
        return s_FrameZones;
    };

    for (Entity entity : world->GetEntitiesWithComponent<ClothComponent>()) {
        auto* c = world->GetComponent<ClothComponent>(entity);
        if (!c) continue;
        if (!c->initialized) BuildCloth(world, entity, *c);
        const i32 count = static_cast<i32>(c->positions.size());
        if (count == 0) continue;

        Math::Matrix4 model = ComputeWorldMatrix(world, entity);

        // Weather wind: see SampleWeatherWind (zones override the global field).
        Math::Vector3 weatherWind(0.0f, 0.0f, 0.0f);
        if (c->useWeatherWind) {
            Math::Vector3 clothPos = XformPoint(model, Math::Vector3(0.0f, 0.0f, 0.0f));
            weatherWind = SampleWeatherWind(frameZones(), wind, clothPos, m_Time) * c->weatherWindScale;
        }

        // Verlet integrate free points; pinned points snap to the transform.
        Math::Vector3 accel = Math::Vector3(0.0f, -9.81f * c->gravityScale, 0.0f) + c->wind + weatherWind;
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

                if (c->tearable && it == 0 && dist > con.rest * c->tearThreshold * con.strength) {
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

        // Collision: push free points out of world colliders. Friction pulls the
        // previous position toward the contact point, killing slide velocity.
        if (c->collide) {
            const auto& shapes = frameShapes();
            if (!shapes.empty()) {
                const u32 selfIdx = EntityIndex(entity);
                f32 fr = std::clamp(c->friction, 0.0f, 1.0f);
                for (i32 i = 0; i < count; ++i) {
                    if (c->invMass[i] == 0.0f) continue;
                    for (const auto& s : shapes) {
                        if (s.srcIndex == selfIdx) continue;
                        if (ResolvePoint(s, c->positions[i], c->collisionSkin)) {
                            c->prevPositions[i] = c->prevPositions[i] +
                                (c->positions[i] - c->prevPositions[i]) * fr;
                        }
                    }
                }
            }
        }

        // Self-collision: push apart non-neighbor point pairs closer than the
        // fabric thickness, so folds stack instead of interpenetrating. One pass
        // per frame; O(n^2) with a cheap distance-squared early-out is fine at
        // grid scales (a 24x24 sheet is ~165k pair checks of trivial math).
        if (c->selfCollide) {
            const i32 nx = c->resX;
            f32 spacing = std::min(c->width / static_cast<f32>(std::max(c->resX - 1, 1)),
                                   c->height / static_cast<f32>(std::max(c->resY - 1, 1)));
            f32 thick = (c->thickness > 0.0f) ? c->thickness : spacing * 0.5f;
            f32 thick2 = thick * thick;
            for (i32 i = 0; i < count; ++i) {
                i32 ix = i % nx, iy = i / nx;
                for (i32 j = i + 1; j < count; ++j) {
                    i32 jx = j % nx, jy = j / nx;
                    // Immediate grid neighbors are held by constraints already.
                    if (std::abs(ix - jx) + std::abs(iy - jy) < 2) continue;
                    Math::Vector3 d = c->positions[j] - c->positions[i];
                    f32 d2 = d.LengthSquared();
                    if (d2 >= thick2 || d2 < 1e-12f) continue;
                    f32 dist = std::sqrt(d2);
                    f32 wI = c->invMass[i], wJ = c->invMass[j];
                    f32 wSum = wI + wJ;
                    if (wSum <= 0.0f) continue;
                    Math::Vector3 corr = d * ((thick - dist) / (dist * wSum));
                    c->positions[i] = c->positions[i] - corr * wI;
                    c->positions[j] = c->positions[j] + corr * wJ;
                }
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

    // --- Ropes (G3) --------------------------------------------------------
    for (Entity entity : world->GetEntitiesWithComponent<RopeComponent>()) {
        auto* r = world->GetComponent<RopeComponent>(entity);
        if (!r) continue;
        if (!r->initialized) BuildRope(world, entity, *r);
        const i32 n = static_cast<i32>(r->positions.size());
        if (n < 2) continue;

        Math::Matrix4 model = ComputeWorldMatrix(world, entity);

        // Resolve the optional end entity (dangling load or second anchor).
        Entity attach = INVALID_ENTITY;
        TransformComponent* attachXf = nullptr;
        if (!r->endAttachName.empty()) {
            attach = world->FindEntityByName(r->endAttachName);
            if (attach != INVALID_ENTITY)
                attachXf = world->GetComponent<TransformComponent>(attach);
        }

        Math::Vector3 weatherWind(0.0f, 0.0f, 0.0f);
        if (r->useWeatherWind) {
            Math::Vector3 topPos = XformPoint(model, Math::Vector3(0.0f, 0.0f, 0.0f));
            weatherWind = SampleWeatherWind(frameZones(), wind, topPos, m_Time) * r->weatherWindScale;
        }

        // Verlet integrate; pinned ends snap to their anchors.
        Math::Vector3 accel = Math::Vector3(0.0f, -9.81f * r->gravityScale, 0.0f) + r->wind + weatherWind;
        const f32 keep = 1.0f - std::clamp(r->damping, 0.0f, 0.5f);
        for (i32 i = 0; i < n; ++i) {
            if (r->invMass[i] == 0.0f) {
                Math::Vector3 target = (i == 0 || !attachXf)
                    ? XformPoint(model, Math::Vector3(0.0f, 0.0f, 0.0f))
                    : attachXf->position;
                r->prevPositions[i] = r->positions[i] = target;
                continue;
            }
            Math::Vector3 pos = r->positions[i];
            Math::Vector3 vel = (pos - r->prevPositions[i]) * keep;
            r->prevPositions[i] = pos;
            r->positions[i] = pos + vel + accel * (dt * dt);
        }

        // Distance constraints along the chain.
        for (i32 it = 0; it < std::max(r->iterations, 1); ++it) {
            for (i32 i = 0; i + 1 < n; ++i) {
                Math::Vector3 delta = r->positions[i + 1] - r->positions[i];
                f32 dist = delta.Length();
                if (dist < 1e-6f) continue;
                f32 wA = r->invMass[i], wB = r->invMass[i + 1];
                f32 wSum = wA + wB;
                if (wSum <= 0.0f) continue;
                Math::Vector3 corr = delta * ((dist - r->segmentRest) / (dist * wSum));
                r->positions[i] = r->positions[i] + corr * wA;
                r->positions[i + 1] = r->positions[i + 1] - corr * wB;
            }
        }

        // Collider pushout (same shapes as cloth).
        if (r->collide) {
            const auto& shapes = frameShapes();
            if (!shapes.empty()) {
                const u32 selfIdx = EntityIndex(entity);
                f32 fr = std::clamp(r->friction, 0.0f, 1.0f);
                for (i32 i = 0; i < n; ++i) {
                    if (r->invMass[i] == 0.0f) continue;
                    for (const auto& s : shapes) {
                        if (s.srcIndex == selfIdx) continue;
                        if (ResolvePoint(s, r->positions[i], r->collisionSkin + r->thickness)) {
                            r->prevPositions[i] = r->prevPositions[i] +
                                (r->positions[i] - r->prevPositions[i]) * fr;
                        }
                    }
                }
            }
        }

        // Dangling load: drag the attached entity to the rope tip.
        if (attachXf && !r->pinBottom)
            attachXf->position = r->positions[n - 1];

        // Write the tube/link geometry for the new chain positions.
        WriteRopeMesh(world, entity, *r, model);
    }
}

} // namespace Gameplay
} // namespace Enjin
