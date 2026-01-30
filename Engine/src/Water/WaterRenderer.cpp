#include "Enjin/Water/WaterRenderer.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cmath>

namespace Enjin {
namespace Water {

WaterRenderer::WaterRenderer() {
}

WaterRenderer::~WaterRenderer() {
    Shutdown();
}

bool WaterRenderer::Initialize(Renderer::VulkanRenderer* renderer) {
    m_Renderer = renderer;

    InitializeDefaultWaves();

    ENJIN_LOG_INFO(Renderer, "Water Renderer initialized with %zu Gerstner waves", m_Waves.size());
    return true;
}

void WaterRenderer::Shutdown() {
    m_Waves.clear();
}

void WaterRenderer::Render(f32 deltaTime, const Math::Vector3& cameraPosition) {
    m_Time += deltaTime;

    // Water surface rendering:
    // In a full GPU implementation, the Gerstner wave displacement would be
    // computed in the vertex shader for a subdivided plane mesh. The CPU side
    // provides wave parameters as uniforms and the shader displaces vertices.
    //
    // The rendering pipeline would:
    // 1. Render scene to reflection texture (camera mirrored at water level)
    // 2. Render scene to refraction texture (below water clip plane)
    // 3. Render water mesh with Gerstner displacement in vertex shader
    // 4. Fragment shader blends reflection/refraction based on Fresnel term
    // 5. Apply foam at wave crests (steepness threshold)
    //
    // Underwater effects when camera is below water level:
    if (cameraPosition.y < m_WaterLevel) {
        // Underwater: apply color tint, caustics, fog density based on depth
        f32 depth = m_WaterLevel - cameraPosition.y;
        f32 fogDensity = Math::Min(depth / 50.0f, 1.0f);
        (void)fogDensity; // Would be passed to post-processing as underwater fog
    }
}

void WaterRenderer::InitializeDefaultWaves() {
    m_Waves.clear();

    // Primary wave - dominant ocean swell
    GerstnerWave wave1;
    wave1.direction = Math::Vector2(1.0f, 0.3f);
    wave1.amplitude = m_WaveAmplitude;
    wave1.wavelength = 12.0f;
    wave1.speed = m_WaveSpeed;
    wave1.steepness = 0.4f;
    m_Waves.push_back(wave1);

    // Secondary wave - cross swell
    GerstnerWave wave2;
    wave2.direction = Math::Vector2(0.5f, 1.0f);
    wave2.amplitude = m_WaveAmplitude * 0.5f;
    wave2.wavelength = 7.0f;
    wave2.speed = m_WaveSpeed * 1.3f;
    wave2.steepness = 0.3f;
    m_Waves.push_back(wave2);

    // Tertiary wave - fine detail ripples
    GerstnerWave wave3;
    wave3.direction = Math::Vector2(-0.3f, 0.7f);
    wave3.amplitude = m_WaveAmplitude * 0.2f;
    wave3.wavelength = 3.0f;
    wave3.speed = m_WaveSpeed * 1.8f;
    wave3.steepness = 0.5f;
    m_Waves.push_back(wave3);

    // Fourth wave - broad ocean movement
    GerstnerWave wave4;
    wave4.direction = Math::Vector2(0.8f, -0.4f);
    wave4.amplitude = m_WaveAmplitude * 0.3f;
    wave4.wavelength = 20.0f;
    wave4.speed = m_WaveSpeed * 0.7f;
    wave4.steepness = 0.2f;
    m_Waves.push_back(wave4);
}

Math::Vector3 WaterRenderer::CalculateGerstnerDisplacement(f32 x, f32 z) const {
    Math::Vector3 displacement(0.0f, 0.0f, 0.0f);

    for (const auto& wave : m_Waves) {
        // Normalize direction
        f32 dirLen = Math::Sqrt(wave.direction.x * wave.direction.x + wave.direction.y * wave.direction.y);
        if (dirLen < Math::EPSILON) continue;
        f32 dx = wave.direction.x / dirLen;
        f32 dz = wave.direction.y / dirLen;

        // Wave number k = 2*PI / wavelength
        f32 k = 2.0f * Math::PI / wave.wavelength;
        // Angular frequency
        f32 omega = Math::Sqrt(9.81f * k); // Deep water dispersion relation

        // Phase
        f32 phase = k * (dx * x + dz * z) - omega * m_Time * wave.speed;

        // Gerstner displacement
        f32 Q = wave.steepness / (k * wave.amplitude * static_cast<f32>(m_Waves.size()));
        Q = Math::Clamp(Q, 0.0f, 1.0f);

        displacement.x += Q * wave.amplitude * dx * std::cos(phase);
        displacement.y += wave.amplitude * std::sin(phase);
        displacement.z += Q * wave.amplitude * dz * std::cos(phase);
    }

    return displacement;
}

Math::Vector3 WaterRenderer::CalculateGerstnerNormal(f32 x, f32 z) const {
    Math::Vector3 normal(0.0f, 1.0f, 0.0f);

    for (const auto& wave : m_Waves) {
        f32 dirLen = Math::Sqrt(wave.direction.x * wave.direction.x + wave.direction.y * wave.direction.y);
        if (dirLen < Math::EPSILON) continue;
        f32 dx = wave.direction.x / dirLen;
        f32 dz = wave.direction.y / dirLen;

        f32 k = 2.0f * Math::PI / wave.wavelength;
        f32 omega = Math::Sqrt(9.81f * k);
        f32 phase = k * (dx * x + dz * z) - omega * m_Time * wave.speed;

        f32 WA = k * wave.amplitude;
        f32 S = std::sin(phase);
        f32 C = std::cos(phase);

        normal.x -= dx * WA * C;
        normal.y -= wave.steepness * WA * S;
        normal.z -= dz * WA * C;
    }

    // Normalize
    f32 len = Math::Sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (len > Math::EPSILON) {
        normal = normal * (1.0f / len);
    }

    return normal;
}

f32 WaterRenderer::GetWaterHeight(f32 x, f32 z) const {
    Math::Vector3 disp = CalculateGerstnerDisplacement(x, z);
    return m_WaterLevel + disp.y;
}

bool WaterRenderer::IsUnderwater(const Math::Vector3& point) const {
    return point.y < GetWaterHeight(point.x, point.z);
}

} // namespace Water
} // namespace Enjin
