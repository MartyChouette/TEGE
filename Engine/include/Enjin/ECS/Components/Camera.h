#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Quaternion.h"
#include <vector>
#include <algorithm>
#include <climits>

namespace Enjin {
namespace ECS {

// Projection type for cameras
enum class ProjectionType : u32 {
    Perspective = 0,
    Orthographic = 1
};

// Common camera presets for quick setup
enum class CameraPreset : u32 {
    Custom = 0,
    Isometric45,
    Isometric30,
    TopDown,
    SideScroller,
    FirstPerson,
    ThirdPerson,
    CinematicWide,
    SecurityCam,
    BirdsEye,
    Count
};

inline const char* CameraPresetName(CameraPreset p) {
    switch (p) {
        case CameraPreset::Custom:         return "Custom";
        case CameraPreset::Isometric45:    return "Isometric 45";
        case CameraPreset::Isometric30:    return "Isometric 30";
        case CameraPreset::TopDown:        return "Top-Down";
        case CameraPreset::SideScroller:   return "Side Scroller";
        case CameraPreset::FirstPerson:    return "First Person";
        case CameraPreset::ThirdPerson:    return "Third Person";
        case CameraPreset::CinematicWide:  return "Cinematic Wide";
        case CameraPreset::SecurityCam:    return "Security Cam";
        case CameraPreset::BirdsEye:       return "Bird's Eye";
        default:                           return "Unknown";
    }
}

// Camera component - attach to entities to create game cameras
// The editor has its own camera; these are for in-game use
struct ENJIN_API CameraComponent {
    // Projection type
    ProjectionType projectionType = ProjectionType::Perspective;

    // Perspective settings
    f32 fieldOfView = 60.0f;      // Degrees (vertical FOV)
    f32 nearPlane = 0.1f;
    f32 farPlane = 1000.0f;

    // Orthographic settings
    f32 orthoSize = 10.0f;        // Half-height of the orthographic view

    // Virtual camera system
    i32 priority = 0;             // Higher priority cameras take precedence
    bool isActive = true;         // Whether this camera is eligible for activation

    // Clear settings
    bool clearDepth = true;
    bool clearColor = true;
    Math::Vector3 backgroundColor = Math::Vector3(0.68f, 0.75f, 0.72f); // Soft teal/sage

    // Viewport (normalized 0-1)
    f32 viewportX = 0.0f;
    f32 viewportY = 0.0f;
    f32 viewportWidth = 1.0f;
    f32 viewportHeight = 1.0f;

    // Rendering options
    u32 cullingMask = 0xFFFFFFFF;  // Layers to render

