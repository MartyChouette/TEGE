#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include <vector>

namespace Enjin {
namespace Editor {

// Generates collider shapes from sprite alpha data
class ENJIN_API SpriteColliderGenerator {
public:
    // Fit a box collider to the non-transparent region of a sprite
    static ECS::BoxColliderComponent FitBoxCollider(
        const u8* pixels, u32 w, u32 h,
        const Math::Vector2& spriteSize,
        const Math::Vector2& pivot = Math::Vector2(0.5f, 0.5f),
        u8 alphaThreshold = 10);

    // Fit a capsule collider to the non-transparent region
    static ECS::CapsuleColliderComponent FitCapsuleCollider(
        const u8* pixels, u32 w, u32 h,
        const Math::Vector2& spriteSize,
        const Math::Vector2& pivot = Math::Vector2(0.5f, 0.5f),
        u8 alphaThreshold = 10);

    // Fit a sphere collider (enclosing circle) to the non-transparent region
    static ECS::SphereColliderComponent FitSphereCollider(
        const u8* pixels, u32 w, u32 h,
        const Math::Vector2& spriteSize,
        const Math::Vector2& pivot = Math::Vector2(0.5f, 0.5f),
        u8 alphaThreshold = 10);

    // Fit a polygon collider from sprite alpha contour
    static ECS::PolygonCollider2DComponent FitPolygonCollider(
        const u8* pixels, u32 w, u32 h,
        const Math::Vector2& spriteSize,
        const Math::Vector2& pivot = Math::Vector2(0.5f, 0.5f),
        f32 tolerance = 1.0f, u8 alphaThreshold = 10);

private:
    // Find the bounding rect of non-transparent pixels (in pixel coords)
    struct PixelBounds { u32 minX, minY, maxX, maxY; bool valid; };
    static PixelBounds FindOpaqueBounds(const u8* pixels, u32 w, u32 h, u8 alphaThreshold);
};

} // namespace Editor
} // namespace Enjin
