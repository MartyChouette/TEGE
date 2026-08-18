#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
#include <vector>
#include <unordered_map>

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanImage;

// Bindless resource handle
using BindlessHandle = u32;
constexpr BindlessHandle INVALID_BINDLESS_HANDLE = UINT32_MAX;

// Bindless resource manager
// INNOVATION: Single descriptor set with all resources, access by index
// Eliminates descriptor set binding overhead
class ENJIN_API BindlessResourceManager {
public:
    BindlessResourceManager(VulkanContext* context);
    ~BindlessResourceManager();

    bool Initialize();
    void Shutdown();

    // Register texture (returns handle for shader access)
    BindlessHandle RegisterTexture(VkImageView imageView, VkSampler sampler);
    BindlessHandle RegisterTexture(VulkanImage* image, VkSampler sampler = VK_NULL_HANDLE);
    void UnregisterTexture(BindlessHandle handle);

    // Global texture sampler settings. All material textures share one sampler
    // built from this. filter: 0=Point(nearest), 1=Bilinear, 2=Trilinear.
    // wrap: 0=Repeat, 1=Clamp, 2=Mirror. anisotropy: 0=off else 2/4/8/16.
    struct SamplerConfig {
        u32 filter = 2;       // trilinear
        u32 wrap = 0;         // repeat
        u32 anisotropy = 8;   // 8x
        bool mipmaps = true;
        bool operator==(const SamplerConfig& o) const {
            return filter == o.filter && wrap == o.wrap &&
                   anisotropy == o.anisotropy && mipmaps == o.mipmaps;
        }
        bool operator!=(const SamplerConfig& o) const { return !(*this == o); }
    };
    // Recreate the shared sampler from cfg and repoint every texture that uses the
    // default sampler at it (no-op if cfg is unchanged). Safe to call each frame.
    void SetSamplerConfig(const SamplerConfig& cfg);
    const SamplerConfig& GetSamplerConfig() const { return m_SamplerConfig; }

    // Build a VkSampler for a given config (caller owns it). Used for the shared
    // default sampler and, later, per-material override samplers.
    VkSampler CreateSampler(const SamplerConfig& cfg);

    // Cached sampler for per-material filter overrides. The manager owns these and
    // destroys them at Shutdown, so callers never free them. Distinct configs are
    // few (a handful of override modes), so the cache stays tiny.
    VkSampler GetOrCreateSampler(const SamplerConfig& cfg);

    // Repoint an already-registered texture slot at a different sampler (used when a
    // material override's derived config changes). Marks the descriptor set dirty.
    void SetTextureSampler(BindlessHandle handle, VkSampler sampler);

    // Create default sampler (uses the current SamplerConfig)
    VkSampler CreateDefaultSampler();

    // Register buffer (returns handle for shader access)
    BindlessHandle RegisterBuffer(VkBuffer buffer, VkDescriptorType type);
    void UnregisterBuffer(BindlessHandle handle);

    // Update descriptor set (call before rendering or when resources change)
    void UpdateDescriptorSet();
    
    // Rebuild descriptor set (internal, called by UpdateDescriptorSet)
    void RebuildDescriptorSet();

    // Build + write the 8-slot sampler table (set 1 binding 2). Slot 0 mirrors
    // the global SamplerConfig; 1/2/3 = Point/Bilinear/Trilinear with the
    // global wrap; 4-7 reserved (global). Samplers come from the cache, so old
    // table entries stay alive across rebuilds (no in-flight destroy hazard).
    void BuildSamplerTable();
    static constexpr u32 SAMPLER_TABLE_SIZE = 8;

    // Get descriptor set for binding
    VkDescriptorSet GetDescriptorSet() const { return m_DescriptorSet; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_DescriptorSetLayout; }

    // Get bindless texture handle in shader: textures[handle]
    // Get bindless buffer handle in shader: buffers[handle]

    // Statistics (returns count of registered/valid entries, not capacity)
    u32 GetTextureCount() const { return m_TextureCount; }
    u32 GetBufferCount() const { return m_BufferCount; }

private:
    bool CreateDescriptorSetLayout();
    bool AllocateDescriptorSet();

    VulkanContext* m_Context = nullptr;

    // Descriptor set
    VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;

    // Resource tracking
    struct TextureEntry {
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        bool valid = false;
    };
    std::vector<TextureEntry> m_Textures;
    std::vector<BindlessHandle> m_FreeTextureSlots;

    // Shared sampler every material texture uses (built from m_SamplerConfig).
    SamplerConfig m_SamplerConfig;
    VkSampler m_DefaultSampler = VK_NULL_HANDLE;
    VkSampler m_SamplerTable[8] = {};

    // Cache of override samplers keyed by a packed SamplerConfig (see GetOrCreateSampler).
    std::unordered_map<u32, VkSampler> m_SamplerCache;

    struct BufferEntry {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bool valid = false;
    };
    std::vector<BufferEntry> m_Buffers;
    std::vector<BindlessHandle> m_FreeBufferSlots;

    static constexpr u32 MAX_TEXTURES = 1000000; // 1 million textures
    static constexpr u32 MAX_BUFFERS = 100000;    // 100k buffers
    u32 m_TextureCount = 0;    // Number of valid registered textures
    u32 m_BufferCount = 0;     // Number of valid registered buffers
    u32 m_TextureHighWater = 0; // Highest texture slot index + 1 (limits rebuild iteration)
    u32 m_BufferHighWater = 0;  // Highest buffer slot index + 1 (limits rebuild iteration)
    bool m_Dirty = true;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
