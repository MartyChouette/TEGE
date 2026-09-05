#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vector>

namespace Enjin {
namespace ECS {

// ---------------------------------------------------------------------------
// ProceduralTextureComponent
// ---------------------------------------------------------------------------
// One route for CPU-generated pixels onto an entity's material, the texture
// counterpart of ProceduralMeshComponent. A system writes RGBA8 into `pixels`
// and raises `dirty`; the renderer uploads it and binds it as the entity's base
// colour, overriding whatever texture path the material holds.
//
// Before this existed there was no way to get generated pixels onto a surface
// at all. The FR-4 script render targets alias a RenderTarget's colour view,
// which is GPU-to-GPU, and the text path builds its texture inline at the draw
// site with a hardcoded TextComponent check. Reaction-diffusion and Physarum
// both bake straight to RGBA8 and had nowhere to send it.
//
// Uploads follow the text-texture protocol exactly: a replacement texture frees
// the old bindless slot and parks the old texture in the graveyard rather than
// destroying it, because in-flight frames still reference it through bound
// descriptor sets. Destroying it immediately is the mid-frame GPU crash class.
//
// `pixels` is runtime data and is NOT serialized: it is an output. The owning
// system regenerates it from the authored simulation parameters on load.
struct ENJIN_API ProceduralTextureComponent {
    enum class Source : u8 {
        Unknown = 0,
        ReactionDiffusion,
        Physarum,
        Script,
        Count
    };

    Source source = Source::Unknown;

    std::vector<u8> pixels;   // RGBA8, width * height * 4
    u32 width = 0;
    u32 height = 0;

    // Raised by the producing system when `pixels` changed. Cleared by the
    // renderer after upload. Nothing else should clear it.
    bool dirty = false;

    // Set false to keep the current image and stop the owning system
    // regenerating it, the same freeze switch ProceduralMeshComponent has.
    bool regenerate = true;

    bool Valid() const {
        return width > 0 && height > 0 &&
               pixels.size() == static_cast<usize>(width) * height * 4u;
    }
};

inline const char* ProceduralTextureSourceName(ProceduralTextureComponent::Source s) {
    switch (s) {
        case ProceduralTextureComponent::Source::ReactionDiffusion: return "Reaction-Diffusion";
        case ProceduralTextureComponent::Source::Physarum:          return "Physarum";
        case ProceduralTextureComponent::Source::Script:            return "Script";
        default:                                                    return "Unknown";
    }
}

} // namespace ECS
} // namespace Enjin
