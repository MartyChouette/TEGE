#include "Enjin/Renderer/ReflectionProbeSystem.h"
#include "Enjin/Renderer/Vulkan/VulkanContext.h"
#include "Enjin/Renderer/Vulkan/VulkanRenderer.h"
#include "Enjin/Renderer/RenderTarget.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/ReflectionProbe.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/BrushSolid.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Logging/Log.h"
#include <cmath>
#include <cstring>

namespace Enjin {
namespace Renderer {

ReflectionProbeSystem::~ReflectionProbeSystem() {
    Shutdown();
}

void ReflectionProbeSystem::Initialize(VulkanContext* context) {
    m_Context = context;
    m_Initialized = true;
}

void ReflectionProbeSystem::Shutdown() {
    if (!m_Context) return;

    m_Context->WaitForGPU();

    for (auto& [entity, cubemap] : m_BakedCubemaps) {
        DestroyCubemap(cubemap);
    }
    m_BakedCubemaps.clear();
    m_PendingBakes.clear();
    m_ActiveBakedDescriptorValid = false;
    m_Initialized = false;
}

ReflectionProbeData ReflectionProbeSystem::FindNearestProbe(
    ECS::World* world, const Math::Vector3& position) const {

    ReflectionProbeData result{};
    m_ActiveBakedDescriptorValid = false;

    if (!world) return result;

    i32 bestPriority = -1;
    f32 bestWeight = 0.0f;
    u64 bestEntity = 0;

    for (ECS::Entity entity : world->GetEntitiesWithComponent<ECS::ReflectionProbeComponent>()) {
        auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(entity);
        auto* transform = world->GetComponent<ECS::TransformComponent>(entity);
        if (!probe || !transform || !probe->isActive) continue;

        // Compute world-space AABB from probe center + box offsets
        Math::Vector3 center = transform->position;
        Math::Vector3 worldMin(center.x + probe->boxMin.x,
                               center.y + probe->boxMin.y,
                               center.z + probe->boxMin.z);
        Math::Vector3 worldMax(center.x + probe->boxMax.x,
                               center.y + probe->boxMax.y,
                               center.z + probe->boxMax.z);

        // Check if position is inside the probe's AABB
        if (position.x < worldMin.x || position.x > worldMax.x ||
            position.y < worldMin.y || position.y > worldMax.y ||
            position.z < worldMin.z || position.z > worldMax.z) {
            continue;
        }

        // Compute blend weight based on distance from box edges
        f32 blend = probe->blendDistance;
        f32 weight = 1.0f;
        if (blend > 0.001f) {
            f32 dx0 = position.x - worldMin.x;
            f32 dx1 = worldMax.x - position.x;
            f32 dy0 = position.y - worldMin.y;
            f32 dy1 = worldMax.y - position.y;
            f32 dz0 = position.z - worldMin.z;
            f32 dz1 = worldMax.z - position.z;

            f32 minDist = Math::Min(dx0, Math::Min(dx1,
                          Math::Min(dy0, Math::Min(dy1,
                          Math::Min(dz0, dz1)))));

            weight = Math::Clamp(minDist / blend, 0.0f, 1.0f);
        }

        // Pick the probe with highest priority, then best blend weight
        i32 probePriority = static_cast<i32>(probe->priority);
        if (probePriority > bestPriority ||
            (probePriority == bestPriority && weight > bestWeight)) {
            bestPriority = probePriority;
            bestWeight = weight;
            bestEntity = entity;

            result.probePosition = center;
            result.intensity = probe->intensity * weight;
            result.boxMin = worldMin;
            result.boxMax = worldMax;
            result.blendDistance = probe->blendDistance;
            result.isBaked = (probe->baked && probe->cubemapTextureId >= 0) ? 1.0f : 0.0f;
        }
    }

    // Nothing placed covers this point. Fall back to the scene-wide capture,
    // which is what makes a scene nobody configured still reflect its own room
    // instead of a sky gradient.
    if (result.intensity <= 0.0f && m_ImplicitActive) {
        auto it = m_BakedCubemaps.find(kImplicitProbeKey);
        if (it != m_BakedCubemaps.end() && it->second.view != VK_NULL_HANDLE) {
            result.probePosition = m_ImplicitCenter;
            result.boxMin = m_ImplicitMin;
            result.boxMax = m_ImplicitMax;
            result.blendDistance = 0.0f;   // no edge falloff: it IS the whole scene
            result.intensity = 1.0f;
            result.isBaked = 1.0f;
            m_ActiveBakedDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            m_ActiveBakedDescriptor.imageView = it->second.view;
            m_ActiveBakedDescriptor.sampler = it->second.sampler;
            m_ActiveBakedDescriptorValid = true;
            m_ActiveBakedMipLevels = it->second.mipLevels;
            return result;
        }
    }

    // If the best probe is baked, set up the active cubemap descriptor
    if (result.isBaked > 0.5f && bestEntity != 0) {
        auto it = m_BakedCubemaps.find(bestEntity);
        if (it != m_BakedCubemaps.end() && it->second.view != VK_NULL_HANDLE) {
            m_ActiveBakedDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            m_ActiveBakedDescriptor.imageView = it->second.view;
            m_ActiveBakedDescriptor.sampler = it->second.sampler;
            m_ActiveBakedDescriptorValid = true;
            m_ActiveBakedMipLevels = it->second.mipLevels;
        }
    }

    return result;
}

u64 ReflectionProbeSystem::ComputeGeometryFingerprint(ECS::World* world) const {
    if (!world) return 0;

    // FNV-1a over the things that change a probe's view of the world. Brush
    // solids carry the hash their geometry was built from, so this reads a value
    // that was computed anyway rather than walking any vertices.
    u64 h = 1469598103934665603ull;
    auto mix = [&h](u64 v) {
        for (int i = 0; i < 8; ++i) { h ^= (v >> (i * 8)) & 0xffull; h *= 1099511628211ull; }
    };

    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::BrushSolidComponent>()) {
        auto* solid = world->GetComponent<ECS::BrushSolidComponent>(e);
        if (!solid) continue;
        mix(solid->builtHash);
        mix(static_cast<u64>(solid->brushes.size()));
        if (auto* t = world->GetComponent<ECS::TransformComponent>(e)) {
            // Quantised to a centimetre: a probe cannot see a sub-centimetre
            // move, and hashing raw floats would re-bake on camera-driven
            // float noise that never reaches the geometry.
            mix(static_cast<u64>(static_cast<i64>(t->position.x * 100.0f)));
            mix(static_cast<u64>(static_cast<i64>(t->position.y * 100.0f)));
            mix(static_cast<u64>(static_cast<i64>(t->position.z * 100.0f)));
        }
    }