    // Helper to get aspect ratio from viewport
    f32 GetAspectRatio(u32 screenWidth, u32 screenHeight) const {
        f32 width = viewportWidth * static_cast<f32>(screenWidth);
        f32 height = viewportHeight * static_cast<f32>(screenHeight);
        return (height > 0.0f) ? (width / height) : 1.0f;
    }
};

// Preset result: camera settings + recommended Euler rotation for the entity transform
struct CameraPresetResult {
    CameraComponent camera;
    Math::Vector3 rotation; // Euler angles (degrees)
};

// Apply a camera preset — returns configured camera + recommended rotation
inline CameraPresetResult ApplyCameraPreset(CameraPreset preset) {
    CameraPresetResult r;
    r.rotation = Math::Vector3(0.0f, 0.0f, 0.0f);

    switch (preset) {
        case CameraPreset::Isometric45:
            r.camera.projectionType = ProjectionType::Orthographic;
            r.camera.orthoSize = 10.0f;
            r.camera.nearPlane = 0.1f;
            r.camera.farPlane = 500.0f;
            r.rotation = Math::Vector3(-45.0f, 45.0f, 0.0f);
            break;
        case CameraPreset::Isometric30:
            r.camera.projectionType = ProjectionType::Orthographic;
            r.camera.orthoSize = 12.0f;
            r.camera.nearPlane = 0.1f;
            r.camera.farPlane = 500.0f;
            r.rotation = Math::Vector3(-30.0f, 45.0f, 0.0f);
            break;
        case CameraPreset::TopDown:
            r.camera.projectionType = ProjectionType::Orthographic;
            r.camera.orthoSize = 15.0f;
            r.camera.nearPlane = 0.1f;
            r.camera.farPlane = 500.0f;
            r.rotation = Math::Vector3(-90.0f, 0.0f, 0.0f);
            break;
        case CameraPreset::SideScroller:
            r.camera.projectionType = ProjectionType::Orthographic;
            r.camera.orthoSize = 8.0f;
            r.camera.nearPlane = 0.1f;
            r.camera.farPlane = 100.0f;
            r.rotation = Math::Vector3(0.0f, 0.0f, 0.0f);
            break;
        case CameraPreset::FirstPerson:
            r.camera.projectionType = ProjectionType::Perspective;
            r.camera.fieldOfView = 75.0f;
            r.camera.nearPlane = 0.1f;
            r.camera.farPlane = 1000.0f;
            break;
        case CameraPreset::ThirdPerson:
            r.camera.projectionType = ProjectionType::Perspective;
            r.camera.fieldOfView = 60.0f;
            r.camera.nearPlane = 0.3f;
            r.camera.farPlane = 1000.0f;
            r.rotation = Math::Vector3(-15.0f, 0.0f, 0.0f);
            break;
        case CameraPreset::CinematicWide:
            r.camera.projectionType = ProjectionType::Perspective;
            r.camera.fieldOfView = 40.0f;
            r.camera.nearPlane = 0.5f;
            r.camera.farPlane = 2000.0f;
            break;
        case CameraPreset::SecurityCam:
            r.camera.projectionType = ProjectionType::Perspective;
            r.camera.fieldOfView = 90.0f;
            r.camera.nearPlane = 0.1f;
            r.camera.farPlane = 50.0f;
            r.rotation = Math::Vector3(-30.0f, 0.0f, 0.0f);
            break;
        case CameraPreset::BirdsEye:
            r.camera.projectionType = ProjectionType::Perspective;
            r.camera.fieldOfView = 50.0f;
            r.camera.nearPlane = 1.0f;
            r.camera.farPlane = 5000.0f;
            r.rotation = Math::Vector3(-70.0f, 0.0f, 0.0f);
            break;
        default:
            break;
    }
    return r;
}

// Virtual camera manager helper - finds the highest priority active camera
// This is a utility struct, not a component
struct CameraManager {
    // Find the active camera with highest priority
    // Returns INVALID_ENTITY if no cameras are found
    // Usage: auto cameraEntity = CameraManager::GetActiveCamera(world);
    template<typename WorldType>
    static u64 GetActiveCamera(WorldType* world) {
        u64 bestCamera = 0; // INVALID_ENTITY
        i32 highestPriority = INT_MIN;

        // Iterate through all camera components
        // (requires World to expose camera component iteration)
        for (auto entity : world->template GetEntitiesWithComponent<CameraComponent>()) {
            auto* cam = world->template GetComponent<CameraComponent>(entity);
            if (cam && cam->isActive && cam->priority > highestPriority) {
                highestPriority = cam->priority;
                bestCamera = entity;
            }
        }

        return bestCamera;
    }

    // Get all active cameras sorted by priority (highest first)
    template<typename WorldType>
    static std::vector<u64> GetAllActiveCameras(WorldType* world) {
        std::vector<std::pair<i32, u64>> cameras;

        for (auto entity : world->template GetEntitiesWithComponent<CameraComponent>()) {
            auto* cam = world->template GetComponent<CameraComponent>(entity);
            if (cam && cam->isActive) {
                cameras.push_back({cam->priority, entity});
            }
        }

        // Sort by priority (highest first)
        std::sort(cameras.begin(), cameras.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

        std::vector<u64> result;
        result.reserve(cameras.size());
        for (const auto& pair : cameras) {
            result.push_back(pair.second);
        }
        return result;
    }
};

} // namespace ECS
} // namespace Enjin
