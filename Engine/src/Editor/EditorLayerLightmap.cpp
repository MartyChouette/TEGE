// Baking the scene's lightmap.
//
// The same argument as the background-plate baker: the runtime can display
// baked light, but without a button that produces it the technique is reachable
// only by someone willing to write their own tool. The engine already knows the
// geometry and the lights, so it can hand you the bake.
//
// What this does, in order: collect every mesh that opted in, move it into
// world space, generate a lightmap UV layout, trace the light, write three PNGs
// into the project, and point the scene at them.

#include "Enjin/Editor/EditorLayer.h"
#include "Enjin/Editor/EditorWidgets.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/LightmapBake.h"
#include "Enjin/Renderer/LightmapUnwrap.h"
#include "Enjin/Platform/Paths.h"
#include "Enjin/Logging/Log.h"

#include <imgui.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

extern "C" {
    int stbi_write_png(const char* filename, int w, int h, int comp,
                       const void* data, int stride_in_bytes);
}

namespace Enjin {
namespace Editor {

bool EditorLayer::BakeSceneLightmap(std::string& outStatus) {
    if (!m_World || !m_RenderSystem) {
        outStatus = "No scene to bake.";
        return false;
    }
    if (m_SceneManager.GetProjectPath().empty()) {
        outStatus = "Open a project first: a bake is saved into it.";
        return false;
    }

    // --- Collect the geometry that asked to be lit this way.
    //
    // Opting IN per material rather than baking everything is deliberate. A
    // lightmap can only describe things that never move, and a bake that
    // silently included a door or a crate would burn its shadow into the floor
    // permanently -- the classic way baked lighting goes wrong.
    std::vector<Math::Vector3> worldPos;
    std::vector<u32> indices;
    std::vector<Math::Vector3> worldNrm;
    std::vector<Math::Vector4> worldTan;

    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::MeshComponent>()) {
        const auto* mat = m_World->GetComponent<ECS::MaterialComponent>(e);
        if (!mat || !mat->lightmapped) continue;
        const auto* mesh = m_World->GetComponent<ECS::MeshComponent>(e);
        const auto* xf = m_World->GetComponent<ECS::TransformComponent>(e);
        if (!mesh || !xf || mesh->vertices.empty() || mesh->indices.empty()) continue;

        // The full parent chain, not just this entity's local transform: a wall
        // under a rotated room root would otherwise bake in the wrong place.
        const Math::Matrix4 model = ECS::ComputeWorldMatrix(m_World, e);
        const u32 base = static_cast<u32>(worldPos.size());
        for (const auto& v : mesh->vertices) {
            const Math::Vector4 wp = model * Math::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
            worldPos.push_back(Math::Vector3(wp.x, wp.y, wp.z));
            // Normals and tangents are DIRECTIONS, so they go through with w = 0.
            // Carrying the translation would leave every surface thinking it
            // faced the origin.
            const Math::Vector4 wn = model * Math::Vector4(v.normal.x, v.normal.y, v.normal.z, 0.0f);
            worldNrm.push_back(Math::Vector3(wn.x, wn.y, wn.z).Normalized());
            const Math::Vector4 wt = model * Math::Vector4(v.tangent.x, v.tangent.y, v.tangent.z, 0.0f);
            const Math::Vector3 t = Math::Vector3(wt.x, wt.y, wt.z).Normalized();
            worldTan.push_back(Math::Vector4(t.x, t.y, t.z, v.tangent.w));
        }
        for (u32 i : mesh->indices) indices.push_back(base + i);
    }

    if (indices.empty()) {
        outStatus = "Nothing opted in. Tick Lightmapped on the materials you want baked.";
        return false;
    }

    // --- Lay the atlas out.
    Renderer::LightmapUnwrapOptions uo;
    uo.atlasSize = m_LightmapAtlasSize;
    uo.texelsPerUnit = m_LightmapTexelsPerUnit;
    std::vector<Math::Vector2> uv;
    const auto unwrap = Renderer::BuildLightmapUVs(worldPos, indices, uo, uv);
    if (!unwrap.ok) {
        outStatus = "Unwrap failed: " + unwrap.error;
        return false;
    }