    // Entities appearing and disappearing count as a change even when no brush
    // solid moved -- placing a Water plane or a Ladder changes what a probe sees.
    mix(static_cast<u64>(world->GetEntitiesWithComponent<ECS::MeshComponent>().size()));
    return h;
}

bool ReflectionProbeSystem::ComputeSceneBounds(ECS::World* world,
                                              Math::Vector3& outMin,
                                              Math::Vector3& outMax) const {
    if (!world) return false;

    bool any = false;
    Math::Vector3 lo(1e30f, 1e30f, 1e30f);
    Math::Vector3 hi(-1e30f, -1e30f, -1e30f);

    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::MeshComponent>()) {
        auto* mesh = world->GetComponent<ECS::MeshComponent>(e);
        auto* xf = world->GetComponent<ECS::TransformComponent>(e);
        if (!mesh || !xf || !mesh->IsValid()) continue;

        // The cached AABB uses min > max to mean "not computed yet". Rather than
        // walk the vertices of every mesh in the scene to fix that here, fall
        // back to the entity's position: it still bounds WHERE the thing is,
        // which is all a probe placement needs.
        Math::Vector3 localMin = mesh->cachedAABBMin;
        Math::Vector3 localMax = mesh->cachedAABBMax;
        const bool haveLocal = (localMin.x <= localMax.x &&
                                localMin.y <= localMax.y &&
                                localMin.z <= localMax.z);

        if (!haveLocal) {
            lo.x = Math::Min(lo.x, xf->position.x); hi.x = Math::Max(hi.x, xf->position.x);
            lo.y = Math::Min(lo.y, xf->position.y); hi.y = Math::Max(hi.y, xf->position.y);
            lo.z = Math::Min(lo.z, xf->position.z); hi.z = Math::Max(hi.z, xf->position.z);
            any = true;
            continue;
        }

        // Every corner through the world matrix, because a rotated box's world
        // bounds are not its rotated extents.
        const Math::Matrix4 m = ECS::ComputeWorldMatrix(world, e);
        for (int c = 0; c < 8; ++c) {
            const Math::Vector3 corner((c & 1) ? localMax.x : localMin.x,
                                       (c & 2) ? localMax.y : localMin.y,
                                       (c & 4) ? localMax.z : localMin.z);
            const Math::Vector4 h4 = m * Math::Vector4(corner.x, corner.y, corner.z, 1.0f);
            const Math::Vector3 w(h4.x, h4.y, h4.z);
            lo.x = Math::Min(lo.x, w.x); hi.x = Math::Max(hi.x, w.x);
            lo.y = Math::Min(lo.y, w.y); hi.y = Math::Max(hi.y, w.y);
            lo.z = Math::Min(lo.z, w.z); hi.z = Math::Max(hi.z, w.z);
        }
        any = true;
    }

