#include "Enjin/Geometry/CSG.h"
#include "Enjin/ECS/Components/Gameplay.h"   // MeshColliderComponent
#include "Enjin/Logging/Log.h"

#include <unordered_map>

#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Geometry {

namespace {

// Big enough to cover any sane level before clipping, small enough that
// float precision on the clip maths stays comfortable. A starting quad is only
// ever a scaffold: every one of them gets clipped by the brush's other planes.
constexpr f32 kQuadExtent = 4096.0f;

Math::Vector3 Normalized(const Math::Vector3& v) {
    const f32 len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-12f) return Math::Vector3(0.0f, 1.0f, 0.0f);
    return Math::Vector3(v.x / len, v.y / len, v.z / len);
}

// Any vector not parallel to n, so the cross product cannot collapse.
Math::Vector3 PerpendicularTo(const Math::Vector3& n) {
    const Math::Vector3 helper = (std::fabs(n.y) < 0.99f) ? Math::Vector3(0.0f, 1.0f, 0.0f)
                                                          : Math::Vector3(1.0f, 0.0f, 0.0f);
    return Normalized(n.Cross(helper));
}

// A large quad lying on the plane, wound counter-clockwise seen from outside
// (that is, from the direction the normal points).
BrushFace QuadOnPlane(const Plane& plane) {
    const Math::Vector3 n = plane.normal;
    const Math::Vector3 origin = n * (-plane.offset);
    const Math::Vector3 u = PerpendicularTo(n);
    const Math::Vector3 v = n.Cross(u);

    BrushFace poly;
    poly.plane = plane;
    poly.vertices = {
        origin + (u * -kQuadExtent) + (v * -kQuadExtent),
        origin + (u *  kQuadExtent) + (v * -kQuadExtent),
        origin + (u *  kQuadExtent) + (v *  kQuadExtent),
        origin + (u * -kQuadExtent) + (v *  kQuadExtent),
    };
    return poly;
}

// Drop vertices that repeat, which clipping produces whenever an edge lands
// exactly on a plane. Left in, they become zero-area triangles.
void RemoveDuplicateVertices(BrushFace& poly) {
    if (poly.vertices.size() < 2) return;
    std::vector<Math::Vector3> out;
    out.reserve(poly.vertices.size());
    for (usize i = 0; i < poly.vertices.size(); ++i) {
        const Math::Vector3& a = poly.vertices[i];
        const Math::Vector3& b = poly.vertices[(i + 1) % poly.vertices.size()];
        const Math::Vector3 d(a.x - b.x, a.y - b.y, a.z - b.z);
        if (d.x * d.x + d.y * d.y + d.z * d.z > kPlaneEpsilon * kPlaneEpsilon) {
            out.push_back(a);
        }
    }
    poly.vertices.swap(out);
}

} // namespace

Plane Plane::FromPointNormal(const Math::Vector3& point, const Math::Vector3& n) {
    Plane p;
    p.normal = Normalized(n);
    p.offset = -(p.normal.x * point.x + p.normal.y * point.y + p.normal.z * point.z);
    return p;
}

// ---------------------------------------------------------------------------
// Brush construction
// ---------------------------------------------------------------------------

Brush Brush::Box(const Math::Vector3& center, const Math::Vector3& halfExtents,
                 const Math::Quaternion& rotation) {
    Brush b;
    b.planes.reserve(6);

    // The box's own axes, rotated. Planes are built from these rather than from
    // world axes, so a rotated box stays an exact brush.
    const Math::Vector3 axes[3] = {
        rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f)),
        rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f)),
        rotation.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f)),
    };
    const f32 ext[3] = { std::fabs(halfExtents.x), std::fabs(halfExtents.y), std::fabs(halfExtents.z) };

    for (int i = 0; i < 3; ++i) {
        const Math::Vector3& a = axes[i];
        b.planes.push_back(Plane::FromPointNormal(center + a * ext[i], a));
        b.planes.push_back(Plane::FromPointNormal(center - a * ext[i], a * -1.0f));
    }
    return b;
}

