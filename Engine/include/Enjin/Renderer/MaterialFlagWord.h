#pragma once

#include "Enjin/Renderer/PipelineVariantCache.h"
#include "Enjin/Platform/Platform.h"
#include "Enjin/ECS/Components/Material.h"

// The material flag word, in ONE place.
//
// SEVEN sites in RenderSystem.cpp build this word by hand: RenderToTarget and
// its per-slot path, the splitscreen builder, RenderEntity, the GPU-driven
// indirect path, BuildSlotPushConstants and FillArenaMaterial. Every new
// material mode is therefore a seven-place edit, and nothing in the language
// makes the seven agree.
//
// Measured 2026-09-18 before writing this: they DO all agree today. This is not
// repairing drift, it is removing the room for it -- which is why the function
// arrived before the call sites were migrated. Migrating them is a change with
// no intended behaviour difference, in the hottest path in the engine, and the
// test suite cannot see a wrong flag word because tests do not render. That
// wants a golden capture either side, so it is staged rather than rushed.
//
// Two artefacts of the hand-written versions worth not reproducing: one site
// contains the sdfText line TWICE (harmless, OR is idempotent) and the bit
// ordering differs between sites for no reason. Both are what seven copies do.
//
// BIT MAP (Vulkan; the web word agrees on everything it implements):
//   0  double sided        1  casts shadows        2  receives shadows
//   3  SDF text           10  has height texture   12  UV quantise
//  13  gouraud only       16  has base colour tex  17  has normal tex
//  18  has metallic/rough 19  has emissive tex     20  flat shading
//  21  affine texturing   22  vertex snapping      23  stipple transparency
//   8-9   alpha mode            14-15 shadow dither mode
//  24-28  vertex snap resolution / 8   29-31 shadow dither pattern

namespace Enjin {
namespace Renderer {

// Which texture slots are actually bound for this draw. Kept separate from the
// material because a material can NAME a texture that failed to load, and the
// flag has to describe what the shader will really find.
struct MaterialTextureBindings {
    bool baseColor = false;
    bool normal = false;
    bool metallicRoughness = false;
    bool emissive = false;
    bool height = false;
};

// Global overrides (the m_Global* art-style switches). Applied ON TOP, matching
// every hand-written site: a global forces the mode on, it never turns one off.
struct MaterialFlagOverrides {
    bool flatShading = false;
    bool affineTexturing = false;
    bool vertexSnapping = false;
    bool uvQuantize = false;
    bool gouraudOnly = false;
    // BuildSlotPushConstants forces this one globally too. It was missing here
    // while the struct's comment claimed to match "every hand-written site",
    // which is what a struct written from five of six sites looks like.
    bool stippleTransparency = false;
};

inline i32 BuildMaterialFlagWord(const ECS::MaterialComponent& m,
                                 const MaterialTextureBindings& tex = {},
                                 const MaterialFlagOverrides& global = {}) {
    i32 f = 0;

    if (m.doubleSided)    f |= 1;
    if (m.castShadows)    f |= 2;
    if (m.receiveShadows) f |= 4;
    // NOT bit 3: that is FLAG_SKINNED in the vertex shader, and the two shared
    // one word. sdfText is a specialization constant now (adr-0008 phase 2).

    // Texture presence is what is BOUND, not what is named.
    if (tex.baseColor)         f |= (1 << 16);
    if (tex.normal)            f |= (1 << 17);
    if (tex.metallicRoughness) f |= (1 << 18);
    if (tex.emissive)          f |= (1 << 19);
    if (tex.height)            f |= (1 << 10);

    // Retro / art-style modes, per material then globally forced.
    if (m.flatShading         || global.flatShading)      f |= (1 << 20);
    if (m.affineTexturing     || global.affineTexturing)  f |= (1 << 21);
    if (m.vertexSnapping      || global.vertexSnapping)   f |= (1 << 22);
    if (m.uvQuantize          || global.uvQuantize)       f |= (1 << 12);
    if (m.gouraudOnly         || global.gouraudOnly)      f |= (1 << 13);
    if (m.stippleTransparency || global.stippleTransparency) f |= (1 << 23);

    // Packed fields. Masked at the width the shader reads, so an out-of-range
    // authored value cannot bleed into the neighbouring bits -- which is the
    // failure mode a hand-written site is one missing mask away from.
    // alphaMode is NOT packed here any more (adr-0008 phase 4). It is
    // material-static, so it is SPEC_ALPHA_MODE, and bits 8-9 of the
    // push-constant word are free.
    //
    // MaterialGPU still carries it, and that is not drift: rt_shadow.rchit and
    // rt_ao.rchit read `materials[gl_InstanceCustomIndexEXT].flags >> 8`. A
    // ray-tracing hit shader indexes materials from a buffer and cannot be
    // specialized per material, so material-static data still needs a BUFFER
    // representation for consumers that have no pipeline of their own.
    f |= (static_cast<i32>(m.shadowDitherMode) & 0x3) << 14;
    f |= (static_cast<i32>(m.vertexSnapResolution / 8) & 0x1F) << 24;
    f |= (static_cast<i32>(m.shadowDitherPattern) & 0x7) << 29;

    return f;
}

// The specialization key for the SAME material, from the SAME inputs.
//
// Deliberately next to BuildMaterialFlagWord and taking identical arguments.
// The shader gates every specialized feature as
// `SPEC_X != 0 && (mat_flags & FLAG_X) != 0`, so the key and the flag word have
// to agree about what X is: a key that says "no base colour texture" while the
// flag word says there is one silently drops the texture, and the two being
// built from one call site with one set of inputs is what prevents that.
//
// adr-0008: this is the STATIC half. Anything here is baked into a pipeline
// variant and must not change without the material changing. Per-draw state
// (skinning, wind sway, water) stays in the flag word and never appears here.
inline MaterialSpecKey BuildMaterialSpecKey(const ECS::MaterialComponent& m,
                                            const MaterialTextureBindings& tex = {},
                                            const MaterialFlagOverrides& global = {}) {
    MaterialSpecKey key{};
    if (tex.baseColor)         key.bits |= MaterialSpecKey::BASE_COLOR_TEX;
    if (tex.normal)            key.bits |= MaterialSpecKey::NORMAL_TEX;
    if (tex.metallicRoughness) key.bits |= MaterialSpecKey::METALLIC_TEX;
    if (tex.emissive)          key.bits |= MaterialSpecKey::EMISSIVE_TEX;
    if (tex.height)            key.bits |= MaterialSpecKey::HEIGHT_TEX;
    if (m.doubleSided)         key.bits |= MaterialSpecKey::DOUBLE_SIDED;
    if (m.flatShading || global.flatShading) key.bits |= MaterialSpecKey::FLAT_SHADING;
    key.SetAlphaMode(static_cast<u32>(m.alphaMode) & 0x3u);
    // adr-0008 phase 2: material-static, and formerly colliding with the
    // vertex shader's SKINNED and WIND_SWAY bits.
    if (m.sdfText)                 key.bits |= MaterialSpecKey::SDF_TEXT;
    if (m.excludeFromCelShading)   key.bits |= MaterialSpecKey::EXCLUDE_CEL;
    return key;
}

} // namespace Renderer
} // namespace Enjin
