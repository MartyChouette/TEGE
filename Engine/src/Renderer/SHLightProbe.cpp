#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/Math/Math.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

namespace Enjin {
namespace Renderer {

u32 SHLightingSystem::AddProbe(const Math::Vector3& position) {
    SHProbe probe;
    probe.id = m_NextId++;
    probe.position = position;
    probe.baked = false;
    m_Probes.push_back(probe);
    return probe.id;
}

void SHLightingSystem::RemoveProbe(u32 probeId) {
    m_Probes.erase(
        std::remove_if(m_Probes.begin(), m_Probes.end(),
            [probeId](const SHProbe& p) { return p.id == probeId; }),
        m_Probes.end());
}

void SHLightingSystem::Clear() {
    m_Probes.clear();
    m_NextId = 1;
}

// SH basis functions for L0, L1, L2 bands
// Index ordering: Y00, Y1-1, Y10, Y11, Y2-2, Y2-1, Y20, Y21, Y22
static void EvaluateSHBasis(const Math::Vector3& dir, f32 sh[9]) {
    // L0
    sh[0] = 0.2821f;  // Y00 = 0.5 * sqrt(1/PI) ~ 0.2821

    // L1
    sh[1] = 0.4886f * dir.y;   // Y1-1
    sh[2] = 0.4886f * dir.z;   // Y10
    sh[3] = 0.4886f * dir.x;   // Y11

    // L2
    sh[4] = 1.0925f * dir.x * dir.y;                              // Y2-2
    sh[5] = 1.0925f * dir.y * dir.z;                              // Y2-1
    sh[6] = 0.3154f * (3.0f * dir.z * dir.z - 1.0f);             // Y20
    sh[7] = 1.0925f * dir.x * dir.z;                              // Y21
    sh[8] = 0.5463f * (dir.x * dir.x - dir.y * dir.y);           // Y22
}

// Compute the radiance contribution of a single light at a probe position along a given direction
static Math::Vector3 EvaluateLightRadiance(
    const ECS::LightComponent& light,
    const Math::Vector3& lightPos,
    const Math::Vector3& lightDir,
    const Math::Vector3& probePos,
    const Math::Vector3& sampleDir) {

    Math::Vector3 radiance(0.0f, 0.0f, 0.0f);

    if (light.type == ECS::LightType::Directional) {
        // Directional light: radiance comes from the opposite of light direction
        // The light shines *from* -lightDir toward the scene
        // A sample direction receives light if it faces toward the light source
        f32 ndotl = Math::Max(0.0f, sampleDir.Dot(Math::Vector3(-lightDir.x, -lightDir.y, -lightDir.z)));
        radiance.x = light.color.x * light.intensity * ndotl;
        radiance.y = light.color.y * light.intensity * ndotl;
        radiance.z = light.color.z * light.intensity * ndotl;
    }
    else if (light.type == ECS::LightType::Point) {
        // Point light: compute direction from probe to light, then attenuation
        Math::Vector3 toLight(lightPos.x - probePos.x, lightPos.y - probePos.y, lightPos.z - probePos.z);
        f32 dist = toLight.Length();
        if (dist > 0.001f && dist < light.range) {
            Math::Vector3 toLightDir(toLight.x / dist, toLight.y / dist, toLight.z / dist);
            f32 ndotl = Math::Max(0.0f, sampleDir.Dot(toLightDir));
            f32 attenuation = light.CalculateAttenuation(dist);
            radiance.x = light.color.x * light.intensity * attenuation * ndotl;
            radiance.y = light.color.y * light.intensity * attenuation * ndotl;
            radiance.z = light.color.z * light.intensity * attenuation * ndotl;
        }
    }
    else if (light.type == ECS::LightType::Spot) {
        // Spot light: like point light but with cone falloff
        Math::Vector3 toLight(lightPos.x - probePos.x, lightPos.y - probePos.y, lightPos.z - probePos.z);
        f32 dist = toLight.Length();
        if (dist > 0.001f && dist < light.range) {
            Math::Vector3 toLightDir(toLight.x / dist, toLight.y / dist, toLight.z / dist);

            // Check if probe is within the spot cone
            // lightDir points in the direction the spot light shines
            f32 cosAngle = -(toLightDir.Dot(lightDir));  // negate because toLightDir points toward light
            f32 cosOuter = std::cos(Math::Radians(light.outerConeAngle));
            f32 cosInner = std::cos(Math::Radians(light.innerConeAngle));

            if (cosAngle > cosOuter) {
                f32 spotFactor = 1.0f;
                if (cosAngle < cosInner) {
                    spotFactor = (cosAngle - cosOuter) / Math::Max(cosInner - cosOuter, 0.001f);
                }
                f32 ndotl = Math::Max(0.0f, sampleDir.Dot(toLightDir));
                f32 attenuation = light.CalculateAttenuation(dist);
                radiance.x = light.color.x * light.intensity * attenuation * spotFactor * ndotl;
                radiance.y = light.color.y * light.intensity * attenuation * spotFactor * ndotl;
                radiance.z = light.color.z * light.intensity * attenuation * spotFactor * ndotl;
            }
        }
    }

    return radiance;
}

void SHLightingSystem::BakeProbe(u32 probeId, ECS::World* world) {
    if (!world) return;

    for (auto& probe : m_Probes) {
        if (probe.id != probeId) continue;

        // Zero all coefficients
        for (int i = 0; i < 9; i++) {
            probe.coefficientsR[i] = 0.0f;
            probe.coefficientsG[i] = 0.0f;
            probe.coefficientsB[i] = 0.0f;
        }

        // Gather all scene lights
        struct LightInfo {
            ECS::LightComponent light;
            Math::Vector3 position;
            Math::Vector3 direction;  // forward direction for directional/spot
        };
        std::vector<LightInfo> lights;

        for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::LightComponent>()) {
            auto* lc = world->GetComponent<ECS::LightComponent>(entity);
            auto* tc = world->GetComponent<ECS::TransformComponent>(entity);
            if (!lc || !tc) continue;

            LightInfo info;
            info.light = *lc;
            info.position = tc->position;
            info.direction = tc->rotation.GetForward();
            lights.push_back(info);
        }

        // Monte Carlo integration over the sphere using stratified sampling
        // We sample N directions uniformly on the sphere and project onto SH
        constexpr u32 kNumSamplesTheta = 32;
        constexpr u32 kNumSamplesPhi = 64;
        constexpr u32 kTotalSamples = kNumSamplesTheta * kNumSamplesPhi;

        for (u32 ti = 0; ti < kNumSamplesTheta; ++ti) {
            for (u32 pi = 0; pi < kNumSamplesPhi; ++pi) {
                // Stratified uniform sphere sampling
                f32 u = (static_cast<f32>(ti) + 0.5f) / kNumSamplesTheta;
                f32 v = (static_cast<f32>(pi) + 0.5f) / kNumSamplesPhi;

                // Convert to spherical coordinates (uniform on sphere)
                f32 theta = std::acos(1.0f - 2.0f * u);  // polar angle [0, PI]
                f32 phi = 2.0f * Math::PI * v;             // azimuthal angle [0, 2*PI]

                // Convert to Cartesian direction
                f32 sinTheta = std::sin(theta);
                Math::Vector3 dir(
                    sinTheta * std::cos(phi),
                    sinTheta * std::sin(phi),
                    std::cos(theta)
                );

                // Evaluate SH basis at this direction
                f32 sh[9];
                EvaluateSHBasis(dir, sh);

                // Accumulate radiance from all lights in this direction
                Math::Vector3 totalRadiance(0.0f, 0.0f, 0.0f);
                for (const auto& lightInfo : lights) {
                    Math::Vector3 r = EvaluateLightRadiance(
                        lightInfo.light, lightInfo.position, lightInfo.direction,
                        probe.position, dir);
                    totalRadiance.x += r.x;
                    totalRadiance.y += r.y;
                    totalRadiance.z += r.z;
                }

                // Project radiance onto SH coefficients
                // Integration weight for uniform sphere sampling: 4*PI / N
                for (int i = 0; i < 9; ++i) {
                    probe.coefficientsR[i] += totalRadiance.x * sh[i];
                    probe.coefficientsG[i] += totalRadiance.y * sh[i];
                    probe.coefficientsB[i] += totalRadiance.z * sh[i];
                }
            }
        }

        // Apply the integration weight: (4 * PI) / N_samples
        f32 weight = (4.0f * Math::PI) / static_cast<f32>(kTotalSamples);
        for (int i = 0; i < 9; ++i) {
            probe.coefficientsR[i] *= weight;
            probe.coefficientsG[i] *= weight;
            probe.coefficientsB[i] *= weight;
        }

        probe.baked = true;
        break;
    }
}

void SHLightingSystem::BakeAll(ECS::World* world) {
    for (auto& probe : m_Probes) {
        BakeProbe(probe.id, world);
    }
}

Math::Vector3 SHLightingSystem::GetIrradiance(const Math::Vector3& position) const {
    const SHProbe* nearest = FindNearestProbe(position);
    if (!nearest || !nearest->baked) {
        return Math::Vector3(0.0f, 0.0f, 0.0f);
    }

    // For irradiance queries we use (0,1,0) as the default normal (upward)
    // This method is called with position only, so use the up direction
    // For a surface normal query, use the overloaded version or pass a normal
    Math::Vector3 normal(0.0f, 1.0f, 0.0f);

    // Evaluate SH basis functions at the normal direction
    f32 sh[9];
    EvaluateSHBasis(normal, sh);

    // Reconstruct irradiance by dotting coefficients with basis functions
    // Apply cosine lobe convolution factors (Ramamoorthi & Hanrahan 2001):
    //   L0: A0 = PI
    //   L1: A1 = 2*PI/3
    //   L2: A2 = PI/4
    constexpr f32 A0 = Math::PI;
    constexpr f32 A1 = 2.0f * Math::PI / 3.0f;
    constexpr f32 A2 = Math::PI / 4.0f;

    f32 bandWeights[9] = { A0, A1, A1, A1, A2, A2, A2, A2, A2 };

    f32 r = 0.0f, g = 0.0f, b = 0.0f;
    for (int i = 0; i < 9; ++i) {
        r += nearest->coefficientsR[i] * sh[i] * bandWeights[i];
        g += nearest->coefficientsG[i] * sh[i] * bandWeights[i];
        b += nearest->coefficientsB[i] * sh[i] * bandWeights[i];
    }

    // Clamp to non-negative
    return Math::Vector3(
        Math::Max(r, 0.0f),
        Math::Max(g, 0.0f),
        Math::Max(b, 0.0f)
    );
}

Math::Vector3 SHLightingSystem::GetIrradiance(const Math::Vector3& position, const Math::Vector3& normal) const {
    const SHProbe* nearest = FindNearestProbe(position);
    if (!nearest || !nearest->baked) {
        return Math::Vector3(0.0f, 0.0f, 0.0f);
    }

    // Normalize the input normal
    Math::Vector3 n = normal.Normalized();

    // Evaluate SH basis functions at the normal direction
    f32 sh[9];
    EvaluateSHBasis(n, sh);

    // Reconstruct irradiance by dotting coefficients with basis functions
    // Apply cosine lobe convolution factors (Ramamoorthi & Hanrahan 2001):
    //   L0: A0 = PI
    //   L1: A1 = 2*PI/3
    //   L2: A2 = PI/4
    constexpr f32 A0 = Math::PI;
    constexpr f32 A1 = 2.0f * Math::PI / 3.0f;
    constexpr f32 A2 = Math::PI / 4.0f;

    f32 bandWeights[9] = { A0, A1, A1, A1, A2, A2, A2, A2, A2 };

    f32 r = 0.0f, g = 0.0f, b = 0.0f;
    for (int i = 0; i < 9; ++i) {
        r += nearest->coefficientsR[i] * sh[i] * bandWeights[i];
        g += nearest->coefficientsG[i] * sh[i] * bandWeights[i];
        b += nearest->coefficientsB[i] * sh[i] * bandWeights[i];
    }

    // Clamp to non-negative
    return Math::Vector3(
        Math::Max(r, 0.0f),
        Math::Max(g, 0.0f),
        Math::Max(b, 0.0f)
    );
}

const SHProbe* SHLightingSystem::FindNearestProbe(const Math::Vector3& position) const {
    if (m_Probes.empty()) return nullptr;

    const SHProbe* nearest = nullptr;
    f32 bestDistSq = std::numeric_limits<f32>::max();

    for (const auto& probe : m_Probes) {
        f32 dx = probe.position.x - position.x;
        f32 dy = probe.position.y - position.y;
        f32 dz = probe.position.z - position.z;
        f32 distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            nearest = &probe;
        }
    }
    return nearest;
}

