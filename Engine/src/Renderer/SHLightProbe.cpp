#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/ECS/World.h"
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

void SHLightingSystem::BakeProbe(u32 probeId, ECS::World* world) {
    if (!world) return;

    for (auto& probe : m_Probes) {
        if (probe.id != probeId) continue;

        // Stub: Initialize with ambient-only SH (L0 band = uniform irradiance)
        // L0 coefficient = sqrt(1/(4*pi)) * irradiance
        // A real implementation would sample the scene from the probe position
        // using a cubemap render or ray casts
        constexpr f32 SH_L0 = 0.2821f; // sqrt(1/(4*PI))

        // Default ambient: soft warm-ish light
        for (int i = 0; i < 9; i++) {
            probe.coefficientsR[i] = 0.0f;
            probe.coefficientsG[i] = 0.0f;
            probe.coefficientsB[i] = 0.0f;
        }
        // L0 band (constant term)
        probe.coefficientsR[0] = SH_L0 * 0.3f;
        probe.coefficientsG[0] = SH_L0 * 0.3f;
        probe.coefficientsB[0] = SH_L0 * 0.35f;

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

    // Return L0 band irradiance (uniform term) from nearest probe
    // Full implementation would evaluate SH basis functions for the surface normal
    constexpr f32 INV_SH_L0 = 3.5449f; // 1/sqrt(1/(4*PI)) = sqrt(4*PI)
    return Math::Vector3(
        nearest->coefficientsR[0] * INV_SH_L0,
        nearest->coefficientsG[0] * INV_SH_L0,
        nearest->coefficientsB[0] * INV_SH_L0
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
