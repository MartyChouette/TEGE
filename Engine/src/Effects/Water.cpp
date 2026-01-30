#include "Enjin/Effects/Water.h"
#include "Enjin/Math/Math.h"

namespace Enjin {
namespace Effects {

void Water3D::Initialize(const Water3DSettings& settings) {
    m_Settings = settings;
    m_WaveTime = 0.0f;
    m_UVOffset = Math::Vector2(0, 0);
}

void Water3D::Update(f32 deltaTime) {
    m_WaveTime += deltaTime * m_Settings.waveSpeed;

    // Update UV scrolling
    m_UVOffset.x += m_Settings.uvScrollSpeed.x * deltaTime;
    m_UVOffset.y += m_Settings.uvScrollSpeed.y * deltaTime;

    // Wrap UV offset
    if (m_UVOffset.x > 1.0f) m_UVOffset.x -= 1.0f;
    if (m_UVOffset.y > 1.0f) m_UVOffset.y -= 1.0f;
}

f32 Water3D::GetWaveHeight(f32 x, f32 z) const {
    // Simple sine wave combination (very PS1/N64)
    f32 wave1 = Math::Sin((x * m_Settings.waveFrequency + m_WaveTime) * m_Settings.waveDirection.x);
    f32 wave2 = Math::Sin((z * m_Settings.waveFrequency * 0.7f + m_WaveTime * 1.3f) * m_Settings.waveDirection.y);
    f32 wave3 = Math::Sin((x + z) * m_Settings.waveFrequency * 0.5f + m_WaveTime * 0.8f) * 0.5f;

    return (wave1 + wave2 + wave3) * m_Settings.waveHeight / 2.5f;
}

Math::Matrix4 Water3D::GetReflectionMatrix() const {
    // Reflection matrix for planar reflection at water surface
    // Reflects across the XZ plane at water Y position
    Math::Matrix4 reflect;
    f32 waterY = m_Settings.position.y;

    // Scale Y by -1, translate to water level
    reflect.m[0] = 1.0f;  reflect.m[1] = 0.0f;  reflect.m[2] = 0.0f;  reflect.m[3] = 0.0f;
    reflect.m[4] = 0.0f;  reflect.m[5] = -1.0f; reflect.m[6] = 0.0f;  reflect.m[7] = 0.0f;
    reflect.m[8] = 0.0f;  reflect.m[9] = 0.0f;  reflect.m[10] = 1.0f; reflect.m[11] = 0.0f;
    reflect.m[12] = 0.0f; reflect.m[13] = 2.0f * waterY; reflect.m[14] = 0.0f; reflect.m[15] = 1.0f;

    return reflect;
}

void Water3D::GenerateMesh(std::vector<Math::Vector3>& positions,
                           std::vector<Math::Vector2>& uvs,
                           std::vector<u32>& indices) const {
    positions.clear();
    uvs.clear();
    indices.clear();

    f32 halfWidth = m_Settings.width * 0.5f;
    f32 halfDepth = m_Settings.depth * 0.5f;

    u32 xSegments = static_cast<u32>(m_Settings.width / m_Settings.tileSize);
    u32 zSegments = static_cast<u32>(m_Settings.depth / m_Settings.tileSize);

    xSegments = Math::Max(xSegments, 1u);
    zSegments = Math::Max(zSegments, 1u);

    // Generate vertices
    for (u32 z = 0; z <= zSegments; ++z) {
        for (u32 x = 0; x <= xSegments; ++x) {
            f32 xPos = m_Settings.position.x - halfWidth + (static_cast<f32>(x) / xSegments) * m_Settings.width;
            f32 zPos = m_Settings.position.z - halfDepth + (static_cast<f32>(z) / zSegments) * m_Settings.depth;
            f32 yPos = m_Settings.position.y;

            // Add wave displacement if using vertex waves
            if (m_Settings.style == WaterStyle::VertexWave ||
                m_Settings.style == WaterStyle::Reflective ||
                m_Settings.style == WaterStyle::Refractive) {
                yPos += GetWaveHeight(xPos, zPos);
            }

            positions.push_back(Math::Vector3(xPos, yPos, zPos));

            f32 u = static_cast<f32>(x) / xSegments;
            f32 v = static_cast<f32>(z) / zSegments;
            uvs.push_back(Math::Vector2(u + m_UVOffset.x, v + m_UVOffset.y));
        }
    }

    // Generate indices
    for (u32 z = 0; z < zSegments; ++z) {
        for (u32 x = 0; x < xSegments; ++x) {
            u32 topLeft = z * (xSegments + 1) + x;
            u32 topRight = topLeft + 1;
            u32 bottomLeft = (z + 1) * (xSegments + 1) + x;
            u32 bottomRight = bottomLeft + 1;

            // Two triangles per quad
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
}

// 2D Water Implementation
void Water2D::Initialize(const Water2DSettings& settings) {
    m_Settings = settings;
    m_Time = 0.0f;
}

void Water2D::Update(f32 deltaTime) {
    m_Time += deltaTime * m_Settings.waveSpeed;
}

f32 Water2D::GetWaveOffset(f32 x) const {
    if (!m_Settings.enableWaveTop) return 0.0f;

    // Simple sine wave for wavy top edge
    return Math::Sin(x * m_Settings.waveFrequency + m_Time) * m_Settings.waveAmplitude;
}

f32 Water2D::GetReflectionOffset(f32 x, f32 y) const {
    if (!m_Settings.enableReflection) return 0.0f;

    // Wobble effect for reflections
    f32 depth = y - m_Settings.position.y;
    return Math::Sin(x * 0.1f + m_Time * 2.0f + depth * 0.05f) * m_Settings.reflectionDistortion;
}

bool Water2D::IsUnderwater(const Math::Vector2& point) const {
    f32 waterTop = m_Settings.position.y + GetWaveOffset(point.x);
    return point.y > waterTop &&
           point.x >= m_Settings.position.x &&
           point.x <= m_Settings.position.x + m_Settings.size.x &&
           point.y <= m_Settings.position.y + m_Settings.size.y;
}

// Water Interaction
void WaterInteraction::SpawnSplash(const SplashSettings& settings) {
    // A splash creates a ripple at the impact point
    // The visual splash particles would be handled by a particle system if available,
    // but the water surface effect is represented as ripples
    RippleSettings ripple;
    ripple.position = settings.position;
    ripple.amplitude = settings.size * 0.5f;
    ripple.maxRadius = settings.size * 3.0f;
    ripple.duration = settings.duration;
    SpawnRipple(ripple);

    // For larger splashes, add secondary ripples
    if (settings.particleCount > 5) {
        RippleSettings secondary;
        secondary.position = settings.position;
        secondary.amplitude = settings.size * 0.25f;
        secondary.maxRadius = settings.size * 5.0f;
        secondary.duration = settings.duration * 1.5f;
        SpawnRipple(secondary);
    }
}

void WaterInteraction::SpawnRipple(const RippleSettings& settings) {
    ActiveRipple ripple;
    ripple.position = settings.position;
    ripple.currentRadius = 0.0f;
    ripple.amplitude = settings.amplitude;
    ripple.progress = 0.0f;

    m_Ripples.push_back(ripple);
}

void WaterInteraction::Update(f32 deltaTime) {
    // Update ripples
    for (auto it = m_Ripples.begin(); it != m_Ripples.end();) {
        it->progress += deltaTime / 2.0f;  // 2 second duration
        it->currentRadius = it->progress * 5.0f;  // Expand radius
        it->amplitude *= (1.0f - deltaTime);  // Fade out

        if (it->progress >= 1.0f) {
            it = m_Ripples.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Effects
} // namespace Enjin