    // --- The scene's lights, as the baker wants them.
    std::vector<Renderer::BakeLight> lights;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::LightComponent>()) {
        const auto* lc = m_World->GetComponent<ECS::LightComponent>(e);
        const auto* xf = m_World->GetComponent<ECS::TransformComponent>(e);
        if (!lc || !xf) continue;
        Renderer::BakeLight bl;
        bl.color = lc->color;
        bl.intensity = lc->intensity;
        bl.range = lc->range;
        if (lc->type == ECS::LightType::Directional) {
            bl.type = Renderer::BakeLight::Type::Directional;
            bl.vector = xf->rotation.GetForward();
        } else if (lc->type == ECS::LightType::Point) {
            bl.type = Renderer::BakeLight::Type::Point;
            bl.vector = xf->position;
        } else {
            // Spot lights are not baked yet: their cone and cookie are a run-time
            // shape, and baking one would freeze it into the floor.
            continue;
        }
        lights.push_back(bl);
    }

    // --- Trace.
    std::vector<Renderer::BakeTriangle> tris;
    tris.reserve(indices.size() / 3);
    for (usize t = 0; t + 2 < indices.size(); t += 3) {
        Renderer::BakeTriangle bt;
        for (u32 k = 0; k < 3; ++k) {
            const u32 vi = indices[t + k];
            bt.position[k] = worldPos[vi];
            bt.normal[k] = worldNrm[vi];
            bt.tangent[k] = worldTan[vi];
            bt.uv1[k] = uv[t + k];
        }
        tris.push_back(bt);
    }

    Renderer::LightmapBakeOptions bo;
    bo.atlasSize = m_LightmapAtlasSize;
    bo.skySamples = m_LightmapSkySamples;
    const auto baked = Renderer::BakeLightmap(tris, lights, bo);
    if (!baked.ok) {
        outStatus = "Bake failed: " + baked.error;
        return false;
    }

    // --- Write the three atlases into the project.
    namespace fs = std::filesystem;
    const fs::path root = fs::path(m_SceneManager.GetProjectPath()).parent_path();
    const fs::path dir = root / "assets" / "lightmaps";
    std::error_code ec;
    fs::create_directories(dir, ec);

    std::string leaf = m_LightmapBakeName.empty() ? std::string("scene") : m_LightmapBakeName;
    if (!Platform::IsSafeFileName(leaf)) {
        outStatus = "That file name is not allowed.";
        return false;
    }

    std::string rel[3];
    for (u32 b = 0; b < Renderer::kRNMBasisCount; ++b) {
        char suffix[32];
        std::snprintf(suffix, sizeof(suffix), "_basis%u.png", b);
        const fs::path out = dir / (leaf + suffix);
        if (!stbi_write_png(out.string().c_str(), static_cast<int>(m_LightmapAtlasSize),
                            static_cast<int>(m_LightmapAtlasSize), 3,
                            baked.basis[b].data(), static_cast<int>(m_LightmapAtlasSize) * 3)) {
            outStatus = "Could not write " + out.string();
            return false;
        }
        rel[b] = "assets/lightmaps/" + leaf + suffix;
    }

