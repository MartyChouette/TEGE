#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <cmath>
#include <vector>

namespace Enjin {
namespace ECS {

enum class FluidType : u32 {
    Water = 0,
    Lava = 1,
    Gas = 2,
    Smoke = 3,
    Steam = 4
};

enum class FluidDimension : u32 {
    Mode2D = 0,
    Mode3D = 1
};

struct ENJIN_API FluidVolumeComponent {
    // Volume extents
    Math::Vector3 halfExtents = Math::Vector3(5.0f, 5.0f, 5.0f);

    // Type and dimension
    FluidType fluidType = FluidType::Water;
    FluidDimension dimension = FluidDimension::Mode2D;
    u32 gridSize = 64;  // 2D max 128, 3D max 48

    // Simulation parameters
    f32 viscosity = 0.0001f;
    f32 diffusion = 0.0001f;
    f32 dissipation = 0.995f;
    f32 velocityDissipation = 0.999f;
    i32 solverIterations = 20;
    f32 buoyancy = 0.0f;

    // Rendering
    Math::Vector3 fluidColor = Math::Vector3(0.2f, 0.4f, 0.8f);
    f32 opacity = 0.7f;
    f32 densityThreshold = 0.01f;
    bool renderEnabled = true;

    // Source injection
    f32 sourceRadius = 2.0f;
    f32 sourceDensity = 100.0f;
    f32 sourceVelocityScale = 50.0f;

    // State
    bool isActive = true;
    bool simulationInitialized = false;
    i32 priority = 0;

    void ApplyPreset() {
        switch (fluidType) {
            case FluidType::Water:
                viscosity = 0.0001f; diffusion = 0.0001f; dissipation = 0.999f;
                velocityDissipation = 0.999f; buoyancy = 0.0f;
                fluidColor = Math::Vector3(0.2f, 0.4f, 0.8f); opacity = 0.7f;
                break;
            case FluidType::Lava:
                viscosity = 0.5f; diffusion = 0.001f; dissipation = 0.998f;
                velocityDissipation = 0.995f; buoyancy = 0.0f;
                fluidColor = Math::Vector3(0.9f, 0.3f, 0.05f); opacity = 1.0f;
                break;
            case FluidType::Gas:
                viscosity = 0.00001f; diffusion = 0.0f; dissipation = 0.99f;
                velocityDissipation = 0.995f; buoyancy = 1.0f;
                fluidColor = Math::Vector3(0.5f, 0.5f, 0.5f); opacity = 0.3f;
                break;
            case FluidType::Smoke:
                viscosity = 0.00001f; diffusion = 0.0f; dissipation = 0.985f;
                velocityDissipation = 0.99f; buoyancy = 0.8f;
                fluidColor = Math::Vector3(0.3f, 0.3f, 0.3f); opacity = 0.4f;
                break;
            case FluidType::Steam:
                viscosity = 0.00001f; diffusion = 0.001f; dissipation = 0.97f;
                velocityDissipation = 0.98f; buoyancy = 2.0f;
                fluidColor = Math::Vector3(0.9f, 0.9f, 0.95f); opacity = 0.25f;
                break;
        }
    }

    bool ContainsPoint(const Math::Vector3& center, const Math::Vector3& point) const {
        return std::abs(point.x - center.x) <= halfExtents.x &&
               std::abs(point.y - center.y) <= halfExtents.y &&
               std::abs(point.z - center.z) <= halfExtents.z;
    }
};

} // namespace ECS
} // namespace Enjin
