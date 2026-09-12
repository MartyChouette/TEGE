#pragma once
// WHOLE_FILE_WEBGPU_GUARD
#if !ENJIN_RENDERER_WEBGPU

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"
#if !ENJIN_RENDERER_WEBGPU
#include <vulkan/vulkan.h>
#endif
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
    u32 mipLevels = 1;   // prefiltered chain: mip 0 = sharp, higher = rougher
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

    // Keep every probe current without anyone pressing a button.
    //
    // Two things made a probe go stale, and both were silent. A probe that had
    // never been baked THIS SESSION had no cubemap at all -- which is every
    // probe in a scene that was just loaded, because the cubemap is a GPU
    // resource and does not survive a save. And a probe whose scene had changed
    // underneath it kept lighting the old geometry, which is the failure mode
    // that made Tiny Glade abandon its probe grid: resize a building and the
    // region that used to be inside is now outside, still being darkened by a
    // probe that has not noticed.
    //
    // Both are worse in a building tool than in a normal game, because in
    // creative mode the geometry changes constantly and by design.
    //
    // Called once per frame from RenderSystem::FlushPendingChanges, immediately
    // before ProcessPendingBakes, so a bake it queues runs in the same safe
    // window as a hand-requested one.
    void Update(ECS::World* world);

    // Force every active probe to re-capture on the next safe frame.
    void MarkAllProbesDirty(ECS::World* world);

    // The key the scene-wide implicit probe is filed under. Not a real entity,
    // and chosen so it can never collide with one.
    static constexpr u64 kImplicitProbeKey = ~0ull;

    // Is the reflection currently coming from the implicit probe rather than one
    // somebody placed? For the editor to be able to SAY so.
    bool IsUsingImplicitProbe() const { return m_ImplicitActive; }

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

    // Mip count of the most recently queried baked probe (set by FindNearestProbe).
    // The shader maps material roughness across LOD 0..(mipLevels-1) for glossy blur.
    u32 GetActiveBakedMipLevels() const { return m_ActiveBakedMipLevels; }

private:
    // A cheap stand-in for "has the world changed shape". Brush solids already
    // carry the hash their geometry was built from, so reading it costs nothing
    // and it is exactly what creative mode moves. Honest about its limits: it
    // does NOT notice an imported mesh sliding across the room, which would need
    // a broader change signal than the renderer currently has.
    u64 ComputeGeometryFingerprint(ECS::World* world) const;

    // The world-space box the scene's renderable geometry occupies. False when
    // there is nothing to bound, which is a scene with nothing in it -- there is
    // no reflection to capture and no probe worth baking.
    bool ComputeSceneBounds(ECS::World* world, Math::Vector3& outMin, Math::Vector3& outMax) const;

    // Keep the implicit probe in step with the scene: created when nobody has
    // placed a probe, destroyed the moment somebody does.
    void UpdateImplicitProbe(ECS::World* world);

    // The shared half of a bake, with no probe entity involved, so the implicit
    // probe can use the same six-face capture as a placed one.
    bool BakeAt(ECS::World* world, ECS::RenderSystem* renderSystem,
                u64 key, const Math::Vector3& position, u32 resolution);

    bool BakeProbeInternal(ECS::World* world, ECS::RenderSystem* renderSystem, u64 probeEntity);
    bool CreateCubemapImage(u32 resolution, BakedCubemap& cubemap);
    void DestroyCubemap(BakedCubemap& cubemap);
    bool UploadFacesToCubemap(BakedCubemap& cubemap, const std::vector<std::vector<u8>>& facePixels, u32 resolution);

    VulkanContext* m_Context = nullptr;
    bool m_Initialized = false;

    // Pending bake requests (entity IDs)
    std::vector<u64> m_PendingBakes;

    // Auto-refresh state. The settle counter is what stops a drag from baking
    // sixty times a second: a bake is six full scene renders, so it waits until
    // the geometry has held still for a few frames before spending one.
    // The scene-wide implicit probe.
    //
    // A reflection probe is a thing a person has to know exists and know to
    // place, which fails the project's own bar twice over: someone offline with
    // no AI in the loop gets sky-gradient reflections and no hint that a better
    // answer was one component away. So when a scene has NO probe, one is
    // captured over the scene's own bounds.
    //
    // Deliberately not an entity. An auto-spawned probe object would clutter the
    // hierarchy, get saved into the scene, and then go stale in the file the way
    // the `baked` flag used to. This is a default, not authored data: it owns no
    // component, serializes nothing, and the instant somebody places a real
    // probe it is destroyed and gets out of the way.
    bool m_ImplicitActive = false;
    // The implicit probe's own settle state: its volume is recomputed from
    // scene bounds every frame, so it needs the hold-still timer that placed
    // probes get from the geometry fingerprint.
    bool m_ImplicitDirty = false;
    u32 m_ImplicitSettleFrames = 0;
    Math::Vector3 m_ImplicitCenter;
    Math::Vector3 m_ImplicitMin;
    Math::Vector3 m_ImplicitMax;

    // Keys whose last bake FAILED.
    //
    // Auto-refresh turns a failing bake into a failing bake EVERY FRAME: the
    // probe has no cubemap, so Update queues it, so it fails, so it still has no
    // cubemap. Six warnings a frame, forever. A failure is remembered and not
    // retried until something actually changes -- a real geometry edit, or a
    // person pressing Bake, which is a deliberate "try again".
    std::vector<u64> m_FailedBakes;

    u64 m_GeometryFingerprint = 0;
    u32 m_FramesSinceGeometryChanged = 0;
    bool m_GeometryDirty = false;
    static constexpr u32 kSettleFrames = 12;

    // Baked cubemaps indexed by entity ID
    std::unordered_map<u64, BakedCubemap> m_BakedCubemaps;

    // Active baked cubemap descriptor (updated by FindNearestProbe)
    mutable VkDescriptorImageInfo m_ActiveBakedDescriptor{};
    mutable bool m_ActiveBakedDescriptorValid = false;
    mutable u32 m_ActiveBakedMipLevels = 1;
};

} // namespace Renderer
} // namespace Enjin
#endif // !ENJIN_RENDERER_WEBGPU