void SHLightingSystem::GenerateGridProbes() {
    Clear();

    if (m_Grid.resolutionX == 0 || m_Grid.resolutionY == 0 || m_Grid.resolutionZ == 0) return;

    Math::Vector3 size = Math::Vector3(
        m_Grid.boundsMax.x - m_Grid.boundsMin.x,
        m_Grid.boundsMax.y - m_Grid.boundsMin.y,
        m_Grid.boundsMax.z - m_Grid.boundsMin.z
    );

    for (u32 z = 0; z < m_Grid.resolutionZ; z++) {
        for (u32 y = 0; y < m_Grid.resolutionY; y++) {
            for (u32 x = 0; x < m_Grid.resolutionX; x++) {
                f32 fx = (m_Grid.resolutionX > 1) ? static_cast<f32>(x) / (m_Grid.resolutionX - 1) : 0.5f;
                f32 fy = (m_Grid.resolutionY > 1) ? static_cast<f32>(y) / (m_Grid.resolutionY - 1) : 0.5f;
                f32 fz = (m_Grid.resolutionZ > 1) ? static_cast<f32>(z) / (m_Grid.resolutionZ - 1) : 0.5f;

                Math::Vector3 pos(
                    m_Grid.boundsMin.x + size.x * fx,
                    m_Grid.boundsMin.y + size.y * fy,
                    m_Grid.boundsMin.z + size.z * fz
                );
                AddProbe(pos);
            }
        }
    }
}

