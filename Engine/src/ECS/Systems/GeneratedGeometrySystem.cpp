#include "Enjin/ECS/Systems/GeneratedGeometrySystem.h"
#include "Enjin/ECS/Components/GeneratedGeometry.h"
#include "Enjin/ECS/Components/ProceduralMesh.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Enjin {
namespace ECS {

namespace {

// Mix a value into an FNV-1a running hash. Used to detect an authored config
// change so a generator only re-initialises when a field actually moved,
// instead of every frame (which would restart an automaton continuously).
inline void HashMix(u64& h, const void* data, usize bytes) {
    const u8* p = static_cast<const u8*>(data);
    for (usize i = 0; i < bytes; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
}
template <typename T>
inline void HashVal(u64& h, const T& v) { HashMix(h, &v, sizeof(T)); }

// Write a generated vertex/index pair into the entity's MeshComponent and raise
// the right procedural flag. Topology changes retire the GPU buffers; a pure
// vertex move re-uploads in place. Returns false if there was nothing to write.
bool WriteMesh(World* world, Entity entity,
               std::vector<MeshComponent::Vertex>&& verts,
               std::vector<u32>&& inds,
               ProceduralMeshComponent::Source source) {
    if (verts.empty() || inds.empty()) {
        // A generator can legitimately produce nothing (an automaton that died
        // out, a metaball group whose blobs all left the grid). Clear the mesh
        // so the last frame's geometry does not linger.
        if (auto* existing = world->GetComponent<MeshComponent>(entity)) {
            if (!existing->vertices.empty()) {
                existing->vertices.clear();
                existing->indices.clear();
                existing->aabbDirty = true;
                if (auto* pm = world->GetComponent<ProceduralMeshComponent>(entity))
                    pm->topologyDirty = true;
            }
        }
        return false;
    }

    if (!world->HasComponent<MeshComponent>(entity)) {
        MeshComponent mesh;
        mesh.vertices = std::move(verts);
        mesh.indices = std::move(inds);
        mesh.aabbDirty = true;
        world->AddComponent<MeshComponent>(entity, std::move(mesh));
    } else {
        auto* mesh = world->GetComponent<MeshComponent>(entity);
        const bool topologyChanged = (mesh->indices.size() != inds.size()) ||
                                     (mesh->vertices.size() != verts.size());
        mesh->vertices = std::move(verts);
        mesh->indices = std::move(inds);
        mesh->aabbDirty = true;
        if (!world->HasComponent<ProceduralMeshComponent>(entity)) {
            ProceduralMeshComponent pm;
            pm.source = source;
            pm.topologyDirty = true;
            world->AddComponent<ProceduralMeshComponent>(entity, pm);
            return true;
        }
        auto* pm = world->GetComponent<ProceduralMeshComponent>(entity);
        pm->source = source;
        if (topologyChanged) pm->topologyDirty = true;
        else                 pm->meshDirty = true;
        return true;
    }

    if (!world->HasComponent<ProceduralMeshComponent>(entity)) {
        ProceduralMeshComponent pm;
        pm.source = source;
        pm.topologyDirty = true;
        world->AddComponent<ProceduralMeshComponent>(entity, pm);
    } else {
        auto* pm = world->GetComponent<ProceduralMeshComponent>(entity);
        pm->source = source;
        pm->topologyDirty = true;
    }
    return true;
}

// --- Contour presets for the Fourier component ----------------------------
std::vector<Math::Vector2> BuildContour(FourierMeshComponent::ContourSource src,
                                        const std::vector<Math::Vector2>& custom,
                                        i32 samples) {
    std::vector<Math::Vector2> pts;
    const i32 n = std::max(16, samples);
    pts.reserve(static_cast<usize>(n));
    const f32 twoPi = 6.28318530718f;

    switch (src) {
        case FourierMeshComponent::ContourSource::Custom:
            return custom;

        case FourierMeshComponent::ContourSource::Circle:
            for (i32 i = 0; i < n; ++i) {
                f32 t = twoPi * static_cast<f32>(i) / static_cast<f32>(n);
                pts.push_back({std::cos(t), std::sin(t)});
            }
            break;

        case FourierMeshComponent::ContourSource::Square:
            // Walk the perimeter so sample spacing stays even along the edges.
            for (i32 i = 0; i < n; ++i) {
                f32 t = 4.0f * static_cast<f32>(i) / static_cast<f32>(n);
                i32 side = static_cast<i32>(t);
                f32 f = t - static_cast<f32>(side);
                switch (side) {
                    case 0:  pts.push_back({-1.0f + 2.0f * f, -1.0f}); break;
                    case 1:  pts.push_back({ 1.0f, -1.0f + 2.0f * f}); break;
                    case 2:  pts.push_back({ 1.0f - 2.0f * f,  1.0f}); break;
                    default: pts.push_back({-1.0f,  1.0f - 2.0f * f}); break;
                }
            }
            break;

        case FourierMeshComponent::ContourSource::Star:
            for (i32 i = 0; i < n; ++i) {
                f32 t = twoPi * static_cast<f32>(i) / static_cast<f32>(n);
                f32 r = 0.55f + 0.45f * std::cos(5.0f * t);
                pts.push_back({r * std::cos(t), r * std::sin(t)});
            }
            break;

        case FourierMeshComponent::ContourSource::Heart:
        default:
            for (i32 i = 0; i < n; ++i) {
                f32 t = twoPi * static_cast<f32>(i) / static_cast<f32>(n);
                f32 s = std::sin(t);
                f32 x = 16.0f * s * s * s;
                f32 y = 13.0f * std::cos(t) - 5.0f * std::cos(2.0f * t)
                      - 2.0f * std::cos(3.0f * t) - std::cos(4.0f * t);
                pts.push_back({x / 16.0f, y / 16.0f});
            }
            break;
    }
    return pts;
}

} // namespace

// ---------------------------------------------------------------------------

void GeneratedGeometrySystem::Update(World* world, f32 deltaTime) {
    if (!world) world = m_World;
    if (!world) return;

    UpdateMetaballs(world, deltaTime);
    UpdateCellularAutomata(world, deltaTime);
    UpdateProjection4D(world, deltaTime);
    UpdateFourierMeshes(world, deltaTime);
}

void GeneratedGeometrySystem::Reset() {
    m_CAStates.clear();
    m_P4DStates.clear();
    m_FourierStates.clear();
    m_MetaballAccum = 0.0f;
}

usize GeneratedGeometrySystem::ActiveCount() const {
    return m_CAStates.size() + m_P4DStates.size() + m_FourierStates.size();
}

// --- Metaballs -------------------------------------------------------------

void GeneratedGeometrySystem::UpdateMetaballs(World* world, f32 deltaTime) {
    auto surfaces = world->GetEntitiesWithComponent<MetaballSurfaceComponent>();
    if (surfaces.empty()) return;

    // One evaluation pass serves every surface: MetaballSystem gathers all
    // MetaballComponents in the world and buckets them by groupId.
    const auto* first = world->GetComponent<MetaballSurfaceComponent>(surfaces[0]);
    f32 rate = first ? first->updateRate : 30.0f;
    if (rate > 0.0f) {
        m_MetaballAccum += deltaTime;
        if (m_MetaballAccum < (1.0f / rate)) return;
        m_MetaballAccum = 0.0f;
    }

    Effects::MetaballConfig cfg;
    if (first) {
        cfg.gridResolution = std::clamp(first->gridResolution, 16, 64);
        cfg.gridSize = first->gridSize;
        cfg.smoothNormals = first->smoothNormals;
        cfg.autoCenter = first->autoCenter;
        cfg.updateRate = 0.0f;   // this system owns the throttle
    }
    m_Metaballs.SetConfig(cfg);
    m_Metaballs.Update(world);

    for (Entity e : surfaces) {
        auto* surf = world->GetComponent<MetaballSurfaceComponent>(e);
        if (!surf) continue;
        if (auto* pm = world->GetComponent<ProceduralMeshComponent>(e))
            if (!pm->regenerate) continue;

        const Effects::MetaballMeshData* src = m_Metaballs.GetGroupMesh(surf->groupId);
        std::vector<MeshComponent::Vertex> verts;
        std::vector<u32> inds;
        if (src) {
            verts.reserve(src->vertices.size());
            for (const auto& v : src->vertices) {
                MeshComponent::Vertex mv;
                mv.position = v.position;
                mv.normal = v.normal;
                mv.uv = v.uv;
                mv.color = surf->useBlobColors
                    ? Math::Vector4(v.color.x, v.color.y, v.color.z, 1.0f)
                    : Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                verts.push_back(mv);
            }
            inds = src->indices;
        }
        WriteMesh(world, e, std::move(verts), std::move(inds),
                  ProceduralMeshComponent::Source::Metaball);
    }
}

// --- Cellular automata -----------------------------------------------------

void GeneratedGeometrySystem::UpdateCellularAutomata(World* world, f32 deltaTime) {
    std::unordered_set<u32> alive;

    for (Entity e : world->GetEntitiesWithComponent<CellularAutomataComponent>()) {
        auto* ca = world->GetComponent<CellularAutomataComponent>(e);
        if (!ca) continue;
        const u32 key = EntityIndex(e);
        alive.insert(key);

        u64 h = 1469598103934665603ULL;
        HashVal(h, ca->rule);      HashVal(h, ca->meshMode);
        HashVal(h, ca->width);     HashVal(h, ca->height);   HashVal(h, ca->depth);
        HashVal(h, ca->cellSize);  HashVal(h, ca->seed);     HashVal(h, ca->wrapEdges);
        HashVal(h, ca->isoLevel);  HashVal(h, ca->initialFillPercent);
        HashMix(h, ca->stampPattern.data(), ca->stampPattern.size());

        auto& st = m_CAStates[key];
        const bool needInit = (st.configHash != h) || ca->resetRequested;
        if (needInit) {
            Effects::CAGeoConfig cfg;
            cfg.width = std::max(2u, ca->width);
            cfg.height = std::max(2u, ca->height);
            cfg.depth = std::max(1u, ca->depth);
            cfg.rule = ca->rule;
            cfg.meshMode = ca->meshMode;
            cfg.cellSize = ca->cellSize;
            cfg.updateInterval = ca->updateInterval;
            cfg.initialFillPercent = ca->stampPattern.empty() ? ca->initialFillPercent : 0.0f;
            cfg.seed = ca->seed;
            cfg.wrapEdges = ca->wrapEdges;
            cfg.isoLevel = ca->isoLevel;
            cfg.liveColor = ca->liveColor;
            cfg.dyingColor = ca->dyingColor;
            st.sim.Initialize(cfg);

            if (!ca->stampPattern.empty()) {
                const u32 cx = cfg.width / 2, cy = cfg.height / 2;
                if (ca->stampPattern == "glider")         st.sim.StampGlider(2, 2);
                else if (ca->stampPattern == "pulsar")    st.sim.StampPulsar(cx, cy);
                else if (ca->stampPattern == "gospergun") st.sim.StampGosperGun(2, cy);
                else ENJIN_LOG_WARN(Game,
                        "CellularAutomata: unknown stamp pattern '%s' (glider, pulsar, gospergun)",
                        ca->stampPattern.c_str());
            }
            st.configHash = h;
            ca->resetRequested = false;
        }

        if (ca->running) st.sim.Update(deltaTime);

        ca->generation = st.sim.GetGeneration();
        ca->liveCells = st.sim.GetLiveCellCount();

        if (auto* pm = world->GetComponent<ProceduralMeshComponent>(e))
            if (!pm->regenerate) continue;

        Effects::CAMeshData md = st.sim.GenerateMesh();
        std::vector<MeshComponent::Vertex> verts;
        verts.reserve(md.vertices.size());
        for (const auto& v : md.vertices) {
            MeshComponent::Vertex mv;
            mv.position = v.position;
            mv.normal = v.normal;
            mv.uv = Math::Vector2(0.0f, 0.0f);
            mv.color = Math::Vector4(v.color.x, v.color.y, v.color.z, 1.0f);
            verts.push_back(mv);
        }
        WriteMesh(world, e, std::move(verts), std::move(md.indices),
                  ProceduralMeshComponent::Source::CellularAutomata);
    }

    // Drop state for entities that no longer exist or lost the component.
    for (auto it = m_CAStates.begin(); it != m_CAStates.end(); ) {
        it = (alive.count(it->first) == 0) ? m_CAStates.erase(it) : std::next(it);
    }
}

// --- 4D projection ---------------------------------------------------------

void GeneratedGeometrySystem::UpdateProjection4D(World* world, f32 deltaTime) {
    std::unordered_set<u32> alive;

    for (Entity e : world->GetEntitiesWithComponent<Projection4DComponent>()) {
        auto* p4 = world->GetComponent<Projection4DComponent>(e);
        if (!p4) continue;
        const u32 key = EntityIndex(e);
        alive.insert(key);

        auto& st = m_P4DStates[key];
        const u8 want = static_cast<u8>(p4->polytope);
        if (st.builtFrom != want) {
            switch (p4->polytope) {
                case Projection4DComponent::Polytope::Tesseract:
                    st.polytope = Effects::Projection4D::GenerateTesseract(); break;
                case Projection4DComponent::Polytope::Cell5:
                    st.polytope = Effects::Projection4D::Generate5Cell(); break;
                case Projection4DComponent::Polytope::Cell16:
                    st.polytope = Effects::Projection4D::Generate16Cell(); break;
                case Projection4DComponent::Polytope::Cell24:
                    st.polytope = Effects::Projection4D::Generate24Cell(); break;
                case Projection4DComponent::Polytope::Cell120:
                    st.polytope = Effects::Projection4D::Generate120Cell(); break;
                default:
                    st.polytope = Effects::Projection4D::GenerateTesseract(); break;
            }
            st.projector.ResetAngles();
            st.builtFrom = want;
            st.accum = 1e9f;   // force a rebuild this frame
        }

        if (p4->animate) {
            st.projector.AnimateRotation(st.polytope, deltaTime, p4->rotation);
        }

        // Rebuild throttle. The 120-cell is 1200 edges; a tube mesh per edge per
        // frame is the single most expensive thing this system can be asked for.
        if (p4->updateRate > 0.0f) {
            st.accum += deltaTime;
            if (st.accum < (1.0f / p4->updateRate)) continue;
            st.accum = 0.0f;
        }

        if (auto* pm = world->GetComponent<ProceduralMeshComponent>(e))
            if (!pm->regenerate) continue;

        Effects::Projection4DMeshData md = Effects::Projection4D::GenerateWireframeMesh(
            st.polytope, p4->lineWidth, p4->projectionDistance,
            std::max(3u, p4->tubeSegments));

        std::vector<MeshComponent::Vertex> verts;
        verts.reserve(md.vertices.size());
        const f32 s = p4->scale;
        for (const auto& v : md.vertices) {
            MeshComponent::Vertex mv;
            mv.position = Math::Vector3(v.position.x * s, v.position.y * s, v.position.z * s);
            mv.normal = v.normal;
            mv.uv = v.uv;
            verts.push_back(mv);
        }
        WriteMesh(world, e, std::move(verts), std::move(md.indices),
                  ProceduralMeshComponent::Source::Projection4D);
    }

    for (auto it = m_P4DStates.begin(); it != m_P4DStates.end(); ) {
        it = (alive.count(it->first) == 0) ? m_P4DStates.erase(it) : std::next(it);
    }
}

// --- Fourier contours ------------------------------------------------------

void GeneratedGeometrySystem::UpdateFourierMeshes(World* world, f32 deltaTime) {
    std::unordered_set<u32> alive;

    for (Entity e : world->GetEntitiesWithComponent<FourierMeshComponent>()) {
        auto* fm = world->GetComponent<FourierMeshComponent>(e);
        if (!fm) continue;
        const u32 key = EntityIndex(e);
        alive.insert(key);

        u64 h = 1469598103934665603ULL;
        HashVal(h, fm->source);
        HashVal(h, fm->coefficients);
        HashVal(h, fm->samples);
        if (fm->source == FourierMeshComponent::ContourSource::Custom)
            HashMix(h, fm->customContour.data(),
                    fm->customContour.size() * sizeof(Math::Vector2));

        auto& st = m_FourierStates[key];
        if (st.configHash != h) {
            auto contour = BuildContour(fm->source, fm->customContour, fm->samples);
            if (contour.size() >= 3) {
                st.dft.Decompose(contour, fm->coefficients);
                st.configHash = h;
                fm->elapsed = 0.0f;
            } else {
                ENJIN_LOG_WARN(Game,
                    "FourierMesh: contour has %zu points, needs at least 3", contour.size());
                continue;
            }
        }
        if (!st.dft.HasData()) continue;

        fm->elapsed += deltaTime;

        i32 useTerms = fm->terms;
        if (fm->animateTerms && fm->animationSeconds > 0.0f) {
            // Ramp linearly to the authored term count, then hold.
            f32 f = std::min(1.0f, fm->elapsed / fm->animationSeconds);
            useTerms = std::max(1, static_cast<i32>(f * static_cast<f32>(fm->terms) + 0.5f));
        }
        fm->activeTerms = useTerms;

        if (auto* pm = world->GetComponent<ProceduralMeshComponent>(e))
            if (!pm->regenerate) continue;

        auto approx = st.dft.GenerateApproximation(useTerms, fm->samples);
        if (approx.size() < 3) continue;

        Effects::FourierMeshData md = (fm->extrudeDepth > 0.0f)
            ? Effects::FourierMeshDecomposition::GenerateExtrudedMesh(approx, fm->extrudeDepth)
            : Effects::FourierMeshDecomposition::GenerateMeshFromContour(approx);

        std::vector<MeshComponent::Vertex> verts;
        verts.reserve(md.vertices.size());
        const f32 s = fm->scale;
        for (const auto& v : md.vertices) {
            MeshComponent::Vertex mv;
            mv.position = Math::Vector3(v.position.x * s, v.position.y * s, v.position.z * s);
            mv.normal = v.normal;
            mv.uv = v.uv;
            verts.push_back(mv);
        }
        WriteMesh(world, e, std::move(verts), std::move(md.indices),
                  ProceduralMeshComponent::Source::Fourier);
    }

    for (auto it = m_FourierStates.begin(); it != m_FourierStates.end(); ) {
        it = (alive.count(it->first) == 0) ? m_FourierStates.erase(it) : std::next(it);
    }
}

} // namespace ECS
} // namespace Enjin
