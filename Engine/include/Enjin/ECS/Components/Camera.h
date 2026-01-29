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
    Math::Vector3 backgroundColor = Math::Vector3(0.1f, 0.1f, 0.15f);

    // Viewport (normalized 0-1)
    f32 viewportX = 0.0f;
    f32 viewportY = 0.0f;
    f32 viewportWidth = 1.0f;
    f32 viewportHeight = 1.0f;

    // Rendering options
    u32 cullingMask = 0xFFFFFFFF;  // Layers to render

    // === Weather Effects (per-camera) ===
    bool weatherEnabled = false;
    u32 weatherType = 0;          // 0=Clear, 1=Cloudy, 2=Rain, 3=HeavyRain, 4=Snow, 5=Fog, 6=Storm
    f32 rainIntensity = 0.7f;     // 0-1
    f32 snowIntensity = 0.7f;     // 0-1
    f32 fogDensity = 0.0f;        // 0-1
    Math::Vector3 fogColor = Math::Vector3(0.5f, 0.5f, 0.6f);
    f32 fogStart = 20.0f;
    f32 fogEnd = 100.0f;
    bool lightningEnabled = true; // Can disable lightning flash in storms

    // === Water Effects (per-camera) ===
    bool waterEnabled = false;
    f32 waterLevel = 0.0f;        // Y position of water plane
    Math::Vector3 waterColor = Math::Vector3(0.1f, 0.3f, 0.5f);
    f32 waterOpacity = 0.7f;
    f32 waveSpeed = 1.0f;
    f32 waveHeight = 0.2f;

    // Helper to get aspect ratio from viewport
    f32 GetAspectRatio(u32 screenWidth, u32 screenHeight) const {
        f32 width = viewportWidth * static_cast<f32>(screenWidth);
        f32 height = viewportHeight * static_cast<f32>(screenHeight);
        return (height > 0.0f) ? (width / height) : 1.0f;
    }
};

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
        for (auto entity : world->GetAllEntities()) {
            if (world->template HasComponent<CameraComponent>(entity)) {
                auto* cam = world->template GetComponent<CameraComponent>(entity);
                if (cam && cam->isActive && cam->priority > highestPriority) {
                    highestPriority = cam->priority;
                    bestCamera = entity;
                }
            }
        }

        return bestCamera;
    }

    // Get all active cameras sorted by priority (highest first)
    template<typename WorldType>
    static std::vector<u64> GetAllActiveCameras(WorldType* world) {
        std::vector<std::pair<i32, u64>> cameras;

        for (auto entity : world->GetAllEntities()) {
            if (world->template HasComponent<CameraComponent>(entity)) {
                auto* cam = world->template GetComponent<CameraComponent>(entity);
                if (cam && cam->isActive) {
                    cameras.push_back({cam->priority, entity});
                }
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
