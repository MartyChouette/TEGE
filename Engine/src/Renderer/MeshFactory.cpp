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
    f32 slopeAngle = (height > 1e-6f) ? Math::Atan(radius / height) : Math::PI * 0.5f;
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

ECS::MeshComponent MeshFactory::CreateCapsule(f32 radius, f32 height, u32 segments, u32 rings) {
    ECS::MeshComponent mesh;
    f32 halfH = height * 0.5f;

    // Top hemisphere
    for (u32 ring = 0; ring <= rings; ++ring) {
        f32 phi = Math::PI * 0.5f * static_cast<f32>(ring) / static_cast<f32>(rings); // 0 to PI/2
        f32 y = Math::Cos(phi);
        f32 ringRadius = Math::Sin(phi);

        for (u32 seg = 0; seg <= segments; ++seg) {
            f32 theta = Math::PI_2 * static_cast<f32>(seg) / static_cast<f32>(segments);
            f32 x = ringRadius * Math::Cos(theta);
            f32 z = ringRadius * Math::Sin(theta);

            Math::Vector3 normal(x, y, z);
            Math::Vector3 pos = Math::Vector3(x * radius, y * radius + halfH, z * radius);
            Math::Vector2 uv(
                static_cast<f32>(seg) / static_cast<f32>(segments),
                static_cast<f32>(ring) / static_cast<f32>(rings * 2 + 1)
            );

            mesh.vertices.push_back({ pos, normal, uv });
        }
    }

    // Cylinder body
    u32 bodyRings = 2;
    for (u32 ring = 0; ring <= bodyRings; ++ring) {
        f32 t = static_cast<f32>(ring) / static_cast<f32>(bodyRings);
        f32 y = halfH - t * height;

        for (u32 seg = 0; seg <= segments; ++seg) {
            f32 theta = Math::PI_2 * static_cast<f32>(seg) / static_cast<f32>(segments);
            f32 x = Math::Cos(theta);
            f32 z = Math::Sin(theta);

            Math::Vector3 normal(x, 0, z);
            Math::Vector3 pos = Math::Vector3(x * radius, y, z * radius);
            f32 vCoord = (static_cast<f32>(rings) + t * static_cast<f32>(bodyRings)) / static_cast<f32>(rings * 2 + bodyRings);
            Math::Vector2 uv(
                static_cast<f32>(seg) / static_cast<f32>(segments),
                vCoord
            );

            mesh.vertices.push_back({ pos, normal, uv });
        }
    }

    // Bottom hemisphere
    for (u32 ring = 0; ring <= rings; ++ring) {
        f32 phi = Math::PI * 0.5f + Math::PI * 0.5f * static_cast<f32>(ring) / static_cast<f32>(rings); // PI/2 to PI
        f32 y = Math::Cos(phi);
        f32 ringRadius = Math::Sin(phi);

        for (u32 seg = 0; seg <= segments; ++seg) {
            f32 theta = Math::PI_2 * static_cast<f32>(seg) / static_cast<f32>(segments);
            f32 x = ringRadius * Math::Cos(theta);
            f32 z = ringRadius * Math::Sin(theta);

            Math::Vector3 normal(x, y, z);
            Math::Vector3 pos = Math::Vector3(x * radius, y * radius - halfH, z * radius);
            Math::Vector2 uv(
                static_cast<f32>(seg) / static_cast<f32>(segments),
                1.0f - static_cast<f32>(rings - ring) / static_cast<f32>(rings * 2 + 1)
            );

            mesh.vertices.push_back({ pos, normal, uv });
        }
    }

    // Generate indices for all ring strips
    u32 totalRings = rings + bodyRings + rings;
    for (u32 ring = 0; ring < totalRings; ++ring) {
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

ECS::MeshComponent MeshFactory::CreateTriangle(f32 size) {
    ECS::MeshComponent mesh;

    // Equilateral triangle centered at origin, lying in XY plane facing +Z
    f32 h = size * Math::Sqrt(3.0f) * 0.5f;
    f32 yBottom = -h / 3.0f;
    f32 yTop = 2.0f * h / 3.0f;
    f32 halfBase = size * 0.5f;

    Math::Vector3 normal(0.0f, 0.0f, 1.0f);

    // Front face
    mesh.vertices.push_back({ Math::Vector3(-halfBase, yBottom, 0.0f), normal, Math::Vector2(0.0f, 0.0f) });
    mesh.vertices.push_back({ Math::Vector3( halfBase, yBottom, 0.0f), normal, Math::Vector2(1.0f, 0.0f) });
    mesh.vertices.push_back({ Math::Vector3(0.0f, yTop, 0.0f), normal, Math::Vector2(0.5f, 1.0f) });

    // Back face
    Math::Vector3 backNormal(0.0f, 0.0f, -1.0f);
    mesh.vertices.push_back({ Math::Vector3( halfBase, yBottom, 0.0f), backNormal, Math::Vector2(0.0f, 0.0f) });
    mesh.vertices.push_back({ Math::Vector3(-halfBase, yBottom, 0.0f), backNormal, Math::Vector2(1.0f, 0.0f) });
    mesh.vertices.push_back({ Math::Vector3(0.0f, yTop, 0.0f), backNormal, Math::Vector2(0.5f, 1.0f) });

    mesh.indices = { 0, 1, 2, 3, 4, 5 };

    return mesh;
}

ECS::MeshComponent MeshFactory::CreatePyramid(f32 base, f32 height) {
    ECS::MeshComponent mesh;
    f32 hb = base * 0.5f;
    f32 halfH = height * 0.5f;

    // 5 unique positions
    Math::Vector3 apex(0.0f, halfH, 0.0f);
    Math::Vector3 bl(-hb, -halfH, -hb);
    Math::Vector3 br( hb, -halfH, -hb);
    Math::Vector3 fr( hb, -halfH,  hb);
    Math::Vector3 fl(-hb, -halfH,  hb);

    // Helper: compute face normal from 3 points
    auto faceNormal = [](const Math::Vector3& a, const Math::Vector3& b, const Math::Vector3& c) {
        Math::Vector3 e1 = b - a;
        Math::Vector3 e2 = c - a;
        Math::Vector3 n(
            e1.y * e2.z - e1.z * e2.y,
            e1.z * e2.x - e1.x * e2.z,
            e1.x * e2.y - e1.y * e2.x
        );
        f32 len = Math::Sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-6f) { n.x /= len; n.y /= len; n.z /= len; }
        return n;
    };

    // Front face (fl, fr, apex)
    Math::Vector3 nFront = faceNormal(fl, fr, apex);
    mesh.vertices.push_back({ fl, nFront, Math::Vector2(0, 0) });
    mesh.vertices.push_back({ fr, nFront, Math::Vector2(1, 0) });
    mesh.vertices.push_back({ apex, nFront, Math::Vector2(0.5f, 1) });

    // Right face (fr, br, apex)
    Math::Vector3 nRight = faceNormal(fr, br, apex);
    mesh.vertices.push_back({ fr, nRight, Math::Vector2(0, 0) });
    mesh.vertices.push_back({ br, nRight, Math::Vector2(1, 0) });
    mesh.vertices.push_back({ apex, nRight, Math::Vector2(0.5f, 1) });

    // Back face (br, bl, apex)
    Math::Vector3 nBack = faceNormal(br, bl, apex);
    mesh.vertices.push_back({ br, nBack, Math::Vector2(0, 0) });
    mesh.vertices.push_back({ bl, nBack, Math::Vector2(1, 0) });
    mesh.vertices.push_back({ apex, nBack, Math::Vector2(0.5f, 1) });

    // Left face (bl, fl, apex)
    Math::Vector3 nLeft = faceNormal(bl, fl, apex);
    mesh.vertices.push_back({ bl, nLeft, Math::Vector2(0, 0) });
    mesh.vertices.push_back({ fl, nLeft, Math::Vector2(1, 0) });
    mesh.vertices.push_back({ apex, nLeft, Math::Vector2(0.5f, 1) });

    // Bottom face (two triangles)
    Math::Vector3 nBottom(0, -1, 0);
    mesh.vertices.push_back({ bl, nBottom, Math::Vector2(0, 0) });
    mesh.vertices.push_back({ br, nBottom, Math::Vector2(1, 0) });
    mesh.vertices.push_back({ fr, nBottom, Math::Vector2(1, 1) });
    mesh.vertices.push_back({ fl, nBottom, Math::Vector2(0, 1) });

    // Indices: 4 side faces (3 each) + bottom (6)
    for (u32 i = 0; i < 12; ++i) {
        mesh.indices.push_back(i);
    }
    // Bottom quad
    mesh.indices.push_back(12);
    mesh.indices.push_back(13);
    mesh.indices.push_back(14);
    mesh.indices.push_back(12);
    mesh.indices.push_back(14);
    mesh.indices.push_back(15);

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

ECS::MeshComponent MeshFactory::CreateCapsule2D(f32 width, f32 height, u32 capSegments) {
    ECS::MeshComponent mesh;
    f32 radius = width * 0.5f;
    f32 bodyHalf = (height - width) * 0.5f;
    if (bodyHalf < 0.0f) bodyHalf = 0.0f;

    Math::Vector3 normal(0.0f, 0.0f, 1.0f);

    // Center vertex for fan triangulation
    u32 centerIdx = 0;
    mesh.vertices.push_back({ Math::Vector3(0.0f, 0.0f, 0.0f), normal, Math::Vector2(0.5f, 0.5f) });

    // Build outline vertices: bottom cap, left side, top cap, right side
    // Bottom semicircle (from bottom-right going clockwise to bottom-left)
    for (u32 i = 0; i <= capSegments; ++i) {
        f32 angle = Math::PI + Math::PI * static_cast<f32>(i) / static_cast<f32>(capSegments);
        f32 x = radius * Math::Cos(angle);
        f32 y = -bodyHalf + radius * Math::Sin(angle);

        f32 u = (x + radius) / width;
        f32 v = (y + bodyHalf + radius) / height;
        mesh.vertices.push_back({ Math::Vector3(x, y, 0.0f), normal, Math::Vector2(u, v) });
    }

    // Top semicircle (from top-left going clockwise to top-right)
    for (u32 i = 0; i <= capSegments; ++i) {
        f32 angle = Math::PI * static_cast<f32>(i) / static_cast<f32>(capSegments);
        f32 x = radius * Math::Cos(angle);
        f32 y = bodyHalf + radius * Math::Sin(angle);

        f32 u = (x + radius) / width;
        f32 v = (y + bodyHalf + radius) / height;
        mesh.vertices.push_back({ Math::Vector3(x, y, 0.0f), normal, Math::Vector2(u, v) });
    }

    // Fan triangles from center to outline
    u32 outlineCount = static_cast<u32>(mesh.vertices.size()) - 1; // exclude center
    for (u32 i = 0; i < outlineCount; ++i) {
        u32 current = 1 + i;
        u32 next = 1 + ((i + 1) % outlineCount);
        mesh.indices.push_back(centerIdx);
        mesh.indices.push_back(current);
        mesh.indices.push_back(next);
    }

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

ECS::MeshComponent MeshFactory::CreateTerrain(const ECS::TerrainComponent& terrain) {
    ECS::MeshComponent mesh;

    u32 w = terrain.gridWidth;
    u32 h = terrain.gridHeight;
    f32 cellSize = terrain.cellSize;

    // Center the terrain
    f32 halfW = (w - 1) * cellSize * 0.5f;
    f32 halfH = (h - 1) * cellSize * 0.5f;

    // Generate vertices
    for (u32 z = 0; z < h; ++z) {
        for (u32 x = 0; x < w; ++x) {
            f32 height = terrain.GetHeight(x, z);

            Math::Vector3 pos(
                static_cast<f32>(x) * cellSize - halfW,
                height,
                static_cast<f32>(z) * cellSize - halfH
            );

            // Compute normal from neighbors
            f32 hL = (x > 0) ? terrain.GetHeight(x - 1, z) : height;
            f32 hR = (x < w - 1) ? terrain.GetHeight(x + 1, z) : height;
            f32 hD = (z > 0) ? terrain.GetHeight(x, z - 1) : height;
            f32 hU = (z < h - 1) ? terrain.GetHeight(x, z + 1) : height;
            Math::Vector3 normal(hL - hR, 2.0f * cellSize, hD - hU);
            f32 len = Math::Sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (len > 1e-6f) normal = normal * (1.0f / len);

            Math::Vector2 uv(
                static_cast<f32>(x) / static_cast<f32>(w - 1),
                static_cast<f32>(z) / static_cast<f32>(h - 1)
            );

            // Vertex color = splatmap weights (RGBA for 4 texture layers)
            Math::Vector4 color(1.0f, 1.0f, 1.0f, 1.0f);
            if (terrain.splatmap.size() >= static_cast<usize>(w) * h * 4) {
                usize idx = (static_cast<usize>(z) * w + x) * 4;
                color = Math::Vector4(
                    terrain.splatmap[idx + 0],
                    terrain.splatmap[idx + 1],
                    terrain.splatmap[idx + 2],
                    terrain.splatmap[idx + 3]
                );
            }

            ECS::MeshComponent::Vertex vert;
            vert.position = pos;
            vert.normal = normal;
            vert.uv = uv;
            vert.color = color;
            mesh.vertices.push_back(vert);
        }
    }

    // Generate indices
    for (u32 z = 0; z < h - 1; ++z) {
        for (u32 x = 0; x < w - 1; ++x) {
            u32 topLeft = z * w + x;
            u32 topRight = topLeft + 1;
            u32 bottomLeft = (z + 1) * w + x;
            u32 bottomRight = bottomLeft + 1;

            mesh.indices.push_back(topLeft);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(topRight);

            mesh.indices.push_back(topRight);
            mesh.indices.push_back(bottomLeft);
            mesh.indices.push_back(bottomRight);
        }
    }

    CalculateTangents(mesh);
    return mesh;
}

ECS::MeshComponent MeshFactory::CreateTerrain2D(const ECS::Terrain2DComponent& terrain) {
    ECS::MeshComponent mesh;

    if (terrain.controlPoints.size() < 2) return mesh;

    // For each pair of consecutive points, create a quad from surface down to depth
    for (usize i = 0; i + 1 < terrain.controlPoints.size(); ++i) {
        const auto& p0 = terrain.controlPoints[i];
        const auto& p1 = terrain.controlPoints[i + 1];

        f32 u0 = static_cast<f32>(i) * terrain.uvScale;
        f32 u1 = static_cast<f32>(i + 1) * terrain.uvScale;

        u32 baseIdx = static_cast<u32>(mesh.vertices.size());

        // Top-left (surface)
        mesh.vertices.push_back({
            Math::Vector3(p0.x, p0.y, 0.0f),
            Math::Vector3(0.0f, 0.0f, 1.0f),
            Math::Vector2(u0, 0.0f)
        });
        // Top-right (surface)
        mesh.vertices.push_back({
            Math::Vector3(p1.x, p1.y, 0.0f),
            Math::Vector3(0.0f, 0.0f, 1.0f),
            Math::Vector2(u1, 0.0f)
        });
        // Bottom-right (depth)
        mesh.vertices.push_back({
            Math::Vector3(p1.x, p1.y - terrain.depth, 0.0f),
            Math::Vector3(0.0f, 0.0f, 1.0f),
            Math::Vector2(u1, 1.0f)
        });
        // Bottom-left (depth)
        mesh.vertices.push_back({
            Math::Vector3(p0.x, p0.y - terrain.depth, 0.0f),
            Math::Vector3(0.0f, 0.0f, 1.0f),
            Math::Vector2(u0, 1.0f)
        });

        // Two triangles per quad
        mesh.indices.push_back(baseIdx + 0);
        mesh.indices.push_back(baseIdx + 3);
        mesh.indices.push_back(baseIdx + 1);

        mesh.indices.push_back(baseIdx + 1);
        mesh.indices.push_back(baseIdx + 3);
        mesh.indices.push_back(baseIdx + 2);
    }

    CalculateTangents(mesh);
    return mesh;
}

ECS::MeshComponent MeshFactory::CreateSpriteQuad(f32 width, f32 height,
    f32 pivotX, f32 pivotY,
    f32 uvLeft, f32 uvTop, f32 uvRight, f32 uvBottom,
    bool flipX, bool flipY) {
    ECS::MeshComponent mesh;

    // Offset so pivot point is at local origin
    f32 x0 = -pivotX * width;
    f32 y0 = -pivotY * height;
    f32 x1 = x0 + width;
    f32 y1 = y0 + height;

    // Apply flipping by swapping UV coordinates
    f32 u0 = flipX ? uvRight : uvLeft;
    f32 u1 = flipX ? uvLeft : uvRight;
    f32 v0 = flipY ? uvBottom : uvTop;
    f32 v1 = flipY ? uvTop : uvBottom;

    Math::Vector3 normal(0.0f, 0.0f, 1.0f);

    // 4 vertices on XY plane facing +Z
    mesh.vertices = {
        { Math::Vector3(x0, y0, 0), normal, Math::Vector2(u0, v0) },
        { Math::Vector3(x1, y0, 0), normal, Math::Vector2(u1, v0) },
        { Math::Vector3(x1, y1, 0), normal, Math::Vector2(u1, v1) },
        { Math::Vector3(x0, y1, 0), normal, Math::Vector2(u0, v1) }
    };

    mesh.indices = { 0, 1, 2, 0, 2, 3 };

    return mesh;
}

ECS::MeshComponent MeshFactory::CreateTilemapMesh(const ECS::TilemapComponent& tilemap) {
    ECS::MeshComponent mesh;

    if (tilemap.width == 0 || tilemap.height == 0 || tilemap.tiles.empty() || tilemap.tilesetColumns == 0) {
        return mesh;
    }

    // Reserve approximate space
    u32 tileCount = tilemap.width * tilemap.height;
    mesh.vertices.reserve(tileCount * 4);
    mesh.indices.reserve(tileCount * 6);

    // Compute tileset texture dimensions in tiles
    // We don't know the exact texture pixel size, so UVs are computed as
    // tile-index-based fractions: u = col_in_tileset / tilesetColumns
    // For rows, we estimate max rows from the tile data
    u32 maxTileIndex = 0;
    for (i32 t : tilemap.tiles) {
        if (t > static_cast<i32>(maxTileIndex)) maxTileIndex = static_cast<u32>(t);
    }
    u32 tilesetRows = (maxTileIndex / tilemap.tilesetColumns) + 1;

    f32 uTileSize = 1.0f / static_cast<f32>(tilemap.tilesetColumns);
    f32 vTileSize = 1.0f / static_cast<f32>(tilesetRows);

    Math::Vector3 normal(0.0f, 0.0f, 1.0f);

    for (u32 row = 0; row < tilemap.height; ++row) {
        for (u32 col = 0; col < tilemap.width; ++col) {
            i32 tileIndex = tilemap.tiles[row * tilemap.width + col];
            if (tileIndex < 0) continue; // Empty tile

            // World position of this tile
            f32 x = static_cast<f32>(col) * tilemap.worldTileWidth;
            f32 y = static_cast<f32>(row) * tilemap.worldTileHeight;

            // UV coordinates from tile index
            u32 tileCol = static_cast<u32>(tileIndex) % tilemap.tilesetColumns;
            u32 tileRow = static_cast<u32>(tileIndex) / tilemap.tilesetColumns;
            f32 u0 = static_cast<f32>(tileCol) * uTileSize;
            f32 v0 = static_cast<f32>(tileRow) * vTileSize;
            f32 u1 = u0 + uTileSize;
            f32 v1 = v0 + vTileSize;

            u32 baseIdx = static_cast<u32>(mesh.vertices.size());

            mesh.vertices.push_back({ Math::Vector3(x, y, 0), normal, Math::Vector2(u0, v0) });
            mesh.vertices.push_back({ Math::Vector3(x + tilemap.worldTileWidth, y, 0), normal, Math::Vector2(u1, v0) });
            mesh.vertices.push_back({ Math::Vector3(x + tilemap.worldTileWidth, y + tilemap.worldTileHeight, 0), normal, Math::Vector2(u1, v1) });
            mesh.vertices.push_back({ Math::Vector3(x, y + tilemap.worldTileHeight, 0), normal, Math::Vector2(u0, v1) });

            mesh.indices.push_back(baseIdx + 0);
            mesh.indices.push_back(baseIdx + 1);
            mesh.indices.push_back(baseIdx + 2);
            mesh.indices.push_back(baseIdx + 0);
            mesh.indices.push_back(baseIdx + 2);
            mesh.indices.push_back(baseIdx + 3);
        }
    }

    return mesh;
}

} // namespace Renderer
} // namespace Enjin
