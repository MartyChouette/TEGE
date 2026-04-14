#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Texture.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace Enjin {
namespace Effects {

// UV region within the atlas texture for a single packed subtexture
struct AtlasRegion {
    u32 x, y, width, height;              // Pixel rect in atlas
    f32 uvLeft, uvTop, uvRight, uvBottom;  // Normalized UVs
};

// Runtime sprite texture atlas — shelf-packs small sprite textures into a single
// 4096x4096 GPU texture so sprites with different source images share one draw call.
// Textures larger than MAX_SUBTEXTURE_SIZE or that don't fit are excluded and
// rendered via the normal per-texture path.
class ENJIN_API SpriteTextureAtlas {
public:
    static constexpr u32 ATLAS_SIZE = 4096;
    static constexpr u32 MAX_SUBTEXTURE_SIZE = 512;
    static constexpr u32 PADDING = 1;

    SpriteTextureAtlas() = default;
    ~SpriteTextureAtlas();

    bool Initialize(Renderer::VulkanContext* context);
    void Shutdown();

    // Add a texture path to the request set. Sets dirty if this is a new path.
    bool RequestTexture(const std::string& path);

    // Load pixels, shelf-pack, upload GPU texture. Call when IsDirty() is true.
    bool Build();

    // Look up the atlas region for a texture path. Returns nullptr if not in atlas.
    const AtlasRegion* GetRegion(const std::string& path) const;

    // Get the combined atlas GPU texture (for descriptor binding)
    Renderer::Texture* GetAtlasTexture() const { return m_AtlasTexture.get(); }

    bool IsValid() const { return m_AtlasTexture && m_AtlasTexture->IsValid(); }
    bool IsDirty() const { return m_Dirty; }

    // Force rebuild next frame (e.g. after texture hot-reload)
    void Invalidate();

    // Clear all state (requests, regions, GPU texture)
    void Clear();

private:
    Renderer::VulkanContext* m_Context = nullptr;
    std::shared_ptr<Renderer::Texture> m_AtlasTexture;
    std::unordered_map<std::string, AtlasRegion> m_Regions;
    std::unordered_set<std::string> m_RequestedPaths;
    std::unordered_set<std::string> m_ExcludedPaths;  // Too large or failed to load
    bool m_Dirty = true;
    bool m_Initialized = false;
};

} // namespace Effects
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