    if (!any) return false;

    // A scene of one flat floor has zero height, and a probe volume with no
    // thickness contains nothing -- including the camera, so it would never be
    // picked. Pad so the box is always something you can stand inside.
    constexpr f32 kMinHalf = 2.0f;
    for (int axis = 0; axis < 3; ++axis) {
        f32& a = (axis == 0) ? lo.x : (axis == 1) ? lo.y : lo.z;
        f32& b = (axis == 0) ? hi.x : (axis == 1) ? hi.y : hi.z;
        const f32 mid = (a + b) * 0.5f;
        const f32 half = Math::Max((b - a) * 0.5f, kMinHalf);
        a = mid - half;
        b = mid + half;
    }

    outMin = lo;
    outMax = hi;
    return true;
}

void ReflectionProbeSystem::UpdateImplicitProbe(ECS::World* world) {
    if (!world) return;

    // Somebody placed a probe: theirs wins, and ours gets out of the way rather
    // than sitting in memory competing.
    bool anyPlaced = false;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::ReflectionProbeComponent>()) {
        auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(e);
        if (probe && probe->isActive) { anyPlaced = true; break; }
    }

    if (anyPlaced) {
        if (m_ImplicitActive) {
            auto it = m_BakedCubemaps.find(kImplicitProbeKey);
            if (it != m_BakedCubemaps.end()) {
                DestroyCubemap(it->second);
                m_BakedCubemaps.erase(it);
            }
            m_ImplicitActive = false;
            ENJIN_LOG_INFO(Renderer, "Reflection: a placed probe took over from the scene-wide one");
        }
        return;
    }

    Math::Vector3 lo, hi;
    if (!ComputeSceneBounds(world, lo, hi)) {
        // Nothing in the scene to reflect. No probe, and the sky fallback is the
        // honest answer rather than a capture of empty space.
        m_ImplicitActive = false;
        return;
    }

    const Math::Vector3 centre((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f);

    // Only re-bake when the volume has actually moved. Scene bounds jitter by
    // millimetres as things settle, and a probe is six full scene renders.
    constexpr f32 kMoved = 0.25f;
    const bool moved = !m_ImplicitActive ||
        Math::Abs(centre.x - m_ImplicitCenter.x) > kMoved ||
        Math::Abs(centre.y - m_ImplicitCenter.y) > kMoved ||
        Math::Abs(centre.z - m_ImplicitCenter.z) > kMoved ||
        Math::Abs(lo.x - m_ImplicitMin.x) > kMoved ||
        Math::Abs(hi.x - m_ImplicitMax.x) > kMoved ||
        Math::Abs(lo.z - m_ImplicitMin.z) > kMoved;

    m_ImplicitCenter = centre;
    m_ImplicitMin = lo;
    m_ImplicitMax = hi;

    const bool firstTime = !m_ImplicitActive;
    m_ImplicitActive = true;

    if (firstTime) {
        ENJIN_LOG_INFO(Renderer,
            "Reflection: no probe in the scene, capturing a scene-wide one at (%.1f, %.1f, %.1f)",
            centre.x, centre.y, centre.z);
    }
    bool implicitBlocked = false;
    for (u64 f : m_FailedBakes) { if (f == kImplicitProbeKey) { implicitBlocked = true; break; } }
    if (moved) {
        RequestBake(kImplicitProbeKey);   // a real change: worth another try
    } else if (!implicitBlocked && m_BakedCubemaps.find(kImplicitProbeKey) == m_BakedCubemaps.end()) {
        RequestBake(kImplicitProbeKey);
    }
}