nlohmann::json SHLightingSystem::Serialize() const {
    nlohmann::json j;

    // Grid config
    j["grid"]["boundsMin"] = { m_Grid.boundsMin.x, m_Grid.boundsMin.y, m_Grid.boundsMin.z };
    j["grid"]["boundsMax"] = { m_Grid.boundsMax.x, m_Grid.boundsMax.y, m_Grid.boundsMax.z };
    j["grid"]["resolutionX"] = m_Grid.resolutionX;
    j["grid"]["resolutionY"] = m_Grid.resolutionY;
    j["grid"]["resolutionZ"] = m_Grid.resolutionZ;
    j["grid"]["autoGenerate"] = m_Grid.autoGenerate;

    // Probes
    nlohmann::json probeArr = nlohmann::json::array();
    for (const auto& p : m_Probes) {
        nlohmann::json pj;
        pj["id"] = p.id;
        pj["position"] = { p.position.x, p.position.y, p.position.z };
        pj["baked"] = p.baked;
        pj["coeffR"] = std::vector<f32>(p.coefficientsR, p.coefficientsR + 9);
        pj["coeffG"] = std::vector<f32>(p.coefficientsG, p.coefficientsG + 9);
        pj["coeffB"] = std::vector<f32>(p.coefficientsB, p.coefficientsB + 9);
        probeArr.push_back(pj);
    }
    j["probes"] = probeArr;

    return j;
}

