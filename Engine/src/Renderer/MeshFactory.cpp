#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Math/Math.h"

namespace Enjin {
namespace Renderer {

ECS::MeshComponent MeshFactory::CreateCube(f32 size) {
    ECS::MeshComponent mesh;
    f32 h = size * 0.5f;

    // Vertices with normals and UVs for each face
    // Front face (z+)
    mesh.vertices.push_back({ Math::Vector3(-h, -h,  h), Math::Vector3(0, 0, 1), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3( h, -h,  h), Math::Vector3(0, 0, 1), Math::Vector2(1, 0) });
    mesh.vertices.push_back({ Math::Vector3( h,  h,  h), Math::Vector3(0, 0, 1), Math::Vector2(1, 1) });
    mesh.vertices.push_back({ Math::Vector3(-h,  h,  h), Math::Vector3(0, 0, 1), Math::Vector2(0, 1) });

    // Back face (z-)
    mesh.vertices.push_back({ Math::Vector3( h, -h, -h), Math::Vector3(0, 0, -1), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3(-h, -h, -h), Math::Vector3(0, 0, -1), Math::Vector2(1, 0) });
    mesh.vertices.push_back({ Math::Vector3(-h,  h, -h), Math::Vector3(0, 0, -1), Math::Vector2(1, 1) });
    mesh.vertices.push_back({ Math::Vector3( h,  h, -h), Math::Vector3(0, 0, -1), Math::Vector2(0, 1) });

    // Top face (y+)
    mesh.vertices.push_back({ Math::Vector3(-h,  h,  h), Math::Vector3(0, 1, 0), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3( h,  h,  h), Math::Vector3(0, 1, 0), Math::Vector2(1, 0) });
    mesh.vertices.push_back({ Math::Vector3( h,  h, -h), Math::Vector3(0, 1, 0), Math::Vector2(1, 1) });
    mesh.vertices.push_back({ Math::Vector3(-h,  h, -h), Math::Vector3(0, 1, 0), Math::Vector2(0, 1) });

    // Bottom face (y-)
    mesh.vertices.push_back({ Math::Vector3(-h, -h, -h), Math::Vector3(0, -1, 0), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3( h, -h, -h), Math::Vector3(0, -1, 0), Math::Vector2(1, 0) });
    mesh.vertices.push_back({ Math::Vector3( h, -h,  h), Math::Vector3(0, -1, 0), Math::Vector2(1, 1) });
    mesh.vertices.push_back({ Math::Vector3(-h, -h,  h), Math::Vector3(0, -1, 0), Math::Vector2(0, 1) });

    // Right face (x+)
    mesh.vertices.push_back({ Math::Vector3( h, -h,  h), Math::Vector3(1, 0, 0), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3( h, -h, -h), Math::Vector3(1, 0, 0), Math::Vector2(1, 0) });
    mesh.vertices.push_back({ Math::Vector3( h,  h, -h), Math::Vector3(1, 0, 0), Math::Vector2(1, 1) });
    mesh.vertices.push_back({ Math::Vector3( h,  h,  h), Math::Vector3(1, 0, 0), Math::Vector2(0, 1) });

    // Left face (x-)
    mesh.vertices.push_back({ Math::Vector3(-h, -h, -h), Math::Vector3(-1, 0, 0), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3(-h, -h,  h), Math::Vector3(-1, 0, 0), Math::Vector2(1, 0) });
    mesh.vertices.push_back({ Math::Vector3(-h,  h,  h), Math::Vector3(-1, 0, 0), Math::Vector2(1, 1) });
    mesh.vertices.push_back({ Math::Vector3(-h,  h, -h), Math::Vector3(-1, 0, 0), Math::Vector2(0, 1) });

    // Indices (two triangles per face)
    for (u32 face = 0; face < 6; ++face) {
        u32 base = face * 4;
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 0);
        mesh.indices.push_back(base + 2);
        mesh.indices.push_back(base + 3);
    }

