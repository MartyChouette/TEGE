#include "Enjin/Effects/ScreenDistortion.h"
#include <cmath>
#include <algorithm>

namespace Enjin {
namespace Effects {

// ---------------------------------------------------------------------------
// Update — advance timers, expand shockwaves, expire finished effects
// ---------------------------------------------------------------------------
void ScreenDistortionSystem::Update(ECS::World* world, f32 deltaTime) {
    if (!world || !m_Config.enabled) return;

    m_Time += deltaTime;
    m_ActiveSourceCount = 0;

    // Collect entities to destroy after iteration (shockwave expiry)
    std::vector<ECS::Entity> toDestroy;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<DistortionSourceComponent>()) {
        auto* src = world->GetComponent<DistortionSourceComponent>(entity);
        if (!src || src->type == DistortionType::None) continue;

        m_ActiveSourceCount++;

        // Advance phase for continuous animation
        src->phase += deltaTime * src->speed;

        // Shockwave expansion
        if (src->type == DistortionType::Shockwave) {
            src->shockwaveLife += deltaTime;
            src->shockwaveRadius += src->shockwaveSpeed * deltaTime;

            // Fade strength as the shockwave reaches end of life
            if (src->shockwaveLife >= src->shockwaveMaxLife) {
                toDestroy.push_back(entity);
            }
        }
    }

