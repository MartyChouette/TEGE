#pragma once

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace Enjin {

// Forward declarations
namespace ECS { class World; class RenderSystem; }

namespace Renderer {

class VulkanContext;
class VulkanRenderer;
class RenderTarget;
class Camera;

// Result of a reflection probe query — contains the data needed for GPU upload
struct ReflectionProbeData {
    Math::Vector3 probePosition;    // World-space probe center
    f32 intensity = 0.0f;           // 0 = no probe found
    Math::Vector3 boxMin;           // World-space AABB min (probe center + component boxMin)
    f32 blendDistance = 1.0f;
    Math::Vector3 boxMax;           // World-space AABB max (probe center + component boxMax)
    f32 isBaked = 0.0f;             // 1.0 = baked cubemap available, 0.0 = skybox fallback
};

// Baked cubemap resources for a single reflection probe
struct BakedCubemap {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    u32 resolution = 0;
};

// Reflection probe system — manages probe queries and cubemap baking for the renderer.
// Supports per-probe baked cubemaps captured from 6 camera directions at the probe position.
//
// Baking workflow:
// 1. Editor click "Bake" -> RequestBake(entity) queues a deferred bake
// 2. RenderSystem::FlushPendingChanges() calls ProcessPendingBakes() between frames
// 3. ProcessPendingBakes() renders 6 faces, captures pixels, uploads to cubemap
// 4. Fragment shader samples baked cubemap at binding 19 for box-projected reflections
class ENJIN_API ReflectionProbeSystem {
public:
    ReflectionProbeSystem() = default;
    ~ReflectionProbeSystem();

    // Initialize with Vulkan context for cubemap resource management
    void Initialize(VulkanContext* context);

    // Clean up all baked cubemap resources
    void Shutdown();

    // Find the highest-priority active probe that contains the given position.
    // Returns a ReflectionProbeData with intensity > 0 if a probe was found.
    ReflectionProbeData FindNearestProbe(ECS::World* world, const Math::Vector3& position) const;

    // Queue a probe bake request (deferred until next FlushPendingChanges).
    // Safe to call mid-frame from the editor — actual bake runs between frames.
    void RequestBake(u64 probeEntity);

    // Process any pending bake requests. Called by RenderSystem::FlushPendingChanges()
    // when the GPU is idle and no frame is in progress.
    void ProcessPendingBakes(ECS::World* world, ECS::RenderSystem* renderSystem);

    // Check if any bake is pending
    bool HasPendingBake() const { return !m_PendingBakes.empty(); }

    // Get the baked cubemap descriptor info for the active probe (if baked).
    VkDescriptorImageInfo GetBakedCubemapDescriptor(u64 probeEntity) const;

    // Check if any probe has a baked cubemap
    bool HasAnyBakedProbe() const { return !m_BakedCubemaps.empty(); }

    // Get the cubemap descriptor for the most recently queried baked probe
    // (set by FindNearestProbe when the found probe is baked)
    VkDescriptorImageInfo GetActiveBakedCubemapDescriptor() const { return m_ActiveBakedDescriptor; }
    bool HasActiveBakedCubemap() const { return m_ActiveBakedDescriptorValid; }

private:
    bool BakeProbeInternal(ECS::World* world, ECS::RenderSystem* renderSystem, u64 probeEntity);
    bool CreateCubemapImage(u32 resolution, BakedCubemap& cubemap);
    void DestroyCubemap(BakedCubemap& cubemap);
    bool UploadFacesToCubemap(BakedCubemap& cubemap, const std::vector<std::vector<u8>>& facePixels, u32 resolution);

    VulkanContext* m_Context = nullptr;
    bool m_Initialized = false;

    // Pending bake requests (entity IDs)
    std::vector<u64> m_PendingBakes;

    // Baked cubemaps indexed by entity ID
    std::unordered_map<u64, BakedCubemap> m_BakedCubemaps;

    // Active baked cubemap descriptor (updated by FindNearestProbe)
    mutable VkDescriptorImageInfo m_ActiveBakedDescriptor{};
    mutable bool m_ActiveBakedDescriptorValid = false;
};

} // namespace Renderer
} // namespace Enjin