void ReflectionProbeSystem::MarkAllProbesDirty(ECS::World* world) {
    if (!world) return;
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::ReflectionProbeComponent>()) {
        auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(e);
        if (probe && probe->isActive) RequestBake(static_cast<u64>(e));
    }
}

void ReflectionProbeSystem::Update(ECS::World* world) {
    if (!world || !m_Initialized) return;

    // --- probes with no cubemap at all -------------------------------------
    // Every probe in a freshly loaded scene is in this state, because the
    // cubemap is a GPU resource and does not survive a save. These are queued
    // without waiting for the settle timer: there is nothing to preserve, the
    // reflection is currently the sky-gradient fallback, and the sooner it is
    // right the better.
    for (ECS::Entity e : world->GetEntitiesWithComponent<ECS::ReflectionProbeComponent>()) {
        auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(e);
        if (!probe || !probe->isActive) continue;
        bool blocked = false;
        for (u64 f : m_FailedBakes) { if (f == static_cast<u64>(e)) { blocked = true; break; } }
        if (!blocked && m_BakedCubemaps.find(static_cast<u64>(e)) == m_BakedCubemaps.end()) {
            // Keep the component honest while it waits, so nothing downstream
            // reads a baked flag with no cubemap behind it.
            probe->baked = false;
            probe->cubemapTextureId = -1;
            RequestBake(static_cast<u64>(e));
        }
    }

    // --- the scene-wide fallback -------------------------------------------
    UpdateImplicitProbe(world);

    // --- probes whose world moved under them --------------------------------
    const u64 fingerprint = ComputeGeometryFingerprint(world);
    if (fingerprint != m_GeometryFingerprint) {
        m_GeometryFingerprint = fingerprint;
        m_FramesSinceGeometryChanged = 0;
        m_GeometryDirty = true;
        return;   // still moving; do not spend a bake yet
    }

    if (!m_GeometryDirty) return;

    // Held still long enough to be worth six scene renders. Without this a
    // single resize drag would bake on every frame of the gesture.
    if (++m_FramesSinceGeometryChanged < kSettleFrames) return;

    m_GeometryDirty = false;
    m_FramesSinceGeometryChanged = 0;
    MarkAllProbesDirty(world);
    // The implicit probe is not in the component list, so it has to be asked
    // separately -- and it is the one that matters most here, because a scene
    // with no placed probe is exactly the scene relying on it.
    if (m_ImplicitActive) RequestBake(kImplicitProbeKey);
}

void ReflectionProbeSystem::RequestBake(u64 probeEntity) {
    // Avoid duplicate requests
    for (u64 id : m_PendingBakes) {
        if (id == probeEntity) return;
    }
    // An explicit request is a deliberate retry, so it lifts any previous
    // failure block on this key.
    for (auto it = m_FailedBakes.begin(); it != m_FailedBakes.end(); ++it) {
        if (*it == probeEntity) { m_FailedBakes.erase(it); break; }
    }
    m_PendingBakes.push_back(probeEntity);
    ENJIN_LOG_INFO(Renderer, "Reflection probe bake queued for entity %llu (will bake next frame)", probeEntity);
}

void ReflectionProbeSystem::ProcessPendingBakes(ECS::World* world, ECS::RenderSystem* renderSystem) {
    if (m_PendingBakes.empty()) return;

    // Process all pending bakes
    for (u64 entity : m_PendingBakes) {
        if (!BakeProbeInternal(world, renderSystem, entity)) {
            bool known = false;
            for (u64 f : m_FailedBakes) { if (f == entity) { known = true; break; } }
            if (!known) {
                m_FailedBakes.push_back(entity);
                ENJIN_LOG_WARN(Renderer,
                    "Reflection probe %llu failed to bake; not retrying until the scene changes "
                    "or you bake it by hand", entity);
            }
        }
    }
    m_PendingBakes.clear();
}