    // Destroy expired shockwave entities
    for (ECS::Entity e : toDestroy) {
        world->DestroyEntity(e);
        if (m_ActiveSourceCount > 0) m_ActiveSourceCount--;
    }
}

// ---------------------------------------------------------------------------
// ProjectToScreen — world position to normalized [0,1] screen coordinates
// ---------------------------------------------------------------------------
bool ScreenDistortionSystem::ProjectToScreen(const Math::Vector3& worldPos,
                                              const DistortionCameraInfo& camera,
                                              Math::Vector2& screenPos) {
    Math::Vector4 clip = camera.viewProjection * Math::Vector4(worldPos, 1.0f);

    // Behind camera
    if (clip.w <= 0.0f) return false;

    f32 invW = 1.0f / clip.w;
    // NDC to [0,1]
    screenPos.x = (clip.x * invW * 0.5f) + 0.5f;
    screenPos.y = (clip.y * invW * 0.5f) + 0.5f;

    return true;
}

// ---------------------------------------------------------------------------
// ComputeDistortionOffset — per-pixel UV offset for a given distortion type
// ---------------------------------------------------------------------------
Math::Vector2 ScreenDistortionSystem::ComputeDistortionOffset(
    DistortionType type,
    const Math::Vector2& pixelUV,
    const Math::Vector2& sourceScreenPos,
    f32 normalizedDist,   // 0..1 relative to screen-space radius
    f32 worldDist,        // actual world distance
    const DistortionSourceComponent& source,
    f32 time)
{
    Math::Vector2 offset(0.0f, 0.0f);

    // Common falloff factor: 1 at center, 0 at radius edge
    f32 falloffFactor = 1.0f - normalizedDist;
    if (source.falloff != 1.0f) {
        falloffFactor = Math::Pow(Math::Max(falloffFactor, 0.0f), source.falloff);
    }

    f32 str = source.strength;

    switch (type) {
        case DistortionType::HeatHaze: {
            // Rising wavy distortion — primarily vertical displacement
            f32 wave = Math::Sin(pixelUV.y * source.frequency * 20.0f + time * source.speed * 3.0f);
            f32 wave2 = Math::Sin(pixelUV.y * source.frequency * 13.7f + time * source.speed * 2.3f + 1.7f);
            offset.x = str * (wave * 0.3f + wave2 * 0.15f) * falloffFactor;
            offset.y = str * Math::Sin(pixelUV.x * source.frequency * 7.0f + time * source.speed * 1.5f)
                        * 0.5f * falloffFactor;
            break;
        }

        case DistortionType::Shockwave: {
            // Expanding ring distortion
            // Distance from source screen pos in UV space
            f32 dx = pixelUV.x - sourceScreenPos.x;
            f32 dy = pixelUV.y - sourceScreenPos.y;
            f32 screenDist = Math::Sqrt(dx * dx + dy * dy);

            // Ring profile: gaussian bump at shockwaveRadius
            f32 ringNorm = source.shockwaveRadius / Math::Max(source.radius, 0.01f);
            f32 diff = screenDist - ringNorm;
            f32 halfWidth = source.shockwaveWidth * 0.05f; // Scale width to UV space
            if (halfWidth < 0.001f) halfWidth = 0.001f;
            f32 ringProfile = Math::Exp(-(diff * diff) / (2.0f * halfWidth * halfWidth));

            // Fade out over shockwave lifetime
            f32 lifeFade = 1.0f - Math::Clamp(source.shockwaveLife / Math::Max(source.shockwaveMaxLife, 0.01f), 0.0f, 1.0f);
            lifeFade = lifeFade * lifeFade; // Quadratic fade

            // Radial displacement
            if (screenDist > 0.0001f) {
                f32 invDist = 1.0f / screenDist;
                offset.x = dx * invDist * str * ringProfile * lifeFade;
                offset.y = dy * invDist * str * ringProfile * lifeFade;
            }
            break;
        }

        case DistortionType::Underwater: {
            // Wavy full-screen caustic-like distortion
            f32 t = time * source.speed;
            offset.x = str * Math::Sin(pixelUV.x * source.frequency * 10.0f + t * 1.1f)
                      * Math::Cos(pixelUV.y * source.frequency * 7.3f + t * 0.9f) * falloffFactor;
            offset.y = str * Math::Cos(pixelUV.y * source.frequency * 10.0f * 0.7f + t * 1.3f)
                      * Math::Sin(pixelUV.x * source.frequency * 5.1f + t * 0.7f) * falloffFactor;
            break;
        }

        case DistortionType::PortalEdge: {
            // Radial distortion near portal center, intensity ~ 1/dist^2
            f32 dx = pixelUV.x - sourceScreenPos.x;
            f32 dy = pixelUV.y - sourceScreenPos.y;
            f32 screenDist = Math::Sqrt(dx * dx + dy * dy);
            if (screenDist > 0.001f) {
                f32 invDistSq = 1.0f / (1.0f + screenDist * screenDist * 100.0f);
                f32 wave = Math::Sin(screenDist * source.frequency * 50.0f - time * source.speed * 5.0f);
                offset.x = dx * str * invDistSq * wave * falloffFactor;
                offset.y = dy * str * invDistSq * wave * falloffFactor;
            }
            break;
        }

        case DistortionType::Ripple: {
            // Concentric ripples from source point
            f32 dx = pixelUV.x - sourceScreenPos.x;
            f32 dy = pixelUV.y - sourceScreenPos.y;
            f32 screenDist = Math::Sqrt(dx * dx + dy * dy);

            f32 wave = Math::Sin(screenDist * source.frequency * 60.0f - time * source.speed * 8.0f);
            f32 atten = 1.0f / (1.0f + screenDist * source.falloff * 20.0f);

            if (screenDist > 0.0001f) {
                f32 invDist = 1.0f / screenDist;
                offset.x = dx * invDist * str * wave * atten;
                offset.y = dy * invDist * str * wave * atten;
            }
            break;
        }

        case DistortionType::BarrelFisheye: {
            // Barrel/fisheye lens distortion: offset = (pos - center) * k * |pos - center|^2
            f32 dx = pixelUV.x - sourceScreenPos.x;
            f32 dy = pixelUV.y - sourceScreenPos.y;
            f32 r2 = dx * dx + dy * dy;
            f32 k = str * 10.0f; // Scale coefficient for visible effect
            offset.x = dx * k * r2 * falloffFactor;
            offset.y = dy * k * r2 * falloffFactor;
            break;
        }

        case DistortionType::Custom: {
            // Custom distortion map would be sampled here
            // For now, provide a simple perlin-noise-like procedural pattern
            f32 t = time * source.speed;
            f32 nx = Math::Sin(pixelUV.x * 17.3f + t) * Math::Cos(pixelUV.y * 13.7f + t * 0.8f);
            f32 ny = Math::Cos(pixelUV.x * 11.1f + t * 1.1f) * Math::Sin(pixelUV.y * 19.9f + t * 0.6f);
            offset.x = str * nx * falloffFactor;
            offset.y = str * ny * falloffFactor;
            break;
        }

        case DistortionType::None:
        default:
            break;
    }

    return offset;
}

// ---------------------------------------------------------------------------
// ComputeDistortionField — iterate all sources, composite into field
// ---------------------------------------------------------------------------
DistortionField ScreenDistortionSystem::ComputeDistortionField(
    ECS::World* world,
    const DistortionCameraInfo& camera,
    i32 width, i32 height)
{
    // Apply downscale
    i32 fieldW = std::max(1, width / std::max(1, m_Config.fieldDownscale));
    i32 fieldH = std::max(1, height / std::max(1, m_Config.fieldDownscale));

    // Reuse cached field to avoid per-frame allocation
    if (m_CachedField.width != fieldW || m_CachedField.height != fieldH) {
        m_CachedField.Resize(fieldW, fieldH);
    }
    m_CachedField.Clear();

    if (!world || !m_Config.enabled) return m_CachedField;

    f32 globalStr = m_Config.globalStrength;
    f32 maxDisp = m_Config.maxDisplacement;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<DistortionSourceComponent>()) {
        auto* src = world->GetComponent<DistortionSourceComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!src || !transform || src->type == DistortionType::None) continue;

