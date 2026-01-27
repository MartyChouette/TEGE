#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Components/Mesh.h"
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

    // Create from glTF primitive data
    static ECS::MeshComponent CreateFromGLTF(const Assets::GLTFPrimitive& primitive);

    // Create a simple quad (for UI, sprites, etc.)
    static ECS::MeshComponent CreateQuad(f32 width = 1.0f, f32 height = 1.0f);

    // Create coordinate axes visualization
    static ECS::MeshComponent CreateAxes(f32 length = 1.0f);

    // Create grid for ground plane
    static ECS::MeshComponent CreateGrid(f32 size = 10.0f, u32 divisions = 10);

private:
    static void CalculateTangents(ECS::MeshComponent& mesh);
};

} // namespace Renderer
} // namespace Enjin
