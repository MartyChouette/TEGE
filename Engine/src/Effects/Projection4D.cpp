#include "Enjin/Effects/Projection4D.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace Enjin {
namespace Effects {

// --- Matrix4D Rotation Factories ---

Matrix4D Matrix4D::RotateXY(f32 angle) {
    Matrix4D m;
    f32 c = Math::Cos(angle);
    f32 s = Math::Sin(angle);
    m.m[0][0] = c;  m.m[0][1] = -s;
    m.m[1][0] = s;  m.m[1][1] = c;
    return m;
}

Matrix4D Matrix4D::RotateXZ(f32 angle) {
    Matrix4D m;
    f32 c = Math::Cos(angle);
    f32 s = Math::Sin(angle);
    m.m[0][0] = c;  m.m[0][2] = -s;
    m.m[2][0] = s;  m.m[2][2] = c;
    return m;
}

Matrix4D Matrix4D::RotateXW(f32 angle) {
    Matrix4D m;
    f32 c = Math::Cos(angle);
    f32 s = Math::Sin(angle);
    m.m[0][0] = c;  m.m[0][3] = -s;
    m.m[3][0] = s;  m.m[3][3] = c;
    return m;
}

Matrix4D Matrix4D::RotateYZ(f32 angle) {
    Matrix4D m;
    f32 c = Math::Cos(angle);
    f32 s = Math::Sin(angle);
    m.m[1][1] = c;  m.m[1][2] = -s;
    m.m[2][1] = s;  m.m[2][2] = c;
    return m;
}

Matrix4D Matrix4D::RotateYW(f32 angle) {
    Matrix4D m;
    f32 c = Math::Cos(angle);
    f32 s = Math::Sin(angle);
    m.m[1][1] = c;  m.m[1][3] = -s;
    m.m[3][1] = s;  m.m[3][3] = c;
    return m;
}

Matrix4D Matrix4D::RotateZW(f32 angle) {
    Matrix4D m;
    f32 c = Math::Cos(angle);
    f32 s = Math::Sin(angle);
    m.m[2][2] = c;  m.m[2][3] = -s;
    m.m[3][2] = s;  m.m[3][3] = c;
    return m;
}

// --- Projection Methods ---

Math::Vector3 Projection4D::StereographicProject(const Vector4D& point, f32 projectionDistance) {
    // Stereographic projection from 4D to 3D
    // Project from w = projectionDistance through the origin
    f32 denom = projectionDistance - point.w;
    if (Math::Abs(denom) < Math::EPSILON) {
        denom = Math::EPSILON;
    }
    f32 scale = projectionDistance / denom;
    return Math::Vector3(point.x * scale, point.y * scale, point.z * scale);
}

Math::Vector3 Projection4D::PerspectiveProject4D(const Vector4D& point, f32 fov4D) {
    // Perspective projection: divide by w-component (offset to avoid singularity)
    f32 tanHalfFOV = Math::Tan(fov4D * 0.5f);
    if (tanHalfFOV < Math::EPSILON) tanHalfFOV = Math::EPSILON;

    f32 wFactor = 1.0f / (point.w + 2.0f);  // Offset so w=0 maps to reasonable depth
    f32 scale = wFactor / tanHalfFOV;

    return Math::Vector3(point.x * scale, point.y * scale, point.z * scale);
}

// --- Built-in Polytopes ---

Polytope4D Projection4D::GenerateTesseract() {
    Polytope4D p;
    // 16 vertices: all combinations of (+/-1, +/-1, +/-1, +/-1)
    p.vertices.reserve(16);
    for (i32 i = 0; i < 16; ++i) {
        f32 x = (i & 1) ? 1.0f : -1.0f;
        f32 y = (i & 2) ? 1.0f : -1.0f;
        f32 z = (i & 4) ? 1.0f : -1.0f;
        f32 w = (i & 8) ? 1.0f : -1.0f;
        p.vertices.push_back({x, y, z, w});
    }

    // 32 edges: connect vertices that differ in exactly one coordinate
    p.edges.reserve(32);
    for (u32 i = 0; i < 16; ++i) {
        for (u32 j = i + 1; j < 16; ++j) {
            // Count differing bits
            u32 diff = i ^ j;
            // If exactly one bit differs, it's an edge
            if (diff != 0 && (diff & (diff - 1)) == 0) {
                p.edges.push_back({i, j});
            }
        }
    }

    return p;
}

Polytope4D Projection4D::Generate5Cell() {
    Polytope4D p;
    // 5-cell (pentachoron / 4-simplex)
    // 5 vertices in 4D, forming a regular simplex
    p.vertices.reserve(5);

    // Standard coordinates for a regular 4-simplex
    f32 a = 1.0f / Math::Sqrt(10.0f);
    f32 b = 1.0f / Math::Sqrt(6.0f);
    f32 c = 1.0f / Math::Sqrt(3.0f);

    p.vertices.push_back({1.0f, 0.0f, -a, -a});
    p.vertices.push_back({-1.0f, 0.0f, -a, -a});
    p.vertices.push_back({0.0f, 1.0f, b, -a});
    p.vertices.push_back({0.0f, -1.0f, b, -a});
    p.vertices.push_back({0.0f, 0.0f, 0.0f, 4.0f * a});

    // Normalize all vertices to unit distance from origin
    for (auto& v : p.vertices) {
        v = v.Normalized();
    }

    // 10 edges: every pair of vertices is connected
    p.edges.reserve(10);
    for (u32 i = 0; i < 5; ++i) {
        for (u32 j = i + 1; j < 5; ++j) {
            p.edges.push_back({i, j});
        }
    }

    return p;
}

Polytope4D Projection4D::Generate16Cell() {
    Polytope4D p;
    // 16-cell (hexadecachoron / 4-orthoplex)
    // 8 vertices: +/-1 along each axis
    p.vertices.reserve(8);
    p.vertices.push_back({ 1, 0, 0, 0});
    p.vertices.push_back({-1, 0, 0, 0});
    p.vertices.push_back({0,  1, 0, 0});
    p.vertices.push_back({0, -1, 0, 0});
    p.vertices.push_back({0, 0,  1, 0});
    p.vertices.push_back({0, 0, -1, 0});
    p.vertices.push_back({0, 0, 0,  1});
    p.vertices.push_back({0, 0, 0, -1});

    // 24 edges: connect every pair that is NOT antipodal (i.e., not opposite)
    p.edges.reserve(24);
    for (u32 i = 0; i < 8; ++i) {
        for (u32 j = i + 1; j < 8; ++j) {
            // Antipodal pairs: (0,1), (2,3), (4,5), (6,7)
            // Two vertices are NOT antipodal if they aren't in the same pair
            bool antipodal = (i / 2 == j / 2) && (i % 2 != j % 2);
            if (!antipodal) {
                p.edges.push_back({i, j});
            }
        }
    }

    return p;
}

Polytope4D Projection4D::Generate24Cell() {
    Polytope4D p;
    // 24-cell (icositetrachoron)
    // 24 vertices: permutations of (+/-1, +/-1, 0, 0)
    p.vertices.reserve(24);

    // All permutations of (+/-1, +/-1, 0, 0)
    f32 coords[4] = {0, 0, 0, 0};
    for (i32 axis1 = 0; axis1 < 4; ++axis1) {
        for (i32 axis2 = axis1 + 1; axis2 < 4; ++axis2) {
            for (i32 s1 = -1; s1 <= 1; s1 += 2) {
                for (i32 s2 = -1; s2 <= 1; s2 += 2) {
                    f32 v[4] = {0, 0, 0, 0};
                    v[axis1] = static_cast<f32>(s1);
                    v[axis2] = static_cast<f32>(s2);
                    p.vertices.push_back({v[0], v[1], v[2], v[3]});
                }
            }
        }
    }

    // 96 edges: vertices at distance sqrt(2) apart
    p.edges.reserve(96);
    f32 edgeLenSq = 2.0f;
    f32 tolerance = 0.01f;
    for (u32 i = 0; i < static_cast<u32>(p.vertices.size()); ++i) {
        for (u32 j = i + 1; j < static_cast<u32>(p.vertices.size()); ++j) {
            auto diff = p.vertices[i] - p.vertices[j];
            f32 distSq = diff.Dot(diff);
            if (Math::Abs(distSq - edgeLenSq) < tolerance) {
                p.edges.push_back({i, j});
            }
        }
    }

    return p;
}

Polytope4D Projection4D::Generate120Cell() {
    Polytope4D p;
    // 120-cell (hecatonicosachoron)
    // 600 vertices, 1200 edges
    //
    // The 120-cell has 600 vertices. We use the standard construction based on
    // permutations of coordinates involving the golden ratio phi = (1+sqrt(5))/2.

    f32 phi = (1.0f + Math::Sqrt(5.0f)) * 0.5f;       // ~1.618
    f32 phi2 = phi * phi;                               // ~2.618
    f32 phiInv = 1.0f / phi;                            // ~0.618
    f32 phi2Inv = 1.0f / phi2;                          // ~0.382

    // Helper: add all even permutations with all sign combinations
    auto addAllSignPerms = [&](f32 a, f32 b, f32 c, f32 d) {
        f32 vals[4] = {a, b, c, d};
        // Generate all 24 permutations
        i32 perm[4] = {0, 1, 2, 3};
        // Use all permutations (sorted initially)
        std::sort(perm, perm + 4);
        do {
            f32 base[4] = {vals[perm[0]], vals[perm[1]], vals[perm[2]], vals[perm[3]]};
            // All sign combinations (2^k for non-zero components)
            for (i32 signs = 0; signs < 16; ++signs) {
                f32 v[4];
                v[0] = (signs & 1) ? -base[0] : base[0];
                v[1] = (signs & 2) ? -base[1] : base[1];
                v[2] = (signs & 4) ? -base[2] : base[2];
                v[3] = (signs & 8) ? -base[3] : base[3];
                // Skip if a zero component gets a negative sign (would duplicate)
                bool skip = false;
                for (i32 k = 0; k < 4; ++k) {
                    if (Math::Abs(base[k]) < Math::EPSILON && (signs & (1 << k))) {
                        skip = true;
                        break;
                    }
                }
                if (skip) continue;
                p.vertices.push_back({v[0], v[1], v[2], v[3]});
            }
        } while (std::next_permutation(perm, perm + 4));
    };

    // The 120-cell vertices are combinations of:
    // All permutations of (0, 0, +/-2, +/-2) - 24 vertices
    addAllSignPerms(0.0f, 0.0f, 2.0f, 2.0f);

    // All permutations of (+/-1, +/-1, +/-1, +/-sqrt(5))
    f32 sqrt5 = Math::Sqrt(5.0f);
    addAllSignPerms(1.0f, 1.0f, 1.0f, sqrt5);

    // All permutations of (+/-phi^-2, +/-phi, +/-phi, +/-phi)
    addAllSignPerms(phiInv, phi, phi, phi);

    // All permutations of (+/-phi^-1, +/-phi^-1, +/-phi^-1, +/-phi^2)
    addAllSignPerms(phi2Inv, phiInv, phiInv, phi2);

    // Remove duplicate vertices (within tolerance)
    std::vector<Vector4D> unique;
    unique.reserve(600);
    for (const auto& v : p.vertices) {
        bool dup = false;
        for (const auto& u : unique) {
            auto diff = v - u;
            if (diff.Dot(diff) < 0.001f) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            unique.push_back(v);
        }
    }
    p.vertices = unique;

    // Normalize all vertices to the same radius
    if (!p.vertices.empty()) {
        f32 radius = p.vertices[0].Length();
        if (radius > Math::EPSILON) {
            for (auto& v : p.vertices) {
                f32 len = v.Length();
                if (len > Math::EPSILON) {
                    v = v * (radius / len);
                }
            }
        }
    }

    // Find edges: vertices at the minimum non-zero distance apart
    // First pass: find the minimum edge length
    f32 minDistSq = Math::FLOAT_MAX;
    u32 vCount = static_cast<u32>(p.vertices.size());
    for (u32 i = 0; i < vCount && i < 50; ++i) {  // Sample subset for speed
        for (u32 j = i + 1; j < vCount; ++j) {
            auto diff = p.vertices[i] - p.vertices[j];
            f32 dSq = diff.Dot(diff);
            if (dSq > Math::EPSILON && dSq < minDistSq) {
                minDistSq = dSq;
            }
        }
    }

    // Second pass: collect all edges at minimum distance
    f32 edgeTolerance = minDistSq * 0.1f;
    p.edges.reserve(1200);
    for (u32 i = 0; i < vCount; ++i) {
        for (u32 j = i + 1; j < vCount; ++j) {
            auto diff = p.vertices[i] - p.vertices[j];
            f32 dSq = diff.Dot(diff);
            if (Math::Abs(dSq - minDistSq) < edgeTolerance) {
                p.edges.push_back({i, j});
            }
        }
    }

    return p;
}

// --- Rotation & Animation ---

Matrix4D Projection4D::BuildRotationMatrix(f32 dt, const RotationConfig4D& config) {
    Matrix4D result;  // Identity

    if (Math::Abs(config.speedXY) > Math::EPSILON)
        result = result * Matrix4D::RotateXY(config.speedXY * dt);
    if (Math::Abs(config.speedXZ) > Math::EPSILON)
        result = result * Matrix4D::RotateXZ(config.speedXZ * dt);
    if (Math::Abs(config.speedXW) > Math::EPSILON)
        result = result * Matrix4D::RotateXW(config.speedXW * dt);
    if (Math::Abs(config.speedYZ) > Math::EPSILON)
        result = result * Matrix4D::RotateYZ(config.speedYZ * dt);
    if (Math::Abs(config.speedYW) > Math::EPSILON)
        result = result * Matrix4D::RotateYW(config.speedYW * dt);
    if (Math::Abs(config.speedZW) > Math::EPSILON)
        result = result * Matrix4D::RotateZW(config.speedZW * dt);

    return result;
}

void Projection4D::ApplyRotation(Polytope4D& polytope, const Matrix4D& rotation) {
    for (auto& v : polytope.vertices) {
        v = rotation.Transform(v);
    }
}

void Projection4D::AnimateRotation(Polytope4D& polytope, f32 dt, const RotationConfig4D& config) {
    m_AngleXY += config.speedXY * dt;
    m_AngleXZ += config.speedXZ * dt;
    m_AngleXW += config.speedXW * dt;
    m_AngleYZ += config.speedYZ * dt;
    m_AngleYW += config.speedYW * dt;
    m_AngleZW += config.speedZW * dt;

    Matrix4D rotation = BuildRotationMatrix(dt, config);
    ApplyRotation(polytope, rotation);
}

void Projection4D::ResetAngles() {
    m_AngleXY = m_AngleXZ = m_AngleXW = 0.0f;
    m_AngleYZ = m_AngleYW = m_AngleZW = 0.0f;
}

// --- Mesh Generation ---

Projection4DMeshData Projection4D::GenerateWireframeMesh(
    const Polytope4D& polytope,
    f32 lineWidth,
    f32 projectionDistance,
    u32 tubeSegments)
{
    Projection4DMeshData mesh;
    if (polytope.edges.empty() || polytope.vertices.empty()) return mesh;

    // Pre-project all vertices to 3D
    std::vector<Math::Vector3> projected;
    projected.reserve(polytope.vertices.size());
    for (const auto& v : polytope.vertices) {
        projected.push_back(StereographicProject(v, projectionDistance));
    }

    // Generate tube mesh for each edge
    for (const auto& [idxA, idxB] : polytope.edges) {
        if (idxA >= projected.size() || idxB >= projected.size()) continue;

        Math::Vector3 start = projected[idxA];
        Math::Vector3 end = projected[idxB];
        Math::Vector3 dir = end - start;
        f32 length = dir.Length();
        if (length < Math::EPSILON) continue;
        dir = dir / length;

        // Build a local coordinate frame perpendicular to the edge
        Math::Vector3 up(0.0f, 1.0f, 0.0f);
        if (Math::Abs(dir.Dot(up)) > 0.99f) {
            up = Math::Vector3(1.0f, 0.0f, 0.0f);
        }
        Math::Vector3 right = dir.Cross(up).Normalized();
        Math::Vector3 actualUp = right.Cross(dir).Normalized();

        u32 baseIndex = static_cast<u32>(mesh.vertices.size());

        // Generate tube ring at start and end of edge
        for (u32 ring = 0; ring < 2; ++ring) {
            Math::Vector3 center = (ring == 0) ? start : end;
            f32 vCoord = (ring == 0) ? 0.0f : 1.0f;

            for (u32 seg = 0; seg < tubeSegments; ++seg) {
                f32 angle = 2.0f * Math::PI * static_cast<f32>(seg) / static_cast<f32>(tubeSegments);
                f32 cosA = Math::Cos(angle);
                f32 sinA = Math::Sin(angle);

                Math::Vector3 offset = right * (cosA * lineWidth) + actualUp * (sinA * lineWidth);
                Math::Vector3 normal = offset.Normalized();

                Projection4DMeshData::Vertex v;
                v.position = center + offset;
                v.normal = normal;
                v.uv = Math::Vector2(static_cast<f32>(seg) / static_cast<f32>(tubeSegments), vCoord);
                mesh.vertices.push_back(v);
            }
        }

        // Generate indices (connect start ring to end ring)
        for (u32 seg = 0; seg < tubeSegments; ++seg) {
            u32 s0 = baseIndex + seg;
            u32 s1 = baseIndex + (seg + 1) % tubeSegments;
            u32 e0 = baseIndex + tubeSegments + seg;
            u32 e1 = baseIndex + tubeSegments + (seg + 1) % tubeSegments;

            mesh.indices.push_back(s0);
            mesh.indices.push_back(e0);
            mesh.indices.push_back(s1);

            mesh.indices.push_back(s1);
            mesh.indices.push_back(e0);
            mesh.indices.push_back(e1);
        }
    }

    return mesh;
}

} // namespace Effects
} // namespace Enjin
