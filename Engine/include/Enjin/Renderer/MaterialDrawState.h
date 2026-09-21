#pragma once

#include "Enjin/Renderer/MaterialFlagWord.h"
#include "Enjin/ECS/Components/ArtStyle.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/Platform/Platform.h"

#include <cmath>
#include <cstring>

// The per-entity material draw state, in ONE place.
//
// A draw needs two things from its material that the shader reads as one unit: the flag
// word, and the three surfaceParam floats every art mode shares as a numeric band. They
// have to be built together, because the bands are a CASCADE -- palette overrides surface
// noise overrides elemental -- and because an ArtStyleComponent override writes into both.
// Splitting them is how the two got out of step.
//
// THREE near-copies of this cascade lived inside RenderSystem's three push-constant
// builders, and they had already drifted (measured 2026-09-21, 26 / 23 / 20 writes):
//
//   RenderToTarget      the editor viewport and offscreen path. Had everything.
//   RenderSplitscreen   missing the ArtStyle cel rim and MaterialExpression noise.
//   RenderEntity        missing both of those AND the Water3D foam block -- and this is
//                       the MAIN PASS, which is editor play mode and every exported game.
//
// So a CelToon rim strength and a MaterialExpression surface noise did nothing outside
// the editor viewport, which is the same shape as the specialization-variant bug found the
// same day. Neither is exercised by any example project, so no capture can prove they are
// fixed -- only that unifying them changed nothing that WAS exercised. That is the honest
// limit of the evidence here.
//
// WATER IS NOT IN HERE, deliberately. Water3D and WaterVolume write the same three floats,
// but they are describing a SURFACE SIMULATION rather than a material mode, they override
// whatever the cascade produced, and they carry flag bits of their own. They stay at the
// call site, applied after this, where the comment about overriding is next to the code
// that overrides.