bool ReflectionProbeSystem::BakeProbeInternal(ECS::World* world, ECS::RenderSystem* renderSystem, u64 probeEntity) {
    if (!world) return false;

    // The implicit scene-wide probe has no entity to read: it is a default, not
    // a component, so its position and resolution come from the system itself.
    if (probeEntity == kImplicitProbeKey) {
        if (!m_ImplicitActive) return false;
        return BakeAt(world, renderSystem, probeEntity, m_ImplicitCenter, 256);
    }

    auto* probe = world->GetComponent<ECS::ReflectionProbeComponent>(static_cast<ECS::Entity>(probeEntity));
    auto* transform = world->GetComponent<ECS::TransformComponent>(static_cast<ECS::Entity>(probeEntity));
    if (!probe || !transform) {
        ENJIN_LOG_ERROR(Renderer, "ReflectionProbeSystem::BakeProbe - entity missing required components");
        return false;
    }
    return BakeAt(world, renderSystem, probeEntity, transform->position, probe->resolution);
}

bool ReflectionProbeSystem::BakeAt(ECS::World* world, ECS::RenderSystem* renderSystem,
                                   u64 key, const Math::Vector3& position, u32 bakeResolution) {
    if (!world || !renderSystem || !m_Context || !m_Initialized) {
        ENJIN_LOG_ERROR(Renderer, "ReflectionProbeSystem::BakeProbe - not initialized or null parameters");
        return false;
    }
    const u64 probeEntity = key;

    VulkanRenderer* vulkanRenderer = renderSystem->GetVulkanRenderer();
    if (!vulkanRenderer) {
        ENJIN_LOG_ERROR(Renderer, "ReflectionProbeSystem::BakeProbe - no VulkanRenderer available");
        return false;
    }

    u32 resolution = bakeResolution;
    if (resolution < 32) resolution = 32;
    if (resolution > 1024) resolution = 1024;

    Math::Vector3 probePos = position;

    ENJIN_LOG_INFO(Renderer, "Baking reflection probe at (%.1f, %.1f, %.1f) resolution %u...",
        probePos.x, probePos.y, probePos.z, resolution);

    // Ensure GPU is idle before modifying resources
    m_Context->WaitForGPU();

    // Destroy previous cubemap if it exists
    auto existing = m_BakedCubemaps.find(probeEntity);
    if (existing != m_BakedCubemaps.end()) {
        DestroyCubemap(existing->second);
        m_BakedCubemaps.erase(existing);
    }

    // Create the cubemap image (6 layers, RGBA8)
    BakedCubemap cubemap;
    if (!CreateCubemapImage(resolution, cubemap)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create cubemap image for probe bake");
        return false;
    }

    // Cubemap face directions and up vectors (standard OpenGL cubemap convention)
    // Order: +X, -X, +Y, -Y, +Z, -Z
    struct FaceDef {
        Math::Vector3 dir;
        Math::Vector3 up;
    };

    FaceDef faces[6] = {
        { Math::Vector3( 1.0f,  0.0f,  0.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // +X
        { Math::Vector3(-1.0f,  0.0f,  0.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // -X
        { Math::Vector3( 0.0f,  1.0f,  0.0f), Math::Vector3(0.0f,  0.0f,  1.0f) },  // +Y
        { Math::Vector3( 0.0f, -1.0f,  0.0f), Math::Vector3(0.0f,  0.0f, -1.0f) },  // -Y
        { Math::Vector3( 0.0f,  0.0f,  1.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // +Z
        { Math::Vector3( 0.0f,  0.0f, -1.0f), Math::Vector3(0.0f, -1.0f,  0.0f) },  // -Z
    };

    // Create a temporary render target for face captures
    auto faceTarget = std::make_unique<RenderTarget>();
    if (!faceTarget->Create(vulkanRenderer, resolution, resolution)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create render target for probe bake");
        DestroyCubemap(cubemap);
        return false;
    }

    // Render each face and capture pixels
    std::vector<std::vector<u8>> facePixels(6);
    usize expectedFaceBytes = static_cast<usize>(resolution) * static_cast<usize>(resolution) * 4;
    bool allFacesOk = true;
    // Faces that actually rendered. A failed face is filled with flat mid-grey,
    // so a cubemap where NOTHING rendered is six grey walls -- which the shader
    // would happily reflect as a uniform wash, indistinguishable from a real
    // room that happens to be grey.
    u32 renderedFaces = 0;

    for (int face = 0; face < 6; ++face) {
        // Set up camera for this face: 90 degree FOV, 1:1 aspect
        Camera faceCamera;
        Math::Vector3 target(probePos.x + faces[face].dir.x,
                             probePos.y + faces[face].dir.y,
                             probePos.z + faces[face].dir.z);
        faceCamera.SetLookAt(probePos, target, faces[face].up);
        faceCamera.SetPerspective(90.0f, 1.0f, 0.1f, 1000.0f);

        // Begin a frame, render the scene to the target, end the frame
        if (!vulkanRenderer->BeginFrameVulkan()) {
            ENJIN_LOG_WARN(Renderer, "Probe bake face %d: BeginFrame failed", face);
            facePixels[face].resize(expectedFaceBytes, 128);
            allFacesOk = false;
            continue;
        }

        VkCommandBuffer cmd = vulkanRenderer->GetCurrentCommandBuffer();
        if (cmd == VK_NULL_HANDLE) {
            ENJIN_LOG_WARN(Renderer, "Probe bake face %d: no command buffer", face);
            vulkanRenderer->EndFrame();
            facePixels[face].resize(expectedFaceBytes, 128);
            allFacesOk = false;
            continue;
        }

        // Shadow pass for this face, INSIDE the frame.
        //
        // It used to run before BeginFrameVulkan, under a comment claiming it
        // needed its own command buffer submission. It does not: it RECORDS into
        // whatever GetCurrentCommandBuffer returns. Outside a frame that is a
        // pre-allocated buffer which is valid, non-null, and NOT in the
        // recording state, so the null check inside RenderShadowPass passed and
        // the driver access-violated on the first vkCmd (nvoglv64.dll, captured
        // 2026-09-08 via --probe-bake-test; see
        // _docs_internal/PROBE_BAKE_INVESTIGATION.md).
        //
        // Shadows must also be rendered BEFORE the face is drawn, or the cubemap
        // captures the scene lit by the previous face's shadow map.
        renderSystem->RenderShadowPassForCamera(&faceCamera);

        // Begin render target, render scene, end render target
        faceTarget->Begin(cmd);
        renderSystem->RenderToTarget(faceTarget.get(), &faceCamera);
        faceTarget->End(cmd);

        // End the frame (submits command buffer)
        vulkanRenderer->EndFrame();
        m_Context->WaitForGPU();

        // Capture rendered pixels from the render target
        auto pixels = faceTarget->CaptureToPixels();
        if (pixels.size() >= expectedFaceBytes) {
            facePixels[face] = std::move(pixels);
            ++renderedFaces;
        } else {
            ENJIN_LOG_WARN(Renderer, "Probe bake face %d: capture returned %zu bytes, expected %zu",
                face, pixels.size(), expectedFaceBytes);
            facePixels[face].resize(expectedFaceBytes, 128);
            allFacesOk = false;
        }
    }

    if (!allFacesOk) {
        ENJIN_LOG_WARN(Renderer, "Some probe faces failed to render, cubemap may have artifacts");
    }
    // NO face rendered: every face is the flat grey fill, so storing this hands
    // the shader six grey walls and the scene reflects a uniform wash -- worse
    // than the sky-gradient fallback it replaced, and impossible to tell from a
    // room that is genuinely grey. Fail honestly instead, so the caller records
    // it, stops retrying, and the reflection falls back to the sky.
    if (renderedFaces == 0) {
        ENJIN_LOG_ERROR(Renderer, "Probe bake produced no faces; leaving the reflection unbaked");
        faceTarget->Destroy();
        faceTarget.reset();
        DestroyCubemap(cubemap);
        return false;
    }

    // Clean up the render target
    faceTarget->Destroy();
    faceTarget.reset();

    // Upload all 6 faces to the cubemap
    if (!UploadFacesToCubemap(cubemap, facePixels, resolution)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload face data to cubemap");
        DestroyCubemap(cubemap);
        return false;
    }

    // The previous cubemap for this key was already destroyed at the top of the
    // bake, right after WaitForGPU -- which is what makes repeated refreshes
    // safe rather than a leak per bake.
    m_BakedCubemaps[probeEntity] = cubemap;

    // A placed probe carries the flags. The implicit scene-wide one has no
    // component to mark -- the system's own state is its record.
    if (probeEntity != kImplicitProbeKey) {
        if (auto* placed = world->GetComponent<ECS::ReflectionProbeComponent>(
                static_cast<ECS::Entity>(probeEntity))) {
            placed->baked = true;
            placed->cubemapTextureId = static_cast<i32>(probeEntity & 0x7FFFFFFF);
        }
    }

    ENJIN_LOG_INFO(Renderer, "Reflection probe baked successfully (%ux%u per face, 6 faces)",
        resolution, resolution);

    return true;
}

VkDescriptorImageInfo ReflectionProbeSystem::GetBakedCubemapDescriptor(u64 probeEntity) const {
    VkDescriptorImageInfo info{};
    auto it = m_BakedCubemaps.find(probeEntity);
    if (it != m_BakedCubemaps.end() && it->second.view != VK_NULL_HANDLE) {
        info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        info.imageView = it->second.view;
        info.sampler = it->second.sampler;
    }
    return info;
}

bool ReflectionProbeSystem::CreateCubemapImage(u32 resolution, BakedCubemap& cubemap) {
    VkDevice device = m_Context->GetDevice();
    cubemap.resolution = resolution;

    // Prefiltered mip chain: mip 0 is the sharp capture, each higher mip is a
    // linear-downsampled (blurrier) copy that stands in for rougher surfaces.
    // Cap the chain so the coarsest mip stays >= 4x4 (a 1x1 mip is a single
    // averaged color and adds no useful glossy detail).
    u32 fullMips = 1;
    { u32 r = resolution; while (r > 4) { r >>= 1; ++fullMips; } }
    u32 mipLevels = Math::Min(fullMips, 6u);
    cubemap.mipLevels = mipLevels;

    // Create cubemap image (6 layers, RGBA8, mip chain, transfer src+dst + sampled)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { resolution, resolution, 1 };
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // TRANSFER_SRC so mip N can be blitted from mip N-1 during prefiltering.
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &cubemap.image) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap image");
        return false;
    }

    // Allocate device-local memory
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, cubemap.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &cubemap.memory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate probe cubemap memory");
        vkDestroyImage(device, cubemap.image, nullptr);
        cubemap.image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(device, cubemap.image, cubemap.memory, 0);

    // Create cubemap image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = cubemap.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    if (vkCreateImageView(device, &viewInfo, nullptr, &cubemap.view) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap image view");
        vkFreeMemory(device, cubemap.memory, nullptr);
        vkDestroyImage(device, cubemap.image, nullptr);
        cubemap.image = VK_NULL_HANDLE;
        cubemap.memory = VK_NULL_HANDLE;
        return false;
    }

    // Create sampler with linear filtering and clamp-to-edge
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<f32>(mipLevels);
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &cubemap.sampler) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap sampler");
        vkDestroyImageView(device, cubemap.view, nullptr);
        vkFreeMemory(device, cubemap.memory, nullptr);
        vkDestroyImage(device, cubemap.image, nullptr);
        cubemap = {};
        return false;
    }

    return true;
}

bool ReflectionProbeSystem::UploadFacesToCubemap(
    BakedCubemap& cubemap, const std::vector<std::vector<u8>>& facePixels, u32 resolution) {

    if (facePixels.size() < 6 || !m_Context) return false;

    VkDevice device = m_Context->GetDevice();
    usize faceBytes = static_cast<usize>(resolution) * static_cast<usize>(resolution) * 4;
    usize totalBytes = faceBytes * 6;

    // Create staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = totalBytes;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create probe cubemap staging buffer");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_Context->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate probe cubemap staging memory");
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    // Copy face data to staging buffer
    void* mapped = nullptr;
    if (vkMapMemory(device, stagingMemory, 0, totalBytes, 0, &mapped) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map probe cubemap staging memory");
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    for (int f = 0; f < 6; ++f) {
        usize srcBytes = std::min(facePixels[f].size(), faceBytes);
        memcpy(static_cast<u8*>(mapped) + f * faceBytes, facePixels[f].data(), srcBytes);
        // Zero-fill remainder if source is short
        if (srcBytes < faceBytes) {
            memset(static_cast<u8*>(mapped) + f * faceBytes + srcBytes, 128, faceBytes - srcBytes);
        }
    }
    vkUnmapMemory(device, stagingMemory);

    // Create temporary command pool and buffer for upload
    VkCommandPool tempPool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = m_Context->GetGraphicsQueueFamily();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.commandPool = tempPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &cmdAllocInfo, &cmd) != VK_SUCCESS) {
        vkDestroyCommandPool(device, tempPool, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    const u32 mipLevels = cubemap.mipLevels;

    // Transition ALL mips of ALL 6 faces to TRANSFER_DST_OPTIMAL.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = cubemap.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy each face's captured pixels into mip 0.
    for (u32 f = 0; f < 6; ++f) {
        VkBufferImageCopy region{};
        region.bufferOffset = f * faceBytes;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = f;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { resolution, resolution, 1 };

        vkCmdCopyBufferToImage(cmd, stagingBuffer, cubemap.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    // Prefilter: generate each successive mip by a linear blit-downsample from
    // the previous mip (all 6 faces at once). This is a cheap glossy prefilter —
    // rough surfaces sample the blurrier high mips. Every mip level is blitted
    // independently per face, which cube-face seams tolerate at these blur levels.
    i32 mipW = static_cast<i32>(resolution);
    i32 mipH = static_cast<i32>(resolution);
    for (u32 i = 1; i < mipLevels; ++i) {
        // Source mip (i-1): TRANSFER_DST -> TRANSFER_SRC
        VkImageMemoryBarrier toSrc = barrier;
        toSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toSrc.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toSrc.subresourceRange.baseMipLevel = i - 1;
        toSrc.subresourceRange.levelCount = 1;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toSrc);

        i32 nextW = mipW > 1 ? mipW / 2 : 1;
        i32 nextH = mipH > 1 ? mipH / 2 : 1;

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 6;
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipW, mipH, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 6;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { nextW, nextH, 1 };

        vkCmdBlitImage(cmd,
            cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            cubemap.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit, VK_FILTER_LINEAR);

        mipW = nextW;
        mipH = nextH;
    }

    // Final layout transitions to SHADER_READ_ONLY_OPTIMAL. Mips 0..n-2 ended in
    // TRANSFER_SRC (they were blit sources); the last mip is still TRANSFER_DST.
    if (mipLevels > 1) {
        VkImageMemoryBarrier srcToRead = barrier;
        srcToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        srcToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        srcToRead.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcToRead.subresourceRange.baseMipLevel = 0;
        srcToRead.subresourceRange.levelCount = mipLevels - 1;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &srcToRead);
    }
    VkImageMemoryBarrier lastToRead = barrier;
    lastToRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    lastToRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    lastToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    lastToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    lastToRead.subresourceRange.baseMipLevel = mipLevels - 1;
    lastToRead.subresourceRange.levelCount = 1;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &lastToRead);

    vkEndCommandBuffer(cmd);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    if (vkQueueSubmit(m_Context->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to submit reflection probe cubemap upload commands");
    }
    vkQueueWaitIdle(m_Context->GetGraphicsQueue());

    // Cleanup staging resources
    vkDestroyCommandPool(device, tempPool, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyBuffer(device, stagingBuffer, nullptr);

    return true;
}

void ReflectionProbeSystem::DestroyCubemap(BakedCubemap& cubemap) {
    if (!m_Context) return;
    VkDevice device = m_Context->GetDevice();

    if (cubemap.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, cubemap.sampler, nullptr);
    }
    if (cubemap.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, cubemap.view, nullptr);
    }
    if (cubemap.image != VK_NULL_HANDLE) {
        vkDestroyImage(device, cubemap.image, nullptr);
    }
    if (cubemap.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, cubemap.memory, nullptr);
    }
    cubemap = {};
}

} // namespace Renderer
} // namespace Enjin
