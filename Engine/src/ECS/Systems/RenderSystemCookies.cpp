// Light cookie (gobo) textures.
//
// A cookie is authored as a RECIPE (Renderer::CookieParams) rather than an
// image, so the texture is generated here and cached per light entity. That is
// what lets a cookie stay editable after a save and keeps scene files free of
// baked pixels; a baked path is still honoured when one is set, which is how a
// hand-painted gobo gets in without the generator having to be able to draw it.
//
// Building one uploads a texture and registers a bindless slot, so it runs from
// FlushPendingChanges. The per-frame UBO fill only looks the result up: it must
// never allocate, because it runs with a command buffer open.

#include "Enjin/ECS/Systems/RenderSystem.h"

#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/Renderer/LightCookie.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/BindlessResources.h"
#include "Enjin/Logging/Log.h"

#include <vector>

namespace Enjin {
namespace ECS {

namespace {

// Whether two recipes describe the same image, so an unchanged light is not
// rebuilt every time FlushPendingChanges runs. Field by field rather than
// memcmp, because the struct carries a std::string.
bool SameCookie(const Renderer::CookieParams& a, const Renderer::CookieParams& b) {
    return a.pattern == b.pattern && a.resolution == b.resolution &&
           a.columns == b.columns && a.rows == b.rows &&
           a.barWidth == b.barWidth && a.softness == b.softness &&
           a.rotation == b.rotation && a.contrast == b.contrast &&
           a.brightness == b.brightness && a.vignette == b.vignette &&
           a.invert == b.invert && a.seed == b.seed &&
           a.sourcePath == b.sourcePath;
}

} // namespace

void RenderSystem::ClearLightCookies() {
    // Dropping the cache is what stops a recreated entity inheriting the
    // previous scene's cookie: entity slots are recycled, so the key alone is
    // not enough to tell them apart.
    m_LightCookies.clear();
}

u32 RenderSystem::ResolveLightCookie(Entity e, const LightComponent& light) const {
    if (!light.cookieEnabled) return UINT32_MAX;
    const auto it = m_LightCookies.find(EntityIndex(e));
    if (it == m_LightCookies.end()) return UINT32_MAX;
    // An entry built from different params is stale for exactly one frame, which
    // is better than projecting a cookie the light no longer has.
    if (it->second.path != light.cookieTexturePath) return UINT32_MAX;
    if (light.cookieTexturePath.empty() && !SameCookie(it->second.params, light.cookie)) {
        return UINT32_MAX;
    }
    return it->second.bindless;
}

void RenderSystem::UpdateLightCookies() {
    if (!m_World || !m_VulkanRenderer || !m_BindlessManager) return;

    for (Entity e : m_World->GetEntitiesWithComponent<LightComponent>()) {
        const LightComponent* lightPtr = m_World->GetComponent<LightComponent>(e);
        if (!lightPtr) continue;
        const LightComponent& light = *lightPtr;
        if (!light.cookieEnabled) continue;
        // Only a spot light projects through a cone, so only it can carry a
        // gobo. Building a texture for the others would be work nothing samples.
        if (light.type != LightType::Spot) continue;

        const u32 key = EntityIndex(e);
        auto it = m_LightCookies.find(key);
        const bool fresh =
            it != m_LightCookies.end() &&
            it->second.bindless != UINT32_MAX &&
            it->second.path == light.cookieTexturePath &&
            (!light.cookieTexturePath.empty() || SameCookie(it->second.params, light.cookie));
        if (fresh) continue;

        Renderer::CookieParams params = light.cookie;
        Renderer::ClampCookieParams(params);

        auto tex = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
        bool built = false;

        if (!light.cookieTexturePath.empty()) {
            built = tex->LoadFromFile(light.cookieTexturePath);
            if (!built) {
                ENJIN_LOG_WARN(Renderer, "Light cookie image '%s' could not be loaded; "
                               "falling back to the generated pattern",
                               light.cookieTexturePath.c_str());
            }
        }

        if (!built) {
            std::vector<u8> grey;
            Renderer::GenerateCookie(params, grey);

            // Expanded to RGBA rather than uploaded as R8: the bindless set is a
            // single sampled-image array shared with material textures, so one
            // odd format here would need a second array. At 256 square this is
            // 256 KB, which is not worth a format split.
            std::vector<u8> rgba(grey.size() * 4);
            for (usize i = 0; i < grey.size(); ++i) {
                const u8 v = grey[i];
                rgba[i * 4 + 0] = v;
                rgba[i * 4 + 1] = v;
                rgba[i * 4 + 2] = v;
                rgba[i * 4 + 3] = 255;
            }
            // UNORM, not SRGB: a cookie is a mask, not a colour. Through an SRGB
            // view the midtones would be lifted and the pattern would read
            // washed out against the light it is meant to shape.
            built = tex->CreateFromData(rgba.data(), params.resolution, params.resolution, 4,
                                        VK_FORMAT_R8G8B8A8_UNORM);
        }

        if (!built || !tex->IsValid()) {
            ENJIN_LOG_ERROR(Renderer, "Light cookie texture failed to build for entity %u", key);
            continue;
        }

        const u32 slot = m_BindlessManager->RegisterTexture(tex->GetImageView(), tex->GetSampler());
        if (slot == UINT32_MAX) {
            ENJIN_LOG_ERROR(Renderer, "Light cookie got no bindless slot for entity %u", key);
            continue;
        }

        LightCookieEntry entry;
        entry.params = params;
        entry.path = light.cookieTexturePath;
        entry.bindless = slot;
        entry.texture = tex;
        m_LightCookies[key] = std::move(entry);

        ENJIN_LOG_INFO(Renderer, "Light cookie built for entity %u: %s %ux%u (bindless %u)",
                       key, Renderer::CookiePatternName(params.pattern),
                       params.resolution, params.resolution, slot);
    }
}

} // namespace ECS
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