namespace Enjin {
namespace Renderer {

struct MaterialDrawState {
    i32 flags = 0;
    f32 surfaceParam1 = 0.0f;
    f32 surfaceParam2 = 0.0f;
    f32 surfaceParam3 = 0.0f;
};

// Pack a 0-1 RGB triple into the low 30 bits of a float's bit pattern, which is how
// dithered transparency smuggles a blend colour through a float parameter.
inline f32 PackDitherBlendColor(const Math::Vector3& c) {
    const u32 r = static_cast<u32>(c.x * 1023.0f) & 0x3FF;
    const u32 g = static_cast<u32>(c.y * 1023.0f) & 0x3FF;
    const u32 b = static_cast<u32>(c.z * 1023.0f) & 0x3FF;
    const u32 packed = (r << 20) | (g << 10) | b;
    f32 out = 0.0f;
    std::memcpy(&out, &packed, sizeof(out));
    return out;
}

// `paletteBand` is the caller's RenderSystem::PaletteBandFor(m.paletteSlot). It is a
// parameter rather than computed here because it clamps against the palettes the SCENE
// actually has -- a material can outlive the table it pointed at -- and that count is
// renderer state, not material state. Ignored unless the material is palette-indexed.
//
// `elemental` and `artStyle` are optional; pass nullptr when the entity has neither.
// `globalVertexSnapResolution` is the m_Global* override, 0 when it is not forced.
inline MaterialDrawState BuildMaterialDrawState(
    const ECS::MaterialComponent& m,
    const MaterialTextureBindings& tex,
    const MaterialFlagOverrides& global,
    f32 paletteBand,
    const ECS::ElementalSurfaceComponent* elemental = nullptr,
    const ECS::ArtStyleComponent* artStyle = nullptr,
    u32 globalVertexSnapResolution = 0)
{
    MaterialDrawState s;
    s.flags = BuildMaterialFlagWord(m, tex, global);

    // Artistic surface params, the below-100 meaning of the three slots.
    s.surfaceParam1 = m.reflectivity;
    s.surfaceParam2 = m.fresnelPower;
    s.surfaceParam3 = m.rimLightStrength;

    // The BANDS, in the order they have always been applied. Each one that claims
    // surfaceParam1 excludes the ones after it that test for a free slot, so the order is
    // the meaning -- see Material.h for the range registry (700 is the next free band).
    if (m.ditherGradient) {
        s.flags |= (1 << 20);   // dithering needs flat shading
        s.surfaceParam1 = 100.0f + static_cast<f32>(m.ditherGradientBands)
                        + static_cast<f32>(m.ditherGradientPattern) * 0.1f;
    }
    if (m.ditherTransparency) {
        s.surfaceParam1 = 200.0f + static_cast<f32>(m.ditherTransPattern);
        s.surfaceParam2 = m.ditherTransOpacity;
        s.surfaceParam3 = PackDitherBlendColor(m.ditherTransBlendColor);
    }
    if (!m.ditherGradient && !m.ditherTransparency && elemental) {
        if (elemental->charAmount > 0.01f || elemental->wetness > 0.01f ||
            elemental->snowCoverage > 0.01f || elemental->frostAmount > 0.01f) {
            s.surfaceParam1 = 300.0f + elemental->charAmount;
            s.surfaceParam2 = elemental->wetness + std::floor(elemental->snowCoverage * 256.0f);
            s.surfaceParam3 = elemental->frostAmount;
        }
    }
    if (!m.ditherGradient && !m.ditherTransparency &&
        m.surfaceNoiseScale > 0.0f && s.surfaceParam1 < 100.0f) {
        s.surfaceParam1 = 400.0f + m.surfaceNoiseScale;
        s.surfaceParam2 = m.surfaceNoiseStrength;
    }
    if (m.paletteIndexed) {
        s.surfaceParam1 = paletteBand;
    }
    if (m.lightmapped) {
        // After the palette so it wins the slot when both are set. They cannot coexist:
        // the flags word has no free bits, so every mode here shares one float.
        s.surfaceParam1 = ECS::MaterialGPU::SURFACE_PARAM1_LIGHTMAPPED;
    }

    // Global retro override, applied over whatever the material asked for.
    if (global.vertexSnapping && globalVertexSnapResolution > 0) {
        s.flags = (s.flags & ~(0x1F << 24))
                | (static_cast<i32>((globalVertexSnapResolution / 8) & 0x1F) << 24);
    }

    // Per-entity art style, last, so it overrides the material.
    if (artStyle && artStyle->style != ECS::ArtStyleType::Inherit) {
        switch (artStyle->style) {
        case ECS::ArtStyleType::PrePBR:
            if (artStyle->prePBR_flatShading) s.flags |= (1 << 20);
            if (artStyle->prePBR_gouraudOnly) s.flags |= (1 << 13);
            break;
        case ECS::ArtStyleType::CelToon:
            // Rim strength only; the outline is the outline pass's job.
            s.surfaceParam3 = artStyle->cel_rimStrength;
            break;
        case ECS::ArtStyleType::Retro:
            if (artStyle->retro_flatShading)    s.flags |= (1 << 20);
            if (artStyle->retro_affineTexturing) s.flags |= (1 << 21);
            if (artStyle->retro_vertexSnapping)  s.flags |= (1 << 22);
            if (artStyle->retro_uvQuantize)      s.flags |= (1 << 12);
            if (artStyle->retro_vertexSnapping && artStyle->retro_snapResolution > 0) {
                s.flags = (s.flags & ~(0x1F << 24))
                        | (static_cast<i32>((artStyle->retro_snapResolution / 8) & 0x1F) << 24);
            }
            break;
        case ECS::ArtStyleType::MaterialExpression:
            // Only into a free slot: a band already claimed is a mode already chosen.
            if (artStyle->matExpr_surfaceNoiseScale > 0.0f && s.surfaceParam1 < 100.0f) {
                s.surfaceParam1 = 400.0f + artStyle->matExpr_surfaceNoiseScale;
                s.surfaceParam2 = artStyle->matExpr_surfaceNoiseStrength;
            }
            break;
        case ECS::ArtStyleType::HandPainted:
        case ECS::ArtStyleType::NPR:
        case ECS::ArtStyleType::PixelArt:
        case ECS::ArtStyleType::Analog:
        case ECS::ArtStyleType::Inherit:
        case ECS::ArtStyleType::Count:
            // Scene-wide or nothing to do per entity. A full-screen pass cannot grain one
            // object, so those styles are applied from the camera's component instead.
            break;
        }
    }

    return s;
}

} // namespace Renderer
} // namespace Enjin
