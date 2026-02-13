#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"
#include <vulkan/vulkan.h>

namespace Enjin {
namespace Renderer {

// Forward declarations
class VulkanContext;

// OIT weight function for Weighted Blended OIT (McGuire & Bavoil 2013)
enum class OITWeightFunction : u8 {
    DepthBased,     // Weight by depth (closer = higher weight)
    AlphaBased,     // Weight by alpha value
    Combined,       // Depth * alpha combined weighting
    Count
};

// Configuration for Order-Independent Transparency
struct OITConfig {
    bool enabled = false;
    OITWeightFunction weightFunction = OITWeightFunction::DepthBased;
    f32 depthRange = 100.0f;  // Max depth for weight normalization
};

// Weighted Blended Order-Independent Transparency manager
// Uses accumulation + revealage textures (two-pass approach):
//   Pass 1: Render transparent objects → accumulation (RGBA16F) + revealage (R8)
//   Pass 2: Composite over opaque scene using fullscreen quad
class OITManager {
public:
    OITManager() = default;
    ~OITManager();

    // Lifecycle
    bool Initialize(VulkanContext* context, u32 width, u32 height);
    void Shutdown();

    // Resize handling (recreates textures)
    void Resize(u32 width, u32 height);

    // Rendering interface
    // BeginTransparentPass: Bind accumulation + revealage as render targets, clear them
    // Transparent geometry renders with additive blending (accumulation) and zero-multiply (revealage)
    void BeginTransparentPass(VkCommandBuffer cmd);

    // EndTransparentPass: Transition images for reading
    void EndTransparentPass(VkCommandBuffer cmd);

    // CompositePass: Fullscreen quad blending OIT result over opaque framebuffer
    // Note: Requires compiled composite shader — stub until SPIR-V is available
    void CompositePass(VkCommandBuffer cmd);

    // Configuration
    OITConfig& GetConfig() { return m_Config; }
    const OITConfig& GetConfig() const { return m_Config; }
    void SetConfig(const OITConfig& config) { m_Config = config; }

    bool IsInitialized() const { return m_Initialized; }
    bool IsEnabled() const { return m_Config.enabled && m_Initialized; }

    // Access textures for external binding (e.g., composite shader)
    VkImageView GetAccumulationView() const { return m_AccumulationView; }
    VkImageView GetRevealageView() const { return m_RevealageView; }

private:
    bool CreateTextures(u32 width, u32 height);
    void DestroyTextures();

    VulkanContext* m_Context = nullptr;
    OITConfig m_Config;

    // Accumulation texture (RGBA16F) — stores weighted color sum
    VkImage m_AccumulationImage = VK_NULL_HANDLE;
    VkDeviceMemory m_AccumulationMemory = VK_NULL_HANDLE;
    VkImageView m_AccumulationView = VK_NULL_HANDLE;

    // Revealage texture (R8) — stores product of (1 - alpha)
    VkImage m_RevealageImage = VK_NULL_HANDLE;
    VkDeviceMemory m_RevealageMemory = VK_NULL_HANDLE;
    VkImageView m_RevealageView = VK_NULL_HANDLE;

    u32 m_Width = 0;
    u32 m_Height = 0;
    bool m_Initialized = false;
};

} // namespace Renderer
} // namespace Enjin
