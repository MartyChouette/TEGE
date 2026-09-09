// The scene palettes, and the cycling that animates them.
//
// A palette-indexed material stores an INDEX in its base colour texture's red
// channel; this uploads the tables those indices point at. Rotating a run of a
// table animates every pixel using it, which is why the per-frame cost here is
// one small texture upload regardless of how much of the screen is moving.
//
// There are up to kMaxPaletteSlots tables and they are ROWS OF ONE TEXTURE, not
// one texture each. That is what makes "unlimited recolours" true rather than a
// slogan: a faction, a season, a damage state and a night version of the same
// art are four rows of 1 KB, and the art itself is stored once. A material
// picks its row with MaterialComponent::paletteSlot.
//
// Palettes are scene-level because that is how it worked when it was hardware:
// a few tables, many materials indexing into them.

#include "Enjin/ECS/Systems/RenderSystem.h"

#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/Texture.h"
#include "Enjin/Renderer/Vulkan/VulkanSampler.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/Vulkan/BindlessResources.h"
#include "Enjin/Logging/Log.h"

namespace Enjin {
namespace ECS {

namespace {
// Returned for a slot that does not exist. Static so a caller can hold the
// reference: the alternative is a throw, and every caller here is UI or draw
// code that has to keep running against an index that went stale.
const Renderer::Palette& EmptyPalette() {
    static const Renderer::Palette kEmpty;
    return kEmpty;
}
const std::vector<Renderer::PaletteCycleRange>& EmptyCycles() {
    static const std::vector<Renderer::PaletteCycleRange> kEmpty;
    return kEmpty;
}
} // namespace

const Renderer::Palette& RenderSystem::GetScenePalette(u32 slot) const {
    if (slot >= m_ScenePalettes.size()) return EmptyPalette();
    return m_ScenePalettes[slot].palette;
}

const std::vector<Renderer::PaletteCycleRange>& RenderSystem::GetPaletteCycles(u32 slot) const {
    if (slot >= m_ScenePalettes.size()) return EmptyCycles();
    return m_ScenePalettes[slot].cycles;
}

void RenderSystem::SetScenePalettes(const std::vector<Renderer::ScenePaletteSlot>& slots) {
    m_ScenePalettes = slots;
    if (m_ScenePalettes.size() > Renderer::kMaxPaletteSlots) {
        m_ScenePalettes.resize(Renderer::kMaxPaletteSlots);
    }
    // Every table is padded to full length so an index read out of a texture can
    // never run past the end, whatever the art does.
    for (auto& s : m_ScenePalettes) {
        if (s.palette.colors.size() < Renderer::kPaletteMaxColors) {
            s.palette.colors.resize(Renderer::kPaletteMaxColors);
        }
    }
    // Restart the clock so a newly loaded scene begins where it was authored
    // rather than wherever the previous scene's animation happened to be.
    m_PaletteTime = 0.0f;
}

void RenderSystem::SetScenePalette(u32 slot,
                                   const Renderer::Palette& p,
                                   const std::vector<Renderer::PaletteCycleRange>& ranges) {
    if (slot >= Renderer::kMaxPaletteSlots) return;
    if (slot >= m_ScenePalettes.size()) m_ScenePalettes.resize(slot + 1);

    Renderer::ScenePaletteSlot& s = m_ScenePalettes[slot];
    s.palette = p;
    if (s.palette.colors.size() < Renderer::kPaletteMaxColors) {
        s.palette.colors.resize(Renderer::kPaletteMaxColors);
    }
    s.cycles = ranges;
    // Deliberately NOT resetting m_PaletteTime here. Swapping one slot is the
    // runtime operation -- a faction changing colours, night falling -- and
    // restarting the clock would jump every OTHER palette back to its authored
    // frame at the same moment.
}

void RenderSystem::UpdateScenePalette() {
    if (!m_VulkanRenderer || !m_BindlessManager) return;
    if (m_ScenePalettes.empty()) return;

    // The whole texture is written every frame, including rows past the last
    // authored palette. Those rows stay black, which is what a material pointing
    // at a deleted palette would get -- but the draw path clamps the slot first,
    // so reaching them means something upstream is wrong, not that art broke.
    const usize rowBytes = static_cast<usize>(Renderer::kPaletteMaxColors) * 4;
    m_PaletteUploadScratch.assign(rowBytes * Renderer::kMaxPaletteSlots, 0);

    for (usize slot = 0; slot < m_ScenePalettes.size(); ++slot) {
        const Renderer::ScenePaletteSlot& src = m_ScenePalettes[slot];
        if (src.palette.count == 0) continue;

        // Always recomputed from the AUTHORED colours, never from the previous
        // frame's output. Cycling a cycled table compounds, and the animation
        // would drift away from the art at a rate nobody can predict.
        Renderer::ApplyPaletteCycles(src.palette, src.cycles, m_PaletteTime, m_PaletteCycled);

        u8* row = m_PaletteUploadScratch.data() + slot * rowBytes;
        for (u32 i = 0; i < Renderer::kPaletteMaxColors; ++i) {
            const Renderer::PaletteColor& c = m_PaletteCycled.colors[i];
            row[i * 4 + 0] = c.r;
            row[i * 4 + 1] = c.g;
            row[i * 4 + 2] = c.b;
            row[i * 4 + 3] = c.a;
        }
    }

    if (!m_PaletteTexture) {
        m_PaletteTexture = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
        // UNORM, not SRGB: these are authored colours going straight to albedo,
        // and a second gamma pass would wash every palette out.
        //
        // Nearest, and no mip chain. Both say the same thing: entry 7 and entry
        // 8 are unrelated colours, so anything that averages them invents a
        // colour the artist never chose. Filtering would do it between entries,
        // a mip would do it across the whole table, and with palettes stacked as
        // rows either one would also bleed one palette into the next. No mips is
        // additionally what makes the image eligible for UpdateFromData, which
        // is how the cycling reaches the GPU every frame.
        if (!m_PaletteTexture->CreateFromData(m_PaletteUploadScratch.data(),
                                              Renderer::kPaletteMaxColors,
                                              Renderer::kMaxPaletteSlots, 4,
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
        ENJIN_LOG_INFO(Renderer, "Scene palettes: %zu slot(s), bindless %u",
                       m_ScenePalettes.size(), m_PaletteBindless);
        return;   // freshly created with this frame's contents already in it
    }

    // Re-upload in place. The image keeps its view, sampler and bindless slot,
    // so nothing that referenced a palette last frame is invalidated -- which is
    // what makes this safe to do every frame.
    if (!m_PaletteTexture->UpdateFromData(m_PaletteUploadScratch.data(),
                                          Renderer::kPaletteMaxColors,
                                          Renderer::kMaxPaletteSlots, 4)) {
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
