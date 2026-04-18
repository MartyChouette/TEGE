#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Platform/Types.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/Renderer/Camera.h"
#include "Enjin/Math/Vector.h"
#include "Enjin/Math/Matrix.h"
#include "Enjin/Build/AssetReader.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

namespace Enjin::Renderer {

// ============================================================================
// GPU-side UBO structs — must exactly match WGSL struct layout (std140-like)
// ============================================================================

// Matches pbr.wgsl ViewProjection (group 0, binding 0)
struct WebViewProjectionUBO {
    Math::Matrix4 view;           // 64 bytes
    Math::Matrix4 proj;           // 64 bytes
    Math::Vector3 viewPos;        // 12 bytes
    f32 time;                     // 4 bytes
    // Total: 144 bytes
};

// Matches pbr.wgsl LightingUBO (group 0, binding 1)
struct WebLightingUBO {
    Math::Vector4 lightDir[8];    // directional directions + point positions
    Math::Vector4 lightColor[8];  // rgb + intensity
    Math::Vector4 lightParams[8]; // range, spotAngle, etc.
    Math::Vector4 ambientColor;   // rgb + intensity
    Math::Vector4 fogColor;       // rgb + density
    Math::Vector4 fogParams;      // start, end, heightFalloff, unused
    Math::Vector4 shadowParams;   // strength, softness, distance, unused
    Math::Vector4 lightCount;     // x = dirCount, y = pointCount, z = spotCount
    // Total: 464 bytes
};

// Matches pbr.wgsl ObjectData (group 1, binding 0)
struct WebObjectDataUBO {
    Math::Matrix4 model;          // 64 bytes
    Math::Vector3 baseColor;      // 12 bytes
    f32 metallic;                 // 4 bytes
    Math::Vector3 emissiveColor;  // 12 bytes
    f32 roughness;                // 4 bytes
    f32 emissiveStrength;         // 4 bytes
    f32 opacity;                  // 4 bytes
    f32 alphaCutoff;              // 4 bytes
    i32 flags;                    // 4 bytes
    f32 parallaxScale;            // 4 bytes
    f32 _pad[3];                  // 12 bytes padding → 128 total (WGSL struct alignment)
};

// Per-entity GPU resource tracking
struct EntityGPUData {
    WebGPUBufferHandle vertexBuffer;
    WebGPUBufferHandle indexBuffer;
    u32 indexCount = 0;
    bool uploaded = false;
    bool hasTextures = false;
    WGPUBindGroup textureBindGroup = nullptr;  // group 2 per-entity
};

// Texture cache entry
struct WebTextureEntry {
    WebGPUTextureHandle handle;
    std::string path;
};

// Render stats
struct WebRenderStats {
    u32 drawCalls = 0;
    u32 entityCount = 0;
    u32 triangleCount = 0;
    u64 gpuBufferBytes = 0;
    u64 gpuTextureBytes = 0;
};

// ============================================================================
// WebRenderPipeline — Production web rendering pipeline
//
// Standalone class designed for browser constraints. NOT a port of RenderSystem.
// Owns all GPU resources, pipelines, bind groups. Uses pbr.wgsl for Cook-Torrance
// BRDF with multi-light support and texture binding.
// ============================================================================
class ENJIN_API WebRenderPipeline {
public:
    explicit WebRenderPipeline(WebGPURenderer* renderer);
    ~WebRenderPipeline();

    // Lifecycle
    bool Initialize();
    void Shutdown();

    // Asset reader for loading textures from enjpak
    void SetAssetReader(Build::AssetReader* reader) { m_AssetReader = reader; }

    // Per-frame rendering
    void BeginFrame(const Camera* camera, ECS::World* world);
    void Render();
    void EndFrame();

    // Canvas resize (called from JS ResizeObserver via cwrap)
    void OnResize(u32 width, u32 height, f32 dpr);

    // Stats
    const WebRenderStats& GetStats() const { return m_Stats; }

private:
    // Pipeline creation
    void CreatePBRPipeline();
    void CreateUniformBuffers();
    void CreateDefaultTextures();

    // Per-frame updates
    void UpdateViewProjectionUBO(const Camera* camera);
    void UpdateLightingUBO(ECS::World* world);
    void UpdateObjectDataUBO(ECS::Entity entity, ECS::World* world);

    // Entity GPU resource management
    void EnsureEntityGPUData(ECS::Entity entity, const ECS::MeshComponent* mesh);
    void CleanupDestroyedEntities(ECS::World* world);

    // Texture loading
    void LoadEntityTextures(ECS::Entity entity, const ECS::MaterialComponent* mat);
    WGPUBindGroup CreateTextureBindGroup(const WebGPUTextureHandle& baseColor,
                                          const WebGPUTextureHandle& normal,
                                          const WebGPUTextureHandle& metallicRoughness);
    WebGPUTextureHandle LoadTexture(const std::string& path);

    // Core state
    WebGPURenderer* m_Renderer = nullptr;
    Build::AssetReader* m_AssetReader = nullptr;
    bool m_Initialized = false;

    // Pipeline
    WGPURenderPipeline m_Pipeline = nullptr;
    WGPUBindGroupLayout m_ViewLightLayout = nullptr;  // group 0
    WGPUBindGroupLayout m_ObjectLayout = nullptr;      // group 1
    WGPUBindGroupLayout m_TextureLayout = nullptr;     // group 2
    WGPUPipelineLayout m_PipelineLayout = nullptr;

    // Uniform buffers
    WebGPUBufferHandle m_ViewProjBuffer;   // group 0, binding 0
    WebGPUBufferHandle m_LightingBuffer;   // group 0, binding 1
    WebGPUBufferHandle m_ObjectBuffer;     // group 1, binding 0

    // Bind groups
    WGPUBindGroup m_ViewLightBindGroup = nullptr;  // group 0
    WGPUBindGroup m_ObjectBindGroup = nullptr;     // group 1

    // Default textures (1x1 white/normal/black)
    WebGPUTextureHandle m_DefaultWhiteTexture;
    WebGPUTextureHandle m_DefaultNormalTexture;
    WebGPUTextureHandle m_DefaultBlackTexture;
    WGPUBindGroup m_DefaultTextureBindGroup = nullptr;  // group 2 fallback

    // Entity tracking
    std::unordered_map<ECS::Entity, EntityGPUData> m_EntityGPUData;

    // Texture cache: path → handle
    std::unordered_map<std::string, WebGPUTextureHandle> m_TextureCache;

    // Canvas state
    u32 m_CanvasWidth = 0;
    u32 m_CanvasHeight = 0;
    f32 m_DevicePixelRatio = 1.0f;

    // Frame state (valid between BeginFrame/EndFrame)
    ECS::World* m_FrameWorld = nullptr;
    const Camera* m_FrameCamera = nullptr;
    bool m_CameraSynced = false;
    WebRenderStats m_Stats;
    f32 m_Time = 0.0f;
};

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