Brush Brush::Prism(const Math::Vector3& center, f32 radius, f32 halfHeight, u32 sides,
                   const Math::Quaternion& rotation) {
    Brush b;
    if (sides < 3) sides = 3;
    b.planes.reserve(sides + 2);

    const Math::Vector3 up    = rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
    const Math::Vector3 right = rotation.Rotate(Math::Vector3(1.0f, 0.0f, 0.0f));
    const Math::Vector3 fwd   = rotation.Rotate(Math::Vector3(0.0f, 0.0f, 1.0f));

    b.planes.push_back(Plane::FromPointNormal(center + up * halfHeight, up));
    b.planes.push_back(Plane::FromPointNormal(center - up * halfHeight, up * -1.0f));

    for (u32 i = 0; i < sides; ++i) {
        const f32 a = (6.2831853f * static_cast<f32>(i)) / static_cast<f32>(sides);
        const Math::Vector3 n = Normalized(right * std::cos(a) + fwd * std::sin(a));
        b.planes.push_back(Plane::FromPointNormal(center + n * radius, n));
    }
    return b;
}

bool Brush::Contains(const Math::Vector3& p, f32 eps) const {
    if (planes.empty()) return false;
    for (const Plane& pl : planes) {
        if (pl.Distance(p) > eps) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Clipping
// ---------------------------------------------------------------------------

BrushFace ClipFace(const BrushFace& poly, const Plane& plane, bool keepInside) {
    BrushFace out;
    out.plane = poly.plane;
    if (poly.vertices.size() < 3) return out;

    const f32 side = keepInside ? 1.0f : -1.0f;
    const usize n = poly.vertices.size();
    out.vertices.reserve(n + 4);

    for (usize i = 0; i < n; ++i) {
        const Math::Vector3& a = poly.vertices[i];
        const Math::Vector3& b = poly.vertices[(i + 1) % n];
        // Positive da means "on the surviving side" for whichever side we keep.
        const f32 da = -side * plane.Distance(a);
        const f32 db = -side * plane.Distance(b);

        const bool aIn = da >= -kPlaneEpsilon;
        const bool bIn = db >= -kPlaneEpsilon;

        if (aIn) out.vertices.push_back(a);

        // Only split on a genuine crossing. Comparing the signs rather than the
        // booleans keeps a vertex sitting exactly on the plane from generating a
        // duplicate intersection point.
        if ((da > kPlaneEpsilon && db < -kPlaneEpsilon) ||
            (da < -kPlaneEpsilon && db > kPlaneEpsilon)) {
            const f32 t = da / (da - db);
            out.vertices.push_back(Math::Vector3(a.x + (b.x - a.x) * t,
                                                 a.y + (b.y - a.y) * t,
                                                 a.z + (b.z - a.z) * t));
        }
        (void)bIn;
    }

    RemoveDuplicateVertices(out);
    if (out.vertices.size() < 3) out.vertices.clear();
    return out;
}

std::vector<BrushFace> BuildFaces(const Brush& brush) {
    std::vector<BrushFace> faces;
    if (brush.planes.size() < 4) return faces;   // fewer than 4 planes bounds nothing

    faces.reserve(brush.planes.size());
    for (usize i = 0; i < brush.planes.size(); ++i) {
        BrushFace face = QuadOnPlane(brush.planes[i]);
        for (usize j = 0; j < brush.planes.size() && face.Valid(); ++j) {
            if (i == j) continue;
            face = ClipFace(face, brush.planes[j], true);
        }
        // A plane that contributes no area is redundant, not an error: it is
        // either outside the volume the others already bound, or exactly
        // coincident with one of them.
        if (face.Valid()) faces.push_back(std::move(face));
    }
    return faces;
}

std::vector<BrushFace> ClipFaceOutsideBrush(const BrushFace& poly, const Brush& brush) {
    std::vector<BrushFace> out;
    if (!poly.Valid() || brush.planes.empty()) {
        if (poly.Valid()) out.push_back(poly);
        return out;
    }

    // Walk the cutter's planes. For each, the part of the remainder that falls
    // OUTSIDE that plane can never be inside the brush, so it is finished and
    // set aside; the part still inside carries on to the next plane. Whatever
    // survives every plane is inside the brush and is dropped.
    BrushFace remainder = poly;
    for (const Plane& plane : brush.planes) {
        if (!remainder.Valid()) break;

        BrushFace outside = ClipFace(remainder, plane, false);
        if (outside.Valid()) out.push_back(std::move(outside));

        remainder = ClipFace(remainder, plane, true);
    }
    return out;
}

// ---------------------------------------------------------------------------
// The operation
// ---------------------------------------------------------------------------

std::vector<BrushFace> BuildSolid(const std::vector<BrushEntry>& brushes) {
    std::vector<BrushFace> solid;

    // The Add brushes seen so far. A Subtract needs them to know where the solid
    // actually IS, so its own faces can be trimmed to that. Without this a
    // cutter that pokes out of the wall leaves the protruding part of its
    // surface floating in mid-air.
    std::vector<Brush> addBrushes;

    for (usize i = 0; i < brushes.size(); ++i) {
        const BrushEntry& entry = brushes[i];
        const std::vector<BrushFace> faces = BuildFaces(entry.brush);
        if (faces.empty()) continue;

        switch (entry.op) {
            case BrushOp::Add: {
                // The new brush's faces, minus anything already-placed brushes
                // would swallow, are not removed here: a later Subtract does
                // that. Union of overlapping Adds leaves interior faces, which
                // is the classic brush-editor behaviour and is what keeps the
                // operation order-independent for Adds.
                solid.insert(solid.end(), faces.begin(), faces.end());
                addBrushes.push_back(entry.brush);
                break;
            }

            case BrushOp::Subtract: {
                // Carve the existing solid.
                std::vector<BrushFace> kept;
                kept.reserve(solid.size());
                for (const BrushFace& p : solid) {
                    std::vector<BrushFace> pieces = ClipFaceOutsideBrush(p, entry.brush);
                    kept.insert(kept.end(), pieces.begin(), pieces.end());
                }

                // The cutter's own faces become the inside surface of the hole,
                // flipped to face into the void. Without this a subtraction
                // leaves an opening you can see through into the back of the
                // wall, which is the single most obvious CSG artefact.
                // Trimmed to the solid being cut, not added whole. A cutter is
                // normally deeper than its target so the hole goes all the way
                // through, and the part sticking out the far side has no
                // business being drawn: it renders as a box floating behind the
                // wall, which is precisely what a screenshot of this showed.
                for (const BrushFace& f : faces) {
                    BrushFace inner = f;
                    std::reverse(inner.vertices.begin(), inner.vertices.end());
                    inner.plane.normal = f.plane.normal * -1.0f;
                    inner.plane.offset = -f.plane.offset;

                    for (const Brush& add : addBrushes) {
                        BrushFace piece = inner;
                        for (const Plane& pl : add.planes) {
                            if (!piece.Valid()) break;
                            piece = ClipFace(piece, pl, true);
                        }
                        if (piece.Valid()) kept.push_back(std::move(piece));
                    }
                }

                solid.swap(kept);
                break;
            }

            case BrushOp::Intersect: {
                std::vector<BrushFace> kept;
                kept.reserve(solid.size());
                for (const BrushFace& p : solid) {
                    BrushFace inside = p;
                    for (const Plane& plane : entry.brush.planes) {
                        if (!inside.Valid()) break;
                        inside = ClipFace(inside, plane, true);
                    }
                    if (inside.Valid()) kept.push_back(std::move(inside));
                }
                solid.swap(kept);
                break;
            }
        }
    }

    return solid;
}

// ---------------------------------------------------------------------------
// Meshing
// ---------------------------------------------------------------------------

ECS::MeshComponent ToMesh(const std::vector<BrushFace>& polygons, f32 uvScale) {
    ECS::MeshComponent mesh;
    if (uvScale <= 0.0f) uvScale = 1.0f;

    for (const BrushFace& poly : polygons) {
        if (!poly.Valid()) continue;

        const Math::Vector3 n = poly.plane.normal;

        // Planar projection on the dominant axis. A wall gets wall-shaped UVs
        // and a floor gets floor-shaped ones without anybody unwrapping
        // anything, which is the whole point of brush geometry.
        const f32 ax = std::fabs(n.x), ay = std::fabs(n.y), az = std::fabs(n.z);
        Math::Vector3 uAxis, vAxis;
        if (ay >= ax && ay >= az) {          // floor or ceiling
            uAxis = Math::Vector3(1.0f, 0.0f, 0.0f);
            vAxis = Math::Vector3(0.0f, 0.0f, 1.0f);
        } else if (ax >= az) {               // wall facing X
            uAxis = Math::Vector3(0.0f, 0.0f, 1.0f);
            vAxis = Math::Vector3(0.0f, 1.0f, 0.0f);
        } else {                             // wall facing Z
            uAxis = Math::Vector3(1.0f, 0.0f, 0.0f);
            vAxis = Math::Vector3(0.0f, 1.0f, 0.0f);
        }

        const u32 base = static_cast<u32>(mesh.vertices.size());
        for (const Math::Vector3& p : poly.vertices) {
            ECS::MeshComponent::Vertex v;
            v.position = p;
            v.normal = n;   // from the plane, so a sliver triangle cannot skew it
            v.uv = Math::Vector2((p.x * uAxis.x + p.y * uAxis.y + p.z * uAxis.z) / uvScale,
                                 (p.x * vAxis.x + p.y * vAxis.y + p.z * vAxis.z) / uvScale);
            mesh.vertices.push_back(v);
        }

        // Convex by construction, so a triangle fan is correct and cheap.
        for (u32 i = 1; i + 1 < static_cast<u32>(poly.vertices.size()); ++i) {
            mesh.indices.push_back(base);
            mesh.indices.push_back(base + i);
            mesh.indices.push_back(base + i + 1);
        }
    }

    if (!mesh.indices.empty()) {
        ECS::MeshComponent::SubMesh sub;
        sub.indexOffset = 0;
        sub.indexCount = static_cast<u32>(mesh.indices.size());
        sub.materialSlot = 0;
        sub.name = "CSG";
        mesh.subMeshes.push_back(std::move(sub));
    }

    return mesh;
}

ECS::MeshComponent BuildMesh(const std::vector<BrushEntry>& brushes, f32 uvScale) {
    return ToMesh(BuildSolid(brushes), uvScale);
}

// ---------------------------------------------------------------------------
// Collision
// ---------------------------------------------------------------------------

CollisionMesh BuildCollision(const std::vector<BrushFace>& faces, f32 weldEpsilon) {
    CollisionMesh out;
    if (weldEpsilon <= 0.0f) weldEpsilon = 1e-3f;
    const f32 inv = 1.0f / weldEpsilon;

    // Snap to a grid of the weld tolerance and key on that. A hash of the
    // quantised position finds coincident corners without an O(n^2) search,
    // which matters because a room is thousands of faces.
    struct Key { i64 x, y, z; bool operator==(const Key& o) const { return x==o.x && y==o.y && z==o.z; } };
    struct KeyHash {
        usize operator()(const Key& k) const {
            u64 h = 1469598103934665603ull;
            for (i64 v : { k.x, k.y, k.z }) {
                h ^= static_cast<u64>(v); h *= 1099511628211ull;
            }
            return static_cast<usize>(h);
        }
    };
    std::unordered_map<Key, u32, KeyHash> lookup;

    auto indexOf = [&](const Math::Vector3& p) -> u32 {
        const Key k{ static_cast<i64>(std::llround(p.x * inv)),
                     static_cast<i64>(std::llround(p.y * inv)),
                     static_cast<i64>(std::llround(p.z * inv)) };
        auto it = lookup.find(k);
        if (it != lookup.end()) return it->second;
        const u32 idx = static_cast<u32>(out.vertices.size());
        out.vertices.push_back(p);
        lookup.emplace(k, idx);
        return idx;
    };

    for (const BrushFace& face : faces) {
        if (!face.Valid()) continue;

        // Same fan as the render mesh: the faces are convex by construction.
        std::vector<u32> ring;
        ring.reserve(face.vertices.size());
        for (const Math::Vector3& p : face.vertices) ring.push_back(indexOf(p));

        for (usize i = 1; i + 1 < ring.size(); ++i) {
            const u32 a = ring[0], b = ring[i], c = ring[i + 1];

            // Welding can collapse a triangle to a line or a point. Emitting it
            // would hand the physics cooker a face with no normal.
            if (a == b || b == c || a == c) continue;

            const Math::Vector3& p0 = out.vertices[a];
            const Math::Vector3& p1 = out.vertices[b];
            const Math::Vector3& p2 = out.vertices[c];
            const Math::Vector3 e1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
            const Math::Vector3 e2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);
            const Math::Vector3 n = e1.Cross(e2);
            const f32 area2 = n.x * n.x + n.y * n.y + n.z * n.z;
            if (area2 < 1e-12f) continue;   // degenerate even after welding

            out.indices.push_back(a);
            out.indices.push_back(b);
            out.indices.push_back(c);
        }
    }

    return out;
}

void FillMeshCollider(ECS::MeshColliderComponent& out, const std::vector<BrushEntry>& brushes) {
    const CollisionMesh cm = BuildCollision(BuildSolid(brushes));

    out.vertices = cm.vertices;
    out.indices = cm.indices;
    // Concave by definition once anything has been subtracted, and a hull would
    // fill the hole straight back in. Static bodies cook the exact triangles
    // regardless, but saying convex here would be wrong the moment somebody
    // makes the solid dynamic.
    out.convex = false;
    out.generated = true;
    out.autoGenerate = false;   // the brush list owns this geometry, not a MeshComponent
}

} // namespace Geometry
} // namespace Enjin