        Math::Vector3 worldPos = transform->position;

        // Project to screen
        Math::Vector2 screenPos;
        bool onScreen = ProjectToScreen(worldPos, camera, screenPos);

        // For full-screen effects (Underwater, BarrelFisheye), we still process
        // even if source is off-screen, using screen center as reference
        if (!onScreen) {
            if (src->type == DistortionType::Underwater ||
                src->type == DistortionType::Custom) {
                screenPos = Math::Vector2(0.5f, 0.5f);
            } else {
                continue;
            }
        }

        // Compute world-space distance from camera to source
        Math::Vector3 diff = worldPos - camera.position;
        f32 worldDistToCamera = Math::Sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);

        // Screen-space radius approximation (simple perspective divide)
        f32 screenRadius = 1.0f;
        if (worldDistToCamera > 0.01f) {
            screenRadius = src->radius / worldDistToCamera;
        }
        screenRadius = Math::Clamp(screenRadius, 0.01f, 2.0f);

        // Iterate field pixels
        for (i32 py = 0; py < fieldH; ++py) {
            f32 uv_y = (static_cast<f32>(py) + 0.5f) / static_cast<f32>(fieldH);
            for (i32 px = 0; px < fieldW; ++px) {
                f32 uv_x = (static_cast<f32>(px) + 0.5f) / static_cast<f32>(fieldW);
                Math::Vector2 pixelUV(uv_x, uv_y);

                // Distance from source in screen UV space
                f32 dx = uv_x - screenPos.x;
                f32 dy = uv_y - screenPos.y;
                f32 screenDist = Math::Sqrt(dx * dx + dy * dy);

                // For localized effects, skip pixels outside the effect radius
                bool isLocalized = (src->type != DistortionType::Underwater &&
                                    src->type != DistortionType::Custom);
                if (isLocalized && screenDist > screenRadius) continue;

                // Normalized distance [0,1] within the effect radius
                f32 normDist = isLocalized ? Math::Clamp(screenDist / screenRadius, 0.0f, 1.0f) : 0.0f;

                Math::Vector2 offset = ComputeDistortionOffset(
                    src->type, pixelUV, screenPos,
                    normDist, worldDistToCamera, *src, m_Time);

                // Apply global strength
                offset.x *= globalStr;
                offset.y *= globalStr;

                // Clamp displacement
                offset.x = Math::Clamp(offset.x, -maxDisp, maxDisp);
                offset.y = Math::Clamp(offset.y, -maxDisp, maxDisp);

                // Additive accumulation
                Math::Vector2& fieldPixel = m_CachedField.At(px, py);
                fieldPixel.x += offset.x;
                fieldPixel.y += offset.y;
            }
        }
    }

    // Final clamp pass
    for (auto& o : m_CachedField.offsets) {
        o.x = Math::Clamp(o.x, -maxDisp, maxDisp);
        o.y = Math::Clamp(o.y, -maxDisp, maxDisp);
    }

    return m_CachedField;
}

// ---------------------------------------------------------------------------
// SampleBilinear — bilinear sampling from pixel buffer
// ---------------------------------------------------------------------------
Math::Vector4 ScreenDistortionSystem::SampleBilinear(const PixelBuffer& buffer, f32 u, f32 v) {
    // Clamp to [0, 1]
    u = Math::Clamp(u, 0.0f, 1.0f);
    v = Math::Clamp(v, 0.0f, 1.0f);

    f32 fx = u * static_cast<f32>(buffer.width - 1);
    f32 fy = v * static_cast<f32>(buffer.height - 1);

    i32 x0 = static_cast<i32>(Math::Floor(fx));
    i32 y0 = static_cast<i32>(Math::Floor(fy));
    i32 x1 = std::min(x0 + 1, buffer.width - 1);
    i32 y1 = std::min(y0 + 1, buffer.height - 1);

    // Clamp to valid range
    x0 = std::max(0, std::min(x0, buffer.width - 1));
    y0 = std::max(0, std::min(y0, buffer.height - 1));

    f32 tx = fx - static_cast<f32>(x0);
    f32 ty = fy - static_cast<f32>(y0);

    const Math::Vector4& c00 = buffer.At(x0, y0);
    const Math::Vector4& c10 = buffer.At(x1, y0);
    const Math::Vector4& c01 = buffer.At(x0, y1);
    const Math::Vector4& c11 = buffer.At(x1, y1);

    // Bilinear interpolation per channel
    Math::Vector4 result;
    result.x = (c00.x * (1.0f - tx) + c10.x * tx) * (1.0f - ty) +
               (c01.x * (1.0f - tx) + c11.x * tx) * ty;
    result.y = (c00.y * (1.0f - tx) + c10.y * tx) * (1.0f - ty) +
               (c01.y * (1.0f - tx) + c11.y * tx) * ty;
    result.z = (c00.z * (1.0f - tx) + c10.z * tx) * (1.0f - ty) +
               (c01.z * (1.0f - tx) + c11.z * tx) * ty;
    result.w = (c00.w * (1.0f - tx) + c10.w * tx) * (1.0f - ty) +
               (c01.w * (1.0f - tx) + c11.w * tx) * ty;

    return result;
}

