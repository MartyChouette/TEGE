#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/Assets/GLTFLoader.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace Renderer {

// Factory for creating mesh primitives and loading meshes
class ENJIN_API MeshFactory {
public:
    // Create primitive meshes
    static ECS::MeshComponent CreateCube(f32 size = 1.0f);
    static ECS::MeshComponent CreatePlane(f32 width = 1.0f, f32 height = 1.0f, u32 subdivisionsX = 1, u32 subdivisionsZ = 1);
    static ECS::MeshComponent CreateSphere(f32 radius = 0.5f, u32 segments = 32, u32 rings = 16);
    static ECS::MeshComponent CreateCylinder(f32 radius = 0.5f, f32 height = 1.0f, u32 segments = 32);
    static ECS::MeshComponent CreateCone(f32 radius = 0.5f, f32 height = 1.0f, u32 segments = 32);
    static ECS::MeshComponent CreateCapsule(f32 radius = 0.5f, f32 height = 1.0f, u32 segments = 16, u32 rings = 8);
    static ECS::MeshComponent CreateTriangle(f32 size = 1.0f);
    static ECS::MeshComponent CreatePyramid(f32 base = 1.0f, f32 height = 1.0f);

    // Create from glTF primitive data
    static ECS::MeshComponent CreateFromGLTF(const Assets::GLTFPrimitive& primitive);

    // Create a simple quad (for UI, sprites, etc.)
    static ECS::MeshComponent CreateQuad(f32 width = 1.0f, f32 height = 1.0f);

    // Create a 2D capsule (rectangle body with semicircle caps, XY plane facing +Z)
    static ECS::MeshComponent CreateCapsule2D(f32 width = 1.0f, f32 height = 2.0f, u32 capSegments = 12);

    // Create coordinate axes visualization
    static ECS::MeshComponent CreateAxes(f32 length = 1.0f);

    // Create grid for ground plane
    static ECS::MeshComponent CreateGrid(f32 size = 10.0f, u32 divisions = 10);

    // Create 3D terrain mesh from heightmap data
    static ECS::MeshComponent CreateTerrain(const ECS::TerrainComponent& terrain);

    // Create 2D terrain mesh from polyline control points
    static ECS::MeshComponent CreateTerrain2D(const ECS::Terrain2DComponent& terrain);

private:
    static void CalculateTangents(ECS::MeshComponent& mesh);
};

} // namespace Renderer
} // namespace Enjin
