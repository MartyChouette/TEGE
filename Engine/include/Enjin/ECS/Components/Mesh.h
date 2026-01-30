#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Component.h"
#include "Enjin/Math/Vector.h"
#include <vector>

namespace Enjin {
namespace ECS {

// Mesh component - stores vertex data
struct ENJIN_API MeshComponent : public IComponent {
    struct Vertex {
        Math::Vector3 position;
        Math::Vector3 normal;
        Math::Vector2 uv;
        Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f); // vertex color (default white)
    };

    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    bool IsValid() const {
        return !vertices.empty() && !indices.empty();
    }
};

} // namespace ECS
} // namespace Enjin