// ---------------------------------------------------------------------------
// ApplyDistortion — remap source image using distortion field UV offsets
// ---------------------------------------------------------------------------
void ScreenDistortionSystem::ApplyDistortion(const PixelBuffer& source,
                                              const DistortionField& field,
                                              PixelBuffer& output) {
    if (source.width <= 0 || source.height <= 0) return;

    output.Resize(source.width, source.height);

    f32 fieldScaleX = static_cast<f32>(field.width) / static_cast<f32>(source.width);
    f32 fieldScaleY = static_cast<f32>(field.height) / static_cast<f32>(source.height);

    for (i32 py = 0; py < source.height; ++py) {
        f32 uv_y = (static_cast<f32>(py) + 0.5f) / static_cast<f32>(source.height);

        // Field coordinates (may be downscaled)
        f32 fieldFY = static_cast<f32>(py) * fieldScaleY;
        i32 fy0 = static_cast<i32>(fieldFY);
        i32 fy1 = std::min(fy0 + 1, field.height - 1);
        fy0 = std::max(0, std::min(fy0, field.height - 1));
        f32 fty = fieldFY - static_cast<f32>(fy0);

        for (i32 px = 0; px < source.width; ++px) {
            f32 uv_x = (static_cast<f32>(px) + 0.5f) / static_cast<f32>(source.width);

            // Bilinear interpolation of distortion field (if downscaled)
            f32 fieldFX = static_cast<f32>(px) * fieldScaleX;
            i32 fx0 = static_cast<i32>(fieldFX);
            i32 fx1 = std::min(fx0 + 1, field.width - 1);
            fx0 = std::max(0, std::min(fx0, field.width - 1));
            f32 ftx = fieldFX - static_cast<f32>(fx0);

            const Math::Vector2& o00 = field.At(fx0, fy0);
            const Math::Vector2& o10 = field.At(fx1, fy0);
            const Math::Vector2& o01 = field.At(fx0, fy1);
            const Math::Vector2& o11 = field.At(fx1, fy1);

            Math::Vector2 offset;
            offset.x = (o00.x * (1.0f - ftx) + o10.x * ftx) * (1.0f - fty) +
                        (o01.x * (1.0f - ftx) + o11.x * ftx) * fty;
            offset.y = (o00.y * (1.0f - ftx) + o10.y * ftx) * (1.0f - fty) +
                        (o01.y * (1.0f - ftx) + o11.y * ftx) * fty;

            // Sample source at distorted position
            f32 srcU = uv_x + offset.x;
            f32 srcV = uv_y + offset.y;

            output.At(px, py) = SampleBilinear(source, srcU, srcV);
        }
    }
}

// ---------------------------------------------------------------------------
// TriggerShockwave — create a one-shot shockwave entity
// ---------------------------------------------------------------------------
void ScreenDistortionSystem::TriggerShockwave(ECS::World* world,
                                               const Math::Vector3& position,
                                               f32 strength,
                                               f32 speed,
                                               f32 width) {
    if (!world) return;

    ECS::Entity entity = world->CreateEntity();

    // Set transform at shockwave origin
    auto& transform = world->AddComponent<ECS::TransformComponent>(entity);
    transform.position = position;
    transform.visible = false; // No visible mesh, distortion only

    // Configure shockwave distortion source
    DistortionSourceComponent src;
    src.type = DistortionType::Shockwave;
    src.strength = strength;
    src.radius = 20.0f; // Large radius so the ring can expand
    src.shockwaveRadius = 0.0f;
    src.shockwaveSpeed = speed;
    src.shockwaveWidth = width;
    src.shockwaveLife = 0.0f;
    src.shockwaveMaxLife = 1.5f;

    world->AddComponent<DistortionSourceComponent>(entity, src);
}

} // namespace Effects
} // namespace Enjin
