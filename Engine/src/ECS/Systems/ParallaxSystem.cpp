#include "Enjin/ECS/Systems/ParallaxSystem.h"
#include "Enjin/ECS/Components/ParallaxMachine.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/Math/Math.h"
#include <algorithm>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>

namespace Enjin::ECS {

void ParallaxSystem::Update(f32 deltaTime) {
    if (!m_World) return;

    for (auto entity : m_World->GetEntitiesWithComponent<ParallaxMachineComponent>()) {
        auto* pm = m_World->GetComponent<ParallaxMachineComponent>(entity);
        if (!pm || !pm->enabled) continue;

        // Advance auto-scroll offset
        pm->autoScrollOffset.x += pm->autoScrollSpeed.x * deltaTime;
        pm->autoScrollOffset.y += pm->autoScrollSpeed.y * deltaTime;
    }
}

VkDescriptorSet ParallaxSystem::GetLayerTexture(const std::string& path) {
    if (path.empty() || !m_RenderSystem) return VK_NULL_HANDLE;

    auto it = m_TextureCache.find(path);
    if (it != m_TextureCache.end()) return it->second;

    // Load texture via RenderSystem's texture cache
    auto tex = m_RenderSystem->LoadTexture(path);
    if (!tex || !tex->IsValid()) {
        m_TextureCache[path] = VK_NULL_HANDLE;
        return VK_NULL_HANDLE;
    }

    VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(
        tex->GetSampler(), tex->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_TextureCache[path] = ds;
    return ds;
}

void ParallaxSystem::ClearTextureCache() {
    for (auto& [path, ds] : m_TextureCache) {
        if (ds != VK_NULL_HANDLE) ImGui_ImplVulkan_RemoveTexture(ds);
    }
    m_TextureCache.clear();
}

void ParallaxSystem::Render(f32 viewportWidth, f32 viewportHeight) {
    if (!m_World || !m_Camera) return;

    // Get camera position for parallax calculation
    Math::Vector3 camPos = m_Camera->GetPosition();

    for (auto entity : m_World->GetEntitiesWithComponent<ParallaxMachineComponent>()) {
        auto* pm = m_World->GetComponent<ParallaxMachineComponent>(entity);
        if (!pm || !pm->enabled || pm->layers.empty()) continue;

        // Sort layers by sortOrder (stable sort to preserve insertion order for equal values)
        std::vector<usize> sortedIndices(pm->layers.size());
        for (usize i = 0; i < sortedIndices.size(); ++i) sortedIndices[i] = i;
        std::stable_sort(sortedIndices.begin(), sortedIndices.end(),
            [&](usize a, usize b) { return pm->layers[a].sortOrder < pm->layers[b].sortOrder; });

        // Get the entity's transform for world positioning
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        Math::Vector3 basePos = transform ? transform->position : Math::Vector3(0.0f);

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();

        for (usize idx : sortedIndices) {
            const ParallaxLayer& layer = pm->layers[idx];
            if (!layer.visible || layer.alpha <= 0.0f) continue;

            // Parallax scroll: layers at greater distance scroll slower
            f32 safeDistance = Math::Max(layer.distance, 0.01f);
            f32 parallaxFactor = (1.0f / safeDistance) * layer.speedMultiplier * pm->globalSpeed;

            f32 scrollX = (camPos.x - pm->origin.x) * parallaxFactor + pm->autoScrollOffset.x;
            f32 scrollY = (camPos.y - pm->origin.y) * parallaxFactor + pm->autoScrollOffset.y;

            // Layer world position = base position + offset - scroll
            f32 worldX = basePos.x + layer.offset.x - scrollX;
            f32 worldY = basePos.y + layer.offset.y - scrollY;

            // Convert world position to screen position using camera projection
            // (simplified for 2D — assumes orthographic camera)
            f32 screenX = (worldX - camPos.x) * (viewportWidth / 20.0f) + viewportWidth * 0.5f;
            f32 screenY = -(worldY - camPos.y) * (viewportHeight / 12.0f) + viewportHeight * 0.5f;
            f32 screenW = layer.scale.x * (viewportWidth / 20.0f);
            f32 screenH = layer.scale.y * (viewportHeight / 12.0f);

            ImU32 color = IM_COL32(
                static_cast<u8>(layer.tint.x * 255),
                static_cast<u8>(layer.tint.y * 255),
                static_cast<u8>(layer.tint.z * 255),
                static_cast<u8>(layer.alpha * 255));

            // Try to load texture for this layer
            VkDescriptorSet texDS = GetLayerTexture(layer.texturePath);

            // Tiling: repeat the layer if repeatX is enabled
            if (layer.repeatX) {
                f32 startX = screenX - screenW * Math::Floor((screenX + screenW) / screenW);
                for (f32 tx = startX; tx < viewportWidth + screenW; tx += screenW) {
                    ImVec2 p0(tx, screenY);
                    ImVec2 p1(tx + screenW, screenY + screenH);
                    if (texDS != VK_NULL_HANDLE) {
                        drawList->AddImage((ImTextureID)texDS, p0, p1,
                            ImVec2(0, 0), ImVec2(1, 1), color);
                    } else {
                        drawList->AddRectFilled(p0, p1, color);
                    }
                }
            } else {
                ImVec2 p0(screenX, screenY);
                ImVec2 p1(screenX + screenW, screenY + screenH);
                if (texDS != VK_NULL_HANDLE) {
                    drawList->AddImage((ImTextureID)texDS, p0, p1,
                        ImVec2(0, 0), ImVec2(1, 1), color);
                } else {
                    drawList->AddRectFilled(p0, p1, color);
                }
            }
        }
    }
}

} // namespace Enjin::ECS