void SHLightingSystem::Deserialize(const nlohmann::json& j) {
    Clear();

    if (j.contains("grid")) {
        const auto& g = j["grid"];
        if (g.contains("boundsMin") && g["boundsMin"].size() >= 3) {
            m_Grid.boundsMin = Math::Vector3(g["boundsMin"][0], g["boundsMin"][1], g["boundsMin"][2]);
        }
        if (g.contains("boundsMax") && g["boundsMax"].size() >= 3) {
            m_Grid.boundsMax = Math::Vector3(g["boundsMax"][0], g["boundsMax"][1], g["boundsMax"][2]);
        }
        if (g.contains("resolutionX")) m_Grid.resolutionX = g["resolutionX"];
        if (g.contains("resolutionY")) m_Grid.resolutionY = g["resolutionY"];
        if (g.contains("resolutionZ")) m_Grid.resolutionZ = g["resolutionZ"];
        if (g.contains("autoGenerate")) m_Grid.autoGenerate = g["autoGenerate"];
    }

    if (j.contains("probes") && j["probes"].is_array()) {
        for (const auto& pj : j["probes"]) {
            SHProbe probe;
            if (pj.contains("id")) probe.id = pj["id"];
            if (pj.contains("position") && pj["position"].size() >= 3) {
                probe.position = Math::Vector3(pj["position"][0], pj["position"][1], pj["position"][2]);
            }
            if (pj.contains("baked")) probe.baked = pj["baked"];

            auto loadCoeffs = [](const nlohmann::json& arr, f32* dst) {
                if (!arr.is_array()) return;
                for (size_t i = 0; i < 9 && i < arr.size(); i++) {
                    dst[i] = arr[i];
                }
            };
            if (pj.contains("coeffR")) loadCoeffs(pj["coeffR"], probe.coefficientsR);
            if (pj.contains("coeffG")) loadCoeffs(pj["coeffG"], probe.coefficientsG);
            if (pj.contains("coeffB")) loadCoeffs(pj["coeffB"], probe.coefficientsB);

            m_Probes.push_back(probe);
            if (probe.id >= m_NextId) m_NextId = probe.id + 1;
        }
    }
}

} // namespace Renderer
} // namespace Enjin