    // --- Write the UVs back onto the meshes.
    //
    // The layout is per-TRIANGLE, so the meshes have to become triangle soups:
    // a vertex on a seam needs a different lightmap UV for each triangle that
    // touches it. This is where a lightmapped mesh stops being shareable, and
    // it is why only opted-in geometry goes through it.
    usize corner = 0;
    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::MeshComponent>()) {
        const auto* mat = m_World->GetComponent<ECS::MaterialComponent>(e);
        if (!mat || !mat->lightmapped) continue;
        auto* mesh = m_World->GetComponent<ECS::MeshComponent>(e);
        if (!mesh || mesh->vertices.empty() || mesh->indices.empty()) continue;

        std::vector<ECS::MeshComponent::Vertex> soup;
        soup.reserve(mesh->indices.size());
        std::vector<u32> newIdx;
        newIdx.reserve(mesh->indices.size());
        for (usize i = 0; i < mesh->indices.size(); ++i) {
            ECS::MeshComponent::Vertex v = mesh->vertices[mesh->indices[i]];
            v.uv1 = (corner < uv.size()) ? uv[corner] : Math::Vector2(0.0f, 0.0f);
            ++corner;
            newIdx.push_back(static_cast<u32>(soup.size()));
            soup.push_back(v);
        }
        mesh->vertices.swap(soup);
        mesh->indices.swap(newIdx);
        // Same signal the brush and generated-geometry systems use after
        // rebuilding a mesh in place.
        mesh->aabbDirty = true;
    }

    // Set on the RUNTIME only. The scene's render settings are captured from
    // the render system when the scene is saved, so writing both here would be
    // two sources for one fact -- and the capture is the one that reaches the
    // file. Same arrangement as every other setting under ADR-0006.
    m_RenderSystem->SetSceneLightmap(true, rel[0], rel[1], rel[2], 1.0f);

    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "Baked %u triangles into %ux%u (%.0f%% of the atlas used, %u rays).",
                  unwrap.triangleCount, m_LightmapAtlasSize, m_LightmapAtlasSize,
                  static_cast<double>(unwrap.coverage) * 100.0, baked.rayCasts);
    outStatus = msg;
    ENJIN_LOG_INFO(Editor, "Lightmap bake: %s -> %s", msg, rel[0].c_str());
    return true;
}

void EditorLayer::DrawLightmapBakerWindow() {
    if (!m_ShowLightmapBaker) return;

    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Bake Lightmap", &m_ShowLightmapBaker)) {
        ImGui::End();
        return;
    }

    ImGui::TextWrapped("Baked light that still reacts to normal maps. Three atlases hold the "
                       "light arriving from three directions, and the shader blends them by "
                       "which way each pixel's bump faces -- so shadows and bounced sky cost "
                       "three texture reads instead of rays.");
    ImGui::Spacing();
    ImGui::TextWrapped("Tick Lightmapped on the materials you want baked. Only things that never "
                       "move belong here: a baked shadow under a door stays there after the door "
                       "opens.");
    ImGui::Spacing();

    ImGui::PushItemWidth(180.0f);
    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", m_LightmapBakeName.c_str());
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) m_LightmapBakeName = nameBuf;

    int atlas = static_cast<int>(m_LightmapAtlasSize);
    const char* sizes[] = { "256", "512", "1024", "2048" };
    int sizeIdx = atlas <= 256 ? 0 : (atlas <= 512 ? 1 : (atlas <= 1024 ? 2 : 3));
    if (ImGui::Combo("Atlas", &sizeIdx, sizes, 4)) {
        m_LightmapAtlasSize = static_cast<u32>(256 << sizeIdx);
    }

    ImGui::DragFloat("Texels per unit", &m_LightmapTexelsPerUnit, 0.25f, 1.0f, 64.0f, "%.1f");
    ImGui::SetItemTooltip("Lighting detail in world terms. Raising it costs atlas area\n"
                          "quadratically and buys detail a normal map is better at.");

    int rays = static_cast<int>(m_LightmapSkySamples);
    if (ImGui::SliderInt("Sky rays", &rays, 0, 128)) m_LightmapSkySamples = static_cast<u32>(rays);
    ImGui::SetItemTooltip("The whole quality dial. This is where the soft darkening in\n"
                          "corners comes from, and where all the bake time goes.");
    ImGui::PopItemWidth();

    ImGui::Spacing();
    if (ImGui::Button("Bake", ImVec2(160.0f, 0.0f))) {
        std::string status;
        const bool ok = BakeSceneLightmap(status);
        m_LightmapBakeStatus = status;
        ShowNotification(status, ok ? NotificationType::Success : NotificationType::Error);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Runs on the CPU and can take a while at high sky-ray counts.\n"
                          "It is offline work: none of this cost is paid again at run time.");
    }

    if (!m_LightmapBakeStatus.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", m_LightmapBakeStatus.c_str());
    }

    ImGui::End();
}

} // namespace Editor
} // namespace Enjin
