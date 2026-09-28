#include "Enjin/ECS/Systems/ParallaxSystem.h"
#include "Enjin/ECS/Components/ParallaxMachine.h"
#include "Enjin/ECS/Components/ParallaxLayer.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/Math/Math.h"
#include <algorithm>
#include <imgui.h>
#if !ENJIN_RENDERER_WEBGPU
#include <backends/imgui_impl_vulkan.h>
#endif

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

    // Per-sprite parallax layers (see ApplyParallaxLayers).
    ApplyParallaxLayers(m_World, deltaTime);
}

u32 ParallaxSystem::ConvertMachinesToLayers(World* world) {
    if (!world) return 0;
    const std::vector<Entity> machines = world->GetEntitiesWithComponent<ParallaxMachineComponent>();
    u32 made = 0;
    for (Entity e : machines) {
        const auto* pmPtr = world->GetComponent<ParallaxMachineComponent>(e);
        if (!pmPtr) continue;
        const ParallaxMachineComponent pm = *pmPtr;   // adding entities may move storage
        Math::Vector3 base(0.0f);
        if (const auto* t = world->GetComponent<TransformComponent>(e)) base = t->position;
        std::string baseName = "Parallax";
        if (const auto* n = world->GetComponent<NameComponent>(e)) baseName = n->name;

        for (usize i = 0; i < pm.layers.size(); ++i) {
            const ParallaxLayer& layer = pm.layers[i];
            const Entity le = world->CreateEntity();
            world->AddComponent<NameComponent>(le, NameComponent{baseName + " Layer " + std::to_string(i + 1)});
            TransformComponent xf;
            xf.position = Math::Vector3(base.x + layer.offset.x, base.y + layer.offset.y, base.z);
            world->AddComponent<TransformComponent>(le, xf);

            Sprite2DComponent sprite;
            sprite.texturePath = layer.texturePath;
            sprite.size = layer.scale;
            sprite.tint = layer.tint;
            sprite.alpha = layer.alpha;
            sprite.visible = layer.visible && pm.enabled;
            // Behind the scene's own sprites, in the machine's layer order
            sprite.sortingLayer = -100;
            sprite.orderInLayer = layer.sortOrder;
            world->AddComponent<Sprite2DComponent>(le, sprite);

            // The machine scrolled a layer at 1/distance of the camera, times
            // its speed and the machine's; in ParallaxLayer terms that is the
            // fraction of world motion, so distance 1 moves with the world and
            // distance 10 at a tenth. Its auto-scroll offset was subtracted
            // from the layer's position, hence the sign.
            const f32 safeDistance = std::max(layer.distance, 0.01f);
            const f32 factor = std::clamp((1.0f / safeDistance) * layer.speedMultiplier * pm.globalSpeed,
                                          0.0f, 1.0f);
            ParallaxLayerComponent pl;
            pl.factor = Math::Vector2(factor, factor);
            pl.autoScroll = Math::Vector2(-pm.autoScrollSpeed.x, -pm.autoScrollSpeed.y);
            world->AddComponent<ParallaxLayerComponent>(le, pl);
            ++made;
        }
        world->RemoveComponent<ParallaxMachineComponent>(e);
    }
    return made;
}

void ParallaxSystem::ApplyParallaxLayers(World* world, f32 deltaTime) {
    if (!world) return;

    // The view is authored by the active camera entity's transform, so read the
    // parallax reference from there (not the render camera, whose sync timing
    // differs between editor and player). No active camera = nothing to do.
    ECS::Entity camEnt = ECS::CameraManager::GetActiveCamera(world);
    if (camEnt == ECS::INVALID_ENTITY) return;
    auto* camT = world->GetComponent<TransformComponent>(camEnt);
    if (!camT) return;

    // Offset each layer's transform by a fraction of the camera's movement from a
    // captured anchor. This rides the ordinary 2D sprite pipeline (the layers ARE
    // sprites), so it batches, sorts, and renders on every backend with no
    // dedicated draw path. Play-mode stop restores the authored positions, so
    // mutating the transform here is safe.
    Math::Vector3 camPos = camT->position;
    for (auto entity : world->GetEntitiesWithComponent<ParallaxLayerComponent>()) {
        auto* pl = world->GetComponent<ParallaxLayerComponent>(entity);
        auto* t = world->GetComponent<TransformComponent>(entity);
        if (!pl || !t) continue;

        if (!pl->anchorCaptured) {
            pl->anchor = t->position;   // the authored position is the anchor
            pl->camStart = camPos;      // reference so there is no start jump
            pl->anchorCaptured = true;
            pl->elapsed = 0.0f;
        }
        pl->elapsed += deltaTime;

        // worldPos = anchor + camMovement * (1 - factor) + autoScroll * time.
        // Screen position then moves at `factor` rate: factor 0 stays locked to
        // the view (far backdrop), factor 1 scrolls fully with the world.
        Math::Vector3 camDelta = camPos - pl->camStart;
        t->position.x = pl->anchor.x + camDelta.x * (1.0f - pl->factor.x) + pl->autoScroll.x * pl->elapsed;
        t->position.y = pl->anchor.y + camDelta.y * (1.0f - pl->factor.y) + pl->autoScroll.y * pl->elapsed;
        // z is left alone — it carries the sprite's sorting-layer depth.
    }
}

#if !ENJIN_RENDERER_WEBGPU
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
#endif // !ENJIN_RENDERER_WEBGPU

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
#if !ENJIN_RENDERER_WEBGPU
            VkDescriptorSet texDS = GetLayerTexture(layer.texturePath);
            bool hasTexture = (texDS != VK_NULL_HANDLE);
#else
            bool hasTexture = false;
            ImTextureID texDS = 0;
#endif

            // Tiling: repeat the layer if repeatX is enabled
            if (layer.repeatX) {
                f32 startX = screenX - screenW * Math::Floor((screenX + screenW) / screenW);
                for (f32 tx = startX; tx < viewportWidth + screenW; tx += screenW) {
                    ImVec2 p0(tx, screenY);
                    ImVec2 p1(tx + screenW, screenY + screenH);
                    if (hasTexture) {
                        drawList->AddImage((ImTextureID)texDS, p0, p1,
                            ImVec2(0, 0), ImVec2(1, 1), color);
                    } else {
                        drawList->AddRectFilled(p0, p1, color);
                    }
                }
            } else {
                ImVec2 p0(screenX, screenY);
                ImVec2 p1(screenX + screenW, screenY + screenH);
                if (hasTexture) {
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
