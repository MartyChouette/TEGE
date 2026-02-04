#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Math/Quaternion.h"
#include "Enjin/ECS/Component.h"

namespace Enjin {
namespace ECS {

// Transform component - position, rotation, scale
struct ENJIN_API TransformComponent : public IComponent {
    Math::Vector3 position = Math::Vector3(0.0f);
    Math::Quaternion rotation = Math::Quaternion::Identity();
    Math::Vector3 scale = Math::Vector3(1.0f);
    bool visible = true;  // false = skip rendering (entity still exists, physics still runs)

    Math::Matrix4 ToMatrix() const {
        Math::Matrix4 translation = Math::Matrix4::Translation(position);
        Math::Matrix4 rotationMat = rotation.ToMatrix();
        Math::Matrix4 scaleMat = Math::Matrix4::Scale(scale);
        return translation * rotationMat * scaleMat;
    }
};

} // namespace ECS
} // namespace Enjin