    return mesh;
}

ECS::MeshComponent MeshFactory::CreatePlane(f32 width, f32 height, u32 subdivisionsX, u32 subdivisionsZ) {
    ECS::MeshComponent mesh;

    f32 halfW = width * 0.5f;
    f32 halfH = height * 0.5f;

    u32 vertsX = subdivisionsX + 1;
    u32 vertsZ = subdivisionsZ + 1;

    // Generate vertices
    for (u32 z = 0; z < vertsZ; ++z) {
        for (u32 x = 0; x < vertsX; ++x) {
            f32 u = static_cast<f32>(x) / subdivisionsX;
            f32 v = static_cast<f32>(z) / subdivisionsZ;

            Math::Vector3 pos(
                -halfW + u * width,
                0.0f,
                -halfH + v * height
            );
            Math::Vector3 normal(0.0f, 1.0f, 0.0f);
            Math::Vector2 uv(u, v);

            mesh.vertices.push_back({ pos, normal, uv });
        }
    }

    // Generate indices
    for (u32 z = 0; z < subdivisionsZ; ++z) {
        for (u32 x = 0; x < subdivisionsX; ++x) {
            u32 topLeft = z * vertsX + x;
            u32 topRight = topLeft + 1;
            u32 bottomLeft = (z + 1) * vertsX + x;
            u32 bottomRight = bottomLeft + 1;

            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);

            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateSphere(f32 radius, u32 segments, u32 rings) {
    ECS::MeshComponent mesh;

    // Generate vertices
    for (u32 ring = 0; ring <= rings; ++ring) {
        f32 phi = Math::PI * static_cast<f32>(ring) / static_cast<f32>(rings);
        f32 y = Math::Cos(phi);
        f32 ringRadius = Math::Sin(phi);

        for (u32 seg = 0; seg <= segments; ++seg) {
            f32 theta = Math::PI_2 * static_cast<f32>(seg) / static_cast<f32>(segments);
            f32 x = ringRadius * Math::Cos(theta);
            f32 z = ringRadius * Math::Sin(theta);

            Math::Vector3 normal(x, y, z);
            Math::Vector3 pos = normal * radius;
            Math::Vector2 uv(
                static_cast<f32>(seg) / static_cast<f32>(segments),
                static_cast<f32>(ring) / static_cast<f32>(rings)
            );

            mesh.vertices.push_back({ pos, normal, uv });
        }
    }

    // Generate indices
    for (u32 ring = 0; ring < rings; ++ring) {
        for (u32 seg = 0; seg < segments; ++seg) {
            u32 current = ring * (segments + 1) + seg;
            u32 next = current + segments + 1;

            mesh.indices.push_back(current);
            mesh.indices.push_back(next);
            mesh.indices.push_back(current + 1);

            mesh.indices.push_back(current + 1);
            mesh.indices.push_back(next);
            mesh.indices.push_back(next + 1);
        }
    }

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateCylinder(f32 radius, f32 height, u32 segments) {
    ECS::MeshComponent mesh;
    f32 halfH = height * 0.5f;

    // Side vertices
    for (u32 i = 0; i <= segments; ++i) {
        f32 theta = Math::PI_2 * static_cast<f32>(i) / static_cast<f32>(segments);
        f32 x = Math::Cos(theta);
        f32 z = Math::Sin(theta);
        f32 u = static_cast<f32>(i) / static_cast<f32>(segments);

        Math::Vector3 normal(x, 0, z);

        // Bottom vertex
        mesh.vertices.push_back({
            Math::Vector3(x * radius, -halfH, z * radius),
            normal,
            Math::Vector2(u, 0.0f)
        });

        // Top vertex
        mesh.vertices.push_back({
            Math::Vector3(x * radius, halfH, z * radius),
            normal,
            Math::Vector2(u, 1.0f)
        });
    }

    // Side indices
    for (u32 i = 0; i < segments; ++i) {
        u32 bl = i * 2;
        u32 tl = bl + 1;
        u32 br = (i + 1) * 2;
        u32 tr = br + 1;

        mesh.indices.push_back(bl);
        mesh.indices.push_back(br);
        mesh.indices.push_back(tl);

        mesh.indices.push_back(tl);
        mesh.indices.push_back(br);
        mesh.indices.push_back(tr);
    }

    // Top cap center
    u32 topCenterIdx = static_cast<u32>(mesh.vertices.size());
    mesh.vertices.push_back({
        Math::Vector3(0, halfH, 0),
        Math::Vector3(0, 1, 0),
        Math::Vector2(0.5f, 0.5f)
    });

    // Top cap vertices
    u32 topStartIdx = static_cast<u32>(mesh.vertices.size());
    for (u32 i = 0; i <= segments; ++i) {
        f32 theta = Math::PI_2 * static_cast<f32>(i) / static_cast<f32>(segments);
        f32 x = Math::Cos(theta);
        f32 z = Math::Sin(theta);

        mesh.vertices.push_back({
            Math::Vector3(x * radius, halfH, z * radius),
            Math::Vector3(0, 1, 0),
            Math::Vector2(x * 0.5f + 0.5f, z * 0.5f + 0.5f)
        });
    }

    // Top cap indices
    for (u32 i = 0; i < segments; ++i) {
        mesh.indices.push_back(topCenterIdx);
        mesh.indices.push_back(topStartIdx + i);
        mesh.indices.push_back(topStartIdx + i + 1);
    }

    // Bottom cap center
    u32 bottomCenterIdx = static_cast<u32>(mesh.vertices.size());
    mesh.vertices.push_back({
        Math::Vector3(0, -halfH, 0),
        Math::Vector3(0, -1, 0),
        Math::Vector2(0.5f, 0.5f)
    });

    // Bottom cap vertices
    u32 bottomStartIdx = static_cast<u32>(mesh.vertices.size());
    for (u32 i = 0; i <= segments; ++i) {
        f32 theta = Math::PI_2 * static_cast<f32>(i) / static_cast<f32>(segments);
        f32 x = Math::Cos(theta);
        f32 z = Math::Sin(theta);

        mesh.vertices.push_back({
            Math::Vector3(x * radius, -halfH, z * radius),
            Math::Vector3(0, -1, 0),
            Math::Vector2(x * 0.5f + 0.5f, z * 0.5f + 0.5f)
        });
    }

    // Bottom cap indices (reversed winding)
    for (u32 i = 0; i < segments; ++i) {
        mesh.indices.push_back(bottomCenterIdx);
        mesh.indices.push_back(bottomStartIdx + i + 1);
        mesh.indices.push_back(bottomStartIdx + i);
    }

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateCone(f32 radius, f32 height, u32 segments) {
    ECS::MeshComponent mesh;
    f32 halfH = height * 0.5f;

    // Apex vertex
    u32 apexIdx = 0;
    mesh.vertices.push_back({
        Math::Vector3(0, halfH, 0),
        Math::Vector3(0, 1, 0), // Will be averaged
        Math::Vector2(0.5f, 0.0f)
    });

    // Side vertices (at base)
    f32 slopeAngle = Math::Atan(radius / height);
    f32 normalY = Math::Sin(slopeAngle);
    f32 normalXZScale = Math::Cos(slopeAngle);

    for (u32 i = 0; i <= segments; ++i) {
        f32 theta = Math::PI_2 * static_cast<f32>(i) / static_cast<f32>(segments);
        f32 x = Math::Cos(theta);
        f32 z = Math::Sin(theta);

        Math::Vector3 normal(x * normalXZScale, normalY, z * normalXZScale);

        mesh.vertices.push_back({
            Math::Vector3(x * radius, -halfH, z * radius),
            normal,
            Math::Vector2(static_cast<f32>(i) / static_cast<f32>(segments), 1.0f)
        });
    }

    // Side indices
    for (u32 i = 0; i < segments; ++i) {
        mesh.indices.push_back(apexIdx);
        mesh.indices.push_back(1 + i + 1);
        mesh.indices.push_back(1 + i);
    }

    // Bottom cap center
    u32 bottomCenterIdx = static_cast<u32>(mesh.vertices.size());
    mesh.vertices.push_back({
        Math::Vector3(0, -halfH, 0),
        Math::Vector3(0, -1, 0),
        Math::Vector2(0.5f, 0.5f)
    });

    // Bottom cap vertices
    u32 bottomStartIdx = static_cast<u32>(mesh.vertices.size());
    for (u32 i = 0; i <= segments; ++i) {
        f32 theta = Math::PI_2 * static_cast<f32>(i) / static_cast<f32>(segments);
        f32 x = Math::Cos(theta);
        f32 z = Math::Sin(theta);

        mesh.vertices.push_back({
            Math::Vector3(x * radius, -halfH, z * radius),
            Math::Vector3(0, -1, 0),
            Math::Vector2(x * 0.5f + 0.5f, z * 0.5f + 0.5f)
        });
    }

    // Bottom cap indices
    for (u32 i = 0; i < segments; ++i) {
        mesh.indices.push_back(bottomCenterIdx);
        mesh.indices.push_back(bottomStartIdx + i + 1);
        mesh.indices.push_back(bottomStartIdx + i);
    }

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateQuad(f32 width, f32 height) {
    ECS::MeshComponent mesh;
    f32 hw = width * 0.5f;
    f32 hh = height * 0.5f;

    mesh.vertices = {
        { Math::Vector3(-hw, -hh, 0), Math::Vector3(0, 0, 1), Math::Vector2(0, 0) },
        { Math::Vector3( hw, -hh, 0), Math::Vector3(0, 0, 1), Math::Vector2(1, 0) },
        { Math::Vector3( hw,  hh, 0), Math::Vector3(0, 0, 1), Math::Vector2(1, 1) },
        { Math::Vector3(-hw,  hh, 0), Math::Vector3(0, 0, 1), Math::Vector2(0, 1) }
    };

    mesh.indices = { 0, 1, 2, 0, 2, 3 };

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateFromGLTF(const Assets::GLTFPrimitive& primitive) {
    ECS::MeshComponent mesh;

    // Convert GLTFVertex to MeshComponent::Vertex
    mesh.vertices.reserve(primitive.vertices.size());
    for (const auto& v : primitive.vertices) {
        mesh.vertices.push_back({
            v.position,
            v.normal,
            v.texCoord
        });
    }

    mesh.indices = primitive.indices;

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateAxes(f32 length) {
    ECS::MeshComponent mesh;

    // X axis (red)
    mesh.vertices.push_back({ Math::Vector3(0, 0, 0), Math::Vector3(1, 0, 0), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3(length, 0, 0), Math::Vector3(1, 0, 0), Math::Vector2(1, 0) });

    // Y axis (green)
    mesh.vertices.push_back({ Math::Vector3(0, 0, 0), Math::Vector3(0, 1, 0), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3(0, length, 0), Math::Vector3(0, 1, 0), Math::Vector2(1, 0) });

    // Z axis (blue)
    mesh.vertices.push_back({ Math::Vector3(0, 0, 0), Math::Vector3(0, 0, 1), Math::Vector2(0, 0) });
    mesh.vertices.push_back({ Math::Vector3(0, 0, length), Math::Vector3(0, 0, 1), Math::Vector2(1, 0) });

    mesh.indices = { 0, 1, 2, 3, 4, 5 };

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateGrid(f32 size, u32 divisions) {
    ECS::MeshComponent mesh;
    f32 halfSize = size * 0.5f;
    f32 step = size / static_cast<f32>(divisions);

    // Horizontal lines (along X)
    for (u32 i = 0; i <= divisions; ++i) {
        f32 z = -halfSize + step * static_cast<f32>(i);
        u32 idx = static_cast<u32>(mesh.vertices.size());

        mesh.vertices.push_back({
            Math::Vector3(-halfSize, 0, z),
            Math::Vector3(0, 1, 0),
            Math::Vector2(0, static_cast<f32>(i) / divisions)
        });
        mesh.vertices.push_back({
            Math::Vector3(halfSize, 0, z),
            Math::Vector3(0, 1, 0),
            Math::Vector2(1, static_cast<f32>(i) / divisions)
        });

        mesh.indices.push_back(idx);
        mesh.indices.push_back(idx + 1);
    }

    // Vertical lines (along Z)
    for (u32 i = 0; i <= divisions; ++i) {
        f32 x = -halfSize + step * static_cast<f32>(i);
        u32 idx = static_cast<u32>(mesh.vertices.size());

        mesh.vertices.push_back({
            Math::Vector3(x, 0, -halfSize),
            Math::Vector3(0, 1, 0),
            Math::Vector2(static_cast<f32>(i) / divisions, 0)
        });
        mesh.vertices.push_back({
            Math::Vector3(x, 0, halfSize),
            Math::Vector3(0, 1, 0),
            Math::Vector2(static_cast<f32>(i) / divisions, 1)
        });

        mesh.indices.push_back(idx);
        mesh.indices.push_back(idx + 1);
    }

    return mesh;
}

void MeshFactory::CalculateTangents(ECS::MeshComponent& mesh) {
    if (mesh.indices.size() < 3 || mesh.vertices.empty()) return;

    // Lengyel's method for computing tangent vectors
    // Accumulate per-triangle tangents into per-vertex tangents
    usize vertCount = mesh.vertices.size();
    std::vector<Math::Vector3> tan1(vertCount, Math::Vector3(0, 0, 0));

    for (usize i = 0; i + 2 < mesh.indices.size(); i += 3) {
        u32 i0 = mesh.indices[i];
        u32 i1 = mesh.indices[i + 1];
        u32 i2 = mesh.indices[i + 2];

        const auto& v0 = mesh.vertices[i0];
        const auto& v1 = mesh.vertices[i1];
        const auto& v2 = mesh.vertices[i2];

        Math::Vector3 edge1 = v1.position - v0.position;
        Math::Vector3 edge2 = v2.position - v0.position;

        f32 du1 = v1.uv.x - v0.uv.x;
        f32 dv1 = v1.uv.y - v0.uv.y;
        f32 du2 = v2.uv.x - v0.uv.x;
        f32 dv2 = v2.uv.y - v0.uv.y;

        f32 denom = du1 * dv2 - du2 * dv1;
        if (Math::Abs(denom) < 1e-8f) continue;
        f32 r = 1.0f / denom;

        Math::Vector3 tangent(
            (dv2 * edge1.x - dv1 * edge2.x) * r,
            (dv2 * edge1.y - dv1 * edge2.y) * r,
            (dv2 * edge1.z - dv1 * edge2.z) * r
        );

        tan1[i0] = tan1[i0] + tangent;
        tan1[i1] = tan1[i1] + tangent;
        tan1[i2] = tan1[i2] + tangent;
    }

    // Gram-Schmidt orthogonalize tangent against normal and store in vertex tangent field
    for (usize i = 0; i < vertCount; ++i) {
        const Math::Vector3& n = mesh.vertices[i].normal;
        const Math::Vector3& t = tan1[i];

        // Gram-Schmidt: tangent = normalize(t - n * dot(n, t))
        f32 dot = n.x * t.x + n.y * t.y + n.z * t.z;
        Math::Vector3 ortho(t.x - n.x * dot, t.y - n.y * dot, t.z - n.z * dot);
        f32 len = Math::Sqrt(ortho.x * ortho.x + ortho.y * ortho.y + ortho.z * ortho.z);
        if (len > 1e-6f) {
            ortho = ortho * (1.0f / len);
        } else {
            // Fallback tangent perpendicular to normal
            if (Math::Abs(n.x) < 0.9f) {
                ortho = Math::Vector3(1, 0, 0);
            } else {
                ortho = Math::Vector3(0, 1, 0);
            }
            dot = n.x * ortho.x + n.y * ortho.y + n.z * ortho.z;
            ortho = Math::Vector3(ortho.x - n.x * dot, ortho.y - n.y * dot, ortho.z - n.z * dot);
            len = Math::Sqrt(ortho.x * ortho.x + ortho.y * ortho.y + ortho.z * ortho.z);
            if (len > 1e-6f) ortho = ortho * (1.0f / len);
        }

        // w = handedness (sign of cross(n, t) · bitangent)
        mesh.vertices[i].tangent = Math::Vector4(ortho.x, ortho.y, ortho.z, 1.0f);
    }
}

} // namespace Renderer
} // namespace Enjin
