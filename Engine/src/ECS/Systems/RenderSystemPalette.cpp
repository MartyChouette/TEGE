// The scene palette, and the cycling that animates it.
//
// A palette-indexed material stores an INDEX in its base colour texture's red
// channel; this uploads the table those indices point at. Rotating a run of that
// table animates every pixel using it, which is why the per-frame cost here is a
// 1 KB texture upload regardless of how much of the screen is moving.
//
// The palette is scene-level because that is how it worked when it was hardware:
// one table, many materials indexing into it. Materials opt in individually
// through MaterialComponent::paletteIndexed.

#include "Enjin/ECS/Systems/RenderSystem.h"

#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Texture.h"
#include "Enjin/Renderer/Vulkan/VulkanSampler.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/BindlessResources.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

void RenderSystem::SetScenePalette(const Renderer::Palette& p,
                                   const std::vector<Renderer::PaletteCycleRange>& ranges) {
    m_ScenePalette = p;
    if (m_ScenePalette.colors.size() < Renderer::kPaletteMaxColors) {
        m_ScenePalette.colors.resize(Renderer::kPaletteMaxColors);
    }
    m_PaletteCycles = ranges;
    // Restart the clock so a newly set palette begins where it was authored
    // rather than wherever the previous scene's animation happened to be.
    m_PaletteTime = 0.0f;
    m_PaletteCycled = m_ScenePalette;
}

void RenderSystem::UpdateScenePalette() {
    if (!m_VulkanRenderer || !m_BindlessManager) return;
    if (m_ScenePalette.count == 0) return;

    // Always recomputed from the AUTHORED colours, never from the previous
    // frame's output. Cycling a cycled table compounds, and the animation would
    // drift away from the art at a rate nobody can predict.
    Renderer::ApplyPaletteCycles(m_ScenePalette, m_PaletteCycles, m_PaletteTime, m_PaletteCycled);

    // 256x1 RGBA8. One row, so a shader lookup is a single tap at v = 0.5.
    m_PaletteUploadScratch.resize(Renderer::kPaletteMaxColors * 4);
    for (u32 i = 0; i < Renderer::kPaletteMaxColors; ++i) {
        const Renderer::PaletteColor& c = m_PaletteCycled.colors[i];
        m_PaletteUploadScratch[i * 4 + 0] = c.r;
        m_PaletteUploadScratch[i * 4 + 1] = c.g;
        m_PaletteUploadScratch[i * 4 + 2] = c.b;
        m_PaletteUploadScratch[i * 4 + 3] = c.a;
    }

    if (!m_PaletteTexture) {
        m_PaletteTexture = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
        // UNORM, not SRGB: these are authored colours going straight to albedo,
        // and a second gamma pass would wash the whole palette out.
        //
        // Nearest, and no mip chain. Both say the same thing: entry 7 and entry
        // 8 are unrelated colours, so anything that averages them invents a
        // colour the artist never chose. Filtering would do it between entries
        // and a mip would do it across the whole table. No mips is also what
        // makes the image eligible for UpdateFromData, which is how the cycling
        // gets to the GPU every frame.
        if (!m_PaletteTexture->CreateFromData(m_PaletteUploadScratch.data(),
                                              Renderer::kPaletteMaxColors, 1, 4,
                                              VK_FORMAT_R8G8B8A8_UNORM,
                                              Renderer::VulkanSampler::Nearest(),
                                              /*generateMips=*/false)) {
            ENJIN_LOG_ERROR(Renderer, "Scene palette texture could not be created");
            m_PaletteTexture.reset();
            return;
        }
        m_PaletteBindless = m_BindlessManager->RegisterTexture(m_PaletteTexture->GetImageView(),
                                                               m_PaletteTexture->GetSampler());
        if (m_PaletteBindless == UINT32_MAX) {
            ENJIN_LOG_ERROR(Renderer, "Scene palette got no bindless slot");
            m_PaletteTexture.reset();
            return;
        }
        ENJIN_LOG_INFO(Renderer, "Scene palette '%s': %u colours, %zu cycling run(s), bindless %u",
                       m_ScenePalette.name.c_str(), m_ScenePalette.count,
                       m_PaletteCycles.size(), m_PaletteBindless);
        return;   // freshly created with this frame's contents already in it
    }

    // Re-upload in place. The image keeps its view, sampler and bindless slot,
    // so nothing that referenced the palette last frame is invalidated -- which
    // is what makes this safe to do every frame.
    if (!m_PaletteTexture->UpdateFromData(m_PaletteUploadScratch.data(),
                                          Renderer::kPaletteMaxColors, 1, 4)) {
        // Loud once rather than a palette that silently stops animating: a
        // static palette looks exactly like the feature being switched off,
        // which is the failure this cost a session to find the first time.
        static bool s_Warned = false;
        if (!s_Warned) {
            s_Warned = true;
            ENJIN_LOG_ERROR(Renderer, "Scene palette upload failed; cycling will not animate");
        }
    }
}

} // namespace ECS
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
