#pragma once

#include "Enjin/Platform/Types.h"
#include "Enjin/Math/Vector.h"
#include <string>
#include <vector>

namespace Enjin::ECS {

// A single parallax layer — represents one scrolling background plane.
// Layers closer to the camera (lower distance) scroll faster,
// creating the illusion of depth in 2D scenes.
struct ParallaxLayer {
    std::string texturePath;                    // Sprite/texture for this layer
    f32 distance = 1.0f;                        // Depth from camera (higher = farther, slower scroll)
    f32 speedMultiplier = 1.0f;                 // Additional speed control (1.0 = normal parallax)
    Math::Vector2 offset = Math::Vector2(0.0f); // Spatial offset in world units
    Math::Vector2 scale = Math::Vector2(1.0f);  // Layer scale (width, height in world units)
    Math::Vector3 tint = Math::Vector3(1.0f);   // Color tint (1,1,1 = no tint)
    f32 alpha = 1.0f;                           // Transparency (1.0 = opaque)
    bool repeatX = true;                        // Tile horizontally
    bool repeatY = false;                       // Tile vertically
    bool visible = true;                        // Show/hide this layer
    i32 sortOrder = 0;                          // Draw order within the parallax stack (lower = behind)
};

// ParallaxMachine — add to any entity to create a multi-layer parallax background.
// Each layer scrolls at a speed inversely proportional to its distance,
// creating depth illusion without actual 3D geometry.
//
// Usage:
//   auto& pm = world->AddComponent<ParallaxMachineComponent>(entity);
//   pm.layers.push_back({ "sky.png", 10.0f, 1.0f, {0,0}, {20,10} });
//   pm.layers.push_back({ "mountains.png", 5.0f, 1.0f, {0,-2}, {20,8} });
//   pm.layers.push_back({ "trees.png", 2.0f, 1.0f, {0,-3}, {20,6} });
//   pm.layers.push_back({ "ground.png", 1.0f, 1.0f, {0,-4}, {20,4} });
//
// The system uses the active camera's X position to compute scroll offset:
//   scrollX = cameraX * (1.0 / layer.distance) * layer.speedMultiplier
//
struct ParallaxMachineComponent {
    std::vector<ParallaxLayer> layers;

    // Reference point for parallax calculation (usually camera position at scene start)
    Math::Vector2 origin = Math::Vector2(0.0f);

    // Global speed multiplier applied to all layers
    f32 globalSpeed = 1.0f;

    // Auto-scroll: constant velocity independent of camera movement (for title screens, etc.)
    Math::Vector2 autoScrollSpeed = Math::Vector2(0.0f);

    // Accumulated auto-scroll offset (runtime state)
    Math::Vector2 autoScrollOffset = Math::Vector2(0.0f);

    bool enabled = true;
};

} // namespace Enjin::ECS
