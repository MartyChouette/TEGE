#include "Enjin/Editor/SpriteColliderGenerator.h"
#include "Enjin/Editor/SpriteContourTracer.h"
#include <algorithm>
#include <cmath>

namespace Enjin {
namespace Editor {

SpriteColliderGenerator::PixelBounds SpriteColliderGenerator::FindOpaqueBounds(
    const u8* pixels, u32 w, u32 h, u8 alphaThreshold) {

    PixelBounds bounds;
    bounds.valid = false;
    bounds.minX = w; bounds.minY = h;
    bounds.maxX = 0; bounds.maxY = 0;

    for (u32 y = 0; y < h; y++) {
        for (u32 x = 0; x < w; x++) {
            u8 alpha = pixels[(y * w + x) * 4 + 3];
            if (alpha > alphaThreshold) {
                bounds.minX = std::min(bounds.minX, x);
                bounds.maxX = std::max(bounds.maxX, x);
                bounds.minY = std::min(bounds.minY, y);
                bounds.maxY = std::max(bounds.maxY, y);
                bounds.valid = true;
            }
        }
    }

    return bounds;
}

ECS::BoxColliderComponent SpriteColliderGenerator::FitBoxCollider(
    const u8* pixels, u32 w, u32 h,
    const Math::Vector2& spriteSize,
    const Math::Vector2& pivot, u8 alphaThreshold) {

    ECS::BoxColliderComponent box;
    box.size = Math::Vector3(spriteSize.x, spriteSize.y, 0.1f);
    box.center = Math::Vector3(0, 0, 0);

    if (!pixels || w == 0 || h == 0) return box;

    auto bounds = FindOpaqueBounds(pixels, w, h, alphaThreshold);
    if (!bounds.valid) return box;

    // Convert pixel bounds to normalized [0,1] range
    f32 normMinX = static_cast<f32>(bounds.minX) / static_cast<f32>(w);
    f32 normMaxX = static_cast<f32>(bounds.maxX + 1) / static_cast<f32>(w);
    f32 normMinY = static_cast<f32>(bounds.minY) / static_cast<f32>(h);
    f32 normMaxY = static_cast<f32>(bounds.maxY + 1) / static_cast<f32>(h);

    // Convert to world-space relative to pivot
    f32 worldMinX = (normMinX - pivot.x) * spriteSize.x;
    f32 worldMaxX = (normMaxX - pivot.x) * spriteSize.x;
    f32 worldMinY = (pivot.y - normMaxY) * spriteSize.y; // Y flipped (pixel Y down, world Y up)
    f32 worldMaxY = (pivot.y - normMinY) * spriteSize.y;

    box.center = Math::Vector3(
        (worldMinX + worldMaxX) * 0.5f,
        (worldMinY + worldMaxY) * 0.5f,
        0.0f
    );
    box.size = Math::Vector3(
        worldMaxX - worldMinX,
        worldMaxY - worldMinY,
        0.1f
    );

    return box;
}

ECS::CapsuleColliderComponent SpriteColliderGenerator::FitCapsuleCollider(
    const u8* pixels, u32 w, u32 h,
    const Math::Vector2& spriteSize,
    const Math::Vector2& pivot, u8 alphaThreshold) {

    ECS::CapsuleColliderComponent capsule;
    capsule.radius = spriteSize.x * 0.5f;
    capsule.height = spriteSize.y;

    if (!pixels || w == 0 || h == 0) return capsule;

    auto bounds = FindOpaqueBounds(pixels, w, h, alphaThreshold);
    if (!bounds.valid) return capsule;

    f32 regionW = static_cast<f32>(bounds.maxX - bounds.minX + 1) / static_cast<f32>(w) * spriteSize.x;
    f32 regionH = static_cast<f32>(bounds.maxY - bounds.minY + 1) / static_cast<f32>(h) * spriteSize.y;

    capsule.radius = regionW * 0.5f;
    capsule.height = regionH;

    f32 centerNormX = (static_cast<f32>(bounds.minX + bounds.maxX + 1) * 0.5f) / static_cast<f32>(w);
    f32 centerNormY = (static_cast<f32>(bounds.minY + bounds.maxY + 1) * 0.5f) / static_cast<f32>(h);

    capsule.center = Math::Vector3(
        (centerNormX - pivot.x) * spriteSize.x,
        (pivot.y - centerNormY) * spriteSize.y,
        0.0f
    );

    return capsule;
}

ECS::SphereColliderComponent SpriteColliderGenerator::FitSphereCollider(
    const u8* pixels, u32 w, u32 h,
    const Math::Vector2& spriteSize,
    const Math::Vector2& pivot, u8 alphaThreshold) {

    ECS::SphereColliderComponent sphere;
    sphere.radius = std::max(spriteSize.x, spriteSize.y) * 0.5f;

    if (!pixels || w == 0 || h == 0) return sphere;

    auto bounds = FindOpaqueBounds(pixels, w, h, alphaThreshold);
    if (!bounds.valid) return sphere;

    f32 regionW = static_cast<f32>(bounds.maxX - bounds.minX + 1) / static_cast<f32>(w) * spriteSize.x;
    f32 regionH = static_cast<f32>(bounds.maxY - bounds.minY + 1) / static_cast<f32>(h) * spriteSize.y;

    // Enclosing circle radius
    sphere.radius = std::sqrt(regionW * regionW + regionH * regionH) * 0.5f;

    f32 centerNormX = (static_cast<f32>(bounds.minX + bounds.maxX + 1) * 0.5f) / static_cast<f32>(w);
    f32 centerNormY = (static_cast<f32>(bounds.minY + bounds.maxY + 1) * 0.5f) / static_cast<f32>(h);

    sphere.center = Math::Vector3(
        (centerNormX - pivot.x) * spriteSize.x,
        (pivot.y - centerNormY) * spriteSize.y,
        0.0f
    );

    return sphere;
}

ECS::PolygonCollider2DComponent SpriteColliderGenerator::FitPolygonCollider(
    const u8* pixels, u32 w, u32 h,
    const Math::Vector2& spriteSize,
    const Math::Vector2& pivot, f32 tolerance, u8 alphaThreshold) {

    ECS::PolygonCollider2DComponent poly;
    if (!pixels || w == 0 || h == 0) return poly;

    // Trace contour in pixel space
    auto contour = SpriteContourTracer::TraceContour(pixels, w, h, alphaThreshold);
    if (contour.size() < 3) return poly;

    // Simplify
    auto simplified = SpriteContourTracer::Simplify(contour, tolerance);
    if (simplified.size() < 3) simplified = contour;

    // Convert from pixel coordinates to world-space relative to pivot
    poly.vertices.reserve(simplified.size());
    for (const auto& pt : simplified) {
        f32 normX = pt.x / static_cast<f32>(w);
        f32 normY = pt.y / static_cast<f32>(h);
        f32 worldX = (normX - pivot.x) * spriteSize.x;
        f32 worldY = (pivot.y - normY) * spriteSize.y; // Y flipped
        poly.vertices.push_back(Math::Vector2(worldX, worldY));
    }

    return poly;
}

} // namespace Editor
} // namespace Enjin
