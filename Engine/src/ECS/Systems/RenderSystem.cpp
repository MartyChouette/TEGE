#include "Enjin/ECS/Systems/RenderSystem.h"
#include "Enjin/Logging/Log.h"
#if !ENJIN_RENDERER_WEBGPU
#include "Enjin/Renderer/SkinningComputeShaderData.h"  // embedded SPIR-V (ADR-0002 compute skinning)
#endif

// Effect renderer headers needed for unique_ptr destructor (incomplete type fix)
#include "Enjin/Effects/WeatherRenderer.h"
#include "Enjin/Effects/ParticleRenderer.h"
#include "Enjin/Effects/FluidRenderer.h"
#include "Enjin/Effects/SpriteBatchRenderer.h"
#include "Enjin/Effects/SpriteTextureAtlas.h"
#include "Enjin/Effects/GrassRenderer.h"
#include "Enjin/Effects/ShrubRenderer.h"
#include "Enjin/Effects/TreeRenderer.h"

// ============================================================================
// WebGPU RenderSystem — renders entities through abstract backend interface.
// Uses PBR WGSL shaders with 3-group bind layout (frame, object, textures).
// ============================================================================
#if ENJIN_RENDERER_WEBGPU

#include "Enjin/Renderer/WebGPU/WebShaderData.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderEncoder.h"
#include "Enjin/Renderer/WebGPU/WebGPUTextureManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/GPUBuffer.h"
#include "Enjin/Renderer/GPUTexture.h"
#include "Enjin/Renderer/GPUPipeline.h"
#include "Enjin/Renderer/GPUShader.h"
#include "Enjin/Renderer/GPUBindGroup.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/Build/AssetReader.h"
#include "Enjin/Math/Math.h"
#include <algorithm>
#include <cstring>
#include <emscripten.h>

namespace Enjin {
namespace ECS {

// UBO structs matching pbr.wgsl expectations (std140 layout)
struct WebViewProjectionUBO {
    alignas(16) Math::Matrix4 view;       // 64
    alignas(16) Math::Matrix4 proj;       // 64
    alignas(16) Math::Vector3 viewPos;    // 12
    f32 time;                              // 4
};                                         // Total: 144 bytes

struct alignas(16) WebLightVec4 { f32 x, y, z, w; };

struct WebLightingUBO {
    WebLightVec4 lightDir[8];              // 128  (0-3: dir directions, 4-7: point positions)
    WebLightVec4 lightColor[8];            // 128  (matching color.rgb + intensity.w)
    WebLightVec4 lightParams[8];           // 128  (point: range, linear, quadratic, constant)
    WebLightVec4 ambientColor;             // 16
    WebLightVec4 fogColor;                 // 16
    WebLightVec4 fogParams;                // 16
    WebLightVec4 shadowParams;             // 16
    WebLightVec4 lightCount;               // 16   (x=dir, y=point, z=spot)
    // Spot lights (separate arrays since they need both position and direction)
    WebLightVec4 spotPos[4];               // 64   position.xyz, range.w
    WebLightVec4 spotDir[4];               // 64   direction.xyz
    WebLightVec4 spotColor[4];             // 64   color.rgb, intensity.w
    WebLightVec4 spotParams[4];            // 64   innerCutoff.x, outerCutoff.y
};                                         // Total: 720 bytes

struct WebObjectDataUBO {
    alignas(16) Math::Matrix4 model;       // 64
    alignas(16) Math::Vector3 baseColor;   // 12
    f32 metallic;                           // 4
    alignas(16) Math::Vector3 emissiveColor;// 12
    f32 roughness;                          // 4
    f32 emissiveStrength;                   // 4
    f32 opacity;                            // 4
    f32 alphaCutoff;                        // 4
    i32 flags;                              // 4
    f32 parallaxScale;                      // 4
    f32 _pad[3];                            // 12
};                                          // Total: 128 bytes

// Spot shadow VP UBO: 2 lights x (view + proj) = 4 matrices
struct WebSpotShadowVPUBO {
    alignas(16) Math::Matrix4 viewProj[4];    // [0]=light0.view, [1]=light0.proj, [2]=light1.view, [3]=light1.proj
};                                             // Total: 256 bytes

// Point shadow VP UBO: 6 faces x (view + proj) = 12 matrices
struct WebPointShadowVPUBO {
    alignas(16) Math::Matrix4 viewProj[12];   // [0]=face0.view, [1]=face0.proj, ... [10]=face5.view, [11]=face5.proj
};                                             // Total: 768 bytes

// WebGPU perspective projection: depth [0,1], Y-up (no Vulkan Y-flip)
// WebGPU perspective for spot light shadows (Y-up for 2D shadow maps)
static Math::Matrix4 WebGPUPerspective(f32 fov, f32 aspect, f32 nearPlane, f32 farPlane) {
    f32 tanHalfFov = std::tan(fov * 0.5f);
    Math::Matrix4 r;
    std::memset(&r, 0, sizeof(r));
    r.m[0]  = 1.0f / (aspect * tanHalfFov);
    r.m[5]  = 1.0f / tanHalfFov;
    r.m[10] = farPlane / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = (nearPlane * farPlane) / (nearPlane - farPlane);
    return r;
}

// Cubemap shadow perspective. The face view matrices use the Vulkan/GL cube
// convention (up = -Y for the side faces), which assumes a Y-DOWN NDC rasterizer.
// WebGPU rasterizes Y-UP, so without a Y-flip here every rendered face lands
// vertically mirrored vs. the (spec-fixed) cube sampler layout — point shadows
// detach from casters, move mirrored, and show face-seam "corners".
static Math::Matrix4 WebGPUCubemapPerspective(f32 fov, f32 aspect, f32 nearPlane, f32 farPlane) {
    f32 tanHalfFov = std::tan(fov * 0.5f);
    Math::Matrix4 r;
    std::memset(&r, 0, sizeof(r));
    r.m[0]  = 1.0f / (aspect * tanHalfFov);
    r.m[5]  = -1.0f / tanHalfFov;  // Y-flip: compensate WebGPU's Y-up NDC (see above)
    r.m[10] = farPlane / (nearPlane - farPlane);
    r.m[11] = -1.0f;
    r.m[14] = (nearPlane * farPlane) / (nearPlane - farPlane);
    return r;
}

// ============================================================================
// Lifecycle
// ============================================================================

RenderSystem::RenderSystem(World* world, Renderer::IRenderBackend* renderer)
    : m_World(world), m_Renderer(renderer) {
    m_Camera = nullptr;
}

RenderSystem::~RenderSystem() { Shutdown(); }

void RenderSystem::Initialize() {
    if (m_Initialized) return;
    m_FrameAllocator = std::make_unique<FrameAllocator>(8 * 1024 * 1024);

    auto* shaderMgr = m_Renderer->GetShaderManager();
    auto* pipeMgr = m_Renderer->GetPipelineManager();
    auto* bufMgr = m_Renderer->GetBufferManager();
    auto* texMgr = m_Renderer->GetTextureManager();
    auto* bindMgr = m_Renderer->GetBindGroupManager();
    if (!shaderMgr || !pipeMgr || !bufMgr || !texMgr || !bindMgr) {
        ENJIN_LOG_ERROR(Renderer, "RenderSystem: Backend managers not available");
        return;
    }

    // Load PBR shader (single WGSL source, both vertex and fragment)
    m_MainVertexShader = shaderMgr->LoadShader(
        Renderer::WebShaderData::PBR_WGSL,
        std::strlen(Renderer::WebShaderData::PBR_WGSL),
        Renderer::GPUShaderStage::Vertex, "PBR_VS");
    m_MainFragmentShader = m_MainVertexShader;  // Same module for both stages in WGSL

    // Create bind group layouts
    using BType = Renderer::GPUBindingType;
    using SStage = Renderer::GPUShaderStage;

    // Group 0: ViewProjection + Lighting
    Renderer::GPUBindGroupLayoutDesc frameLayoutDesc;
    frameLayoutDesc.entries = {
        {0, BType::UniformBuffer, SStage::Vertex | SStage::Fragment, sizeof(WebViewProjectionUBO)},
        {1, BType::UniformBuffer, SStage::Fragment, sizeof(WebLightingUBO)},
    };
    m_WebFrameLayout = bindMgr->CreateBindGroupLayout(frameLayoutDesc);

    // Group 1: ObjectData + BoneMatrices
    Renderer::GPUBindGroupLayoutDesc objectLayoutDesc;
    objectLayoutDesc.entries = {
        {0, BType::StorageBufferReadOnly, SStage::Vertex | SStage::Fragment, 0},  // ObjectData SSBO (instanced)
        {1, BType::StorageBufferReadOnly, SStage::Vertex, 0},  // bone matrices SSBO
    };
    m_WebObjectLayout = bindMgr->CreateBindGroupLayout(objectLayoutDesc);

    // Group 2: Textures (3 texture + 3 sampler)
    Renderer::GPUBindGroupLayoutDesc texLayoutDesc;
    texLayoutDesc.entries = {
        {0, BType::SampledTexture, SStage::Fragment, 0},
        {1, BType::Sampler, SStage::Fragment, 0},
        {2, BType::SampledTexture, SStage::Fragment, 0},
        {3, BType::Sampler, SStage::Fragment, 0},
        {4, BType::SampledTexture, SStage::Fragment, 0},
        {5, BType::Sampler, SStage::Fragment, 0},
    };
    m_WebTextureLayout = bindMgr->CreateBindGroupLayout(texLayoutDesc);

    // Group 3: Shadow sampling (directional + spot + point shadow maps)
    Renderer::GPUBindGroupLayoutDesc shadowSampleLayoutDesc;
    shadowSampleLayoutDesc.entries = {
        {0, BType::UniformBuffer, SStage::Vertex | SStage::Fragment, sizeof(WebViewProjectionUBO)},  // dir shadow VP
        {1, BType::DepthTexture, SStage::Fragment, 0},              // dir shadow map
        {2, BType::ComparisonSampler, SStage::Fragment, 0},         // shared comparison sampler
        {3, BType::UniformBuffer, SStage::Fragment, sizeof(WebSpotShadowVPUBO)},   // spot shadow VPs
        {4, BType::DepthTexture, SStage::Fragment, 0},              // spot shadow map 0
        {5, BType::DepthTexture, SStage::Fragment, 0},              // spot shadow map 1
        {6, BType::UniformBuffer, SStage::Fragment, sizeof(WebPointShadowVPUBO)},  // point shadow VPs (6 faces)
        {7, BType::DepthTextureCube, SStage::Fragment, 0},          // point shadow cubemap
    };
    m_WebShadowSampleLayout = bindMgr->CreateBindGroupLayout(shadowSampleLayoutDesc);

    // Create pipeline
    Renderer::GPURenderPipelineDesc pipeDesc;
    pipeDesc.vertexShader = m_MainVertexShader;
    pipeDesc.fragmentShader = m_MainFragmentShader;
    pipeDesc.bindGroupLayouts = {m_WebFrameLayout, m_WebObjectLayout, m_WebTextureLayout, m_WebShadowSampleLayout};
    pipeDesc.topology = Renderer::GPUPrimitiveTopology::TriangleList;
    pipeDesc.cullMode = Renderer::GPUCullMode::None;
    pipeDesc.frontFace = Renderer::GPUFrontFace::CCW;
    pipeDesc.depthTest = true;
    pipeDesc.depthWrite = true;
    pipeDesc.depthCompare = Renderer::GPUCompareFunction::Less;
    pipeDesc.alphaBlend = true;
    pipeDesc.blendState.srcColor = Renderer::GPUBlendFactor::SrcAlpha;
    pipeDesc.blendState.dstColor = Renderer::GPUBlendFactor::OneMinusSrcAlpha;
    pipeDesc.blendState.srcAlpha = Renderer::GPUBlendFactor::One;
    pipeDesc.blendState.dstAlpha = Renderer::GPUBlendFactor::OneMinusSrcAlpha;
    pipeDesc.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;  // Render to HDR offscreen target
    pipeDesc.depthFormat = Renderer::GPUTextureFormat::Depth24PlusStencil8;
    pipeDesc.sampleCount = 4;  // MSAA 4x
    pipeDesc.label = "PBR_Pipeline";

    // Vertex layout: position(vec3), normal(vec3), uv(vec2), color(vec4), tangent(vec4), boneWeights(vec4), boneIndices(u32x4)
    Renderer::GPUVertexBufferLayoutDesc vertLayout;
    vertLayout.stride = sizeof(MeshComponent::Vertex);
    vertLayout.attributes = {
        {Renderer::GPUVertexFormat::Float32x3, 0, 0},                                                            // position
        {Renderer::GPUVertexFormat::Float32x3, static_cast<u32>(offsetof(MeshComponent::Vertex, normal)), 1},    // normal
        {Renderer::GPUVertexFormat::Float32x2, static_cast<u32>(offsetof(MeshComponent::Vertex, uv)), 2},       // uv
        {Renderer::GPUVertexFormat::Float32x4, static_cast<u32>(offsetof(MeshComponent::Vertex, tangent)), 3},   // tangent
        {Renderer::GPUVertexFormat::Float32x4, static_cast<u32>(offsetof(MeshComponent::Vertex, boneWeights)), 4}, // boneWeights
        {Renderer::GPUVertexFormat::Uint32x4,  static_cast<u32>(offsetof(MeshComponent::Vertex, boneIndices)), 5}, // boneIndices
    };
    pipeDesc.vertexBuffers = {vertLayout};

    m_MainPipeline = pipeMgr->CreateRenderPipeline(pipeDesc);
    if (!m_MainPipeline.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "RenderSystem: Pipeline creation failed");
        return;
    }

    // Create uniform buffers
    Renderer::GPUBufferDesc bufDesc;
    bufDesc.usage = Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst;
    bufDesc.hostVisible = true;

    bufDesc.size = sizeof(WebViewProjectionUBO);
    bufDesc.label = "ViewProjUBO";
    m_WebViewProjBuffer = bufMgr->CreateBuffer(bufDesc);

    bufDesc.size = sizeof(WebLightingUBO);
    bufDesc.label = "LightingUBO";
    m_WebLightingBuffer = bufMgr->CreateBuffer(bufDesc);

    {
        Renderer::GPUBufferDesc objBufDesc;
        objBufDesc.size = sizeof(WebObjectDataUBO);
        objBufDesc.usage = Renderer::GPUBufferUsage::Storage | Renderer::GPUBufferUsage::CopyDst;
        objBufDesc.hostVisible = true;
        objBufDesc.label = "ObjectDataSSBO";
        m_WebObjectBuffer = bufMgr->CreateBuffer(objBufDesc);
    }

    // Create default bone buffer (single identity matrix)
    {
        Math::Matrix4 identity;  // default constructor = identity
        Renderer::GPUBufferDesc boneDesc;
        boneDesc.size = sizeof(Math::Matrix4);
        boneDesc.usage = Renderer::GPUBufferUsage::Storage | Renderer::GPUBufferUsage::CopyDst;
        boneDesc.hostVisible = true;
        boneDesc.label = "DefaultBoneSSBO";
        m_WebDefaultBoneBuffer = bufMgr->CreateBufferWithData(boneDesc, &identity);
    }

    ENJIN_LOG_INFO(Renderer, "RenderSystem: Main pipeline created, setting up shadow mapping...");

    // Create shadow map depth texture (non-fatal — rendering works without shadows)
    {
        Renderer::GPUTextureDesc smDesc;
        smDesc.width = WEB_SHADOW_MAP_SIZE;
        smDesc.height = WEB_SHADOW_MAP_SIZE;
        smDesc.format = Renderer::GPUTextureFormat::Depth32Float;
        smDesc.usage = Renderer::GPUTextureUsage::RenderAttachment | Renderer::GPUTextureUsage::Sampled;
        smDesc.label = "ShadowMap";
        m_WebShadowMapTex = texMgr->CreateTexture(smDesc);
        if (!m_WebShadowMapTex.IsValid()) {
            ENJIN_LOG_WARN(Renderer, "RenderSystem: Shadow map texture creation failed — shadows disabled");
        }
    }

    // Create shadow pipeline (depth-only, uses shadow.wgsl)
    if (m_WebShadowMapTex.IsValid()) {
        m_WebShadowShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::SHADOW_WGSL,
            std::strlen(Renderer::WebShaderData::SHADOW_WGSL),
            Renderer::GPUShaderStage::Vertex, "Shadow_VS");

        // Shadow frame layout (group 0: light VP only)
        Renderer::GPUBindGroupLayoutDesc shadowFrameLD;
        shadowFrameLD.entries = {
            {0, BType::UniformBuffer, SStage::Vertex, sizeof(WebViewProjectionUBO)},
        };
        m_WebShadowFrameLayout = bindMgr->CreateBindGroupLayout(shadowFrameLD);

        // Shadow object layout (group 1: model matrix)
        Renderer::GPUBindGroupLayoutDesc shadowObjLD;
        shadowObjLD.entries = {
            {0, BType::UniformBuffer, SStage::Vertex, sizeof(WebObjectDataUBO)},
        };
        m_WebShadowObjectLayout = bindMgr->CreateBindGroupLayout(shadowObjLD);

        Renderer::GPURenderPipelineDesc shadowPipeDesc;
        shadowPipeDesc.vertexShader = m_WebShadowShader;
        shadowPipeDesc.fragmentShader = {};  // No fragment shader (depth-only)
        shadowPipeDesc.bindGroupLayouts = {m_WebShadowFrameLayout, m_WebShadowObjectLayout};
        shadowPipeDesc.topology = Renderer::GPUPrimitiveTopology::TriangleList;
        shadowPipeDesc.cullMode = Renderer::GPUCullMode::Front;  // Front-face culling reduces shadow acne
        shadowPipeDesc.frontFace = Renderer::GPUFrontFace::CCW;
        shadowPipeDesc.depthTest = true;
        shadowPipeDesc.depthWrite = true;
        shadowPipeDesc.depthCompare = Renderer::GPUCompareFunction::Less;
        shadowPipeDesc.hasColorAttachment = false;
        shadowPipeDesc.depthFormat = Renderer::GPUTextureFormat::Depth32Float;
        shadowPipeDesc.depthBiasEnable = true;
        shadowPipeDesc.depthBiasConstant = 2.0f;
        shadowPipeDesc.depthBiasSlope = 1.5f;
        shadowPipeDesc.label = "ShadowPipeline";

        // Shadow uses only position (location 0)
        Renderer::GPUVertexBufferLayoutDesc shadowVertLayout;
        shadowVertLayout.stride = sizeof(MeshComponent::Vertex);
        shadowVertLayout.attributes = {
            {Renderer::GPUVertexFormat::Float32x3, 0, 0},  // position only
        };
        shadowPipeDesc.vertexBuffers = {shadowVertLayout};

        m_WebShadowPipeline = pipeMgr->CreateRenderPipeline(shadowPipeDesc);

        // Shadow UBOs
        Renderer::GPUBufferDesc svpDesc;
        svpDesc.size = sizeof(WebViewProjectionUBO);
        svpDesc.usage = Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst;
        svpDesc.hostVisible = true;
        svpDesc.label = "ShadowVP_UBO";
        m_WebShadowVPBuffer = bufMgr->CreateBuffer(svpDesc);

        svpDesc.size = sizeof(WebObjectDataUBO);
        svpDesc.label = "ShadowObj_UBO";
        m_WebShadowObjectBuffer = bufMgr->CreateBuffer(svpDesc);

        // Shadow bind groups
        Renderer::GPUBindGroupDesc sfbg;
        sfbg.layout = m_WebShadowFrameLayout;
        sfbg.entries = {{0, m_WebShadowVPBuffer, 0, sizeof(WebViewProjectionUBO), {}, {}}};
        m_WebShadowFrameBG = bindMgr->CreateBindGroup(sfbg);

        Renderer::GPUBindGroupDesc sobg;
        sobg.layout = m_WebShadowObjectLayout;
        sobg.entries = {{0, m_WebShadowObjectBuffer, 0, sizeof(WebObjectDataUBO), {}, {}}};
        m_WebShadowObjectBG = bindMgr->CreateBindGroup(sobg);
    }

    // Create spot shadow map textures (2 individual 2D depth textures)
    for (u32 i = 0; i < WEB_MAX_SPOT_SHADOWS; i++) {
        Renderer::GPUTextureDesc spotSmDesc;
        spotSmDesc.width = WEB_SPOT_SHADOW_SIZE;
        spotSmDesc.height = WEB_SPOT_SHADOW_SIZE;
        spotSmDesc.format = Renderer::GPUTextureFormat::Depth32Float;
        spotSmDesc.usage = Renderer::GPUTextureUsage::RenderAttachment | Renderer::GPUTextureUsage::Sampled;
        m_WebSpotShadowTex[i] = texMgr->CreateTexture(spotSmDesc);
    }

    // Create spot shadow VP UBO
    {
        Renderer::GPUBufferDesc spotVPDesc;
        spotVPDesc.size = sizeof(WebSpotShadowVPUBO);
        spotVPDesc.usage = Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst;
        spotVPDesc.hostVisible = true;
        spotVPDesc.label = "SpotShadowVP_UBO";
        m_WebSpotShadowVPBuffer = bufMgr->CreateBuffer(spotVPDesc);
    }

    // Create point shadow cubemap + per-face views
    {
        auto* webRenderer = static_cast<Renderer::WebGPURenderer*>(m_Renderer);
        auto nativeCubemap = webRenderer->CreateCubemapTexture(
            WEB_POINT_SHADOW_SIZE, WGPUTextureFormat_Depth32Float,
            WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding);

        // Store cubemap as a managed texture handle (for bind group usage)
        auto* webTexMgr = static_cast<Renderer::WebGPUTextureManager*>(texMgr);
        m_WebPointShadowCubemap = webTexMgr->RegisterNativeTexture(nativeCubemap);

        // Create per-face 2D views for rendering
        for (u32 f = 0; f < 6; f++) {
            m_WebPointShadowFaceViews[f] = webRenderer->CreateCubeFaceView(
                nativeCubemap.texture, WGPUTextureFormat_Depth32Float, f);
        }
    }

    // Create point shadow VP UBO
    {
        Renderer::GPUBufferDesc ptVPDesc;
        ptVPDesc.size = sizeof(WebPointShadowVPUBO);
        ptVPDesc.usage = Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst;
        ptVPDesc.hostVisible = true;
        ptVPDesc.label = "PointShadowVP_UBO";
        m_WebPointShadowVPBuffer = bufMgr->CreateBuffer(ptVPDesc);
    }

    // Create default textures
    m_WebDefaultWhiteTex = texMgr->CreateSolidColor(255, 255, 255, 255);
    m_WebDefaultNormalTex = texMgr->CreateSolidColor(128, 128, 255, 255);  // flat +Z normal
    m_WebDefaultBlackTex = texMgr->CreateSolidColor(0, 255, 0, 255);      // metallic=0, roughness=1

    // Create frame bind group (group 0)
    Renderer::GPUBindGroupDesc frameBGDesc;
    frameBGDesc.layout = m_WebFrameLayout;
    frameBGDesc.entries = {
        {0, m_WebViewProjBuffer, 0, sizeof(WebViewProjectionUBO), {}, {}},
        {1, m_WebLightingBuffer, 0, sizeof(WebLightingUBO), {}, {}},
    };
    m_WebFrameBindGroup = bindMgr->CreateBindGroup(frameBGDesc);
    if (!m_WebFrameBindGroup.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "RenderSystem: Failed to create frame bind group");
        return;
    }

    // Create object bind group (group 1: ObjectData UBO + bone SSBO)
    Renderer::GPUBindGroupDesc objBGDesc;
    objBGDesc.layout = m_WebObjectLayout;
    objBGDesc.entries = {
        {0, m_WebObjectBuffer, 0, sizeof(WebObjectDataUBO), {}, {}},
        {1, m_WebDefaultBoneBuffer, 0, 0, {}, {}},
    };
    m_WebObjectBindGroup = bindMgr->CreateBindGroup(objBGDesc);

    // Create default texture bind group (group 2)
    Renderer::GPUBindGroupDesc defTexBGDesc;
    defTexBGDesc.layout = m_WebTextureLayout;
    defTexBGDesc.entries = {
        {0, {}, 0, 0, m_WebDefaultWhiteTex, {}},
        {1, {}, 0, 0, {}, m_WebDefaultWhiteTex},   // sampler from white tex
        {2, {}, 0, 0, m_WebDefaultNormalTex, {}},
        {3, {}, 0, 0, {}, m_WebDefaultNormalTex},
        {4, {}, 0, 0, m_WebDefaultBlackTex, {}},
        {5, {}, 0, 0, {}, m_WebDefaultBlackTex},
    };
    m_WebDefaultTexBindGroup = bindMgr->CreateBindGroup(defTexBGDesc);

    // Create shadow sample bind group (group 3: all shadow maps)
    if (m_WebShadowMapTex.IsValid() && m_WebShadowVPBuffer.IsValid()) {
        Renderer::GPUBindGroupDesc shadowSampleBGDesc;
        shadowSampleBGDesc.layout = m_WebShadowSampleLayout;
        shadowSampleBGDesc.entries = {
            {0, m_WebShadowVPBuffer, 0, sizeof(WebViewProjectionUBO), {}, {}},     // dir shadow VP
            {1, {}, 0, 0, m_WebShadowMapTex, {}},                                  // dir shadow depth
            {2, {}, 0, 0, {}, m_WebShadowMapTex},                                  // comparison sampler
            {3, m_WebSpotShadowVPBuffer, 0, sizeof(WebSpotShadowVPUBO), {}, {}},   // spot shadow VPs
            {4, {}, 0, 0, m_WebSpotShadowTex[0], {}},                              // spot shadow 0
            {5, {}, 0, 0, m_WebSpotShadowTex[1], {}},                              // spot shadow 1
            {6, m_WebPointShadowVPBuffer, 0, sizeof(WebPointShadowVPUBO), {}, {}}, // point shadow VPs
            {7, {}, 0, 0, m_WebPointShadowCubemap, {}},                            // point shadow cubemap
        };
        m_WebShadowSampleBG = bindMgr->CreateBindGroup(shadowSampleBGDesc);
        if (m_WebShadowSampleBG.IsValid()) {
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Shadow sample bind group created (group 3, dir+spot+point)");
        }
    }

    // Post-processing: offscreen scene texture + ACES tonemap pass
    {
        auto* webRenderer = static_cast<Renderer::WebGPURenderer*>(m_Renderer);
        u32 sceneW = m_Renderer->GetSwapchainWidth();
        u32 sceneH = m_Renderer->GetSwapchainHeight();

        // Offscreen scene color texture (RGBA16Float for HDR)
        WGPUTextureDescriptor sceneTexDesc = {};
        sceneTexDesc.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
        sceneTexDesc.dimension = WGPUTextureDimension_2D;
        sceneTexDesc.size = {sceneW, sceneH, 1};
        sceneTexDesc.format = WGPUTextureFormat_RGBA16Float;
        sceneTexDesc.mipLevelCount = 1;
        sceneTexDesc.sampleCount = 1;
        WGPUTexture sceneTex = wgpuDeviceCreateTexture(webRenderer->GetDevice(), &sceneTexDesc);
        WGPUTextureViewDescriptor sceneViewDesc = {};
        sceneViewDesc.format = WGPUTextureFormat_RGBA16Float;
        sceneViewDesc.dimension = WGPUTextureViewDimension_2D;
        sceneViewDesc.mipLevelCount = 1;
        sceneViewDesc.arrayLayerCount = 1;
        m_WebSceneColorView = wgpuTextureCreateView(sceneTex, &sceneViewDesc);
        // Register with texture manager for bind group creation
        Renderer::WebGPUTextureHandle nativeSceneTex;
        nativeSceneTex.texture = sceneTex;
        nativeSceneTex.view = static_cast<WGPUTextureView>(m_WebSceneColorView);
        nativeSceneTex.width = sceneW;
        nativeSceneTex.height = sceneH;
        nativeSceneTex.format = WGPUTextureFormat_RGBA16Float;
        // Create a linear sampler for the scene texture
        WGPUSamplerDescriptor smpDesc = {};
        smpDesc.addressModeU = WGPUAddressMode_ClampToEdge;
        smpDesc.addressModeV = WGPUAddressMode_ClampToEdge;
        smpDesc.addressModeW = WGPUAddressMode_ClampToEdge;
        smpDesc.magFilter = WGPUFilterMode_Linear;
        smpDesc.minFilter = WGPUFilterMode_Linear;
        smpDesc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
        smpDesc.maxAnisotropy = 1;
        nativeSceneTex.sampler = wgpuDeviceCreateSampler(webRenderer->GetDevice(), &smpDesc);
        auto* webTexMgr = static_cast<Renderer::WebGPUTextureManager*>(texMgr);
        m_WebSceneColorTex = webTexMgr->RegisterNativeTexture(nativeSceneTex);

        // MSAA 4x color texture (render target, resolved to sceneColorTex)
        WGPUTextureDescriptor msaaColorDesc = {};
        msaaColorDesc.usage = WGPUTextureUsage_RenderAttachment;
        msaaColorDesc.dimension = WGPUTextureDimension_2D;
        msaaColorDesc.size = {sceneW, sceneH, 1};
        msaaColorDesc.format = WGPUTextureFormat_RGBA16Float;
        msaaColorDesc.mipLevelCount = 1;
        msaaColorDesc.sampleCount = 4;
        WGPUTexture msaaTex = wgpuDeviceCreateTexture(webRenderer->GetDevice(), &msaaColorDesc);
        WGPUTextureViewDescriptor msaaViewDesc = {};
        msaaViewDesc.format = WGPUTextureFormat_RGBA16Float;
        msaaViewDesc.dimension = WGPUTextureViewDimension_2D;
        msaaViewDesc.mipLevelCount = 1;
        msaaViewDesc.arrayLayerCount = 1;
        m_WebMSAAColorTex = msaaTex;
        m_WebMSAAColorView = wgpuTextureCreateView(msaaTex, &msaaViewDesc);

        // Offscreen depth texture (4x MSAA, matches pipeline sample count)
        WGPUTextureDescriptor depthDesc = {};
        depthDesc.usage = WGPUTextureUsage_RenderAttachment;
        depthDesc.dimension = WGPUTextureDimension_2D;
        depthDesc.size = {sceneW, sceneH, 1};
        depthDesc.format = Renderer::GetDepthStencilFormat();
        depthDesc.mipLevelCount = 1;
        depthDesc.sampleCount = 4;
        WGPUTexture depthTex = wgpuDeviceCreateTexture(webRenderer->GetDevice(), &depthDesc);
        WGPUTextureViewDescriptor depthViewDesc = {};
        depthViewDesc.format = Renderer::GetDepthStencilFormat();
        depthViewDesc.dimension = WGPUTextureViewDimension_2D;
        depthViewDesc.mipLevelCount = 1;
        depthViewDesc.arrayLayerCount = 1;
        m_WebSceneDepthTex = depthTex;
        m_WebSceneDepthView = wgpuTextureCreateView(depthTex, &depthViewDesc);

        // Compile post-process shader
        m_WebPostProcessShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::POSTPROCESS_WGSL,
            std::strlen(Renderer::WebShaderData::POSTPROCESS_WGSL),
            Renderer::GPUShaderStage::Vertex, "PostProcess");

        // Create accessibility uniform buffer for post-process
        {
            Renderer::GPUBufferDesc ppParamsBufDesc;
            ppParamsBufDesc.size = sizeof(WebPPAccessibilityParams);
            ppParamsBufDesc.usage = Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst;
            ppParamsBufDesc.hostVisible = true;
            ppParamsBufDesc.label = "PostProcessParams";
            m_WebPPAccessibilityBuffer = bufMgr->CreateBufferWithData(ppParamsBufDesc, &m_WebPPAccessibility);
        }

        // Post-process bind group layout (group 0: texture + sampler + params UBO)
        Renderer::GPUBindGroupLayoutDesc ppLayoutDesc;
        ppLayoutDesc.entries = {
            {0, BType::SampledTexture, SStage::Fragment, 0},
            {1, BType::Sampler, SStage::Fragment, 0},
            {2, BType::UniformBuffer, SStage::Fragment, sizeof(WebPPAccessibilityParams)},
        };
        m_WebPostProcessLayout = bindMgr->CreateBindGroupLayout(ppLayoutDesc);

        // Post-process pipeline (fullscreen triangle, no depth, writes to swapchain format)
        Renderer::GPURenderPipelineDesc ppPipeDesc;
        ppPipeDesc.vertexShader = m_WebPostProcessShader;
        ppPipeDesc.fragmentShader = m_WebPostProcessShader;
        ppPipeDesc.bindGroupLayouts = {m_WebPostProcessLayout};
        ppPipeDesc.topology = Renderer::GPUPrimitiveTopology::TriangleList;
        ppPipeDesc.cullMode = Renderer::GPUCullMode::None;
        ppPipeDesc.frontFace = Renderer::GPUFrontFace::CCW;
        ppPipeDesc.depthTest = false;
        ppPipeDesc.depthWrite = false;
        ppPipeDesc.hasColorAttachment = true;
        ppPipeDesc.colorFormat = Renderer::GPUTextureFormat::BGRA8Unorm;
        ppPipeDesc.alphaBlend = false;
        ppPipeDesc.label = "PostProcessPipeline";
        // No vertex attributes — fullscreen triangle from vertex_index
        m_WebPostProcessPipeline = pipeMgr->CreateRenderPipeline(ppPipeDesc);

        // Post-process bind group (texture + sampler + params UBO from scene color)
        Renderer::GPUBindGroupDesc ppBGDesc;
        ppBGDesc.layout = m_WebPostProcessLayout;
        ppBGDesc.entries = {
            {0, {}, 0, 0, m_WebSceneColorTex, {}},  // texture
            {1, {}, 0, 0, {}, m_WebSceneColorTex},   // sampler
            {2, m_WebPPAccessibilityBuffer, 0, sizeof(WebPPAccessibilityParams), {}, {}},  // params UBO
        };
        m_WebPostProcessBG = bindMgr->CreateBindGroup(ppBGDesc);

        if (m_WebPostProcessPipeline.IsValid() && m_WebPostProcessBG.IsValid()) {
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Post-processing initialized (ACES tonemap)");
        }

        // Procedural sky shader + pipeline (renders after scene, same offscreen target)
        m_WebSkyShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::SKY_WGSL,
            std::strlen(Renderer::WebShaderData::SKY_WGSL),
            Renderer::GPUShaderStage::Vertex, "Sky");

        Renderer::GPURenderPipelineDesc skyPipeDesc;
        skyPipeDesc.vertexShader = m_WebSkyShader;
        skyPipeDesc.fragmentShader = m_WebSkyShader;
        skyPipeDesc.bindGroupLayouts = {m_WebFrameLayout};  // Reuse frame layout (ViewProj at group 0)
        skyPipeDesc.topology = Renderer::GPUPrimitiveTopology::TriangleList;
        skyPipeDesc.cullMode = Renderer::GPUCullMode::None;
        skyPipeDesc.frontFace = Renderer::GPUFrontFace::CCW;
        skyPipeDesc.depthTest = true;
        skyPipeDesc.depthWrite = false;     // Don't overwrite scene depth
        skyPipeDesc.depthCompare = Renderer::GPUCompareFunction::LessEqual;  // z=1.0 passes at max depth
        skyPipeDesc.hasColorAttachment = true;
        skyPipeDesc.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;
        skyPipeDesc.depthFormat = Renderer::GPUTextureFormat::Depth24PlusStencil8;
        skyPipeDesc.sampleCount = 4;  // Must match scene MSAA
        skyPipeDesc.alphaBlend = false;
        skyPipeDesc.label = "SkyPipeline";
        m_WebSkyPipeline = pipeMgr->CreateRenderPipeline(skyPipeDesc);
        if (m_WebSkyPipeline.IsValid()) {
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Procedural sky pipeline initialized");
        }

        // Bloom: compile shaders, create pipelines, allocate mip chain textures
        m_WebBloomThresholdShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::BLOOM_THRESHOLD_WGSL,
            std::strlen(Renderer::WebShaderData::BLOOM_THRESHOLD_WGSL),
            Renderer::GPUShaderStage::Vertex, "BloomThreshold");
        m_WebBloomDownShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::BLOOM_DOWN_WGSL,
            std::strlen(Renderer::WebShaderData::BLOOM_DOWN_WGSL),
            Renderer::GPUShaderStage::Vertex, "BloomDown");
        m_WebBloomUpShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::BLOOM_UP_WGSL,
            std::strlen(Renderer::WebShaderData::BLOOM_UP_WGSL),
            Renderer::GPUShaderStage::Vertex, "BloomUp");
        m_WebBloomCompositeShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::BLOOM_COMPOSITE_WGSL,
            std::strlen(Renderer::WebShaderData::BLOOM_COMPOSITE_WGSL),
            Renderer::GPUShaderStage::Vertex, "BloomComposite");

        // Single-texture layout (threshold, downsample, upsample all use 1 tex + 1 sampler)
        Renderer::GPUBindGroupLayoutDesc bloomTexLD;
        bloomTexLD.entries = {
            {0, BType::SampledTexture, SStage::Fragment, 0},
            {1, BType::Sampler, SStage::Fragment, 0},
        };
        m_WebBloomSingleTexLayout = bindMgr->CreateBindGroupLayout(bloomTexLD);

        // Composite layout (scene + bloom, 2 textures + 2 samplers)
        Renderer::GPUBindGroupLayoutDesc bloomCompLD;
        bloomCompLD.entries = {
            {0, BType::SampledTexture, SStage::Fragment, 0},
            {1, BType::Sampler, SStage::Fragment, 0},
            {2, BType::SampledTexture, SStage::Fragment, 0},
            {3, BType::Sampler, SStage::Fragment, 0},
        };
        m_WebBloomCompositeLayout = bindMgr->CreateBindGroupLayout(bloomCompLD);

        // Bloom pipelines (all fullscreen triangle, no depth, RGBA16Float)
        auto makeBloomPipe = [&](Renderer::GPUShaderHandle shader, Renderer::GPUBindGroupLayoutHandle layout, const char* label) {
            Renderer::GPURenderPipelineDesc pd;
            pd.vertexShader = shader;
            pd.fragmentShader = shader;
            pd.bindGroupLayouts = {layout};
            pd.topology = Renderer::GPUPrimitiveTopology::TriangleList;
            pd.cullMode = Renderer::GPUCullMode::None;
            pd.frontFace = Renderer::GPUFrontFace::CCW;
            pd.depthTest = false;
            pd.depthWrite = false;
            pd.hasColorAttachment = true;
            pd.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;
            pd.alphaBlend = false;
            pd.label = label;
            return pipeMgr->CreateRenderPipeline(pd);
        };
        m_WebBloomThresholdPipeline = makeBloomPipe(m_WebBloomThresholdShader, m_WebBloomSingleTexLayout, "BloomThreshold");
        m_WebBloomDownPipeline = makeBloomPipe(m_WebBloomDownShader, m_WebBloomSingleTexLayout, "BloomDown");
        m_WebBloomCompositePipeline = makeBloomPipe(m_WebBloomCompositeShader, m_WebBloomCompositeLayout, "BloomComposite");
        // Upsample pipeline with additive blending (adds upsampled result to existing bloom level)
        {
            Renderer::GPURenderPipelineDesc pd;
            pd.vertexShader = m_WebBloomUpShader;
            pd.fragmentShader = m_WebBloomUpShader;
            pd.bindGroupLayouts = {m_WebBloomSingleTexLayout};
            pd.topology = Renderer::GPUPrimitiveTopology::TriangleList;
            pd.cullMode = Renderer::GPUCullMode::None;
            pd.frontFace = Renderer::GPUFrontFace::CCW;
            pd.depthTest = false;
            pd.depthWrite = false;
            pd.hasColorAttachment = true;
            pd.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;
            pd.alphaBlend = true;
            pd.blendState.srcColor = Renderer::GPUBlendFactor::One;
            pd.blendState.dstColor = Renderer::GPUBlendFactor::One;  // Additive
            pd.blendState.srcAlpha = Renderer::GPUBlendFactor::One;
            pd.blendState.dstAlpha = Renderer::GPUBlendFactor::One;
            pd.label = "BloomUp";
            m_WebBloomUpPipeline = pipeMgr->CreateRenderPipeline(pd);
        }

        // Create bloom mip chain textures (half-res each level)
        auto* webTexMgrB = static_cast<Renderer::WebGPUTextureManager*>(texMgr);
        u32 bw = sceneW / 2, bh = sceneH / 2;
        for (u32 i = 0; i < WEB_BLOOM_LEVELS; i++) {
            bw = std::max(bw, 1u);
            bh = std::max(bh, 1u);
            WGPUTextureDescriptor btd = {};
            btd.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
            btd.dimension = WGPUTextureDimension_2D;
            btd.size = {bw, bh, 1};
            btd.format = WGPUTextureFormat_RGBA16Float;
            btd.mipLevelCount = 1;
            btd.sampleCount = 1;
            WGPUTexture bt = wgpuDeviceCreateTexture(webRenderer->GetDevice(), &btd);
            WGPUTextureViewDescriptor bvd = {};
            bvd.format = WGPUTextureFormat_RGBA16Float;
            bvd.dimension = WGPUTextureViewDimension_2D;
            bvd.mipLevelCount = 1;
            bvd.arrayLayerCount = 1;
            m_WebBloomView[i] = wgpuTextureCreateView(bt, &bvd);
            Renderer::WebGPUTextureHandle nbt;
            nbt.texture = bt;
            nbt.view = static_cast<WGPUTextureView>(m_WebBloomView[i]);
            nbt.width = bw;
            nbt.height = bh;
            nbt.format = WGPUTextureFormat_RGBA16Float;
            nbt.sampler = nativeSceneTex.sampler;  // Reuse linear sampler
            m_WebBloomTex[i] = webTexMgrB->RegisterNativeTexture(nbt);
            bw /= 2;
            bh /= 2;
        }

        // Scratch texture for composite output (same size as scene)
        {
            WGPUTextureDescriptor std2 = {};
            std2.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
            std2.dimension = WGPUTextureDimension_2D;
            std2.size = {sceneW, sceneH, 1};
            std2.format = WGPUTextureFormat_RGBA16Float;
            std2.mipLevelCount = 1;
            std2.sampleCount = 1;
            WGPUTexture st = wgpuDeviceCreateTexture(webRenderer->GetDevice(), &std2);
            WGPUTextureViewDescriptor svd = {};
            svd.format = WGPUTextureFormat_RGBA16Float;
            svd.dimension = WGPUTextureViewDimension_2D;
            svd.mipLevelCount = 1;
            svd.arrayLayerCount = 1;
            m_WebBloomScratchView = wgpuTextureCreateView(st, &svd);
            Renderer::WebGPUTextureHandle nst;
            nst.texture = st;
            nst.view = static_cast<WGPUTextureView>(m_WebBloomScratchView);
            nst.width = sceneW;
            nst.height = sceneH;
            nst.format = WGPUTextureFormat_RGBA16Float;
            nst.sampler = nativeSceneTex.sampler;
            m_WebBloomScratchTex = webTexMgrB->RegisterNativeTexture(nst);
        }

        // Bind groups: threshold (scene → bloom[0])
        {
            Renderer::GPUBindGroupDesc bg;
            bg.layout = m_WebBloomSingleTexLayout;
            bg.entries = {{0, {}, 0, 0, m_WebSceneColorTex, {}}, {1, {}, 0, 0, {}, m_WebSceneColorTex}};
            m_WebBloomThresholdBG = bindMgr->CreateBindGroup(bg);
        }
        // Bind groups: downsample (bloom[i] → bloom[i+1])
        for (u32 i = 0; i < WEB_BLOOM_LEVELS; i++) {
            Renderer::GPUBindGroupDesc bg;
            bg.layout = m_WebBloomSingleTexLayout;
            Renderer::GPUTextureHandle src = (i == 0) ? m_WebBloomTex[0] : m_WebBloomTex[i];
            bg.entries = {{0, {}, 0, 0, src, {}}, {1, {}, 0, 0, {}, src}};
            m_WebBloomDownBG[i] = bindMgr->CreateBindGroup(bg);
        }
        // Bind groups: upsample (bloom[i+1] → read, blend into bloom[i])
        for (u32 i = 0; i < WEB_BLOOM_LEVELS; i++) {
            Renderer::GPUBindGroupDesc bg;
            bg.layout = m_WebBloomSingleTexLayout;
            bg.entries = {{0, {}, 0, 0, m_WebBloomTex[i], {}}, {1, {}, 0, 0, {}, m_WebBloomTex[i]}};
            m_WebBloomUpBG[i] = bindMgr->CreateBindGroup(bg);
        }
        // Bind group: composite (scene + bloom[0])
        {
            Renderer::GPUBindGroupDesc bg;
            bg.layout = m_WebBloomCompositeLayout;
            bg.entries = {
                {0, {}, 0, 0, m_WebSceneColorTex, {}}, {1, {}, 0, 0, {}, m_WebSceneColorTex},
                {2, {}, 0, 0, m_WebBloomTex[0], {}}, {3, {}, 0, 0, {}, m_WebBloomTex[0]},
            };
            m_WebBloomCompositeBG = bindMgr->CreateBindGroup(bg);
        }

        if (m_WebBloomThresholdPipeline.IsValid()) {
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Bloom initialized (Dual Kawase, %u levels)", WEB_BLOOM_LEVELS);
            // Update post-process BG to read from scratch (composited scene+bloom)
            bindMgr->DestroyBindGroup(m_WebPostProcessBG);
            Renderer::GPUBindGroupDesc ppBG2;
            ppBG2.layout = m_WebPostProcessLayout;
            ppBG2.entries = {
                {0, {}, 0, 0, m_WebBloomScratchTex, {}},
                {1, {}, 0, 0, {}, m_WebBloomScratchTex},
                {2, m_WebPPAccessibilityBuffer, 0, sizeof(WebPPAccessibilityParams), {}, {}},
            };
            m_WebPostProcessBG = bindMgr->CreateBindGroup(ppBG2);
        }
    }

    // ========================================================================
    // Particle pipeline + shared quad mesh
    // ========================================================================
    {
        m_WebParticleShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::PARTICLE_WGSL,
            std::strlen(Renderer::WebShaderData::PARTICLE_WGSL),
            Renderer::GPUShaderStage::Vertex, "Particle");

        Renderer::GPURenderPipelineDesc pd;
        pd.vertexShader = m_WebParticleShader;
        pd.fragmentShader = m_WebParticleShader;
        pd.bindGroupLayouts = {m_WebFrameLayout};
        pd.topology = Renderer::GPUPrimitiveTopology::TriangleList;
        pd.cullMode = Renderer::GPUCullMode::None;
        pd.frontFace = Renderer::GPUFrontFace::CCW;
        pd.depthTest = true;
        pd.depthWrite = false;  // Particles don't occlude
        pd.depthCompare = Renderer::GPUCompareFunction::Less;
        pd.alphaBlend = true;
        pd.blendState.srcColor = Renderer::GPUBlendFactor::SrcAlpha;
        pd.blendState.dstColor = Renderer::GPUBlendFactor::OneMinusSrcAlpha;
        pd.blendState.srcAlpha = Renderer::GPUBlendFactor::One;
        pd.blendState.dstAlpha = Renderer::GPUBlendFactor::OneMinusSrcAlpha;
        pd.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;
        pd.depthFormat = Renderer::GPUTextureFormat::Depth24PlusStencil8;
        pd.sampleCount = 4;
        pd.label = "ParticlePipeline";
        // Vertex layout: slot 0 = quad (per-vertex), slot 1 = instance data (per-instance)
        Renderer::GPUVertexBufferLayoutDesc quadLayout;
        quadLayout.stride = 4 * sizeof(f32);  // pos(vec2) + uv(vec2)
        quadLayout.perInstance = false;
        quadLayout.attributes = {
            {Renderer::GPUVertexFormat::Float32x2, 0, 0},                   // position
            {Renderer::GPUVertexFormat::Float32x2, 2 * sizeof(f32), 1},     // uv
        };
        Renderer::GPUVertexBufferLayoutDesc instanceLayout;
        instanceLayout.stride = 8 * sizeof(f32);  // pos(3) + size(1) + alpha(1) + rgb(3)
        instanceLayout.perInstance = true;
        instanceLayout.attributes = {
            {Renderer::GPUVertexFormat::Float32x3, 0, 2},                   // worldPos
            {Renderer::GPUVertexFormat::Float32, 3 * sizeof(f32), 3},       // size
            {Renderer::GPUVertexFormat::Float32, 4 * sizeof(f32), 4},       // alpha
            {Renderer::GPUVertexFormat::Float32, 5 * sizeof(f32), 5},       // colorR
            {Renderer::GPUVertexFormat::Float32, 6 * sizeof(f32), 6},       // colorG
            {Renderer::GPUVertexFormat::Float32, 7 * sizeof(f32), 7},       // colorB
        };
        pd.vertexBuffers = {quadLayout, instanceLayout};
        m_WebParticlePipeline = pipeMgr->CreateRenderPipeline(pd);

        // Shared billboard quad (4 vertices, 6 indices)
        f32 quadVerts[] = {
            -0.5f, -0.5f,  0.0f, 0.0f,  // bottom-left
             0.5f, -0.5f,  1.0f, 0.0f,  // bottom-right
             0.5f,  0.5f,  1.0f, 1.0f,  // top-right
            -0.5f,  0.5f,  0.0f, 1.0f,  // top-left
        };
        u32 quadIndices[] = {0, 1, 2, 0, 2, 3};
        Renderer::GPUBufferDesc qvb;
        qvb.size = sizeof(quadVerts);
        qvb.usage = Renderer::GPUBufferUsage::Vertex | Renderer::GPUBufferUsage::CopyDst;
        qvb.hostVisible = true;
        m_WebParticleQuadVB = bufMgr->CreateBufferWithData(qvb, quadVerts);
        Renderer::GPUBufferDesc qib;
        qib.size = sizeof(quadIndices);
        qib.usage = Renderer::GPUBufferUsage::Index | Renderer::GPUBufferUsage::CopyDst;
        qib.hostVisible = true;
        m_WebParticleQuadIB = bufMgr->CreateBufferWithData(qib, quadIndices);

        if (m_WebParticlePipeline.IsValid())
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Particle pipeline initialized");
    }

    // ========================================================================
    // Grass + Tree volume params bind group layout (shared)
    // ========================================================================
    {
        Renderer::GPUBindGroupLayoutDesc volLD;
        volLD.entries = {
            {0, BType::UniformBuffer, SStage::Vertex, 64},  // VolumeParams UBO
        };
        m_WebVolumeParamsLayout = bindMgr->CreateBindGroupLayout(volLD);
    }

    // ========================================================================
    // Grass pipeline + blade mesh
    // ========================================================================
    {
        m_WebGrassShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::GRASS_WGSL,
            std::strlen(Renderer::WebShaderData::GRASS_WGSL),
            Renderer::GPUShaderStage::Vertex, "Grass");

        Renderer::GPURenderPipelineDesc pd;
        pd.vertexShader = m_WebGrassShader;
        pd.fragmentShader = m_WebGrassShader;
        pd.bindGroupLayouts = {m_WebFrameLayout, m_WebVolumeParamsLayout};
        pd.topology = Renderer::GPUPrimitiveTopology::TriangleList;
        pd.cullMode = Renderer::GPUCullMode::None;
        pd.frontFace = Renderer::GPUFrontFace::CCW;
        pd.depthTest = true;
        pd.depthWrite = true;
        pd.depthCompare = Renderer::GPUCompareFunction::Less;
        pd.alphaBlend = false;
        pd.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;
        pd.depthFormat = Renderer::GPUTextureFormat::Depth24PlusStencil8;
        pd.sampleCount = 4;
        pd.label = "GrassPipeline";
        // Blade vertex: pos(vec3) + normal(vec3) + uv(vec2) = 8 floats
        Renderer::GPUVertexBufferLayoutDesc bladeLayout;
        bladeLayout.stride = 8 * sizeof(f32);
        bladeLayout.attributes = {
            {Renderer::GPUVertexFormat::Float32x3, 0, 0},
            {Renderer::GPUVertexFormat::Float32x3, 3 * sizeof(f32), 1},
            {Renderer::GPUVertexFormat::Float32x2, 6 * sizeof(f32), 2},
        };
        pd.vertexBuffers = {bladeLayout};
        m_WebGrassPipeline = pipeMgr->CreateRenderPipeline(pd);

        // 7-vertex blade mesh (tapered triangle strip)
        struct BladeVert { f32 px, py, pz, nx, ny, nz, u, v; };
        BladeVert bladeVerts[] = {
            {-0.5f, 0.0f, 0.0f,  0,0,1,  0.0f, 0.0f},  // v0 base-left
            { 0.5f, 0.0f, 0.0f,  0,0,1,  1.0f, 0.0f},  // v1 base-right
            {-0.4f, 0.33f, 0.0f, 0,0,1,  0.1f, 0.33f}, // v2 mid-lower-left
            { 0.4f, 0.33f, 0.0f, 0,0,1,  0.9f, 0.33f}, // v3 mid-lower-right
            {-0.2f, 0.66f, 0.0f, 0,0,1,  0.3f, 0.66f}, // v4 mid-upper-left
            { 0.2f, 0.66f, 0.0f, 0,0,1,  0.7f, 0.66f}, // v5 mid-upper-right
            { 0.0f, 1.0f, 0.0f,  0,0,1,  0.5f, 1.0f},  // v6 tip
        };
        u32 bladeIndices[] = {0,1,3, 0,3,2, 2,3,5, 2,5,4, 4,5,6};
        m_WebGrassBladeIndexCount = 15;
        Renderer::GPUBufferDesc bvb;
        bvb.size = sizeof(bladeVerts);
        bvb.usage = Renderer::GPUBufferUsage::Vertex | Renderer::GPUBufferUsage::CopyDst;
        bvb.hostVisible = true;
        m_WebGrassBladeVB = bufMgr->CreateBufferWithData(bvb, bladeVerts);
        Renderer::GPUBufferDesc bib;
        bib.size = sizeof(bladeIndices);
        bib.usage = Renderer::GPUBufferUsage::Index | Renderer::GPUBufferUsage::CopyDst;
        bib.hostVisible = true;
        m_WebGrassBladeIB = bufMgr->CreateBufferWithData(bib, bladeIndices);

        if (m_WebGrassPipeline.IsValid())
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Grass pipeline initialized");
    }

    // ========================================================================
    // Tree pipeline + mesh (trunk quads + canopy billboards)
    // ========================================================================
    {
        m_WebTreeShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::TREE_WGSL,
            std::strlen(Renderer::WebShaderData::TREE_WGSL),
            Renderer::GPUShaderStage::Vertex, "Tree");

        Renderer::GPURenderPipelineDesc pd;
        pd.vertexShader = m_WebTreeShader;
        pd.fragmentShader = m_WebTreeShader;
        pd.bindGroupLayouts = {m_WebFrameLayout, m_WebVolumeParamsLayout};
        pd.topology = Renderer::GPUPrimitiveTopology::TriangleList;
        pd.cullMode = Renderer::GPUCullMode::None;
        pd.frontFace = Renderer::GPUFrontFace::CCW;
        pd.depthTest = true;
        pd.depthWrite = true;
        pd.depthCompare = Renderer::GPUCompareFunction::Less;
        pd.alphaBlend = false;
        pd.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;
        pd.depthFormat = Renderer::GPUTextureFormat::Depth24PlusStencil8;
        pd.sampleCount = 4;
        pd.label = "TreePipeline";
        // Same vertex format as grass (pos+normal+uv)
        Renderer::GPUVertexBufferLayoutDesc treeLayout;
        treeLayout.stride = 8 * sizeof(f32);
        treeLayout.attributes = {
            {Renderer::GPUVertexFormat::Float32x3, 0, 0},
            {Renderer::GPUVertexFormat::Float32x3, 3 * sizeof(f32), 1},
            {Renderer::GPUVertexFormat::Float32x2, 6 * sizeof(f32), 2},
        };
        pd.vertexBuffers = {treeLayout};
        m_WebTreePipeline = pipeMgr->CreateRenderPipeline(pd);

        // Tree mesh: trunk (2 crossing quads) + canopy (3 crossing billboards)
        struct TreeVert { f32 px, py, pz, nx, ny, nz, u, v; };
        std::vector<TreeVert> treeVerts;
        std::vector<u32> treeIndices;

        auto addQuad = [&](Math::Vector3 bl, Math::Vector3 br, Math::Vector3 tr, Math::Vector3 tl,
                           f32 uvYBase, f32 uvYTop) {
            u32 base = static_cast<u32>(treeVerts.size());
            treeVerts.push_back({bl.x, bl.y, bl.z, 0,0,1, 0, uvYBase});
            treeVerts.push_back({br.x, br.y, br.z, 0,0,1, 1, uvYBase});
            treeVerts.push_back({tr.x, tr.y, tr.z, 0,0,1, 1, uvYTop});
            treeVerts.push_back({tl.x, tl.y, tl.z, 0,0,1, 0, uvYTop});
            treeIndices.insert(treeIndices.end(), {base, base+1, base+2, base, base+2, base+3});
        };

        // Trunk: 2 crossing quads along X and Z axes (uv.y < 0.5 = trunk)
        f32 tw = 0.5f, th = 1.0f;
        addQuad({-tw,0,0}, {tw,0,0}, {tw,th,0}, {-tw,th,0}, 0.0f, 0.4f);  // X-aligned
        addQuad({0,0,-tw}, {0,0,tw}, {0,th,tw}, {0,th,-tw}, 0.0f, 0.4f);  // Z-aligned
        // Canopy: 3 crossing billboard quads at 0°, 60°, 120° (uv.y > 0.5 = canopy)
        f32 cr = 1.5f, co = 1.2f;  // canopy radius/offset
        for (int q = 0; q < 3; q++) {
            f32 angle = static_cast<f32>(q) * 3.14159f / 3.0f;
            f32 cx = std::cos(angle) * cr, cz = std::sin(angle) * cr;
            addQuad({-cx, co, -cz}, {cx, co, cz}, {cx, co + cr*2, cz}, {-cx, co + cr*2, -cz}, 0.6f, 1.0f);
        }
        m_WebTreeIndexCount = static_cast<u32>(treeIndices.size());
        Renderer::GPUBufferDesc tvb;
        tvb.size = treeVerts.size() * sizeof(TreeVert);
        tvb.usage = Renderer::GPUBufferUsage::Vertex | Renderer::GPUBufferUsage::CopyDst;
        tvb.hostVisible = true;
        m_WebTreeMeshVB = bufMgr->CreateBufferWithData(tvb, treeVerts.data());
        Renderer::GPUBufferDesc tib;
        tib.size = treeIndices.size() * sizeof(u32);
        tib.usage = Renderer::GPUBufferUsage::Index | Renderer::GPUBufferUsage::CopyDst;
        tib.hostVisible = true;
        m_WebTreeMeshIB = bufMgr->CreateBufferWithData(tib, treeIndices.data());

        if (m_WebTreePipeline.IsValid())
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Tree pipeline initialized (%u verts, %u indices)",
                static_cast<u32>(treeVerts.size()), m_WebTreeIndexCount);
    }

    // ========================================================================
    // Sprite pipeline
    // ========================================================================
    {
        m_WebSpriteShader = shaderMgr->LoadShader(
            Renderer::WebShaderData::SPRITE_WGSL,
            std::strlen(Renderer::WebShaderData::SPRITE_WGSL),
            Renderer::GPUShaderStage::Vertex, "Sprite");

        // Sprite texture bind group layout (group 1: texture + sampler)
        Renderer::GPUBindGroupLayoutDesc sprTexLD;
        sprTexLD.entries = {
            {0, BType::SampledTexture, SStage::Fragment, 0},
            {1, BType::Sampler, SStage::Fragment, 0},
        };
        m_WebSpriteTexLayout = bindMgr->CreateBindGroupLayout(sprTexLD);

        Renderer::GPURenderPipelineDesc pd;
        pd.vertexShader = m_WebSpriteShader;
        pd.fragmentShader = m_WebSpriteShader;
        pd.bindGroupLayouts = {m_WebFrameLayout, m_WebSpriteTexLayout};
        pd.topology = Renderer::GPUPrimitiveTopology::TriangleList;
        pd.cullMode = Renderer::GPUCullMode::None;
        pd.frontFace = Renderer::GPUFrontFace::CCW;
        pd.depthTest = true;
        pd.depthWrite = false;
        pd.depthCompare = Renderer::GPUCompareFunction::Less;
        pd.alphaBlend = true;
        pd.blendState.srcColor = Renderer::GPUBlendFactor::SrcAlpha;
        pd.blendState.dstColor = Renderer::GPUBlendFactor::OneMinusSrcAlpha;
        pd.blendState.srcAlpha = Renderer::GPUBlendFactor::One;
        pd.blendState.dstAlpha = Renderer::GPUBlendFactor::OneMinusSrcAlpha;
        pd.colorFormat = Renderer::GPUTextureFormat::RGBA16Float;
        pd.depthFormat = Renderer::GPUTextureFormat::Depth24PlusStencil8;
        pd.sampleCount = 4;
        pd.label = "SpritePipeline";
        // Vertex layout: slot 0 = quad, slot 1 = sprite instance
        Renderer::GPUVertexBufferLayoutDesc sprQuadLayout;
        sprQuadLayout.stride = 4 * sizeof(f32);  // pos(vec2) + uv(vec2)
        sprQuadLayout.perInstance = false;
        sprQuadLayout.attributes = {
            {Renderer::GPUVertexFormat::Float32x2, 0, 0},
            {Renderer::GPUVertexFormat::Float32x2, 2 * sizeof(f32), 1},
        };
        Renderer::GPUVertexBufferLayoutDesc sprInstLayout;
        sprInstLayout.stride = 14 * sizeof(f32);  // worldPos(3)+sizeX+sizeY+rot+tintRGBA+uvRect(4)
        sprInstLayout.perInstance = true;
        sprInstLayout.attributes = {
            {Renderer::GPUVertexFormat::Float32x3, 0, 2},                    // worldPos
            {Renderer::GPUVertexFormat::Float32, 3 * sizeof(f32), 3},        // sizeX
            {Renderer::GPUVertexFormat::Float32, 4 * sizeof(f32), 4},        // sizeY
            {Renderer::GPUVertexFormat::Float32, 5 * sizeof(f32), 5},        // rotation
            {Renderer::GPUVertexFormat::Float32, 6 * sizeof(f32), 6},        // tintR
            {Renderer::GPUVertexFormat::Float32, 7 * sizeof(f32), 7},        // tintG
            {Renderer::GPUVertexFormat::Float32, 8 * sizeof(f32), 8},        // tintB
            {Renderer::GPUVertexFormat::Float32, 9 * sizeof(f32), 9},        // tintA
            {Renderer::GPUVertexFormat::Float32, 10 * sizeof(f32), 10},      // uvLeft
            {Renderer::GPUVertexFormat::Float32, 11 * sizeof(f32), 11},      // uvTop
            {Renderer::GPUVertexFormat::Float32, 12 * sizeof(f32), 12},      // uvRight
            {Renderer::GPUVertexFormat::Float32, 13 * sizeof(f32), 13},      // uvBottom
        };
        pd.vertexBuffers = {sprQuadLayout, sprInstLayout};
        m_WebSpritePipeline = pipeMgr->CreateRenderPipeline(pd);

        if (m_WebSpritePipeline.IsValid())
            ENJIN_LOG_INFO(Renderer, "RenderSystem: Sprite pipeline initialized");
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "WebGPU RenderSystem initialized");
}

void RenderSystem::Shutdown() {
    if (!m_Initialized) return;

    if (m_Renderer) m_Renderer->WaitForAllFrames();

    auto* bufMgr = m_Renderer ? m_Renderer->GetBufferManager() : nullptr;
    auto* texMgr = m_Renderer ? m_Renderer->GetTextureManager() : nullptr;
    auto* pipeMgr = m_Renderer ? m_Renderer->GetPipelineManager() : nullptr;
    auto* bindMgr = m_Renderer ? m_Renderer->GetBindGroupManager() : nullptr;
    auto* shaderMgr = m_Renderer ? m_Renderer->GetShaderManager() : nullptr;

    // Destroy entity GPU data
    if (bufMgr) {
        for (auto& rd : m_EntityRenderData) {
            if (rd.valid) {
                if (rd.vertexBuffer.IsValid()) bufMgr->DestroyBuffer(rd.vertexBuffer);
                if (rd.indexBuffer.IsValid()) bufMgr->DestroyBuffer(rd.indexBuffer);
                rd.Invalidate();
            }
        }
    }

    // Destroy bind groups
    if (bindMgr) {
        if (m_WebFrameBindGroup.IsValid()) bindMgr->DestroyBindGroup(m_WebFrameBindGroup);
        if (m_WebObjectBindGroup.IsValid()) bindMgr->DestroyBindGroup(m_WebObjectBindGroup);
        if (m_WebDefaultTexBindGroup.IsValid()) bindMgr->DestroyBindGroup(m_WebDefaultTexBindGroup);
        if (m_WebFrameLayout.IsValid()) bindMgr->DestroyBindGroupLayout(m_WebFrameLayout);
        if (m_WebObjectLayout.IsValid()) bindMgr->DestroyBindGroupLayout(m_WebObjectLayout);
        if (m_WebTextureLayout.IsValid()) bindMgr->DestroyBindGroupLayout(m_WebTextureLayout);
    }

    // Destroy buffers
    if (bufMgr) {
        if (m_WebViewProjBuffer.IsValid()) bufMgr->DestroyBuffer(m_WebViewProjBuffer);
        if (m_WebLightingBuffer.IsValid()) bufMgr->DestroyBuffer(m_WebLightingBuffer);
        if (m_WebObjectBuffer.IsValid()) bufMgr->DestroyBuffer(m_WebObjectBuffer);
        if (m_WebDefaultBoneBuffer.IsValid()) bufMgr->DestroyBuffer(m_WebDefaultBoneBuffer);
    }

    // Destroy cached textures
    if (texMgr) {
        for (auto& [path, handle] : m_WebTextureCache) {
            if (handle.IsValid()) texMgr->DestroyTexture(handle);
        }
        m_WebTextureCache.clear();
        m_WebFailedTextures.clear();
        if (m_WebDefaultWhiteTex.IsValid()) texMgr->DestroyTexture(m_WebDefaultWhiteTex);
        if (m_WebDefaultNormalTex.IsValid()) texMgr->DestroyTexture(m_WebDefaultNormalTex);
        if (m_WebDefaultBlackTex.IsValid()) texMgr->DestroyTexture(m_WebDefaultBlackTex);
    }

    // Destroy shadow resources
    if (bindMgr) {
        if (m_WebShadowSampleBG.IsValid()) bindMgr->DestroyBindGroup(m_WebShadowSampleBG);
        if (m_WebShadowSampleLayout.IsValid()) bindMgr->DestroyBindGroupLayout(m_WebShadowSampleLayout);
        if (m_WebShadowFrameBG.IsValid()) bindMgr->DestroyBindGroup(m_WebShadowFrameBG);
        if (m_WebShadowObjectBG.IsValid()) bindMgr->DestroyBindGroup(m_WebShadowObjectBG);
        if (m_WebShadowFrameLayout.IsValid()) bindMgr->DestroyBindGroupLayout(m_WebShadowFrameLayout);
        if (m_WebShadowObjectLayout.IsValid()) bindMgr->DestroyBindGroupLayout(m_WebShadowObjectLayout);
    }
    if (bufMgr) {
        if (m_WebShadowVPBuffer.IsValid()) bufMgr->DestroyBuffer(m_WebShadowVPBuffer);
        if (m_WebShadowObjectBuffer.IsValid()) bufMgr->DestroyBuffer(m_WebShadowObjectBuffer);
    }
    if (texMgr && m_WebShadowMapTex.IsValid()) texMgr->DestroyTexture(m_WebShadowMapTex);
    if (pipeMgr && m_WebShadowPipeline.IsValid()) pipeMgr->DestroyPipeline(m_WebShadowPipeline);
    if (shaderMgr && m_WebShadowShader.IsValid()) shaderMgr->DestroyShader(m_WebShadowShader);

    // Destroy pipeline and shaders
    if (pipeMgr && m_MainPipeline.IsValid()) pipeMgr->DestroyPipeline(m_MainPipeline);
    if (shaderMgr && m_MainVertexShader.IsValid()) shaderMgr->DestroyShader(m_MainVertexShader);

    m_Initialized = false;
}

// ============================================================================
// Texture loading (WebGPU)
// ============================================================================

Renderer::GPUTextureHandle RenderSystem::WebGetOrLoadTexture(const std::string& path) {
    if (path.empty()) return {};

    // Check cache
    auto it = m_WebTextureCache.find(path);
    if (it != m_WebTextureCache.end()) return it->second;

    // Don't retry failed loads
    if (m_WebFailedTextures.count(path)) return {};

    auto* texMgr = m_Renderer->GetTextureManager();
    if (!texMgr) return {};

    Renderer::GPUTextureHandle handle;

    // Try loading from asset pack first
    if (m_AssetReader && m_AssetReader->IsOpen() && m_AssetReader->HasFile(path)) {
        std::vector<u8> data = m_AssetReader->ReadFile(path);
        if (!data.empty()) {
            handle = texMgr->LoadFromMemory(data.data(), static_cast<u64>(data.size()), path.c_str());
        }
    }

    // Fallback: try loading from virtual filesystem (Emscripten MEMFS)
    if (!handle.IsValid()) {
        FILE* f = fopen(path.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (sz > 0) {
                std::vector<u8> buf(static_cast<usize>(sz));
                fread(buf.data(), 1, static_cast<usize>(sz), f);
                handle = texMgr->LoadFromMemory(buf.data(), static_cast<u64>(sz), path.c_str());
            }
            fclose(f);
        }
    }

    if (handle.IsValid()) {
        m_WebTextureCache[path] = handle;
        static int s_TexLog = 0;
        if (s_TexLog++ < 5) printf("[TEXTURE] loaded: %s\n", path.c_str());
    } else {
        m_WebFailedTextures.insert(path);
        static int s_TexFail = 0;
        if (s_TexFail++ < 5) printf("[TEXTURE] FAILED: %s\n", path.c_str());
    }
    return handle;
}

// ============================================================================
// Frame rendering
// ============================================================================

void RenderSystem::Update(f32 deltaTime) {
    if (!m_Renderer || !m_Initialized || !m_MainPipeline.IsValid()) {
        return;
    }
    if (!m_Camera) {
        return;
    }

    m_WebTime += deltaTime;
    auto* bufMgr = m_Renderer->GetBufferManager();
    if (!bufMgr) return;

    static int s_BuildCheck = 0;
    if (s_BuildCheck++ < 3) {
        EM_ASM({
            console.log('[BUILD_V2] shadow_pipe=' + $0 + ' shadow_tex=' + $1 + ' shadow_bg3=' + $2 + ' camera=' + $3);
        }, m_WebShadowPipeline.IsValid() ? 1 : 0,
           m_WebShadowMapTex.IsValid() ? 1 : 0,
           m_WebShadowSampleBG.IsValid() ? 1 : 0,
           m_Camera ? 1 : 0);
    }

    RefreshStorageCache();
    ResetFrameCounters();

    // Index animators by shared skeleton so ResolveAnimator can match follower
    // meshes to their leader's clock (animators themselves tick in web_main)
    m_CachedFallbackAnimator = nullptr;
    m_SkeletonToAnimator.clear();
    for (Entity animEntity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
        auto* ac = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(animEntity)
                                           : m_World->GetComponent<AnimatorComponent>(animEntity);
        if (!ac) continue;
        if (const auto* skel = ac->animator.GetSkeleton()) {
            if (!m_CachedFallbackAnimator) m_CachedFallbackAnimator = ac;
            m_SkeletonToAnimator[skel] = ac;
        }
    }

    // Upload ViewProjection UBO
    {
        WebViewProjectionUBO vp{};
        vp.view = m_Camera->GetViewMatrix();
        vp.proj = m_Camera->GetProjectionMatrix();
        vp.proj.m[5] = -vp.proj.m[5];  // Flip Y: Vulkan (Y-down) → WebGPU (Y-up)
        vp.viewPos = m_Camera->GetPosition();
        vp.time = m_WebTime;
        bufMgr->UploadData(m_WebViewProjBuffer, &vp, sizeof(vp));
    }

    // The shadow map follows the strongest shadow-casting directional light. It must
    // also sit in lighting slot 0 — pbr.wgsl applies the shadow term to slot 0 only —
    // so slot order and shadow-pass selection have to agree, or shadows detach from
    // the light that visually casts them (backwards-looking shadows with 2+ suns).
    Entity shadowCasterLight = INVALID_ENTITY;
    {
        f32 bestIntensity = -1.0f;
        for (Entity le : m_CachedLightEntities) {
            auto* lc = m_World->GetComponent<LightComponent>(le);
            if (!lc || lc->type != LightType::Directional || !lc->castShadows) continue;
            if (!m_CachedTransformStorage || !m_CachedTransformStorage->Get(le)) continue;
            if (lc->intensity > bestIntensity) {
                bestIntensity = lc->intensity;
                shadowCasterLight = le;
            }
        }
    }

    // Upload Lighting UBO
    {
        WebLightingUBO lit{};
        std::memset(&lit, 0, sizeof(lit));

        u32 dirCount = 0, pointCount = 0, spotCount = 0;
        if (m_CachedTransformStorage) {
            // Shadow caster first so it lands in directional slot 0
            std::vector<Entity> orderedLights(m_CachedLightEntities.begin(), m_CachedLightEntities.end());
            if (shadowCasterLight != INVALID_ENTITY) {
                auto it = std::find(orderedLights.begin(), orderedLights.end(), shadowCasterLight);
                if (it != orderedLights.end()) std::iter_swap(orderedLights.begin(), it);
            }
            for (Entity lightEntity : orderedLights) {
                auto* lc = m_World->GetComponent<LightComponent>(lightEntity);
                auto* xf = m_CachedTransformStorage->Get(lightEntity);
                if (!lc || !xf) continue;

                if (lc->type == LightType::Directional && dirCount < 4) {
                    Math::Vector3 fwd = xf->rotation.GetForward();
                    lit.lightDir[dirCount] = {fwd.x, fwd.y, fwd.z, 0.0f};
                    lit.lightColor[dirCount] = {lc->color.x, lc->color.y, lc->color.z, lc->intensity};
                    dirCount++;
                } else if (lc->type == LightType::Point && pointCount < 4) {
                    u32 idx = 4 + pointCount;
                    Math::Vector3 pos = xf->position;
                    lit.lightDir[idx] = {pos.x, pos.y, pos.z, 1.0f};
                    lit.lightColor[idx] = {lc->color.x, lc->color.y, lc->color.z, lc->intensity};
                    lit.lightParams[idx] = {lc->range, lc->linearAttenuation, lc->quadraticAttenuation, lc->constantAttenuation};
                    pointCount++;
                } else if (lc->type == LightType::Spot && spotCount < 4) {
                    Math::Vector3 pos = xf->position;
                    Math::Vector3 fwd = xf->rotation.GetForward();
                    lit.spotPos[spotCount] = {pos.x, pos.y, pos.z, lc->range};
                    lit.spotDir[spotCount] = {fwd.x, fwd.y, fwd.z, 0.0f};
                    lit.spotColor[spotCount] = {lc->color.x, lc->color.y, lc->color.z, lc->intensity};
                    f32 innerCos = std::cos(lc->innerConeAngle * 3.14159265f / 180.0f);
                    f32 outerCos = std::cos(lc->outerConeAngle * 3.14159265f / 180.0f);
                    lit.spotParams[spotCount] = {innerCos, outerCos, lc->linearAttenuation, lc->quadraticAttenuation};
                    spotCount++;
                }
            }
        }

        // Fallback: if no lights at all, use a default directional
        if (dirCount == 0 && pointCount == 0 && spotCount == 0) {
            lit.lightDir[0] = {0.5f, -0.8f, 0.3f, 0.0f};
            lit.lightColor[0] = {1.0f, 0.95f, 0.9f, 1.2f};
            dirCount = 1;
        }

        lit.ambientColor = {m_AmbientColor.x, m_AmbientColor.y, m_AmbientColor.z, m_AmbientIntensity};
        lit.fogColor = {m_FogColor.x, m_FogColor.y, m_FogColor.z, 0.0f};
        lit.fogParams = {m_FogDensity, m_FogStart, m_FogEnd, m_FogHeightFalloff};
        lit.lightCount = {static_cast<f32>(dirCount), static_cast<f32>(pointCount), static_cast<f32>(spotCount), 0.0f};

        // WebGPU: slight ambient floor so scenes are never pitch-black
        lit.ambientColor = {
            std::max(lit.ambientColor.x, 0.05f),
            std::max(lit.ambientColor.y, 0.05f),
            std::max(lit.ambientColor.z, 0.06f),
            std::max(lit.ambientColor.w, 0.2f)
        };

        static int s_LightLog = 0;
        if (s_LightLog++ < 3) {
            EM_ASM({
                console.log('[LIGHTS] dir=' + $0 + ' point=' + $1 + ' spot=' + $2 +
                    ' ambient=(' + $3.toFixed(2) + ',' + $4.toFixed(2) + ',' + $5.toFixed(2) + ')*' + $6.toFixed(2) +
                    ' entities=' + $7);
            }, dirCount, pointCount, spotCount,
               lit.ambientColor.x, lit.ambientColor.y, lit.ambientColor.z, lit.ambientColor.w,
               static_cast<int>(m_CachedLightEntities.size()));
            for (u32 p = 0; p < pointCount; p++) {
                u32 idx = 4 + p;
                EM_ASM({
                    console.log('[LIGHTS] Point[' + $0 + '] pos=(' + $1.toFixed(1) + ',' + $2.toFixed(1) + ',' + $3.toFixed(1) +
                        ') color=(' + $4.toFixed(2) + ',' + $5.toFixed(2) + ',' + $6.toFixed(2) +
                        ') intensity=' + $7.toFixed(2) + ' range=' + $8.toFixed(1));
                }, p, lit.lightDir[idx].x, lit.lightDir[idx].y, lit.lightDir[idx].z,
                   lit.lightColor[idx].x, lit.lightColor[idx].y, lit.lightColor[idx].z,
                   lit.lightColor[idx].w, lit.lightParams[idx].x);
            }
        }

        // Store first directional light direction for shadow pass
        // Count shadow-casting spot/point lights for shader
        u32 spotShadowCount = 0, pointShadowCount = 0;
        if (m_CachedTransformStorage) {
            for (Entity le : m_CachedLightEntities) {
                auto* lcc = m_World->GetComponent<LightComponent>(le);
                if (!lcc || !lcc->castShadows) continue;
                if (lcc->type == LightType::Spot && spotShadowCount < WEB_MAX_SPOT_SHADOWS) spotShadowCount++;
                if (lcc->type == LightType::Point && pointShadowCount < WEB_MAX_POINT_SHADOWS) pointShadowCount++;
            }
        }
        lit.shadowParams = {1.0f, static_cast<f32>(spotShadowCount), static_cast<f32>(pointShadowCount), 0.0f};

        static int s_ShadowLog = 0;
        if (s_ShadowLog++ < 5) {
            EM_ASM({
                console.log('[SHADOW] dirCount=' + $0 + ' spotShadow=' + $1 + ' pointShadow=' + $2 +
                    ' strength=' + $3.toFixed(2));
            }, dirCount, spotShadowCount, pointShadowCount, lit.shadowParams.x);
        }

        bufMgr->UploadData(m_WebLightingBuffer, &lit, sizeof(lit));
    }

    // ========================================================================
    // Shadow depth pass (single cascade, directional light)
    // ========================================================================
    if (m_WebShadowPipeline.IsValid() && m_WebShadowMapTex.IsValid() && m_Camera) {
        // Same light the lighting UBO put in directional slot 0
        Math::Vector3 shadowLightDir(0.5f, -0.8f, 0.3f);
        bool hasShadowLight = false;
        if (shadowCasterLight != INVALID_ENTITY && m_CachedTransformStorage) {
            if (auto* xf = m_CachedTransformStorage->Get(shadowCasterLight)) {
                shadowLightDir = xf->rotation.GetForward();
                hasShadowLight = true;
            }
        }

        // Compute light view-projection (single cascade covering shadow distance)
        f32 cameraNear = 0.1f;
        f32 shadowDistance = std::max(m_ShadowDistance, 1.0f);  // scene-configurable (was hardcoded 80)
        Math::Matrix4 camView = m_Camera->GetViewMatrix();
        Math::Matrix4 camProj = m_Camera->GetProjectionMatrix();
        Math::Matrix4 invViewProj = (camProj * camView).Inverse();

        // Unproject frustum corners at near/far planes
        Math::Vector3 nearCorners[4], farCorners[4];
        {
            int idx = 0;
            for (int y = 0; y < 2; ++y) {
                for (int x = 0; x < 2; ++x) {
                    Math::Vector4 nearNdc(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 0.0f, 1.0f);
                    Math::Vector4 farNdc(x * 2.0f - 1.0f, y * 2.0f - 1.0f, 1.0f, 1.0f);
                    Math::Vector4 nw = invViewProj * nearNdc;
                    Math::Vector4 fw = invViewProj * farNdc;
                    nearCorners[idx] = Math::Vector3(nw.x / nw.w, nw.y / nw.w, nw.z / nw.w);
                    farCorners[idx]  = Math::Vector3(fw.x / fw.w, fw.y / fw.w, fw.z / fw.w);
                    idx++;
                }
            }
        }

        // Compute world-space frustum corners clamped to shadow distance
        f32 cameraFar = m_Camera->GetFarPlane();
        f32 farT = std::min(shadowDistance, cameraFar) / cameraFar;
        Math::Vector3 corners[8];
        for (int i = 0; i < 4; ++i) {
            Math::Vector3 ray = farCorners[i] - nearCorners[i];
            corners[i]     = nearCorners[i];           // near plane
            corners[i + 4] = nearCorners[i] + ray * farT;  // shadow distance
        }

        // Compute frustum center
        Math::Vector3 center(0.0f);
        for (int i = 0; i < 8; ++i) center = center + corners[i];
        center = center * (1.0f / 8.0f);

        // Build light view matrix
        Math::Vector3 lightDirN = shadowLightDir.Normalized();
        Math::Vector3 lightUp(0.0f, 1.0f, 0.0f);
        if (std::abs(lightDirN.Dot(lightUp)) > 0.99f)
            lightUp = Math::Vector3(0.0f, 0.0f, 1.0f);
        Math::Matrix4 lightView = Math::Matrix4::LookAt(center - lightDirN * 50.0f, center, lightUp);

        // Light-space AABB
        f32 minX = 1e9f, maxX = -1e9f;
        f32 minY = 1e9f, maxY = -1e9f;
        f32 minZ = 1e9f, maxZ = -1e9f;
        for (int i = 0; i < 8; ++i) {
            Math::Vector4 ls = lightView * Math::Vector4(corners[i].x, corners[i].y, corners[i].z, 1.0f);
            minX = std::min(minX, ls.x); maxX = std::max(maxX, ls.x);
            minY = std::min(minY, ls.y); maxY = std::max(maxY, ls.y);
            minZ = std::min(minZ, ls.z); maxZ = std::max(maxZ, ls.z);
        }
        minZ -= 50.0f; maxZ += 50.0f;

        // Texel-size snapping (prevents shadow swimming)
        f32 sizeX = maxX - minX, sizeY = maxY - minY;
        f32 worldUnitsPerTexel = std::max(sizeX, sizeY) / static_cast<f32>(WEB_SHADOW_MAP_SIZE);
        if (worldUnitsPerTexel > 0.0f) {
            sizeX = std::ceil(sizeX / worldUnitsPerTexel) * worldUnitsPerTexel;
            sizeY = std::ceil(sizeY / worldUnitsPerTexel) * worldUnitsPerTexel;
            f32 cx = std::floor((minX + maxX) * 0.5f / worldUnitsPerTexel) * worldUnitsPerTexel;
            f32 cy = std::floor((minY + maxY) * 0.5f / worldUnitsPerTexel) * worldUnitsPerTexel;
            minX = cx - sizeX * 0.5f; maxX = cx + sizeX * 0.5f;
            minY = cy - sizeY * 0.5f; maxY = cy + sizeY * 0.5f;
        }

        // Orthographic projection (depth [0,1])
        Math::Matrix4 lightProj = Math::Matrix4::Identity();
        lightProj.m[0]  =  2.0f / (maxX - minX);
        lightProj.m[5]  =  2.0f / (maxY - minY);
        lightProj.m[10] = -1.0f / (maxZ - minZ);
        lightProj.m[12] = -(maxX + minX) / (maxX - minX);
        lightProj.m[13] = -(maxY + minY) / (maxY - minY);
        lightProj.m[14] =  maxZ / (maxZ - minZ);

        static int s_ShadowFitLog = 0;
        if (s_ShadowFitLog++ < 3) {
            EM_ASM({
                console.log('[SHADOW_FIT] box=' + $0.toFixed(1) + 'x' + $1.toFixed(1) +
                    ' zRange=' + $2.toFixed(1) + ' unitsPerTexel=' + $3.toFixed(3) +
                    ' center=(' + $4.toFixed(1) + ',' + $5.toFixed(1) + ',' + $6.toFixed(1) + ')' +
                    ' camFar=' + $7.toFixed(0) + ' dist=' + $8.toFixed(0));
            }, maxX - minX, maxY - minY, maxZ - minZ,
               std::max(maxX - minX, maxY - minY) / static_cast<f32>(WEB_SHADOW_MAP_SIZE),
               center.x, center.y, center.z, cameraFar, shadowDistance);
        }

        // Upload light VP to shadow UBO (used by both shadow pass and PBR pass group 3)
        WebViewProjectionUBO shadowVP{};
        shadowVP.view = lightView;
        shadowVP.proj = lightProj;
        shadowVP.viewPos = center - lightDirN * 50.0f;
        shadowVP.time = 0.0f;
        bufMgr->UploadData(m_WebShadowVPBuffer, &shadowVP, sizeof(shadowVP));

        // Execute shadow depth pass via raw WebGPU encoder
        auto* webRenderer = static_cast<Renderer::WebGPURenderer*>(m_Renderer);
        auto* texMgr = static_cast<Renderer::WebGPUTextureManager*>(m_Renderer->GetTextureManager());
        auto* pipeMgr = static_cast<Renderer::WebGPUPipelineManager*>(m_Renderer->GetPipelineManager());
        auto* webBufMgr = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        auto* webBindMgr = static_cast<Renderer::WebGPUBindGroupManager*>(m_Renderer->GetBindGroupManager());

        const auto* shadowNative = texMgr->GetNativeTexture(m_WebShadowMapTex);
        if (shadowNative && shadowNative->view) {
            WGPURenderPassEncoder shadowPass = webRenderer->BeginDepthOnlyPass(
                shadowNative->view, WEB_SHADOW_MAP_SIZE, WEB_SHADOW_MAP_SIZE);

            if (shadowPass) {
                WGPURenderPipeline nativeShadowPipeline = pipeMgr->GetNativePipeline(m_WebShadowPipeline);
                WGPUBindGroup nativeShadowFrameBG = webBindMgr->GetNativeGroup(m_WebShadowFrameBG);

                wgpuRenderPassEncoderSetPipeline(shadowPass, nativeShadowPipeline);
                wgpuRenderPassEncoderSetViewport(shadowPass, 0, 0,
                    static_cast<f32>(WEB_SHADOW_MAP_SIZE), static_cast<f32>(WEB_SHADOW_MAP_SIZE), 0.0f, 1.0f);
                wgpuRenderPassEncoderSetBindGroup(shadowPass, 0, nativeShadowFrameBG, 0, nullptr);

                // Draw each mesh entity into shadow map (per-entity buffer + bind group)
                const auto& meshEntities = m_World->GetEntitiesWithComponent<MeshComponent>();
                u32 shadowDrawCount = 0;
                for (Entity entity : meshEntities) {
                    auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
                    auto* xf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                    if (!mesh || !xf || !xf->visible) continue;
                    if (mesh->vertices.empty() || mesh->indices.empty()) continue;

                    u64 eid = EntityIndex(entity);  // dense index: low 32 bits (raw handle has generation in high bits)
                    if (eid >= m_EntityRenderData.size()) continue;
                    auto& rd = m_EntityRenderData[eid];
                    if (!rd.valid || !rd.vertexBuffer.IsValid() || !rd.indexBuffer.IsValid()) continue;

                    // Create per-entity buffer with model matrix (can't reuse one buffer —
                    // wgpuQueueWriteBuffer runs before the command buffer, so last write wins)
                    WebObjectDataUBO shadowObj{};
                    shadowObj.model = xf->ToMatrix();

                    Renderer::GPUBufferDesc perEntDesc;
                    perEntDesc.size = sizeof(WebObjectDataUBO);
                    perEntDesc.usage = Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst;
                    perEntDesc.hostVisible = true;
                    auto perEntBuf = bufMgr->CreateBufferWithData(perEntDesc, &shadowObj);

                    Renderer::GPUBindGroupDesc perEntBGD;
                    perEntBGD.layout = m_WebShadowObjectLayout;
                    perEntBGD.entries = {{0, perEntBuf, 0, sizeof(WebObjectDataUBO), {}, {}}};
                    auto perEntBG = webBindMgr->CreateBindGroup(perEntBGD);

                    WGPUBindGroup nativeBG = webBindMgr->GetNativeGroup(perEntBG);
                    wgpuRenderPassEncoderSetBindGroup(shadowPass, 1, nativeBG, 0, nullptr);

                    WGPUBuffer vb = webBufMgr->GetNativeBuffer(rd.vertexBuffer);
                    WGPUBuffer ib = webBufMgr->GetNativeBuffer(rd.indexBuffer);
                    wgpuRenderPassEncoderSetVertexBuffer(shadowPass, 0, vb, 0, WGPU_WHOLE_SIZE);
                    wgpuRenderPassEncoderSetIndexBuffer(shadowPass, ib, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
                    wgpuRenderPassEncoderDrawIndexed(shadowPass, rd.indexCount, 1, 0, 0, 0);

                    webBindMgr->DestroyBindGroup(perEntBG);
                    bufMgr->DestroyBuffer(perEntBuf);
                    shadowDrawCount++;
                }

                static int s_ShadowLog = 0;
                if (s_ShadowLog++ < 5) {
                    EM_ASM({ console.log('[SHADOW] drew ' + $0 + ' entities into shadow map'); }, shadowDrawCount);
                }

                wgpuRenderPassEncoderEnd(shadowPass);
                wgpuRenderPassEncoderRelease(shadowPass);
            }
        }
    }

    // ========================================================================
    // Spot light shadow passes (max 2 spot lights)
    // ========================================================================
    u32 activeSpotShadows = 0;
    if (m_WebShadowPipeline.IsValid() && m_Camera) {
        auto* webRenderer = static_cast<Renderer::WebGPURenderer*>(m_Renderer);
        auto* webTexMgr = static_cast<Renderer::WebGPUTextureManager*>(m_Renderer->GetTextureManager());
        auto* pipeMgr2 = static_cast<Renderer::WebGPUPipelineManager*>(m_Renderer->GetPipelineManager());
        auto* webBufMgr2 = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        auto* webBindMgr2 = static_cast<Renderer::WebGPUBindGroupManager*>(m_Renderer->GetBindGroupManager());

        WebSpotShadowVPUBO spotVPs{};

        if (m_CachedTransformStorage) {
            for (Entity lightEntity : m_CachedLightEntities) {
                if (activeSpotShadows >= WEB_MAX_SPOT_SHADOWS) break;
                auto* lc = m_World->GetComponent<LightComponent>(lightEntity);
                auto* xf = m_CachedTransformStorage->Get(lightEntity);
                if (!lc || !xf || lc->type != LightType::Spot || !lc->castShadows) continue;

                Math::Vector3 pos = xf->position;
                Math::Vector3 dir = xf->rotation.GetForward();
                f32 fov = lc->outerConeAngle * 2.0f * 3.14159265f / 180.0f;
                fov = std::max(fov, 0.1f);
                f32 range = lc->range > 0.0f ? lc->range : 50.0f;

                // Perspective projection for spot light (WebGPU depth [0,1], Y-up)
                Math::Matrix4 spotView = Math::Matrix4::LookAt(pos, pos + dir, Math::Vector3(0, 1, 0));
                f32 nearZ = 0.1f;
                Math::Matrix4 spotProj = WebGPUPerspective(fov, 1.0f, nearZ, range);

                u32 idx = activeSpotShadows;
                spotVPs.viewProj[idx * 2] = spotView;
                spotVPs.viewProj[idx * 2 + 1] = spotProj;

                // Render shadow pass for this spot light
                const auto* spotTexNative = webTexMgr->GetNativeTexture(m_WebSpotShadowTex[idx]);
                if (spotTexNative && spotTexNative->view) {
                    // Create per-pass VP buffer (can't reuse — wgpuQueueWriteBuffer last-write-wins)
                    WebViewProjectionUBO spotShadowVP{};
                    spotShadowVP.view = spotView;
                    spotShadowVP.proj = spotProj;
                    spotShadowVP.viewPos = pos;
                    auto spotVPBuf = bufMgr->CreateBufferWithData(
                        {sizeof(WebViewProjectionUBO), Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst, true},
                        &spotShadowVP);
                    Renderer::GPUBindGroupDesc spotFrameBGD;
                    spotFrameBGD.layout = m_WebShadowFrameLayout;
                    spotFrameBGD.entries = {{0, spotVPBuf, 0, sizeof(WebViewProjectionUBO), {}, {}}};
                    auto spotFrameBG = webBindMgr2->CreateBindGroup(spotFrameBGD);

                    WGPURenderPassEncoder spotPass = webRenderer->BeginDepthOnlyPass(
                        spotTexNative->view, WEB_SPOT_SHADOW_SIZE, WEB_SPOT_SHADOW_SIZE);
                    if (spotPass) {
                        WGPURenderPipeline nativePipe = pipeMgr2->GetNativePipeline(m_WebShadowPipeline);
                        wgpuRenderPassEncoderSetPipeline(spotPass, nativePipe);
                        wgpuRenderPassEncoderSetViewport(spotPass, 0, 0,
                            static_cast<f32>(WEB_SPOT_SHADOW_SIZE), static_cast<f32>(WEB_SPOT_SHADOW_SIZE), 0.0f, 1.0f);
                        wgpuRenderPassEncoderSetBindGroup(spotPass, 0, webBindMgr2->GetNativeGroup(spotFrameBG), 0, nullptr);

                        const auto& meshEntities = m_World->GetEntitiesWithComponent<MeshComponent>();
                        for (Entity entity : meshEntities) {
                            auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
                            auto* exf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                            if (!mesh || !exf || !exf->visible || mesh->vertices.empty()) continue;
                            u64 eid = EntityIndex(entity);  // dense index: low 32 bits (raw handle has generation in high bits)
                            if (eid >= m_EntityRenderData.size()) continue;
                            auto& rd = m_EntityRenderData[eid];
                            if (!rd.valid || !rd.vertexBuffer.IsValid() || !rd.indexBuffer.IsValid()) continue;

                            WebObjectDataUBO shadowObj{};
                            shadowObj.model = exf->ToMatrix();
                            auto perBuf = bufMgr->CreateBufferWithData(
                                {sizeof(WebObjectDataUBO), Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst, true},
                                &shadowObj);
                            Renderer::GPUBindGroupDesc bgd;
                            bgd.layout = m_WebShadowObjectLayout;
                            bgd.entries = {{0, perBuf, 0, sizeof(WebObjectDataUBO), {}, {}}};
                            auto perBG = webBindMgr2->CreateBindGroup(bgd);

                            wgpuRenderPassEncoderSetBindGroup(spotPass, 1, webBindMgr2->GetNativeGroup(perBG), 0, nullptr);
                            wgpuRenderPassEncoderSetVertexBuffer(spotPass, 0, webBufMgr2->GetNativeBuffer(rd.vertexBuffer), 0, WGPU_WHOLE_SIZE);
                            wgpuRenderPassEncoderSetIndexBuffer(spotPass, webBufMgr2->GetNativeBuffer(rd.indexBuffer), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
                            wgpuRenderPassEncoderDrawIndexed(spotPass, rd.indexCount, 1, 0, 0, 0);
                            webBindMgr2->DestroyBindGroup(perBG);
                            bufMgr->DestroyBuffer(perBuf);
                        }
                        wgpuRenderPassEncoderEnd(spotPass);
                        wgpuRenderPassEncoderRelease(spotPass);
                    }
                    webBindMgr2->DestroyBindGroup(spotFrameBG);
                    bufMgr->DestroyBuffer(spotVPBuf);
                }
                activeSpotShadows++;
            }
        }
        bufMgr->UploadData(m_WebSpotShadowVPBuffer, &spotVPs, sizeof(spotVPs));
    }

    // ========================================================================
    // Point light shadow passes
    // ========================================================================
    u32 activePointShadows = 0;
    static int s_PtLog = 0;
    if (s_PtLog < 3) {
        EM_ASM({
            console.log('[PT_SHADOW] pipeline=' + $0 + ' camera=' + $1 + ' faceViews=' + $2);
        }, m_WebShadowPipeline.IsValid() ? 1 : 0, m_Camera ? 1 : 0, m_WebPointShadowFaceViews[0] ? 1 : 0);
        s_PtLog++;
    }
    if (m_WebShadowPipeline.IsValid() && m_Camera && m_WebPointShadowFaceViews[0]) {
        auto* webRenderer = static_cast<Renderer::WebGPURenderer*>(m_Renderer);
        auto* pipeMgr3 = static_cast<Renderer::WebGPUPipelineManager*>(m_Renderer->GetPipelineManager());
        auto* webBufMgr3 = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        auto* webBindMgr3 = static_cast<Renderer::WebGPUBindGroupManager*>(m_Renderer->GetBindGroupManager());

        WebPointShadowVPUBO pointVPs{};

        if (m_CachedTransformStorage) {
            for (Entity lightEntity : m_CachedLightEntities) {
                if (activePointShadows >= WEB_MAX_POINT_SHADOWS) break;
                auto* lc = m_World->GetComponent<LightComponent>(lightEntity);
                auto* xf = m_CachedTransformStorage->Get(lightEntity);
                if (!lc || !xf || lc->type != LightType::Point || !lc->castShadows) continue;

                Math::Vector3 pos = xf->position;
                f32 range = lc->range > 0.0f ? lc->range : 50.0f;
                f32 nearZ = 0.1f;
                Math::Matrix4 faceProj = WebGPUCubemapPerspective(3.14159265f * 0.5f, 1.0f, nearZ, range);

                // 6 cube face directions: +X, -X, +Y, -Y, +Z, -Z
                struct FaceDir { Math::Vector3 target; Math::Vector3 up; };
                FaceDir faces[6] = {
                    {{pos.x+1, pos.y, pos.z}, {0,-1,0}},  // +X
                    {{pos.x-1, pos.y, pos.z}, {0,-1,0}},  // -X
                    {{pos.x, pos.y+1, pos.z}, {0,0,1}},   // +Y
                    {{pos.x, pos.y-1, pos.z}, {0,0,-1}},  // -Y
                    {{pos.x, pos.y, pos.z+1}, {0,-1,0}},  // +Z
                    {{pos.x, pos.y, pos.z-1}, {0,-1,0}},  // -Z
                };

                for (u32 face = 0; face < 6; face++) {
                    Math::Matrix4 faceView = Math::Matrix4::LookAt(pos, faces[face].target, faces[face].up);
                    pointVPs.viewProj[face * 2] = faceView;
                    pointVPs.viewProj[face * 2 + 1] = faceProj;

                    // Create per-face VP buffer (can't reuse — wgpuQueueWriteBuffer last-write-wins)
                    WebViewProjectionUBO faceShadowVP{};
                    faceShadowVP.view = faceView;
                    faceShadowVP.proj = faceProj;
                    faceShadowVP.viewPos = pos;
                    auto faceVPBuf = bufMgr->CreateBufferWithData(
                        {sizeof(WebViewProjectionUBO), Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst, true},
                        &faceShadowVP);
                    Renderer::GPUBindGroupDesc faceFrameBGD;
                    faceFrameBGD.layout = m_WebShadowFrameLayout;
                    faceFrameBGD.entries = {{0, faceVPBuf, 0, sizeof(WebViewProjectionUBO), {}, {}}};
                    auto faceFrameBG = webBindMgr3->CreateBindGroup(faceFrameBGD);

                    WGPURenderPassEncoder facePass = webRenderer->BeginDepthOnlyPass(
                        static_cast<WGPUTextureView>(m_WebPointShadowFaceViews[face]), WEB_POINT_SHADOW_SIZE, WEB_POINT_SHADOW_SIZE);
                    if (facePass) {
                        wgpuRenderPassEncoderSetPipeline(facePass, pipeMgr3->GetNativePipeline(m_WebShadowPipeline));
                        wgpuRenderPassEncoderSetViewport(facePass, 0, 0,
                            static_cast<f32>(WEB_POINT_SHADOW_SIZE), static_cast<f32>(WEB_POINT_SHADOW_SIZE), 0.0f, 1.0f);
                        wgpuRenderPassEncoderSetBindGroup(facePass, 0, webBindMgr3->GetNativeGroup(faceFrameBG), 0, nullptr);

                        const auto& meshEntities = m_World->GetEntitiesWithComponent<MeshComponent>();
                        for (Entity entity : meshEntities) {
                            auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
                            auto* exf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                            if (!mesh || !exf || !exf->visible || mesh->vertices.empty()) continue;
                            u64 eid = EntityIndex(entity);  // dense index: low 32 bits (raw handle has generation in high bits)
                            if (eid >= m_EntityRenderData.size()) continue;
                            auto& rd = m_EntityRenderData[eid];
                            if (!rd.valid || !rd.vertexBuffer.IsValid() || !rd.indexBuffer.IsValid()) continue;

                            WebObjectDataUBO shadowObj{};
                            shadowObj.model = exf->ToMatrix();
                            auto perBuf = bufMgr->CreateBufferWithData(
                                {sizeof(WebObjectDataUBO), Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst, true},
                                &shadowObj);
                            Renderer::GPUBindGroupDesc bgd;
                            bgd.layout = m_WebShadowObjectLayout;
                            bgd.entries = {{0, perBuf, 0, sizeof(WebObjectDataUBO), {}, {}}};
                            auto perBG = webBindMgr3->CreateBindGroup(bgd);

                            wgpuRenderPassEncoderSetBindGroup(facePass, 1, webBindMgr3->GetNativeGroup(perBG), 0, nullptr);
                            wgpuRenderPassEncoderSetVertexBuffer(facePass, 0, webBufMgr3->GetNativeBuffer(rd.vertexBuffer), 0, WGPU_WHOLE_SIZE);
                            wgpuRenderPassEncoderSetIndexBuffer(facePass, webBufMgr3->GetNativeBuffer(rd.indexBuffer), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
                            wgpuRenderPassEncoderDrawIndexed(facePass, rd.indexCount, 1, 0, 0, 0);
                            webBindMgr3->DestroyBindGroup(perBG);
                            bufMgr->DestroyBuffer(perBuf);
                        }
                        wgpuRenderPassEncoderEnd(facePass);
                        wgpuRenderPassEncoderRelease(facePass);
                    }
                    webBindMgr3->DestroyBindGroup(faceFrameBG);
                    bufMgr->DestroyBuffer(faceVPBuf);
                }
                activePointShadows++;
            }
        }
        bufMgr->UploadData(m_WebPointShadowVPBuffer, &pointVPs, sizeof(pointVPs));
    }

    // Re-upload directional shadow VP (spot/point passes may have overwritten m_WebShadowVPBuffer)
    // This is handled by the directional pass writing first and the bind group referencing the buffer

    // Begin main render pass (offscreen if post-processing enabled, else swapchain)
    auto* webRenderer = static_cast<Renderer::WebGPURenderer*>(m_Renderer);
    auto* webPipeMgr = static_cast<Renderer::WebGPUPipelineManager*>(m_Renderer->GetPipelineManager());
    auto* webBindMgr = static_cast<Renderer::WebGPUBindGroupManager*>(m_Renderer->GetBindGroupManager());
    bool usePostProcess = m_WebPostProcessPipeline.IsValid() && m_WebSceneColorView && m_WebSceneDepthView && m_WebMSAAColorView;
    WGPURenderPassEncoder scenePassEncoder = nullptr;
    Renderer::IRenderEncoder* encoder = nullptr;

    f32 w = static_cast<f32>(m_Renderer->GetSwapchainWidth());
    f32 h = static_cast<f32>(m_Renderer->GetSwapchainHeight());

    // The offscreen scene targets are created ONCE at boot size and are NOT
    // recreated on window resize — the scene pass viewport must match ITS
    // attachments, not the (possibly resized) swapchain, or every frame fails
    // validation and the game dies to black. The post-process pass scales the
    // fixed-size scene texture to the swapchain. TODO: recreate the offscreen
    // chain on resize for native-res rendering after enlarge.
    f32 sceneW = w, sceneH = h;
    if (usePostProcess) {
        if (auto* sceneNative = static_cast<Renderer::WebGPUTextureManager*>(
                m_Renderer->GetTextureManager())->GetNativeTexture(m_WebSceneColorTex)) {
            sceneW = static_cast<f32>(sceneNative->width);
            sceneH = static_cast<f32>(sceneNative->height);
        }
    }

    if (usePostProcess) {
        // Render scene to MSAA texture, resolve to HDR offscreen
        WGPURenderPassColorAttachment colorAtt = {};
        colorAtt.view = static_cast<WGPUTextureView>(m_WebMSAAColorView);  // 4x MSAA render target
        colorAtt.resolveTarget = static_cast<WGPUTextureView>(m_WebSceneColorView);  // 1x resolve target
        colorAtt.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        colorAtt.loadOp = WGPULoadOp_Clear;
        colorAtt.storeOp = WGPUStoreOp_Store;
        colorAtt.clearValue = {0.4, 0.5, 0.65, 1.0};  // sky color

        WGPURenderPassDepthStencilAttachment depthAtt = {};
        depthAtt.view = static_cast<WGPUTextureView>(m_WebSceneDepthView);
        depthAtt.depthLoadOp = WGPULoadOp_Clear;
        depthAtt.depthStoreOp = WGPUStoreOp_Store;
        depthAtt.depthClearValue = 1.0f;
        depthAtt.stencilLoadOp = WGPULoadOp_Clear;
        depthAtt.stencilStoreOp = WGPUStoreOp_Store;

        WGPURenderPassDescriptor passDesc = {};
        passDesc.colorAttachmentCount = 1;
        passDesc.colorAttachments = &colorAtt;
        passDesc.depthStencilAttachment = &depthAtt;
        if (webRenderer->GetCommandEncoder()) {
            scenePassEncoder = wgpuCommandEncoderBeginRenderPass(webRenderer->GetCommandEncoder(), &passDesc);
        }
        if (!scenePassEncoder) { usePostProcess = false; }
    }

    std::unique_ptr<Renderer::WebGPURenderEncoder> sceneEncoder;
    if (!usePostProcess) {
        // Fallback: render directly to swapchain
        Renderer::GPURenderPassDesc defaultDesc;
        encoder = m_Renderer->BeginRenderPass(defaultDesc);
        if (!encoder) return;
    } else {
        // Wrap offscreen pass encoder in abstract encoder so draw calls work identically
        auto* webBufMgr = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        sceneEncoder = std::make_unique<Renderer::WebGPURenderEncoder>(
            webRenderer, scenePassEncoder, webPipeMgr, webBufMgr, webBindMgr);
        encoder = sceneEncoder.get();
    }
    encoder->BindPipeline(m_MainPipeline);
    encoder->SetViewport(0, 0, sceneW, sceneH);
    encoder->SetScissor(0, 0, static_cast<u32>(sceneW), static_cast<u32>(sceneH));
    encoder->SetBindGroup(0, m_WebFrameBindGroup);
    if (m_WebShadowSampleBG.IsValid())
        encoder->SetBindGroup(3, m_WebShadowSampleBG);

    // Render each entity with a mesh
    // WebGPU batching: all ObjectData is written to a large buffer BEFORE the render pass
    // draws, using 256-byte aligned offsets. Each entity gets a dynamic offset into the shared buffer.
    {
        const auto& meshEntities = m_World->GetEntitiesWithComponent<MeshComponent>();

        // LOD: distance-based mesh swap with hysteresis
        if (m_Camera) {
            Math::Vector3 camPos = m_Camera->GetPosition();
            auto* lodStorage = m_World->GetComponentStorage<LODComponent>();
            if (lodStorage) {
                for (Entity entity : meshEntities) {
                    auto* lod = lodStorage->Get(entity);
                    if (!lod || !lod->enabled || lod->levelCount <= 1) continue;
                    auto* xf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                    if (!xf) continue;

                    f32 dist = (xf->position - camPos).Length();
                    i32 newLOD = SelectLOD(*lod, dist);
                    if (newLOD != lod->activeLOD && newLOD < lod->levelCount) {
                        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
                        if (mesh && !lod->levels[newLOD].mesh.vertices.empty()) {
                            *mesh = lod->levels[newLOD].mesh;
                            u64 eid = EntityIndex(entity);  // dense index: low 32 bits (raw handle has generation in high bits)
                            if (eid < m_EntityRenderData.size()) m_EntityRenderData[eid].Invalidate();
                            lod->activeLOD = newLOD;
                        }
                    }
                }
            }
        }

        // Phase 1: Collect visible entities, ensure GPU buffers, build ObjectData array
        constexpr u32 OBJ_ALIGN = 256;  // WebGPU minUniformBufferOffsetAlignment
        struct DrawCmd { Entity entity; u32 offset; };
        std::vector<DrawCmd> drawCmds;
        std::vector<u8> objDataBuf;

        for (Entity entity : meshEntities) {
            auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
            auto* xf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (!mesh || !xf || !xf->visible) continue;
            if (mesh->vertices.empty() || mesh->indices.empty()) continue;

            // Ensure GPU buffers
            u64 eid = EntityIndex(entity);  // dense index: low 32 bits (raw handle has generation in high bits)
            if (eid >= m_EntityRenderData.size()) m_EntityRenderData.resize(eid + 1);
            auto& rd = m_EntityRenderData[eid];

            if (!rd.valid || rd.owner != entity) {  // owner mismatch = slot recycled by a new entity
                Renderer::GPUBufferDesc vbDesc;
                vbDesc.size = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
                vbDesc.usage = Renderer::GPUBufferUsage::Vertex | Renderer::GPUBufferUsage::CopyDst;
                vbDesc.hostVisible = true;
                rd.vertexBuffer = bufMgr->CreateBufferWithData(vbDesc, mesh->vertices.data());

                Renderer::GPUBufferDesc ibDesc;
                ibDesc.size = mesh->indices.size() * sizeof(u32);
                ibDesc.usage = Renderer::GPUBufferUsage::Index | Renderer::GPUBufferUsage::CopyDst;
                ibDesc.hostVisible = true;
                rd.indexBuffer = bufMgr->CreateBufferWithData(ibDesc, mesh->indices.data());

                rd.indexCount = static_cast<u32>(mesh->indices.size());
                rd.valid = true;
                rd.owner = entity;
            }

            if (!rd.vertexBuffer.IsValid() || !rd.indexBuffer.IsValid()) continue;

            auto* mat = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
            // Fallback: try direct query if cached storage failed (WASM template issue)
            if (!mat) mat = m_World->GetComponent<MaterialComponent>(entity);
            static int s_MatLog = 0;
            if (s_MatLog++ < 3) printf("[MAT] entity=%llu mat=%s color=(%.2f,%.2f,%.2f) tex=%s\n",
                static_cast<unsigned long long>(entity), mat ? "YES" : "NULL",
                mat ? mat->baseColor.x : 0, mat ? mat->baseColor.y : 0, mat ? mat->baseColor.z : 0,
                mat && !mat->baseColorTexturePath.empty() ? mat->baseColorTexturePath.c_str() : "none");

            // Build per-entity texture bind group (cached, rebuilt on dirty)
            if (!rd.texBindGroupValid && mat) {
                auto baseColorTex = WebGetOrLoadTexture(mat->baseColorTexturePath);
                auto normalTex = WebGetOrLoadTexture(mat->normalTexturePath);
                auto mrTex = WebGetOrLoadTexture(mat->metallicRoughnessTexturePath);

                // Only create custom bind group if at least one texture loaded
                if (baseColorTex.IsValid() || normalTex.IsValid() || mrTex.IsValid()) {
                    auto bc = baseColorTex.IsValid() ? baseColorTex : m_WebDefaultWhiteTex;
                    auto nm = normalTex.IsValid() ? normalTex : m_WebDefaultNormalTex;
                    auto mr = mrTex.IsValid() ? mrTex : m_WebDefaultBlackTex;

                    Renderer::GPUBindGroupDesc texBGDesc;
                    texBGDesc.layout = m_WebTextureLayout;
                    texBGDesc.entries = {
                        {0, {}, 0, 0, bc, {}},
                        {1, {}, 0, 0, {}, bc},
                        {2, {}, 0, 0, nm, {}},
                        {3, {}, 0, 0, {}, nm},
                        {4, {}, 0, 0, mr, {}},
                        {5, {}, 0, 0, {}, mr},
                    };
                    auto* bm = m_Renderer->GetBindGroupManager();
                    if (bm) rd.texBindGroup = bm->CreateBindGroup(texBGDesc);
                }
                rd.texBindGroupValid = true;
            }

            // Build per-entity ObjectData at aligned offset
            u32 offset = static_cast<u32>(objDataBuf.size());
            objDataBuf.resize(offset + OBJ_ALIGN, 0);

            // Upload bone matrices for skinned meshes (resolve shared animator for follower meshes)
            auto* animComp = ResolveAnimator(entity);
            if (animComp && animComp->animator.GetSkeleton()) {
                auto& skinMats = animComp->animator.GetSkinningMatrices();
                if (!skinMats.empty()) {
                    usize boneDataSize = skinMats.size() * sizeof(Math::Matrix4);
                    if (!rd.boneBuffer.IsValid()) {
                        Renderer::GPUBufferDesc boneDesc;
                        boneDesc.size = boneDataSize;
                        boneDesc.usage = Renderer::GPUBufferUsage::Storage | Renderer::GPUBufferUsage::CopyDst;
                        boneDesc.hostVisible = true;
                        rd.boneBuffer = bufMgr->CreateBufferWithData(boneDesc, skinMats.data());
                    } else {
                        bufMgr->UploadData(rd.boneBuffer, skinMats.data(), boneDataSize);
                    }
                }
            }

            WebObjectDataUBO obj{};
            obj.model = xf->ToMatrix();
            obj.baseColor = mat ? mat->baseColor : Math::Vector3(0.8f, 0.8f, 0.8f);
            obj.metallic = mat ? mat->metallic : 0.0f;
            obj.roughness = mat ? mat->roughness : 0.5f;
            obj.emissiveColor = mat ? mat->emissiveColor : Math::Vector3(0, 0, 0);
            obj.emissiveStrength = mat ? mat->emissiveStrength : 0.0f;
            obj.opacity = mat ? mat->opacity : 1.0f;
            obj.alphaCutoff = mat ? mat->alphaCutoff : 0.0f;
            if (animComp && rd.boneBuffer.IsValid()) obj.flags |= (1 << 3);  // FLAG_SKINNED
            std::memcpy(objDataBuf.data() + offset, &obj, sizeof(obj));

            drawCmds.push_back({entity, offset});
        }

        // Sort draw commands: opaque grouped by mesh+texture (for instancing), then front-to-back
        // Transparent sorted back-to-front (no batching)
        if (m_Camera) {
            Math::Vector3 camPos = m_Camera->GetPosition();
            std::sort(drawCmds.begin(), drawCmds.end(),
                [this, &camPos](const DrawCmd& a, const DrawCmd& b) {
                    auto* matA = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(a.entity) : nullptr;
                    auto* matB = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(b.entity) : nullptr;
                    bool transA = matA && matA->opacity < 1.0f;
                    bool transB = matB && matB->opacity < 1.0f;
                    // Opaque before transparent
                    if (transA != transB) return !transA;
                    // For opaque: group by mesh identity (VB+IB) for instancing batches
                    if (!transA) {
                        u64 eidA = EntityIndex(a.entity), eidB = EntityIndex(b.entity);
                        if (eidA < m_EntityRenderData.size() && eidB < m_EntityRenderData.size()) {
                            auto& rdA = m_EntityRenderData[eidA];
                            auto& rdB = m_EntityRenderData[eidB];
                            u64 keyA = rdA.vertexBuffer.id ^ (rdA.indexBuffer.id << 16);
                            u64 keyB = rdB.vertexBuffer.id ^ (rdB.indexBuffer.id << 16);
                            if (keyA != keyB) return keyA < keyB;
                        }
                    }
                    auto* xfA = m_CachedTransformStorage ? m_CachedTransformStorage->Get(a.entity) : nullptr;
                    auto* xfB = m_CachedTransformStorage ? m_CachedTransformStorage->Get(b.entity) : nullptr;
                    if (!xfA || !xfB) return false;
                    f32 distA = (xfA->position - camPos).LengthSquared();
                    f32 distB = (xfB->position - camPos).LengthSquared();
                    return transA ? distA > distB : distA < distB;
                });
        }

        // Phase 2: Batch entities by mesh+texture, draw instanced where possible
        auto* bindMgr = m_Renderer->GetBindGroupManager();

        // Build batch key for each draw command: entities with same VB+IB+textures can be instanced
        struct BatchKey {
            u32 vbId, ibId, texBGId;
            bool operator==(const BatchKey& o) const { return vbId == o.vbId && ibId == o.ibId && texBGId == o.texBGId; }
        };

        auto getBatchKey = [&](const DrawCmd& cmd) -> BatchKey {
            u64 eid = EntityIndex(cmd.entity);
            auto& rd = m_EntityRenderData[eid];
            auto texBG = rd.texBindGroup.IsValid() ? rd.texBindGroup : m_WebDefaultTexBindGroup;
            return {static_cast<u32>(rd.vertexBuffer.id),
                    static_cast<u32>(rd.indexBuffer.id),
                    static_cast<u32>(texBG.id)};
        };

        auto canBatch = [&](const DrawCmd& cmd) -> bool {
            u64 eid = EntityIndex(cmd.entity);
            auto& rd = m_EntityRenderData[eid];
            if (rd.boneBuffer.IsValid()) return false;  // Skinned — unique bone data
            auto* matSlots = m_CachedMaterialSlotsStorage ? m_CachedMaterialSlotsStorage->Get(cmd.entity) : nullptr;
            auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(cmd.entity) : nullptr;
            if (matSlots && mesh && mesh->HasSubMeshes()) return false;  // Multi-material — unique draw per submesh
            return true;
        };

        usize i = 0;
        while (i < drawCmds.size()) {
            const auto& cmd = drawCmds[i];
            u64 eid = EntityIndex(cmd.entity);
            auto& rd = m_EntityRenderData[eid];

            // Check if this entity can start a batch
            if (canBatch(cmd)) {
                BatchKey key = getBatchKey(cmd);

                // Find batch end: consecutive commands with same key that are batchable
                usize batchEnd = i + 1;
                while (batchEnd < drawCmds.size() && canBatch(drawCmds[batchEnd]) && getBatchKey(drawCmds[batchEnd]) == key) {
                    batchEnd++;
                }
                u32 instanceCount = static_cast<u32>(batchEnd - i);

                // Pack ObjectData for all instances into one contiguous SSBO
                std::vector<u8> batchData(instanceCount * sizeof(WebObjectDataUBO));
                for (usize j = 0; j < instanceCount; j++) {
                    std::memcpy(batchData.data() + j * sizeof(WebObjectDataUBO),
                        objDataBuf.data() + drawCmds[i + j].offset, sizeof(WebObjectDataUBO));
                }

                auto batchBuf = bufMgr->CreateBufferWithData(
                    {batchData.size(), Renderer::GPUBufferUsage::Storage | Renderer::GPUBufferUsage::CopyDst, true},
                    batchData.data());

                Renderer::GPUBindGroupDesc bgd;
                bgd.layout = m_WebObjectLayout;
                bgd.entries = {
                    {0, batchBuf, 0, batchData.size(), {}, {}},
                    {1, m_WebDefaultBoneBuffer, 0, 0, {}, {}},
                };
                auto batchBG = bindMgr->CreateBindGroup(bgd);

                encoder->SetBindGroup(1, batchBG);
                auto texBG = rd.texBindGroup.IsValid() ? rd.texBindGroup : m_WebDefaultTexBindGroup;
                encoder->SetBindGroup(2, texBG);
                encoder->SetVertexBuffer(0, rd.vertexBuffer);
                encoder->SetIndexBuffer(rd.indexBuffer, Renderer::GPUIndexFormat::Uint32);
                encoder->DrawIndexed(rd.indexCount, instanceCount);

                m_DrawCallCount++;
                m_TriangleCount += (rd.indexCount / 3) * instanceCount;

                bindMgr->DestroyBindGroup(batchBG);
                bufMgr->DestroyBuffer(batchBuf);
                i = batchEnd;
            } else {
                // Non-batchable: skinned or multi-material — draw individually
                auto perEntityBuf = bufMgr->CreateBufferWithData(
                    {sizeof(WebObjectDataUBO), Renderer::GPUBufferUsage::Storage | Renderer::GPUBufferUsage::CopyDst, true},
                    objDataBuf.data() + cmd.offset);

                auto boneBuf = rd.boneBuffer.IsValid() ? rd.boneBuffer : m_WebDefaultBoneBuffer;
                Renderer::GPUBindGroupDesc bgd;
                bgd.layout = m_WebObjectLayout;
                bgd.entries = {
                    {0, perEntityBuf, 0, sizeof(WebObjectDataUBO), {}, {}},
                    {1, boneBuf, 0, 0, {}, {}},
                };
                auto perEntityBG = bindMgr->CreateBindGroup(bgd);

                encoder->SetBindGroup(1, perEntityBG);
                encoder->SetVertexBuffer(0, rd.vertexBuffer);
                encoder->SetIndexBuffer(rd.indexBuffer, Renderer::GPUIndexFormat::Uint32);

                // Multi-material path
                auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(cmd.entity) : nullptr;
                auto* matSlots = m_CachedMaterialSlotsStorage ? m_CachedMaterialSlotsStorage->Get(cmd.entity) : nullptr;
                if (matSlots && mesh && mesh->HasSubMeshes()) {
                    for (const auto& subMesh : mesh->subMeshes) {
                        if (subMesh.indexCount == 0) continue;
                        auto* slotMat = (subMesh.materialSlot >= 0 && subMesh.materialSlot < static_cast<i32>(matSlots->slots.size()))
                            ? &matSlots->slots[subMesh.materialSlot] : nullptr;
                        Renderer::GPUBindGroupHandle subTexBG;
                        if (slotMat) {
                            auto bc = WebGetOrLoadTexture(slotMat->baseColorTexturePath);
                            auto nm = WebGetOrLoadTexture(slotMat->normalTexturePath);
                            auto mr = WebGetOrLoadTexture(slotMat->metallicRoughnessTexturePath);
                            Renderer::GPUBindGroupDesc texBGD;
                            texBGD.layout = m_WebTextureLayout;
                            texBGD.entries = {
                                {0, {}, 0, 0, bc.IsValid() ? bc : m_WebDefaultWhiteTex, {}},
                                {1, {}, 0, 0, {}, bc.IsValid() ? bc : m_WebDefaultWhiteTex},
                                {2, {}, 0, 0, nm.IsValid() ? nm : m_WebDefaultNormalTex, {}},
                                {3, {}, 0, 0, {}, nm.IsValid() ? nm : m_WebDefaultNormalTex},
                                {4, {}, 0, 0, mr.IsValid() ? mr : m_WebDefaultBlackTex, {}},
                                {5, {}, 0, 0, {}, mr.IsValid() ? mr : m_WebDefaultBlackTex},
                            };
                            subTexBG = bindMgr->CreateBindGroup(texBGD);
                        }
                        encoder->SetBindGroup(2, subTexBG.IsValid() ? subTexBG : m_WebDefaultTexBindGroup);
                        encoder->DrawIndexed(subMesh.indexCount, 1, subMesh.indexOffset);
                        m_DrawCallCount++;
                        m_TriangleCount += subMesh.indexCount / 3;
                        if (subTexBG.IsValid()) bindMgr->DestroyBindGroup(subTexBG);
                    }
                } else {
                    encoder->SetBindGroup(2, rd.texBindGroup.IsValid() ? rd.texBindGroup : m_WebDefaultTexBindGroup);
                    encoder->DrawIndexed(rd.indexCount);
                    m_DrawCallCount++;
                    m_TriangleCount += rd.indexCount / 3;
                }

                bindMgr->DestroyBindGroup(perEntityBG);
                bufMgr->DestroyBuffer(perEntityBuf);
                i++;
            }
        }
    }

    // ========================================================================
    // Grass volumes (instanced blades, opaque)
    // ========================================================================
    if (usePostProcess && m_WebGrassPipeline.IsValid() && scenePassEncoder) {
        auto* webBufMgrG = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        const auto& grassEntities = m_World->GetEntitiesWithComponent<GrassVolumeComponent>();
        for (Entity ge : grassEntities) {
            auto* gv = m_World->GetComponent<GrassVolumeComponent>(ge);
            auto* gxf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(ge) : nullptr;
            if (!gv || !gxf || gv->density == 0) continue;

            // Volume params UBO: pos(3)+bladeHeight(1) + halfExtents(3)+bladeWidth(1) + baseColor(3)+wind(1) + tipColor(3)+pad(1) = 64 bytes
            struct GrassParams { f32 data[16]; };
            GrassParams gp = {};
            gp.data[0] = gxf->position.x; gp.data[1] = gxf->position.y; gp.data[2] = gxf->position.z;
            gp.data[3] = gv->bladeHeight;
            gp.data[4] = gv->halfExtents.x; gp.data[5] = gv->halfExtents.y; gp.data[6] = gv->halfExtents.z;
            gp.data[7] = gv->bladeWidth;
            gp.data[8] = gv->baseColor.x; gp.data[9] = gv->baseColor.y; gp.data[10] = gv->baseColor.z;
            gp.data[11] = gv->windSwayStrength;
            gp.data[12] = gv->tipColor.x; gp.data[13] = gv->tipColor.y; gp.data[14] = gv->tipColor.z;

            auto gpBuf = bufMgr->CreateBufferWithData(
                {sizeof(GrassParams), Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst, true}, &gp);
            Renderer::GPUBindGroupDesc bgd;
            bgd.layout = m_WebVolumeParamsLayout;
            bgd.entries = {{0, gpBuf, 0, sizeof(GrassParams), {}, {}}};
            auto gpBG = webBindMgr->CreateBindGroup(bgd);

            wgpuRenderPassEncoderSetPipeline(scenePassEncoder, webPipeMgr->GetNativePipeline(m_WebGrassPipeline));
            wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 0, webBindMgr->GetNativeGroup(m_WebFrameBindGroup), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 1, webBindMgr->GetNativeGroup(gpBG), 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(scenePassEncoder, 0, webBufMgrG->GetNativeBuffer(m_WebGrassBladeVB), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(scenePassEncoder, webBufMgrG->GetNativeBuffer(m_WebGrassBladeIB), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(scenePassEncoder, m_WebGrassBladeIndexCount, gv->density, 0, 0, 0);

            webBindMgr->DestroyBindGroup(gpBG);
            bufMgr->DestroyBuffer(gpBuf);
        }
    }

    // ========================================================================
    // Tree volumes (instanced trunk+canopy, opaque)
    // ========================================================================
    if (usePostProcess && m_WebTreePipeline.IsValid() && scenePassEncoder) {
        auto* webBufMgrT = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        const auto& treeEntities = m_World->GetEntitiesWithComponent<TreeVolumeComponent>();
        for (Entity te : treeEntities) {
            auto* tv = m_World->GetComponent<TreeVolumeComponent>(te);
            auto* txf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(te) : nullptr;
            if (!tv || !txf || tv->density == 0) continue;

            struct TreeParams { f32 data[16]; };
            TreeParams tp = {};
            tp.data[0] = txf->position.x; tp.data[1] = txf->position.y; tp.data[2] = txf->position.z;
            tp.data[3] = tv->trunkHeight;
            tp.data[4] = tv->halfExtents.x; tp.data[5] = tv->halfExtents.y; tp.data[6] = tv->halfExtents.z;
            tp.data[7] = tv->trunkWidth;
            tp.data[8] = tv->trunkColor.x; tp.data[9] = tv->trunkColor.y; tp.data[10] = tv->trunkColor.z;
            tp.data[11] = tv->canopyRadius;
            tp.data[12] = tv->canopyBaseColor.x; tp.data[13] = tv->canopyBaseColor.y; tp.data[14] = tv->canopyBaseColor.z;
            tp.data[15] = tv->canopyOffset;

            auto tpBuf = bufMgr->CreateBufferWithData(
                {sizeof(TreeParams), Renderer::GPUBufferUsage::Uniform | Renderer::GPUBufferUsage::CopyDst, true}, &tp);
            Renderer::GPUBindGroupDesc bgd;
            bgd.layout = m_WebVolumeParamsLayout;
            bgd.entries = {{0, tpBuf, 0, sizeof(TreeParams), {}, {}}};
            auto tpBG = webBindMgr->CreateBindGroup(bgd);

            wgpuRenderPassEncoderSetPipeline(scenePassEncoder, webPipeMgr->GetNativePipeline(m_WebTreePipeline));
            wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 0, webBindMgr->GetNativeGroup(m_WebFrameBindGroup), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 1, webBindMgr->GetNativeGroup(tpBG), 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(scenePassEncoder, 0, webBufMgrT->GetNativeBuffer(m_WebTreeMeshVB), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(scenePassEncoder, webBufMgrT->GetNativeBuffer(m_WebTreeMeshIB), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(scenePassEncoder, m_WebTreeIndexCount, tv->density, 0, 0, 0);

            webBindMgr->DestroyBindGroup(tpBG);
            bufMgr->DestroyBuffer(tpBuf);
        }
    }

    // ========================================================================
    // Particles (instanced billboard quads, alpha-blended)
    // ========================================================================
    if (usePostProcess && m_WebParticlePipeline.IsValid() && scenePassEncoder) {
        auto* webBufMgrP = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        const auto& particleEntities = m_World->GetEntitiesWithComponent<ParticleEmitterComponent>();

        // Collect all active particles into instance buffer
        struct ParticleInst { f32 px, py, pz, size, alpha, r, g, b; };
        std::vector<ParticleInst> instances;
        instances.reserve(1024);

        for (Entity pe : particleEntities) {
            auto* emitter = m_World->GetComponent<ParticleEmitterComponent>(pe);
            if (!emitter || emitter->pool.particles.empty()) continue;
            for (const auto& p : emitter->pool.particles) {
                if (p.lifetime <= 0.0f) continue;
                f32 lifeRatio = p.lifetime / std::max(p.maxLifetime, 0.001f);
                instances.push_back({p.position.x, p.position.y, p.position.z,
                    p.size, p.alpha * lifeRatio, p.color.x, p.color.y, p.color.z});
                if (instances.size() >= WEB_MAX_PARTICLES) break;
            }
            if (instances.size() >= WEB_MAX_PARTICLES) break;
        }

        if (!instances.empty()) {
            auto instBuf = bufMgr->CreateBufferWithData(
                {instances.size() * sizeof(ParticleInst), Renderer::GPUBufferUsage::Vertex | Renderer::GPUBufferUsage::CopyDst, true},
                instances.data());

            wgpuRenderPassEncoderSetPipeline(scenePassEncoder, webPipeMgr->GetNativePipeline(m_WebParticlePipeline));
            wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 0, webBindMgr->GetNativeGroup(m_WebFrameBindGroup), 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(scenePassEncoder, 0, webBufMgrP->GetNativeBuffer(m_WebParticleQuadVB), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetVertexBuffer(scenePassEncoder, 1, webBufMgrP->GetNativeBuffer(instBuf), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(scenePassEncoder, webBufMgrP->GetNativeBuffer(m_WebParticleQuadIB), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(scenePassEncoder, 6, static_cast<u32>(instances.size()), 0, 0, 0);

            bufMgr->DestroyBuffer(instBuf);
        }
    }

    // ========================================================================
    // Sprites (instanced textured billboards, alpha-blended)
    // ========================================================================
    if (usePostProcess && m_WebSpritePipeline.IsValid() && scenePassEncoder) {
        auto* webBufMgrS = static_cast<Renderer::WebGPUBufferManager*>(bufMgr);
        const auto& spriteEntities = m_World->GetEntitiesWithComponent<Sprite2DComponent>();

        struct SpriteInst { f32 px, py, pz, sizeX, sizeY, rotation, tintR, tintG, tintB, tintA, uvL, uvT, uvR, uvB; };
        std::vector<SpriteInst> spriteInsts;
        spriteInsts.reserve(spriteEntities.size());

        for (Entity se : spriteEntities) {
            auto* spr = m_World->GetComponent<Sprite2DComponent>(se);
            auto* sxf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(se) : nullptr;
            if (!spr || !sxf || !sxf->visible) continue;
            f32 srcL = spr->srcX, srcT = spr->srcY;
            f32 srcR = spr->srcWidth > 0 ? spr->srcX + spr->srcWidth : 1.0f;
            f32 srcB = spr->srcHeight > 0 ? spr->srcY + spr->srcHeight : 1.0f;
            spriteInsts.push_back({
                sxf->position.x, sxf->position.y, sxf->position.z,
                spr->size.x * sxf->scale.x, spr->size.y * sxf->scale.y,
                0.0f,  // rotation from transform Z euler (simplified)
                spr->tint.x, spr->tint.y, spr->tint.z, spr->alpha,
                srcL, srcT, srcR, srcB
            });
        }

        if (!spriteInsts.empty()) {
            auto instBuf = bufMgr->CreateBufferWithData(
                {spriteInsts.size() * sizeof(SpriteInst), Renderer::GPUBufferUsage::Vertex | Renderer::GPUBufferUsage::CopyDst, true},
                spriteInsts.data());

            // Use default white texture for now (per-sprite texturing would need batching)
            Renderer::GPUBindGroupDesc sprTexBGD;
            sprTexBGD.layout = m_WebSpriteTexLayout;
            sprTexBGD.entries = {
                {0, {}, 0, 0, m_WebDefaultWhiteTex, {}},
                {1, {}, 0, 0, {}, m_WebDefaultWhiteTex},
            };
            auto sprTexBG = webBindMgr->CreateBindGroup(sprTexBGD);

            wgpuRenderPassEncoderSetPipeline(scenePassEncoder, webPipeMgr->GetNativePipeline(m_WebSpritePipeline));
            wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 0, webBindMgr->GetNativeGroup(m_WebFrameBindGroup), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 1, webBindMgr->GetNativeGroup(sprTexBG), 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(scenePassEncoder, 0, webBufMgrS->GetNativeBuffer(m_WebParticleQuadVB), 0, WGPU_WHOLE_SIZE);  // Reuse quad
            wgpuRenderPassEncoderSetVertexBuffer(scenePassEncoder, 1, webBufMgrS->GetNativeBuffer(instBuf), 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderSetIndexBuffer(scenePassEncoder, webBufMgrS->GetNativeBuffer(m_WebParticleQuadIB), WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDrawIndexed(scenePassEncoder, 6, static_cast<u32>(spriteInsts.size()), 0, 0, 0);

            webBindMgr->DestroyBindGroup(sprTexBG);
            bufMgr->DestroyBuffer(instBuf);
        }
    }

    // Draw procedural sky (after scene, before ending pass)
    if (usePostProcess && m_WebSkyPipeline.IsValid() && scenePassEncoder) {
        wgpuRenderPassEncoderSetPipeline(scenePassEncoder, webPipeMgr->GetNativePipeline(m_WebSkyPipeline));
        wgpuRenderPassEncoderSetBindGroup(scenePassEncoder, 0, webBindMgr->GetNativeGroup(m_WebFrameBindGroup), 0, nullptr);
        wgpuRenderPassEncoderDraw(scenePassEncoder, 3, 1, 0, 0);  // Fullscreen triangle at z=1
    }

    // End scene render pass
    if (usePostProcess) {
        // End offscreen scene pass
        sceneEncoder.reset();  // Release encoder wrapper
        wgpuRenderPassEncoderEnd(scenePassEncoder);
        wgpuRenderPassEncoderRelease(scenePassEncoder);

        // Bloom chain (between scene and final tonemap)
        if (m_WebBloomThresholdPipeline.IsValid()) {
            auto bloomPass = [&](WGPUTextureView target, u32 tw, u32 th, WGPURenderPipeline pipe, WGPUBindGroup bg) {
                WGPURenderPassColorAttachment att = {};
                att.view = target;
                att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
                att.loadOp = WGPULoadOp_Clear;
                att.storeOp = WGPUStoreOp_Store;
                att.clearValue = {0, 0, 0, 1};
                WGPURenderPassDescriptor pd = {};
                pd.colorAttachmentCount = 1;
                pd.colorAttachments = &att;
                WGPURenderPassEncoder rp = webRenderer->GetCommandEncoder()
                    ? wgpuCommandEncoderBeginRenderPass(webRenderer->GetCommandEncoder(), &pd) : nullptr;
                if (rp) {
                    wgpuRenderPassEncoderSetPipeline(rp, pipe);
                    wgpuRenderPassEncoderSetViewport(rp, 0, 0, static_cast<f32>(tw), static_cast<f32>(th), 0.0f, 1.0f);
                    wgpuRenderPassEncoderSetBindGroup(rp, 0, bg, 0, nullptr);
                    wgpuRenderPassEncoderDraw(rp, 3, 1, 0, 0);
                    wgpuRenderPassEncoderEnd(rp);
                    wgpuRenderPassEncoderRelease(rp);
                }
            };

            auto* webTexMgrR = static_cast<Renderer::WebGPUTextureManager*>(m_Renderer->GetTextureManager());
            // Step 1: Threshold extract → bloom[0]
            {
                auto* bt = webTexMgrR->GetNativeTexture(m_WebBloomTex[0]);
                bloomPass(static_cast<WGPUTextureView>(m_WebBloomView[0]), bt->width, bt->height,
                    webPipeMgr->GetNativePipeline(m_WebBloomThresholdPipeline),
                    webBindMgr->GetNativeGroup(m_WebBloomThresholdBG));
            }
            // Step 2: Downsample chain bloom[i] → bloom[i+1]
            for (u32 i = 1; i < WEB_BLOOM_LEVELS; i++) {
                auto* bt = webTexMgrR->GetNativeTexture(m_WebBloomTex[i]);
                bloomPass(static_cast<WGPUTextureView>(m_WebBloomView[i]), bt->width, bt->height,
                    webPipeMgr->GetNativePipeline(m_WebBloomDownPipeline),
                    webBindMgr->GetNativeGroup(m_WebBloomDownBG[i - 1]));
            }
            // Step 3: Upsample chain bloom[i+1] → bloom[i] (additive blend via shader)
            for (i32 i = static_cast<i32>(WEB_BLOOM_LEVELS) - 2; i >= 0; i--) {
                auto* bt = webTexMgrR->GetNativeTexture(m_WebBloomTex[i]);
                // Upsample from bloom[i+1], writing to bloom[i] (loadOp=Load to keep existing + add)
                WGPURenderPassColorAttachment att = {};
                att.view = static_cast<WGPUTextureView>(m_WebBloomView[i]);
                att.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
                att.loadOp = WGPULoadOp_Load;  // Keep downsample result, add upsample on top
                att.storeOp = WGPUStoreOp_Store;
                WGPURenderPassDescriptor pd = {};
                pd.colorAttachmentCount = 1;
                pd.colorAttachments = &att;
                WGPURenderPassEncoder rp = webRenderer->GetCommandEncoder()
                    ? wgpuCommandEncoderBeginRenderPass(webRenderer->GetCommandEncoder(), &pd) : nullptr;
                if (rp) {
                    wgpuRenderPassEncoderSetPipeline(rp, webPipeMgr->GetNativePipeline(m_WebBloomUpPipeline));
                    wgpuRenderPassEncoderSetViewport(rp, 0, 0, static_cast<f32>(bt->width), static_cast<f32>(bt->height), 0.0f, 1.0f);
                    wgpuRenderPassEncoderSetBindGroup(rp, 0, webBindMgr->GetNativeGroup(m_WebBloomUpBG[i + 1]), 0, nullptr);
                    wgpuRenderPassEncoderDraw(rp, 3, 1, 0, 0);
                    wgpuRenderPassEncoderEnd(rp);
                    wgpuRenderPassEncoderRelease(rp);
                }
            }
            // Step 4: Composite scene + bloom[0] → scratch texture
            {
                bloomPass(static_cast<WGPUTextureView>(m_WebBloomScratchView),
                    static_cast<u32>(sceneW), static_cast<u32>(sceneH),
                    webPipeMgr->GetNativePipeline(m_WebBloomCompositePipeline),
                    webBindMgr->GetNativeGroup(m_WebBloomCompositeBG));
            }
            // The post-process will read from the scratch texture (scene + bloom composited)
            // We use m_WebPostProcessBG which was set up to read scratch at init time.
        }

        // Upload accessibility params to post-process UBO
        if (m_WebPPAccessibilityBuffer.IsValid() && bufMgr) {
            bufMgr->UploadData(m_WebPPAccessibilityBuffer, &m_WebPPAccessibility, sizeof(WebPPAccessibilityParams));
        }

        // Post-process pass: fullscreen triangle ACES tonemap to swapchain
        WGPURenderPassColorAttachment ppColorAtt = {};
        ppColorAtt.view = webRenderer->GetSwapChainView();
        ppColorAtt.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        ppColorAtt.loadOp = WGPULoadOp_Clear;
        ppColorAtt.storeOp = WGPUStoreOp_Store;
        ppColorAtt.clearValue = {0.0, 0.0, 0.0, 1.0};

        WGPURenderPassDescriptor ppPassDesc = {};
        ppPassDesc.colorAttachmentCount = 1;
        ppPassDesc.colorAttachments = &ppColorAtt;
        ppPassDesc.depthStencilAttachment = nullptr;  // No depth for fullscreen triangle
        WGPURenderPassEncoder ppPass = webRenderer->GetCommandEncoder()
            ? wgpuCommandEncoderBeginRenderPass(webRenderer->GetCommandEncoder(), &ppPassDesc) : nullptr;
        if (ppPass) {
            wgpuRenderPassEncoderSetPipeline(ppPass, webPipeMgr->GetNativePipeline(m_WebPostProcessPipeline));
            wgpuRenderPassEncoderSetViewport(ppPass, 0, 0, w, h, 0.0f, 1.0f);
            wgpuRenderPassEncoderSetBindGroup(ppPass, 0, webBindMgr->GetNativeGroup(m_WebPostProcessBG), 0, nullptr);
            wgpuRenderPassEncoderDraw(ppPass, 3, 1, 0, 0);  // Fullscreen triangle
            wgpuRenderPassEncoderEnd(ppPass);
            wgpuRenderPassEncoderRelease(ppPass);
            // Tell the frame loop the swapchain has the finished image — without this
            // its fallback clear pass runs after us and wipes the frame to sky blue
            webRenderer->MarkSwapchainWritten();
        }
    } else {
        m_Renderer->EndRenderPass(encoder);
    }
}

void RenderSystem::OnEntityAdded(Entity entity) {
    m_LightListDirty = true;
    m_RenderListDirty = true;
}

void RenderSystem::OnEntityRemoved(Entity entity) {
    m_LightListDirty = true;
    m_RenderListDirty = true;

    u64 eid = EntityIndex(entity);  // dense index: low 32 bits (raw handle has generation in high bits)
    if (eid < m_EntityRenderData.size() && m_EntityRenderData[eid].valid) {
        auto* bufMgr = m_Renderer ? m_Renderer->GetBufferManager() : nullptr;
        auto* bindMgr = m_Renderer ? m_Renderer->GetBindGroupManager() : nullptr;
        auto& rd = m_EntityRenderData[eid];
        if (bufMgr) {
            if (rd.vertexBuffer.IsValid()) bufMgr->DestroyBuffer(rd.vertexBuffer);
            if (rd.indexBuffer.IsValid()) bufMgr->DestroyBuffer(rd.indexBuffer);
        }
        if (bindMgr && rd.texBindGroup.IsValid()) bindMgr->DestroyBindGroup(rd.texBindGroup);
        rd.Invalidate();
    }
}

void RenderSystem::OnSceneClear() { m_SceneClearPending = true; }

void RenderSystem::FlushSceneClear() {
    if (!m_SceneClearPending) return;
    m_SceneClearPending = false;

    auto* bufMgr = m_Renderer ? m_Renderer->GetBufferManager() : nullptr;
    if (bufMgr) {
        for (auto& rd : m_EntityRenderData) {
            if (rd.valid) {
                if (rd.vertexBuffer.IsValid()) bufMgr->DestroyBuffer(rd.vertexBuffer);
                if (rd.indexBuffer.IsValid()) bufMgr->DestroyBuffer(rd.indexBuffer);
                rd.Invalidate();
            }
        }
    }
    m_EntityRenderData.clear();
    m_CachedLightEntities.clear();
    m_LightListDirty = true;
}

void RenderSystem::FlushPendingChanges() {
    if (m_SceneClearPending) FlushSceneClear();
}

void RenderSystem::RefreshStorageCache() {
    if (!m_World) return;
    m_CachedTransformStorage = m_World->GetComponentStorage<TransformComponent>();
    m_CachedMeshStorage = m_World->GetComponentStorage<MeshComponent>();
    m_CachedMaterialStorage = m_World->GetComponentStorage<MaterialComponent>();
    m_CachedMaterialSlotsStorage = m_World->GetComponentStorage<MaterialSlotsComponent>();
    m_CachedAnimatorStorage = m_World->GetComponentStorage<AnimatorComponent>();

    // Rebuild light entity list if dirty
    if (m_LightListDirty) {
        m_CachedLightEntities.clear();
        const auto& lightEntities = m_World->GetEntitiesWithComponent<LightComponent>();
        m_CachedLightEntities.assign(lightEntities.begin(), lightEntities.end());
        m_LightListDirty = false;
    }
}

// Web twin of the Vulkan-side ResolveAnimator (kept in both halves of this file's
// #if/#else split): follower meshes of a shared skeleton skin from the leader's
// animator so one imported model runs on a single animation clock.
AnimatorComponent* RenderSystem::ResolveAnimator(Entity entity) {
    AnimatorComponent* own = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity)
                                                     : m_World->GetComponent<AnimatorComponent>(entity);
    if (own) return own;
    if (SkeletonComponent* sk = m_World->GetComponent<SkeletonComponent>(entity)) {
        if (sk->skeleton) {
            auto it = m_SkeletonToAnimator.find(sk->skeleton.get());
            if (it != m_SkeletonToAnimator.end()) return it->second;
        }
    }
    return nullptr;
}

void RenderSystem::RenderEntity(Entity /*entity*/) {}
void RenderSystem::RenderSprites() {}
void RenderSystem::ClassifySceneComposition() {}
void RenderSystem::CreateDefaultMesh() {}
void RenderSystem::CreatePipeline() {}

// Stubs for methods not yet needed on WebGPU
void RenderSystem::SetBackfaceCullingEnabled(bool enabled) { m_BackfaceCulling = enabled; }
void RenderSystem::SetShadowDistance(f32 d) { m_ShadowDistance = d; }  // web: no ShadowMap object
void RenderSystem::SetWireframeEnabled(bool enabled) { m_WireframeMode = enabled; }
void RenderSystem::SetFluidSimulation(Effects::FluidSimulation* /*sim*/) {}
void RenderSystem::RenderWeatherParticles(const Effects::WeatherSystem& /*w*/, bool /*r*/, u32, u32) {}
void RenderSystem::RenderParticles(u32, u32) {}
void RenderSystem::RenderElementalParticles(const Effects::ElementalSystem&, u32, u32) {}
void RenderSystem::RenderFluid(u32, u32) {}
void RenderSystem::RenderGrass(u32, u32) {}
void RenderSystem::RenderShrubs(u32, u32) {}
void RenderSystem::RenderTrees(u32, u32) {}
u32 RenderSystem::GetMaxMSAASamples() const { return 4; }
void RenderSystem::SetAAMode(u32 mode) { m_AAMode = mode; }
void RenderSystem::SetUpscalerType(u32 type) { m_UpscalerType = type; }
void RenderSystem::SetUpscalerQuality(u32 quality) { m_UpscalerQuality = quality; }

} // namespace ECS
} // namespace Enjin

#else // !ENJIN_RENDERER_WEBGPU — Full Vulkan implementation below

// Includes for Vulkan implementation (forward-declared in header, needed here for full definitions)
#include "Enjin/Effects/Wind.h"
#include "Enjin/Effects/WeatherRenderer.h"
#include "Enjin/Effects/ParticleRenderer.h"
#include "Enjin/Effects/FluidRenderer.h"
#include "Enjin/Effects/ElementalSystem.h"
#include "Enjin/Effects/SpriteBatchRenderer.h"
#include "Enjin/Effects/SpriteTextureAtlas.h"
#include "Enjin/Effects/GrassRenderer.h"
#include "Enjin/Effects/ShrubRenderer.h"
#include "Enjin/Effects/TreeRenderer.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Hierarchy.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Name.h"
#include "Enjin/ECS/Components/Skeleton.h"
#include "Enjin/ECS/Components/WaterVolume.h"
#include "Enjin/ECS/Components/Water3D.h"
#include "Enjin/ECS/Components/Vegetation.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/Terrain2D.h"
#include "Enjin/ECS/Components/Flower.h"
#include "Enjin/ECS/Components/LOD.h"
#include "Enjin/ECS/Components/Controllers/CharacterController.h"
#include "Enjin/ECS/Components/Gameplay.h"
#include "Enjin/ECS/Components/Elemental.h"
#include "Enjin/ECS/Components/ArtStyle.h"
#include "Enjin/ECS/Components/MeshRenderer.h"
#include "Enjin/ECS/Components/IKComponents.h"
#include "Enjin/ECS/Components/BoneAttachment.h"
#include "Enjin/Animation/IKSolver.h"
#include "Enjin/Math/Math.h"
#include "Enjin/Renderer/Vulkan/ShaderData.h"
#include "Enjin/Renderer/Vulkan/VulkanPipeline.h"
#include "Enjin/Renderer/PointLightShadowMap.h"
#include "Enjin/Renderer/SpotLightShadowMap.h"
#include "Enjin/Renderer/MeshFactory.h"
#include "Enjin/Renderer/SVGLoader.h"
#include "Enjin/Renderer/RayTracing/AccelerationStructureManager.h"
#include "Enjin/Renderer/RayTracing/RTShadows.h"
#include "Enjin/Renderer/RayTracing/RTReflections.h"
#include "Enjin/Renderer/RayTracing/RTAmbientOcclusion.h"
#include "Enjin/Renderer/RayTracing/RTGlobalIllumination.h"
#include "Enjin/Renderer/RayTracing/RTTranslucency.h"
#include "Enjin/Renderer/RayTracing/RTCaustics.h"
#include "Enjin/Renderer/RayTracing/PathTracer.h"
#include "Enjin/Renderer/RayTracing/SVGFDenoiser.h"
#include "Enjin/Renderer/RayTracing/OIDNDenoiser.h"
#include "Enjin/Renderer/RayTracing/OptiXDenoiser.h"
#include "Enjin/Renderer/RayTracing/RTCompositor.h"
#include "Enjin/Renderer/RayTracing/RTTemporalReuse.h"
#include "Enjin/Renderer/RayTracing/ReSTIR.h"
#include "Enjin/Renderer/RayTracing/LightBVH.h"
#include "Enjin/Renderer/RayTracing/AdaptiveRayBudget.h"
#include "Enjin/Renderer/Vulkan/BindlessResources.h"
#include "Enjin/Renderer/RayTracing/RadianceCache.h"
#include "Enjin/Renderer/RayTracing/SurfelRadianceCache.h"
#include "Enjin/Renderer/RayTracing/RTShaderData.h"
#include "Enjin/Renderer/SHLightProbe.h"
#include "Enjin/Renderer/ReflectionProbeSystem.h"
#include "Enjin/Renderer/SDFScene.h"
#include "Enjin/Renderer/OITManager.h"
#ifdef ENJIN_CLUSTERED_LIGHTING
#include "Enjin/Renderer/ClusteredLighting.h"
#include "Enjin/Renderer/DDGIProbeSystem.h"
#include "Enjin/Renderer/VolumetricFog.h"
#include "Enjin/Effects/GPUParticleSystem.h"
#endif
#ifdef ENJIN_VISIBILITY_BUFFER
#include "Enjin/Renderer/VisibilityBuffer/VisibilityBuffer.h"
#endif
#ifdef ENJIN_VRS
#include "Enjin/Renderer/VRS/VariableRateShading.h"
#endif
#include "Enjin/Renderer/Upscaling/IUpscaler.h"
#include "Enjin/Renderer/Upscaling/FSR2Upscaler.h"
#include "Enjin/Renderer/Upscaling/DLSSUpscaler.h"
#include "Enjin/Renderer/Upscaling/XeSSUpscaler.h"
#include "Enjin/ECS/View.h"
#include <cstring>
#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Enjin {
namespace ECS {

RenderSystem::RenderSystem(World* world, Renderer::IRenderBackend* renderer)
    : m_World(world), m_Renderer(renderer) {
#if !ENJIN_RENDERER_WEBGPU
    m_VulkanRenderer = static_cast<Renderer::VulkanRenderer*>(renderer);
#endif
    m_Camera = nullptr;
}

RenderSystem::~RenderSystem() {
    Shutdown();
}

#if !ENJIN_RENDERER_WEBGPU
Renderer::VulkanRenderer* RenderSystem::GetVulkanRenderer() const {
    return m_VulkanRenderer;
}

Renderer::VulkanSwapchain* RenderSystem::GetSwapchain() const {
    return m_VulkanRenderer ? m_VulkanRenderer->GetSwapchain() : nullptr;
}

bool RenderSystem::IsHDREnabled() const {
    return m_VulkanRenderer ? m_VulkanRenderer->IsHDREnabled() : false;
}

u32 RenderSystem::GetHDROutputMode() const {
    return m_VulkanRenderer ? m_VulkanRenderer->GetHDROutputMode() : 0;
}
#endif

void RenderSystem::RefreshStorageCache() {
    if (!m_World) {
        m_CachedTransformStorage = nullptr;
        m_CachedMeshStorage = nullptr;
        m_CachedMaterialStorage = nullptr;
        m_CachedMaterialSlotsStorage = nullptr;
        m_CachedAnimatorStorage = nullptr;
        m_CachedTextStorage = nullptr;
        m_CachedArtStyleStorage = nullptr;
        m_CachedSpriteStorage = nullptr;
        m_CachedWaterVolumeStorage = nullptr;
        m_CachedWater3DStorage = nullptr;
        return;
    }
    m_CachedTransformStorage = m_World->GetComponentStorage<TransformComponent>();
    m_CachedMeshStorage = m_World->GetComponentStorage<MeshComponent>();
    m_CachedMaterialStorage = m_World->GetComponentStorage<MaterialComponent>();
    m_CachedMaterialSlotsStorage = m_World->GetComponentStorage<MaterialSlotsComponent>();
    m_CachedAnimatorStorage = m_World->GetComponentStorage<AnimatorComponent>();
    m_CachedTextStorage = m_World->GetComponentStorage<TextComponent>();
    m_CachedArtStyleStorage = m_World->GetComponentStorage<ArtStyleComponent>();
    m_CachedSpriteStorage = m_World->GetComponentStorage<Sprite2DComponent>();
    m_CachedWaterVolumeStorage = m_World->GetComponentStorage<WaterVolumeComponent>();
    m_CachedWater3DStorage = m_World->GetComponentStorage<Water3DComponent>();
}

void RenderSystem::Initialize() {
    if (m_Initialized) {
        return;
    }

    ENJIN_LOG_INFO(Renderer, "Initializing RenderSystem...");

    // Create default camera if none provided
    if (!m_Camera) {
        static Renderer::Camera defaultCamera;
        defaultCamera.SetPerspective(45.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        defaultCamera.SetLookAt(
            Math::Vector3(0.0f, 0.0f, 3.0f),  // Camera at z=3 looking at origin
            Math::Vector3(0.0f, 0.0f, 0.0f),
            Math::Vector3(0.0f, 1.0f, 0.0f)
        );
        m_Camera = &defaultCamera;
    }

    // Create shaders
    m_VertexShader = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!m_VertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TriangleVertexShaderData),
        Renderer::ShaderData::TriangleVertexShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load vertex shader");
        return;
    }

    m_FragmentShader = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!m_FragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::TriangleFragmentShaderData),
        Renderer::ShaderData::TriangleFragmentShaderDataSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to load fragment shader");
        return;
    }

    // Create shadow vertex shader (push-constant-based, avoids HOST_COHERENT UBO race)
    m_ShadowVertexShader = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!m_ShadowVertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::ShadowVertexShaderData),
        Renderer::ShaderData::ShadowVertexShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load shadow vertex shader");
    }

    // Initialize bindless resource manager BEFORE pipeline creation — pipelines
    // need the bindless descriptor set layout for set 1 in their pipeline layout.
    m_BindlessManager = std::make_unique<Renderer::BindlessResourceManager>(m_VulkanRenderer->GetContext());
    if (!m_BindlessManager->Initialize()) {
        ENJIN_LOG_WARN(Renderer, "Bindless resource manager initialization failed — per-entity descriptors will be used");
        m_BindlessManager.reset();
    } else {
        // Register default white texture so empty material slots have a valid fallback
        if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
            m_DefaultBindlessHandle = m_BindlessManager->RegisterTexture(
                m_DefaultWhiteTexture->GetImageView(), m_DefaultWhiteTexture->GetSampler());
        }
    }

    // Create pipeline
    CreatePipeline();

    // Create line pipeline for editor grid rendering
    CreateLinePipeline();

    // Create outline shaders and pipeline (inverted-hull geometry outlines)
    m_OutlineVertexShader = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!m_OutlineVertexShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::OutlineVertexShaderData),
        Renderer::ShaderData::OutlineVertexShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load outline vertex shader");
        m_OutlineVertexShader.reset();
    }
    m_OutlineFragmentShader = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!m_OutlineFragmentShader->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::OutlineFragmentShaderData),
        Renderer::ShaderData::OutlineFragmentShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Failed to load outline fragment shader");
        m_OutlineFragmentShader.reset();
    }
    CreateOutlinePipeline();
    CreateWireframeOverlayPipeline();

    // Create cascaded shadow map
    m_ShadowMap = std::make_unique<Renderer::ShadowMap>(m_VulkanRenderer->GetContext());
    Renderer::ShadowMapConfig shadowConfig;
    shadowConfig.resolution = 2048;
    shadowConfig.cascadeCount = 4;
    shadowConfig.shadowDistance = m_ShadowDistance;
    if (!m_ShadowMap->Initialize(shadowConfig)) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize shadow map, shadows disabled");
        m_ShadowsEnabled = false;
        m_ShadowMap.reset();  // Clear the failed shadow map
    } else {
        CreateShadowPipeline();
        if (!m_ShadowPipeline) {
            m_ShadowsEnabled = false;
            m_ShadowMap.reset();
        }
    }

    // Create point light shadow map (cubemap array for up to 4 point lights)
    m_PointShadowMap = std::make_unique<Renderer::PointLightShadowMap>(m_VulkanRenderer->GetContext());
    if (!m_PointShadowMap->Initialize(1024)) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize point light shadow map");
        m_PointShadowMap.reset();
    } else {
        CreatePointShadowPipeline();
    }

    // Create spot light shadow map (2D array for up to 4 spot lights)
    m_SpotShadowMap = std::make_unique<Renderer::SpotLightShadowMap>(m_VulkanRenderer->GetContext());
    if (!m_SpotShadowMap->Initialize(1024)) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize spot light shadow map");
        m_SpotShadowMap.reset();
    } else {
        CreateSpotShadowPipeline();
    }

    // All three shadow-map depth images are bound to the PBR descriptor set
    // (bindings 4/10/11) and sampled every frame. A map whose pass never runs
    // (e.g. no shadow-casting spot/point lights in the scene) stays in UNDEFINED
    // layout, tripping the sampled-image layout check at draw time (validation
    // vkCmdDraw-None-09600). Transition them to DEPTH_STENCIL_READ_ONLY_OPTIMAL
    // once at init; shadow passes that do run leave them in the same layout.
    {
        std::vector<VkImage> shadowDepthImages;
        if (m_ShadowMap)      shadowDepthImages.push_back(m_ShadowMap->GetDepthImage());
        if (m_PointShadowMap) shadowDepthImages.push_back(m_PointShadowMap->GetDepthImage());
        if (m_SpotShadowMap)  shadowDepthImages.push_back(m_SpotShadowMap->GetDepthImage());
        TransitionDepthImagesToReadable(shadowDepthImages);
    }

    // Create default white texture (used when no texture is bound).
    // This MUST succeed — without it, every texture fallback path leads to a null deref.
    m_DefaultWhiteTexture = std::make_unique<Renderer::Texture>(m_VulkanRenderer->GetContext());
    if (!m_DefaultWhiteTexture->CreateSolidColor(255, 255, 255, 255)) {
        ENJIN_LOG_FATAL(Renderer, "Failed to create default white texture — rendering will be broken");
    }

    // Create default bone buffer with 256 identity matrices — covers any bone index
    // a non-skinned mesh might reference without out-of-bounds SSBO reads.
    static constexpr usize DEFAULT_BONE_COUNT = 256;
    m_DefaultBoneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
    if (m_DefaultBoneBuffer->Create(DEFAULT_BONE_COUNT * sizeof(Math::Matrix4), Renderer::BufferUsage::Storage, true)) {
        std::vector<Math::Matrix4> identities(DEFAULT_BONE_COUNT);
        for (auto& mat : identities) mat = Math::Matrix4::Identity();
        m_DefaultBoneBuffer->UploadData(identities.data(), identities.size() * sizeof(Math::Matrix4));
    } else {
        ENJIN_LOG_WARN(Renderer, "Failed to create default bone buffer");
        m_DefaultBoneBuffer.reset();
    }

    // Default morph target buffer (header says targetCount=0, so shader skips morph loop)
    m_DefaultMorphBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
    {
        // Header: [vertexCount as uint bits, targetCount as uint bits] = [0, 0]
        f32 morphHeader[2] = {0.0f, 0.0f};
        if (m_DefaultMorphBuffer->Create(sizeof(morphHeader), Renderer::BufferUsage::Storage, true)) {
            m_DefaultMorphBuffer->UploadData(morphHeader, sizeof(morphHeader));
        } else {
            ENJIN_LOG_WARN(Renderer, "Failed to create default morph buffer");
            m_DefaultMorphBuffer.reset();
        }
    }

    // Create shadow data SSBO for point/spot light shadow matrices
    m_ShadowDataBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
    if (!m_ShadowDataBuffer->Create(sizeof(ShadowDataSSBO), Renderer::BufferUsage::Storage, true)) {
        ENJIN_LOG_WARN(Renderer, "Failed to create shadow data SSBO");
        m_ShadowDataBuffer.reset();
    }

    // Create uniform buffers and descriptor sets. Build the dummy placeholder
    // resources first (idempotent) so the very first descriptor write already has
    // valid views for bindings 19/21/22/23 -- these are statically sampled by the
    // PBR shader even when ray tracing never initializes (RT off or unsupported).
    CreateUniformBuffers();
    CreateRTDummyResources();
    CreateDescriptorSets();

    // Default active rendering target: main pass
    m_ActiveDescriptorSets = &m_DescriptorSets;
    m_ActiveUniformBuffers = &m_UniformBuffers;
    m_ActiveLightingBuffers = &m_LightingBuffers;

    // Create default sphere mesh
    CreateDefaultMesh();

    // Initialize weather particle renderer
    m_WeatherRenderer = std::make_unique<Effects::WeatherRenderer>();
    if (!m_WeatherRenderer->Initialize(m_VulkanRenderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "WeatherRenderer initialization failed, 3D particles disabled");
        m_WeatherRenderer.reset();
    }

    // Initialize particle emitter renderer
    m_ParticleRenderer = std::make_unique<Effects::ParticleRenderer>();
    if (!m_ParticleRenderer->Initialize(m_VulkanRenderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "ParticleRenderer initialization failed, emitter particles disabled");
        m_ParticleRenderer.reset();
    }

    // Initialize fluid renderer
    m_FluidRenderer = std::make_unique<Effects::FluidRenderer>();
    if (!m_FluidRenderer->Initialize(m_VulkanRenderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "FluidRenderer initialization failed, fluid rendering disabled");
        m_FluidRenderer.reset();
    }

    // Initialize grass renderer
    m_GrassRenderer = std::make_unique<Effects::GrassRenderer>();
    if (!m_GrassRenderer->Initialize(m_VulkanRenderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "GrassRenderer initialization failed, grass disabled");
        m_GrassRenderer.reset();
    }

    // Initialize shrub renderer
    m_ShrubRenderer = std::make_unique<Effects::ShrubRenderer>();
    if (!m_ShrubRenderer->Initialize(m_VulkanRenderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "ShrubRenderer initialization failed, shrubs disabled");
        m_ShrubRenderer.reset();
    }

    // Initialize tree renderer
    m_TreeRenderer = std::make_unique<Effects::TreeRenderer>();
    if (!m_TreeRenderer->Initialize(m_VulkanRenderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "TreeRenderer initialization failed, trees disabled");
        m_TreeRenderer.reset();
    }

    // Initialize sprite batch renderer
    m_SpriteBatchRenderer = std::make_unique<Effects::SpriteBatchRenderer>();
    if (!m_SpriteBatchRenderer->Initialize(m_VulkanRenderer, m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_WARN(Renderer, "SpriteBatchRenderer initialization failed, sprite batching disabled");
        m_SpriteBatchRenderer.reset();
    }

    // Initialize sprite texture atlas (auto-packs small sprites into one GPU texture)
    m_SpriteAtlas = std::make_unique<Effects::SpriteTextureAtlas>();
    if (m_SpriteAtlas->Initialize(m_VulkanRenderer->GetContext())) {
        if (m_SpriteBatchRenderer) m_SpriteBatchRenderer->SetAtlas(m_SpriteAtlas.get());
    } else {
        ENJIN_LOG_WARN(Renderer, "SpriteTextureAtlas initialization failed, atlas packing disabled");
        m_SpriteAtlas.reset();
    }

    // Initialize skybox
    m_Skybox.Initialize(m_VulkanRenderer->GetContext());
    CreateSkyboxCubeVBO();
    CreateSkyboxPipeline();

    // Set up shader hot-reload (editor-only)
    FindShaderDirectory();
    if (!m_ShaderDir.empty() && m_ShaderHotReloadEnabled) {
        SetupShaderWatchers();
    }

    // Initialize merged geometry buffer (single VB+IB for all static 3D meshes)
    m_GeometryPool = std::make_unique<Renderer::MergedGeometryBuffer>(m_VulkanRenderer->GetContext());
    if (!m_GeometryPool->Initialize()) {
        ENJIN_LOG_WARN(Renderer, "MergedGeometryBuffer initialization failed, using per-entity buffers");
        m_GeometryPool.reset();
    }

    // Initialize GPU frustum culling system (no readback stall — visibility read from previous frame)
    if (m_GPUCullingEnabled) {
        m_GPUCulling = std::make_unique<Renderer::GPUCullingSystem>(m_VulkanRenderer->GetContext());
        if (!m_GPUCulling->Initialize()) {
            ENJIN_LOG_WARN(Renderer, "GPUCullingSystem initialization failed, using CPU culling");
            m_GPUCulling.reset();
            m_GPUCullingEnabled = false;
        } else {
            ENJIN_LOG_INFO(Renderer, "GPU frustum culling enabled");
        }
    }

    // Initialize texture-grouped indirect draw batcher
    if (m_GPUCullingEnabled && m_GPUCulling) {
        m_IndirectDrawBatcher = std::make_unique<Renderer::IndirectDrawBatcher>();
        if (!m_IndirectDrawBatcher->Initialize(m_VulkanRenderer->GetContext(), m_GPUCulling->GetMaxObjects())) {
            ENJIN_LOG_WARN(Renderer, "IndirectDrawBatcher init failed, textured entities use per-entity draws");
            m_IndirectDrawBatcher.reset();
        } else {
            ENJIN_LOG_INFO(Renderer, "Texture-grouped indirect draw batching enabled");
        }
    }

    // Initialize Device Generated Commands (DGC) — GPU generates entire command stream.
    // DISABLED: the EXT init is incomplete — the IndirectExecutionSet is built from a
    // pipeline lacking VK_PIPELINE_CREATE_2_INDIRECT_BINDABLE_BIT_EXT and the commands
    // layout lacks the EXECUTION_SET token (validation 11153/11019/11011). DGC is
    // off-by-default at render time anyway, so skip init until the EXT path is finished.
    // Multi-draw indirect remains the active GPU-driven path.
    constexpr bool kEnableDGCInit = false;
    if (kEnableDGCInit && m_GPUCullingEnabled && m_GPUCulling && m_VulkanRenderer->GetContext()->IsDGCSupported()) {
        m_DGC = std::make_unique<Renderer::DeviceGeneratedCommands>();
        if (!m_DGC->Initialize(m_VulkanRenderer->GetContext(), m_GPUCulling->GetMaxObjects())) {
            ENJIN_LOG_INFO(Renderer, "DGC not available on this device, using multi-draw indirect");
            m_DGC.reset();
        } else {
            // Create commands layout now that the pipeline exists
            if (m_Pipeline && m_DGC->CreateCommandsLayout(m_Pipeline->GetLayout(), m_Pipeline->GetPipeline())) {
                ENJIN_LOG_INFO(Renderer, "Device Generated Commands initialized (disabled by default, toggle in editor)");
            } else {
                ENJIN_LOG_INFO(Renderer, "DGC commands layout creation deferred (pipeline not ready)");
            }
        }
    }

    // Initialize async compute scheduler for RT/denoise overlap
    {
        m_AsyncComputeScheduler = std::make_unique<Renderer::AsyncComputeScheduler>();
        if (m_AsyncComputeScheduler->Initialize(m_VulkanRenderer->GetContext(), 2)) {
            ENJIN_LOG_INFO(Renderer, "Async compute scheduler enabled (RT/denoise overlap)");
        } else {
            // Not an error — many GPUs share graphics/compute queue
            m_AsyncComputeScheduler.reset();
        }
    }

    // Initialize thread pool and per-thread command buffer pools
    m_ThreadPool.Initialize();
    u32 framesInFlight = 2; // Matches VulkanRenderer::MAX_FRAMES_IN_FLIGHT
    m_CmdBufferPool = std::make_unique<Renderer::CommandBufferPool>();
    if (!m_CmdBufferPool->Initialize(m_VulkanRenderer->GetContext(),
                                     m_ThreadPool.GetThreadCount(), framesInFlight)) {
        ENJIN_LOG_WARN(Renderer, "CommandBufferPool init failed, shadow passes will be single-threaded");
        m_CmdBufferPool.reset();
    }

    // Initialize ray tracing subsystems (if hardware supports it)
    InitializeRayTracing();

    // Now that RT dummy textures exist, write bindings 21-23 that were skipped earlier
    if (m_RTDummyImageView && m_RTDummySampler && !m_DescriptorSets.empty()) {
        CreateDescriptorSets(); // Re-creates with all 24 bindings now that dummies are available
    }

    // Initialize OIT, SH light probes, and SDF scene
    m_OITManager = std::make_unique<Renderer::OITManager>();
    auto extent = m_VulkanRenderer->GetSwapchainExtent();
    if (!m_OITManager->Initialize(m_VulkanRenderer->GetContext(), extent.width, extent.height, m_VulkanRenderer->GetRenderPass())) {
        ENJIN_LOG_WARN(Renderer, "OITManager init failed, OIT disabled");
        m_OITManager.reset();
    }

    m_SHLighting = std::make_unique<Renderer::SHLightingSystem>();
    m_ReflectionProbes = std::make_unique<Renderer::ReflectionProbeSystem>();
    m_ReflectionProbes->Initialize(m_VulkanRenderer->GetContext());
    m_SDFScene = std::make_unique<Renderer::SDFScene>();

    // Per-frame linear allocator: 8 MB supports ~100K entities x 128B each
    m_FrameAllocator = std::make_unique<FrameAllocator>(8 * 1024 * 1024);

    // Initialize clustered forward lighting system
#ifdef ENJIN_CLUSTERED_LIGHTING
    {
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        m_ClusteredLighting = std::make_unique<Renderer::ClusteredLightingSystem>(m_VulkanRenderer->GetContext());
        if (!m_ClusteredLighting->Initialize(extent.width, extent.height)) {
            ENJIN_LOG_WARN(Renderer, "Clustered lighting init failed — falling back to brute-force");
            m_ClusteredLighting.reset();
        }
    }
#endif

    // Initialize DDGI probe system (software-traced GI — no hardware RT required)
    {
        m_DDGISystem = std::make_unique<Renderer::DDGIProbeSystem>(m_VulkanRenderer->GetContext());
        Renderer::DDGIConfig ddgiConfig;
        // Start disabled — enable via editor UI or code when ready
        ddgiConfig.enabled = false;
        if (!m_DDGISystem->Initialize(ddgiConfig)) {
            ENJIN_LOG_WARN(Renderer, "DDGI init failed — software GI disabled");
            m_DDGISystem.reset();
        }
    }

    // Initialize volumetric fog system (froxel-based participating media)
    {
        m_VolumetricFog = std::make_unique<Renderer::VolumetricFogSystem>(m_VulkanRenderer->GetContext());
        Renderer::VolumetricFogConfig fogConfig;
        fogConfig.enabled = false; // Start disabled
        if (m_ClusteredLighting) m_VolumetricFog->SetClusteredLighting(m_ClusteredLighting.get());
        if (!m_VolumetricFog->Initialize(fogConfig)) {
            ENJIN_LOG_WARN(Renderer, "VolumetricFog init failed — volumetric effects disabled");
            m_VolumetricFog.reset();
        }
    }

    // Initialize GPU particle system (compute-based simulation)
    {
        m_GPUParticleSystem = std::make_unique<Effects::GPUParticleSystem>(m_VulkanRenderer->GetContext());
        Effects::GPUEmitterConfig particleConfig;
        // Don't auto-initialize — created on demand when GPU emitters are added
        if (!m_GPUParticleSystem->Initialize(particleConfig)) {
            ENJIN_LOG_WARN(Renderer, "GPUParticleSystem init failed — GPU particles disabled");
            m_GPUParticleSystem.reset();
        }
    }

    // Initialize visibility buffer renderer
#ifdef ENJIN_VISIBILITY_BUFFER
    {
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        m_VisibilityBuffer = std::make_unique<Renderer::VisibilityBufferRenderer>(m_VulkanRenderer->GetContext());
        if (!m_VisibilityBuffer->Initialize(extent.width, extent.height, m_VulkanRenderer->GetRenderPass())) {
            ENJIN_LOG_WARN(Renderer, "Visibility buffer init failed — using standard forward path");
            m_VisibilityBuffer.reset();
        }
    }
#endif

    // Initialize variable rate shading
#ifdef ENJIN_VRS
    if (m_VulkanRenderer->GetContext()->IsVRSSupported()) {
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        m_VRS = std::make_unique<Renderer::VariableRateShading>(m_VulkanRenderer->GetContext());
        if (!m_VRS->Initialize(extent.width, extent.height)) {
            ENJIN_LOG_WARN(Renderer, "VRS init failed — shading rate control disabled");
            m_VRS.reset();
        }
    }
#endif

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "RenderSystem initialized (bindless: %s)",
                   m_BindlessManager ? "enabled" : "disabled");
}

void RenderSystem::OnSceneClear() {
    // Defer the actual clear to the next FlushPendingChanges() call at the
    // frame boundary so we never tear down GPU resources mid-frame.
    m_SceneClearPending = true;
}

void RenderSystem::FlushSceneClear() {
    // Wait for in-flight GPU work before invalidating any resources
    if (m_Renderer && m_VulkanRenderer->GetContext()) {
        m_VulkanRenderer->GetContext()->WaitForGPU();
    }

    m_EntityRenderData.clear();
    m_SortedRenderList.clear();
    m_EntityMaterialIndex.clear();
    m_EntityToCullIndex.clear();
    m_CullableObjects.clear();
    m_CachedLightEntities.clear();
    m_LastBound.Reset();
    // Null ALL cached storage pointers — World::Clear() destroyed the storages
    // they pointed to, so these are dangling. They'll be refreshed in Update()
    // via RefreshStorageCache(). Until then, render code null-checks these.
    m_CachedTransformStorage = nullptr;
    m_CachedMeshStorage = nullptr;
    m_CachedMaterialStorage = nullptr;
    m_CachedAnimatorStorage = nullptr;
    m_CachedFallbackAnimator = nullptr;
    m_CachedTextStorage = nullptr;
    m_MaterialSSBOBuilt = false;
    m_MaterialSSBODirty = true;
    m_LightListDirty = true;
    m_SceneComposition.dirty = true;
    m_SceneClearCooldown = 2;  // Skip game view for 2 frames (double-buffered)

    // Invalidate RT acceleration structures so the driver doesn't access freed geometry
    if (m_ASManager) {
        m_ASManager->InvalidateAll();
        m_ASManager->ResetInstances();
    }
}

void RenderSystem::Shutdown() {
    if (!m_Initialized) {
        return;
    }

    // Wait for GPU to finish (fence-based when renderer is active)
    if (m_Renderer && m_VulkanRenderer->GetContext()) {
        m_VulkanRenderer->GetContext()->WaitForGPU();
    }

#if !ENJIN_RENDERER_WEBGPU
    ShutdownComputeSkinning();
#endif

    // Clean up descriptor pool
    if (m_DescriptorPool != VK_NULL_HANDLE && m_VulkanRenderer->GetContext()) {
        vkDestroyDescriptorPool(m_VulkanRenderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
    }

    // Clean up entity render data
    m_EntityRenderData.clear();

    // Clean up uniform buffers
    m_UniformBuffers.clear();
    m_LightingBuffers.clear();
    m_MaterialBuffers.clear();
    m_DescriptorSets.clear();
    m_OffscreenUniformBuffers.clear();
    m_OffscreenLightingBuffers.clear();
    m_OffscreenDescriptorSets.clear();
    m_ActiveDescriptorSets = nullptr;
    m_ActiveUniformBuffers = nullptr;
    m_ActiveLightingBuffers = nullptr;

    // Clean up performance optimization subsystems
#ifdef ENJIN_CLUSTERED_LIGHTING
    if (m_ClusteredLighting) { m_ClusteredLighting->Shutdown(); m_ClusteredLighting.reset(); }
#endif
#ifdef ENJIN_VISIBILITY_BUFFER
    if (m_VisibilityBuffer) { m_VisibilityBuffer->Shutdown(); m_VisibilityBuffer.reset(); }
#endif
#ifdef ENJIN_VRS
    if (m_VRS) { m_VRS->Shutdown(); m_VRS.reset(); }
#endif

    // Clean up weather, particle, grass, shrub, tree, and sprite batch renderers
    m_WeatherRenderer.reset();
    m_ParticleRenderer.reset();
    m_GrassRenderer.reset();
    m_ShrubRenderer.reset();
    m_TreeRenderer.reset();
    m_SpriteAtlas.reset();
    m_SpriteBatchRenderer.reset();

    // Clean up merged geometry buffer (before entity render data so pool frees are valid)
    m_GeometryPool.reset();

    // Clean up thread pool and command buffer pools (before GPU culling)
    m_ThreadPool.Shutdown();
    m_CmdBufferPool.reset();

    // Clean up async compute scheduler
    if (m_AsyncComputeScheduler) {
        m_AsyncComputeScheduler->Shutdown();
        m_AsyncComputeScheduler.reset();
    }

    // Clean up Device Generated Commands
    if (m_DGC) {
        m_DGC->Shutdown();
        m_DGC.reset();
    }

    // Clean up texture-grouped indirect draw batcher
    if (m_IndirectDrawBatcher) {
        m_IndirectDrawBatcher->Shutdown();
        m_IndirectDrawBatcher.reset();
    }

    // Clean up GPU culling system
    if (m_GPUCulling) {
        m_GPUCulling->Shutdown();
        m_GPUCulling.reset();
    }
    m_CullableObjects.clear();
    m_EntityToCullIndex.clear();

    // Clean up skybox resources
    m_SkyboxVertexBuffer.reset();
    m_SkyboxUniformBuffers.clear();
    if (m_VulkanRenderer->GetContext()) {
        VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();
        if (m_SkyboxPipelineHandle != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, m_SkyboxPipelineHandle, nullptr);
            m_SkyboxPipelineHandle = VK_NULL_HANDLE;
        }
        if (m_SkyboxPipelineLayoutHandle != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, m_SkyboxPipelineLayoutHandle, nullptr);
            m_SkyboxPipelineLayoutHandle = VK_NULL_HANDLE;
        }
        if (m_SkyboxDescriptorSetLayoutHandle != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_SkyboxDescriptorSetLayoutHandle, nullptr);
            m_SkyboxDescriptorSetLayoutHandle = VK_NULL_HANDLE;
        }
        if (m_SkyboxDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, m_SkyboxDescriptorPool, nullptr);
            m_SkyboxDescriptorPool = VK_NULL_HANDLE;
        }
    }
    m_Skybox.Shutdown();

    // Clean up OIT, SH light probes, reflection probes, and SDF scene
    if (m_OITManager) { m_OITManager->Shutdown(); m_OITManager.reset(); }
    m_SHLighting.reset();
    if (m_ReflectionProbes) { m_ReflectionProbes->Shutdown(); m_ReflectionProbes.reset(); }
    m_SDFScene.reset();

    // Clean up ray tracing subsystems
    ShutdownRayTracing();

    // Clean up line pipeline
    m_OffscreenLinePipeline.reset();
    m_LinePipeline.reset();

    // Clean up shadow resources
    m_ShadowPipeline.reset();
    m_ShadowMap.reset();
    m_PointShadowPipeline.reset();
    m_PointShadowMap.reset();
    m_SpotShadowPipeline.reset();
    m_SpotShadowMap.reset();
    m_ShadowDataBuffer.reset();

    // Clean up text texture cache and rasterizer
    m_TextTextureCache.clear();
    m_TextRasterizer.ClearFontCache();

    // Clean up textures and bone buffers
    m_DefaultWhiteTexture.reset();
    m_DefaultBoneBuffer.reset();
    m_GhostBoneBuffer.reset();
    m_GhostBoneBufferCapacity = 0;

    // Clean up pipeline
    m_OffscreenPipeline.reset();
    m_Pipeline.reset();
    m_FragmentShader.reset();
    m_VertexShader.reset();

    m_Initialized = false;
}

void RenderSystem::ProcessPendingRecreation() {
    if (m_PendingRecreation == PendingRecreationType::None) return;

    // Wait for all in-flight frames to finish (2 fences, fast)
    m_VulkanRenderer->WaitForAllFrames();

    switch (m_PendingRecreation) {
        case PendingRecreationType::PipelineOnly:
            RecreatePipelines(true);
            break;
        case PendingRecreationType::MainShader:
            m_VertexShader = std::move(m_PendingVertexShader);
            m_FragmentShader = std::move(m_PendingFragmentShader);
            RecreatePipelines(true);
            ENJIN_LOG_INFO(Renderer, "Shader hot-reload: main shaders reloaded successfully");
            break;
        case PendingRecreationType::SkyboxShader: {
            VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();
            if (m_SkyboxPipelineHandle != VK_NULL_HANDLE) {
                vkDestroyPipeline(device, m_SkyboxPipelineHandle, nullptr);
                m_SkyboxPipelineHandle = VK_NULL_HANDLE;
            }
            if (m_SkyboxPipelineLayoutHandle != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(device, m_SkyboxPipelineLayoutHandle, nullptr);
                m_SkyboxPipelineLayoutHandle = VK_NULL_HANDLE;
            }
            if (m_SkyboxDescriptorSetLayoutHandle != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(device, m_SkyboxDescriptorSetLayoutHandle, nullptr);
                m_SkyboxDescriptorSetLayoutHandle = VK_NULL_HANDLE;
            }
            if (m_SkyboxDescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(device, m_SkyboxDescriptorPool, nullptr);
                m_SkyboxDescriptorPool = VK_NULL_HANDLE;
            }
            m_SkyboxDescriptorSets.clear();
            m_SkyboxUniformBuffers.clear();
            CreateSkyboxPipeline();
            ENJIN_LOG_INFO(Renderer, "Shader hot-reload: skybox shaders reloaded successfully");
            break;
        }
        case PendingRecreationType::ShadowShader:
            m_ShadowVertexShader = std::move(m_PendingVertexShader);
            RecreatePipelines(true);
            ENJIN_LOG_INFO(Renderer, "Shader hot-reload: shadow vertex shader reloaded successfully");
            break;
        default: break;
    }
    m_PendingRecreation = PendingRecreationType::None;
    m_PendingVertexShader.reset();
    m_PendingFragmentShader.reset();
}

void RenderSystem::FlushPendingChanges() {
    if (!m_Renderer || !m_Initialized) return;

    // Flush deferred scene clear (set by OnSceneClear mid-frame)
    if (m_SceneClearPending) {
        m_SceneClearPending = false;
        FlushSceneClear();
    }

    // Apply deferred shadow resolution change before pipeline recreation
    // (ProcessPendingRecreation will wait for the GPU via WaitForAllFrames)
    if (m_PendingShadowResolution != 0 && m_ShadowMap) {
        m_ShadowMap->SetResolution(m_PendingShadowResolution);
        m_PendingShadowResolution = 0;
    }

    ProcessPendingRecreation();

    // Apply deferred skybox config — must happen before any rendering commands
    // reference the old cubemap (including RenderOffscreen for the Game View)
    if (m_PendingSkyboxConfig) {
        m_PendingSkyboxConfig = false;
        if (m_Skybox.IsValid()) {
            m_VulkanRenderer->WaitForAllFrames();
        }
        m_Skybox.SetConfig(m_PendingSkybox);
    }

    // Process pending reflection probe bakes — renders 6 faces per probe.
    // Safe to do here because no frame is in progress yet.
    if (m_ReflectionProbes && m_ReflectionProbes->HasPendingBake()) {
        m_VulkanRenderer->WaitForAllFrames();
        m_ReflectionProbes->ProcessPendingBakes(m_World, this);
        // Update descriptor binding 19 with the newly baked cubemap
        UpdateProbeCubemapDescriptor();
    }
}

void RenderSystem::Update(f32 deltaTime) {
    if (!m_Renderer || !m_Initialized) {
        return;
    }

    // Apply deferred MSAA change (requested mid-frame by editor settings UI).
    // Must happen before any rendering — it recreates swapchain, render pass, pipelines.
    if (m_PendingMSAAChange) {
        ApplyPendingMSAAChange();
    }

    // Process any pending changes not yet flushed (fallback if FlushPendingChanges
    // wasn't called earlier this frame, e.g. in standalone Player without editor)
    FlushPendingChanges();

    // Cache component storage pointers for this frame. Each storage pointer is looked
    // up once here (via type-ID hash) instead of once per entity in the hot loops.
    RefreshStorageCache();

    // Mark all transform world-matrix caches dirty so each entity recomputes at most
    // once this frame (across main pass, shadow pass, outline pass, etc.).
    if (m_CachedTransformStorage) {
        auto& transforms = m_CachedTransformStorage->GetComponents();
        for (auto& t : transforms) {
            t.worldMatrixDirty = true;
        }
    }

    // Reset per-frame stats
    ResetFrameCounters();

    // Begin async compute scheduler frame (reset per-frame state)
    if (m_AsyncComputeScheduler) {
        m_AsyncComputeScheduler->BeginFrame(m_VulkanRenderer->GetCurrentFrameIndex());
    }

    // Reset material SSBO flag — will be rebuilt on first use this frame
    m_MaterialSSBOBuilt = false;

    // Reset per-frame linear allocator (all FrameArray allocations from previous frame are freed)
    if (m_FrameAllocator) m_FrameAllocator->Reset();

    // Rebuild cached light entity list only when dirty (entity add/remove with LightComponent).
    // Also detect dynamic LightComponent add/remove on existing entities via count mismatch.
    if (m_World) {
        const auto& liveLights = m_World->GetEntitiesWithComponent<LightComponent>();
        if (!m_LightListDirty && liveLights.size() != m_CachedLightEntities.size()) {
            m_LightListDirty = true;
        }
        if (m_LightListDirty) {
            m_CachedLightEntities.assign(liveLights.begin(), liveLights.end());
            m_LightListDirty = false;
        }
    } else if (m_LightListDirty) {
        m_CachedLightEntities.clear();
        m_LightListDirty = false;
    }

    // Reset per-thread command buffer pools for this frame
    if (m_CmdBufferPool) {
        m_CmdBufferPool->ResetFrame(m_VulkanRenderer->GetCurrentFrameIndex());
    }

    // Poll texture and shader file watchers every 5 seconds (time-based, not frame-count).
    m_WatcherPollTimer += deltaTime;
    if (m_WatcherPollTimer >= 5.0f) {
        m_WatcherPollTimer = 0.0f;
        m_TextureWatcher.Poll();
        if (m_ShaderHotReloadEnabled && !m_ShaderDir.empty()) {
            m_ShaderWatcher.Poll();
        }
    }

    // Auto-create meshes for water volume entities that don't have one yet
    EnsureWaterMeshes();
    EnsureWater3DMeshes();

    // Regenerate terrain meshes when dirty (only iterate entities that have the component)
    {
        for (Entity entity : m_World->GetEntitiesWithComponent<TerrainComponent>()) {
            auto* terrain = m_World->GetComponent<TerrainComponent>(entity);
            if (terrain && terrain->meshDirty) {
                auto mesh = Renderer::MeshFactory::CreateTerrain(*terrain);
                if (m_World->HasComponent<MeshComponent>(entity)) {
                    *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
                } else {
                    m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
                }
                // Force re-upload of GPU buffers
                if (static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size())
                    m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].Invalidate();
                terrain->meshDirty = false;
            }
        }
        for (Entity entity : m_World->GetEntitiesWithComponent<Terrain2DComponent>()) {
            auto* terrain2d = m_World->GetComponent<Terrain2DComponent>(entity);
            if (terrain2d && terrain2d->meshDirty) {
                auto mesh = Renderer::MeshFactory::CreateTerrain2D(*terrain2d);
                if (m_World->HasComponent<MeshComponent>(entity)) {
                    *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
                } else {
                    m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
                }
                if (static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size())
                    m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].Invalidate();
                terrain2d->meshDirty = false;
            }
        }
        // JellyMesh dirty check (flower system vertex deformation)
        // Re-upload vertex data to existing buffer instead of erase/recreate,
        // because destroying buffers while the GPU is still reading them crashes the driver.
        for (Entity entity : m_World->GetEntitiesWithComponent<JellyMeshComponent>()) {
            auto* jelly = m_World->GetComponent<JellyMeshComponent>(entity);
            if (jelly && jelly->meshDirty) {
                EntityRenderData* rd = GetRenderData(entity);
                if (rd && rd->vertexBuffer) {
                    auto* mesh = m_World->GetComponent<MeshComponent>(entity);
                    if (mesh && !mesh->vertices.empty()) {
                        usize dataSize = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
                        rd->vertexBuffer->UploadData(mesh->vertices.data(), dataSize);
                    }
                } else {
                    // No existing buffer — will be created on next render via SetupEntityBuffers
                }
                jelly->meshDirty = false;
            }
        }

        // Advance animated sprite timers and mark dirty on frame change
        for (Entity entity : m_World->GetEntitiesWithComponent<AnimatedSprite2DComponent>()) {
            auto* anim = m_World->GetComponent<AnimatedSprite2DComponent>(entity);
            auto* sprite = m_World->GetComponent<Sprite2DComponent>(entity);
            if (!anim || !sprite || !anim->playing || anim->frames.empty()) continue;

            anim->frameTimer += deltaTime * anim->playbackSpeed;
            const auto& frame = anim->frames[anim->currentFrame];
            if (anim->frameTimer >= frame.duration) {
                anim->frameTimer -= frame.duration;
                anim->frameChanged = true;
                u32 nextFrame = anim->currentFrame + 1;
                if (nextFrame >= static_cast<u32>(anim->frames.size())) {
                    if (anim->loop) { nextFrame = 0; }
                    else { nextFrame = anim->currentFrame; anim->playing = false; anim->animationComplete = true; }
                }
                anim->currentFrame = nextFrame;
                const auto& newFrame = anim->frames[anim->currentFrame];
                sprite->srcX = newFrame.srcX;
                sprite->srcY = newFrame.srcY;
                sprite->spriteDirty = true;
            } else {
                anim->frameChanged = false;
            }

            // Apply per-frame collider on frame change
            if (anim->frameChanged) {
                auto* pfc = m_World->GetComponent<PerFrameColliderComponent>(entity);
                if (pfc && pfc->autoApply && anim->currentFrame < static_cast<u32>(pfc->frameColliders.size())) {
                    auto* box = m_World->GetComponent<BoxColliderComponent>(entity);
                    if (box) {
                        const auto& fc = pfc->frameColliders[anim->currentFrame];
                        if (fc.enabled) {
                            box->center = Math::Vector3(fc.offset.x, fc.offset.y, 0.0f);
                            box->size = Math::Vector3(fc.size.x, fc.size.y, 0.1f);
                        }
                    }
                }
            }
        }

        // Auto-generate sprite quad meshes when dirty
        for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
            auto* sprite = m_World->GetComponent<Sprite2DComponent>(entity);
            if (!sprite || !sprite->spriteDirty) continue;

            // Resolve texture pixel dimensions for UV normalization
            if (sprite->texPixelWidth == 0 && !sprite->texturePath.empty()) {
                auto tex = GetOrLoadTexture(sprite->texturePath);
                if (tex && tex->IsValid()) {
                    sprite->texPixelWidth = static_cast<f32>(tex->GetWidth());
                    sprite->texPixelHeight = static_cast<f32>(tex->GetHeight());
                }
            }

            // Calculate normalized UVs (srcWidth 0 = full texture)
            f32 uvL = 0.0f, uvT = 0.0f, uvR = 1.0f, uvB = 1.0f;
            if (sprite->srcWidth > 0 && sprite->srcHeight > 0 && sprite->texPixelWidth > 0) {
                uvL = sprite->srcX / sprite->texPixelWidth;
                uvT = sprite->srcY / sprite->texPixelHeight;
                uvR = (sprite->srcX + sprite->srcWidth) / sprite->texPixelWidth;
                uvB = (sprite->srcY + sprite->srcHeight) / sprite->texPixelHeight;
            }

            auto mesh = Renderer::MeshFactory::CreateSpriteQuad(
                sprite->size.x, sprite->size.y,
                sprite->pivot.x, sprite->pivot.y,
                uvL, uvT, uvR, uvB,
                sprite->flipX, sprite->flipY);

            if (m_World->HasComponent<MeshComponent>(entity)) {
                *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
            } else {
                m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
            }
            if (static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size())
                m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].Invalidate();
            sprite->spriteDirty = false;
        }

        // Auto-generate tilemap meshes when dirty
        for (Entity entity : m_World->GetEntitiesWithComponent<TilemapComponent>()) {
            auto* tilemap = m_World->GetComponent<TilemapComponent>(entity);
            if (!tilemap || !tilemap->meshDirty) continue;

            auto mesh = Renderer::MeshFactory::CreateTilemapMesh(*tilemap);
            if (m_World->HasComponent<MeshComponent>(entity)) {
                *m_World->GetComponent<MeshComponent>(entity) = std::move(mesh);
            } else {
                m_World->AddComponent<MeshComponent>(entity, std::move(mesh));
            }
            if (static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size())
                m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].Invalidate();
            tilemap->meshDirty = false;
        }
    }

    // Update skeletal animators and apply IK constraints (single pass over AnimatorComponent entities)
    m_CachedFallbackAnimator = nullptr;
    m_SkeletonToAnimator.clear();
    for (Entity entity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
        AnimatorComponent* animComp = ResolveAnimator(entity);
        if (!animComp) continue;

        // Cache first animator with a skeleton for orphan skinned meshes (Mixamo FBX split imports)
        if (const auto* skel = animComp->animator.GetSkeleton()) {
            if (!m_CachedFallbackAnimator) m_CachedFallbackAnimator = animComp;
            // Index by shared skeleton so follower meshes resolve THIS animator (one clock).
            m_SkeletonToAnimator[skel] = animComp;
        }

        animComp->Update(deltaTime);

        if (!animComp->animator.IsPlaying()) continue;

        auto* lookAtIK = m_World->GetComponent<LookAtIKComponent>(entity);
        if (lookAtIK && lookAtIK->lookWeight > 0.0f) {
            // Resolve target position
            Math::Vector3 targetPos = lookAtIK->targetWorldPos;
            if (lookAtIK->useEntityTarget && lookAtIK->targetEntity != INVALID_ENTITY) {
                auto* targetTransform = m_World->GetComponent<TransformComponent>(lookAtIK->targetEntity);
                if (targetTransform) {
                    targetPos = targetTransform->position;
                }
            }

            // Get head bone world position from entity transform
            auto* entityTransform = m_World->GetComponent<TransformComponent>(entity);
            if (entityTransform) {
                Math::Vector3 headWorldPos = entityTransform->position + Math::Vector3(0, 1.6f, 0);
                Math::Quaternion solved = Animation::LookAtIK::Solve(
                    headWorldPos, targetPos, lookAtIK->currentHeadRotation,
                    lookAtIK->maxRotation, lookAtIK->smoothSpeed, deltaTime);
                lookAtIK->currentHeadRotation = solved;
            }
        }

        auto* interactionIK = m_World->GetComponent<InteractionIKComponent>(entity);
        if (interactionIK && interactionIK->ikWeight > 0.0f) {
            auto* entityTransform = m_World->GetComponent<TransformComponent>(entity);
            if (entityTransform) {
                // Find nearest interactable within radius (only scan InteractableComponent entities)
                Math::Vector3 handPos = entityTransform->position + Math::Vector3(0.3f, 1.0f, 0.5f);
                Math::Vector3 nearestTarget = handPos;
                f32 nearestDist = interactionIK->interactionRadius + 1.0f;

                for (Entity other : m_World->GetEntitiesWithComponent<InteractableComponent>()) {
                    if (other == entity) continue;
                    if (!interactionIK->interactionTag.empty()) {
                        auto* otherTag = m_World->GetComponent<TagComponent>(other);
                        if (!otherTag || !otherTag->HasTag(interactionIK->interactionTag))
                            continue;
                    }
                    auto* otherTransform = m_World->GetComponent<TransformComponent>(other);
                    if (!otherTransform) continue;
                    f32 dist = (otherTransform->position - handPos).Length();
                    if (dist < nearestDist && dist <= interactionIK->interactionRadius) {
                        nearestDist = dist;
                        nearestTarget = otherTransform->position;
                    }
                }

                if (nearestDist <= interactionIK->interactionRadius) {
                    // Simple 3-bone FABRIK solve for hand chain
                    // Reuse member vector to avoid per-frame heap allocation
                    m_IKChainCache.resize(3);
                    m_IKChainCache[0] = entityTransform->position + Math::Vector3(0.2f, 1.3f, 0.0f); // shoulder
                    m_IKChainCache[1] = entityTransform->position + Math::Vector3(0.3f, 1.1f, 0.3f); // elbow
                    m_IKChainCache[2] = handPos;                                                        // hand
                    Animation::FABRIK::Solve(m_IKChainCache, nearestTarget, 5);
                }
            }
        }

        // Two-Bone IK: analytic solve for arm/leg chains
        auto* twoBoneIK = m_World->GetComponent<TwoBoneIKComponent>(entity);
        if (twoBoneIK && twoBoneIK->weight > 0.0f) {
            const auto* skeleton = animComp->animator.GetSkeleton();
            if (skeleton) {
                i32 rootIdx = skeleton->FindBoneIndex(twoBoneIK->rootBoneName);
                i32 midIdx = skeleton->FindBoneIndex(twoBoneIK->midBoneName);
                i32 endIdx = skeleton->FindBoneIndex(twoBoneIK->endBoneName);

                if (rootIdx >= 0 && midIdx >= 0 && endIdx >= 0) {
                    const auto& pose = animComp->animator.GetCurrentPose();

                    // Get entity world transform to convert bone-local to world space
                    auto* entityTransform2 = m_World->GetComponent<TransformComponent>(entity);
                    Math::Matrix4 entityWorld = entityTransform2 ? ComputeWorldMatrix(m_World, entity) : Math::Matrix4::Identity();

                    // Extract bone world positions (pose.worldTransforms are in entity-local space)
                    Math::Matrix4 rootWorld = entityWorld * pose.worldTransforms[rootIdx];
                    Math::Matrix4 midWorld = entityWorld * pose.worldTransforms[midIdx];
                    Math::Matrix4 endWorld = entityWorld * pose.worldTransforms[endIdx];

                    Math::Vector3 rootPos(rootWorld.m[12], rootWorld.m[13], rootWorld.m[14]);
                    Math::Vector3 midPos(midWorld.m[12], midWorld.m[13], midWorld.m[14]);
                    Math::Vector3 endPos(endWorld.m[12], endWorld.m[13], endWorld.m[14]);

                    // Resolve target position
                    Math::Vector3 ikTarget = twoBoneIK->targetPosition;
                    if (twoBoneIK->useEntityTarget && twoBoneIK->targetEntity != INVALID_ENTITY) {
                        auto* targetTransform = m_World->GetComponent<TransformComponent>(twoBoneIK->targetEntity);
                        if (targetTransform) {
                            ikTarget = targetTransform->position;
                        }
                    }

                    // Solve two-bone IK
                    Math::Vector3 solvedMid, solvedEnd;
                    Animation::TwoBoneIK::Solve(
                        rootPos, midPos, endPos, ikTarget,
                        twoBoneIK->poleVector, twoBoneIK->weight,
                        solvedMid, solvedEnd
                    );

                    // Apply IK result by computing rotation deltas for root and mid bones
                    auto& poseMut = const_cast<Animation::SkeletonPose&>(pose);

                    // Root bone rotation delta: rotate the upper limb toward solved mid position
                    {
                        Math::Vector3 origDir = midPos - rootPos;
                        Math::Vector3 newDir = solvedMid - rootPos;
                        f32 origLen = origDir.Length();
                        f32 newLen = newDir.Length();
                        if (origLen > 0.0001f && newLen > 0.0001f) {
                            origDir = origDir * (1.0f / origLen);
                            newDir = newDir * (1.0f / newLen);
                            Math::Vector3 axis = origDir.Cross(newDir);
                            f32 axisMag = axis.Length();
                            f32 dotP = std::clamp(origDir.Dot(newDir), -1.0f, 1.0f);
                            if (axisMag > 0.0001f) {
                                axis = axis * (1.0f / axisMag);
                                Math::Quaternion rotDelta(axis, std::acos(dotP));
                                poseMut.localRotations[rootIdx] = rotDelta * poseMut.localRotations[rootIdx];
                            }
                        }
                    }

                    // Mid bone rotation delta: rotate the lower limb toward solved end position
                    {
                        Math::Vector3 origDir = endPos - midPos;
                        Math::Vector3 newDir = solvedEnd - solvedMid;
                        f32 origLen = origDir.Length();
                        f32 newLen = newDir.Length();
                        if (origLen > 0.0001f && newLen > 0.0001f) {
                            origDir = origDir * (1.0f / origLen);
                            newDir = newDir * (1.0f / newLen);
                            Math::Vector3 axis = origDir.Cross(newDir);
                            f32 axisMag = axis.Length();
                            f32 dotP = std::clamp(origDir.Dot(newDir), -1.0f, 1.0f);
                            if (axisMag > 0.0001f) {
                                axis = axis * (1.0f / axisMag);
                                Math::Quaternion rotDelta(axis, std::acos(dotP));
                                poseMut.localRotations[midIdx] = rotDelta * poseMut.localRotations[midIdx];
                            }
                        }
                    }

                    // Write updated rotations back and mark dirty
                    animComp->animator.SetBoneLocalRotation(
                        twoBoneIK->rootBoneName, poseMut.localRotations[rootIdx]);
                    animComp->animator.SetBoneLocalRotation(
                        twoBoneIK->midBoneName, poseMut.localRotations[midIdx]);
                    animComp->matricesDirty = true;
                }
            }
        }
    }

    // Update bone attachment transforms: snap attached entities to their target bone
    for (Entity entity : m_World->GetEntitiesWithComponent<BoneAttachmentComponent>()) {
        auto* ba = m_World->GetComponent<BoneAttachmentComponent>(entity);
        if (!ba || ba->targetEntity == INVALID_ENTITY || ba->targetBoneName.empty()) continue;
        if (!m_World->IsValid(ba->targetEntity)) continue;

        auto* animComp = m_World->GetComponent<AnimatorComponent>(ba->targetEntity);
        if (!animComp) continue;

        // Get bone world transform (in skeleton/entity-local space)
        Math::Matrix4 boneLocal = animComp->animator.GetBoneWorldTransform(ba->targetBoneName);

        // Multiply by the target entity's world matrix to get the bone in world space
        Math::Matrix4 targetWorld = ComputeWorldMatrix(m_World, ba->targetEntity);
        Math::Matrix4 boneWorld = targetWorld * boneLocal;

        // Extract bone world position
        Math::Vector3 bonePos(boneWorld.m[12], boneWorld.m[13], boneWorld.m[14]);

        // Extract bone world rotation from the 3x3 portion of the matrix
        Math::Quaternion boneRot = Math::Quaternion::FromMatrix(boneWorld);

        // Apply offsets
        Math::Vector3 finalPos = bonePos + boneRot.Rotate(ba->positionOffset);
        Math::Quaternion finalRot = boneRot * ba->rotationOffset;

        // Write to this entity's transform
        auto* transform = m_World->GetComponent<TransformComponent>(entity);
        if (transform) {
            transform->position = finalPos;
            transform->rotation = finalRot;
            transform->worldMatrixDirty = true;
        }
    }

    // Classify scene composition (2D / 2.5D / 3D) before rendering decisions
    ClassifySceneComposition();

    // When main-pass rendering is skipped (editor viewport renders offscreen),
    // skip all pre-pass work (shadows, culling, RT, clustered lighting) but still
    // start the main render pass so ImGui has a valid pass to draw into.
    if (m_SkipMainPassRendering) {
        m_SkipMainPassRendering = false;  // Consume for this frame (RenderOffscreen sets it each frame)
        m_VulkanRenderer->BeginMainRenderPass();
        return;
    }

    // Build list of cullable objects for GPU frustum culling
    // Only done when we have 3D meshes and GPU culling is enabled.
    // In editor mode, skip culling entirely so all entities are visible for editing.
    // In player mode, skip because GPU compute shaders (cull.comp.spv) are not
    // available in built games — indirect draw buffers would be empty, causing
    // entities to be marked as "drawn" but never actually rendered.
    if (m_GPUCullingEnabled && !m_IsEditorMode && !m_PlayerMode && m_SceneComposition.mesh3DCount > 0) {
        BuildCullableObjectList();
    }

    // Select shadow-casting point/spot lights (before shadow passes)
    if (m_ShadowsEnabled && m_SceneComposition.mode == SceneRenderMode::Scene3D) {
        SelectShadowLights();
    } else {
        m_ActivePointShadowCount = 0;
        m_ActiveSpotShadowCount = 0;
    }

    // Shadow pass first (if enabled) - only run when 3D meshes AND shadow-casting lights exist.
    // Pure 2D scenes skip entirely since sprites never cast shadows.
    // Scenes with no shadow-casting lights also skip to avoid rendering 4 cascades for nothing.
    // During play mode, the game view runs its own shadow pass via RenderShadowPassForCamera(),
    // so we skip the main-pass shadows to avoid rendering 8 cascade passes per frame.
    if (!m_SkipMainPassShadows && m_ShadowsEnabled && m_ShadowMap && m_ShadowPipeline &&
        m_SceneComposition.mode == SceneRenderMode::Scene3D &&
        m_SceneComposition.hasShadowCastingLights) {
        RenderShadowPass();
    }

    // Point light shadow pass
    if (!m_SkipMainPassShadows && m_ShadowsEnabled && m_PointShadowMap && m_PointShadowPipeline && m_ActivePointShadowCount > 0) {
        RenderPointShadowPass();
    }

    // Spot light shadow pass
    if (!m_SkipMainPassShadows && m_ShadowsEnabled && m_SpotShadowMap && m_SpotShadowPipeline && m_ActiveSpotShadowCount > 0) {
        RenderSpotShadowPass();
    }

    // GPU frustum culling (compute shader dispatch before main render pass)
    // Upload per-object material/transform data, then run the compute culling shader.
    // When async compute is available, record to dedicated compute command buffer.
    // Skipped in editor mode — the editor scene view shows all entities.
    if (m_GPUCullingEnabled && !m_IsEditorMode && !m_CullableObjects.empty()) {
        UploadObjectData();
        if (m_VulkanRenderer->HasAsyncCompute() && m_VulkanRenderer->BeginComputeCommandBuffer()) {
            // Record culling on async compute queue
            PerformGPUCullingAsync();
            m_VulkanRenderer->EndComputeCommandBuffer();
            m_VulkanRenderer->SubmitCompute();
        } else {
            // Fall back to graphics queue
            PerformGPUCulling();
        }
    }

    // Ray tracing pass (after shadow passes, before main render pass)
    // When async compute is available, dispatch RT effects and denoising on the compute
    // queue so they overlap with main geometry rasterization on the graphics queue.
    // TLAS rebuild stays on graphics (needs vertex/index buffer access).
    // Compositing is deferred until after main pass when compute results are ready.
    if (m_RTEnabled && m_ASManager && m_SceneComposition.mode == SceneRenderMode::Scene3D) {
        VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
        if (commandBuffer != VK_NULL_HANDLE) {
            RebuildTLAS(commandBuffer);

            u32 frameIdx = m_VulkanRenderer->GetCurrentFrameIndex();
            bool asyncRT = m_AsyncComputeScheduler &&
                           m_AsyncComputeScheduler->ShouldUseAsync(Renderer::AsyncComputeWorkType::RTDispatch);

            if (asyncRT) {
                // Dispatch RT effects on async compute queue (overlaps with main geometry)
                DispatchRTEffectsAsync(frameIdx);
                // Denoising also runs on compute queue after RT finishes
                // (submitted as part of the same compute command buffer)
            } else {
                // Single-queue fallback: RT effects + temporal reuse + denoise + composite on graphics queue
                DispatchRTEffects(commandBuffer);
                TemporalReuseRTOutputs(commandBuffer);
                DenoiseRTOutputs(commandBuffer);
                CompositeRTResults(commandBuffer);
            }
        }
    }

    // Clustered forward lighting: build light list and assign to spatial clusters before main render pass
#ifdef ENJIN_CLUSTERED_LIGHTING
    // Clustered lighting now runs in Player builds too: the compute SPIR-V is
    // embedded (ClusterComputeShaderData.h), so m_ClusteredLighting is only
    // non-null when its pipelines actually built. No m_PlayerMode gate needed.
    if (m_ClusteredLighting && m_SceneComposition.mode != SceneRenderMode::Scene2D && m_Camera) {
        VkCommandBuffer cmdBuf = m_VulkanRenderer->GetCurrentCommandBuffer();
        if (cmdBuf != VK_NULL_HANDLE) {
            // Build ClusterLight array from cached light entities (reuse pre-allocated vector)
            std::vector<Renderer::ClusterLight> clusterLights;
            
            auto* lightStorageCL = m_World->GetComponentStorage<LightComponent>();
            for (Entity e : m_CachedLightEntities) {
                auto* light = lightStorageCL ? lightStorageCL->Get(e) : nullptr;
                auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(e) : nullptr;
                if (!light || light->type == LightType::Directional) continue;

                Renderer::ClusterLight cl{};
                cl.position = xform ? xform->position : Math::Vector3(0.0f);
                cl.range = light->range;
                cl.color = light->color;
                cl.intensity = light->intensity;
                if (light->type == LightType::Spot) {
                    Math::Vector3 fwd(0.0f, 0.0f, -1.0f);
                    cl.direction = xform ? xform->rotation.Rotate(fwd).Normalized() : Math::Vector3(0, -1, 0);
                    cl.outerConeAngle = light->outerConeAngle;
                } else {
                    cl.direction = Math::Vector3(0.0f);
                    cl.outerConeAngle = 0.0f;
                }
                clusterLights.push_back(cl);
            }
            // Append transient point lights (fire, muzzle flashes, spells) so the
            // froxel volume scatters them through fog/smoke, matching the surface pass.
            for (const auto& tpl : m_TransientPointLights) {
                Renderer::ClusterLight cl{};
                cl.position = tpl.position;
                cl.range = tpl.range;
                cl.color = tpl.color;
                cl.intensity = tpl.intensity;
                cl.direction = Math::Vector3(0.0f);
                cl.outerConeAngle = 0.0f;
                clusterLights.push_back(cl);
            }
            if (!clusterLights.empty()) {
                Math::Matrix4 viewMatrix = m_Camera->GetViewMatrix();
                m_ClusteredLighting->AssignLights(cmdBuf, clusterLights.data(),
                    static_cast<u32>(clusterLights.size()), viewMatrix);
            }
        }
    }
#endif

    // --- Phase 2/5/6 compute dispatches (after clustered lighting, before main geometry) ---
    VkCommandBuffer computeCmd = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (computeCmd != VK_NULL_HANDLE && m_Camera) {
        u32 frameNumber = m_VulkanRenderer->GetCurrentFrameIndex();
        auto swapExtent = m_VulkanRenderer->GetSwapchainExtent();

        // Find sun direction for GI and fog
        Math::Vector3 sunDir(0.5f, 0.8f, 0.3f);
        Math::Vector3 sunColor(1.0f, 0.95f, 0.9f);
        f32 sunIntensity = 1.0f;
        if (m_CachedLightEntities.size() > 0) {
            auto* lightStor = m_World->GetComponentStorage<LightComponent>();
            for (Entity le : m_CachedLightEntities) {
                auto* light = lightStor ? lightStor->Get(le) : nullptr;
                if (light && light->type == LightType::Directional) {
                    auto* xf = m_CachedTransformStorage ? m_CachedTransformStorage->Get(le) : nullptr;
                    if (xf) sunDir = xf->rotation.Rotate(Math::Vector3(0, 0, -1)).Normalized();
                    sunColor = light->color;
                    sunIntensity = light->intensity;
                    break;
                }
            }
        }

        // DDGI: voxelize scene + update probes + sample irradiance
        if (m_DDGISystem && m_DDGISystem->IsEnabled()) {
            Math::Matrix4 invVP = (m_Camera->GetProjectionMatrix() * m_Camera->GetViewMatrix()).Inverse();
            m_DDGISystem->Update(computeCmd, m_World, frameNumber,
                                 sunDir, sunColor, sunIntensity,
                                 invVP, swapExtent.width, swapExtent.height);
        }

        // Volumetric fog: froxel scattering pass
        if (m_VolumetricFog && m_VolumetricFog->IsEnabled()) {
            Math::Matrix4 invVP = (m_Camera->GetProjectionMatrix() * m_Camera->GetViewMatrix()).Inverse();
            Math::Matrix4 prevVP = m_Camera->GetProjectionMatrix() * m_Camera->GetViewMatrix(); // TODO: use actual prev frame VP
            static f32 s_FogTime = 0.0f; s_FogTime += deltaTime;
            m_VolumetricFog->Update(computeCmd, s_FogTime,
                                     invVP, prevVP, m_Camera->GetViewMatrix(),
                                     m_Camera->GetPosition(),
                                     m_Camera->GetNearPlane(), m_Camera->GetFarPlane(),
                                     swapExtent.width, swapExtent.height,
                                     sunDir, sunColor, sunIntensity);
            m_VolumetricFog->SwapVolumes(); // Double-buffer for temporal reprojection
        }

        // GPU particles: simulate all alive particles
        if (m_GPUParticleSystem) {
            Math::Vector3 wind(0.0f); // TODO: read from WindSystem
            m_GPUParticleSystem->Simulate(computeCmd, deltaTime, frameNumber, wind);
        }
    }

    // Periodic diagnostic warnings (every 300 frames)
    if (++m_DiagnosticFrameCounter >= 300) {
        m_DiagnosticFrameCounter = 0;
        if (m_SceneComposition.spriteCount > 100 && !m_SpriteBatchRenderer) {
            ENJIN_LOG_WARN(Renderer, "Scene has %u sprites without batch renderer — high draw call count",
                           m_SceneComposition.spriteCount);
        }
        // Warn on ortho/perspective camera mixing
        {
            u32 perspCount = 0, orthoCount = 0;
            for (Entity e : m_World->GetEntitiesWithComponent<CameraComponent>()) {
                auto* cam = m_World->GetComponent<CameraComponent>(e);
                if (!cam || !cam->isActive) continue;
                if (cam->projectionType == ProjectionType::Perspective) perspCount++;
                else orthoCount++;
            }
            if (perspCount > 0 && orthoCount > 0) {
                ENJIN_LOG_WARN(Renderer,
                    "Mixed camera projections: %u perspective + %u orthographic — may cause unexpected rendering",
                    perspCount, orthoCount);
            }
        }
    }

    // Begin the main render pass (after any pre-passes like shadows)
    m_VulkanRenderer->BeginMainRenderPass();

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Splitscreen main pass: render each viewport separately
    if (!m_MainPassViewports.empty()) {
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
        Renderer::Camera* prevCamera = m_Camera;

        // Switch to offscreen buffers for per-viewport uniform isolation
        m_ActiveDescriptorSets = &m_OffscreenDescriptorSets;
        m_ActiveUniformBuffers = &m_OffscreenUniformBuffers;
        m_ActiveLightingBuffers = &m_OffscreenLightingBuffers;
        m_OffscreenMode = true;

        // Build sorted render list once for all viewports (maximizes descriptor cache hits).
        // Sort keys use depth=0 since splitscreen viewports have different cameras;
        // the important part is grouping by texture/material hash, not depth ordering.
        {
            m_SortedRenderList.clear();
            ECS::View<TransformComponent, MeshComponent> view(*m_World);
            auto* matStorage = m_World->GetComponentStorage<MaterialComponent>();
            m_SortedRenderList.reserve(view.UpperBound());
            for (Entity entity : view) {
                auto* xform = view.Get<TransformComponent>(entity);
                if (!xform || !xform->visible) continue;
                auto* mat = matStorage ? matStorage->Get(entity) : nullptr;
                if (mat) mat->ComputeSortKey(0.0f);
                m_SortedRenderList.push_back(entity);
            }
            std::sort(m_SortedRenderList.begin(), m_SortedRenderList.end(),
                [matStorage](Entity a, Entity b) {
                    auto* matA = matStorage ? matStorage->Get(a) : nullptr;
                    auto* matB = matStorage ? matStorage->Get(b) : nullptr;
                    u64 keyA = matA ? matA->cachedSortKey : 0;
                    u64 keyB = matB ? matB->cachedSortKey : 0;
                    return keyA < keyB;
                });
        }

        u32 viewportCount = static_cast<u32>(m_MainPassViewports.size());
        if (viewportCount > MAX_SPLITSCREEN_VIEWPORTS) viewportCount = MAX_SPLITSCREEN_VIEWPORTS;

        for (u32 v = 0; v < viewportCount; ++v) {
            const ViewportCamera& vc = m_MainPassViewports[v];
            m_CurrentViewportIndex = v;

            auto* cameraComp = m_World->GetComponent<CameraComponent>(vc.entity);
            auto* cameraTransform = m_World->GetComponent<TransformComponent>(vc.entity);
            if (!cameraComp || !cameraTransform) continue;

            Renderer::Camera viewCamera;
            f32 pixelW = vc.viewportWidth * static_cast<f32>(extent.width);
            f32 pixelH = vc.viewportHeight * static_cast<f32>(extent.height);
            f32 aspect = (pixelH > 0.0f) ? (pixelW / pixelH) : 1.0f;

            if (cameraComp->projectionType == ProjectionType::Perspective) {
                viewCamera.SetPerspective(cameraComp->fieldOfView, aspect,
                                           cameraComp->nearPlane, cameraComp->farPlane);
            } else {
                f32 halfH = cameraComp->orthoSize;
                f32 halfW = halfH * aspect;
                viewCamera.SetOrthographic(-halfW, halfW, -halfH, halfH,
                                            cameraComp->nearPlane, cameraComp->farPlane);
            }

            viewCamera.SetPosition(cameraTransform->position);
            Math::Vector3 forward = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
            Math::Vector3 up = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
            viewCamera.SetLookAt(cameraTransform->position, cameraTransform->position + forward, up);

            m_Camera = &viewCamera;
            UpdateFrameUniforms();
            BuildMaterialSSBO();

            VkViewport vkViewport{};
            vkViewport.x = vc.viewportX * static_cast<f32>(extent.width);
            vkViewport.y = vc.viewportY * static_cast<f32>(extent.height);
            vkViewport.width = pixelW;
            vkViewport.height = pixelH;
            vkViewport.minDepth = 0.0f;
            vkViewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);

            VkRect2D scissor{};
            scissor.offset = { static_cast<i32>(vkViewport.x), static_cast<i32>(vkViewport.y) };
            scissor.extent = { static_cast<u32>(pixelW), static_cast<u32>(pixelH) };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            RenderSkybox(commandBuffer, &vkViewport, &scissor);

            m_Pipeline->Bind(commandBuffer);
            {
                u32 zeroOff = 0;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_Pipeline->GetLayout(), 0, 1,
                    &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
                // Bind bindless texture set 1 for offscreen/splitscreen viewport
                if (m_BindlessManager) {
                    VkDescriptorSet bs = m_BindlessManager->GetDescriptorSet();
                    if (bs) vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        m_Pipeline->GetLayout(), 1, 1, &bs, 0, nullptr);
                }
            }
            vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Reset descriptor cache for each viewport
            m_LastBound.Reset(); m_GeometryPoolBound = false;

            {
            auto* spriteStorageVP = m_World->GetComponentStorage<Sprite2DComponent>();
            for (Entity entity : m_SortedRenderList) {
                auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                if (!xform) continue;
                if (!xform->visible) continue;
                // Skip GPU-culled entities (frustum culling — disabled in editor mode)
                if (m_GPUCullingEnabled && !m_IsEditorMode && m_GPUCulling && !m_CullableObjects.empty()) {
                    usize entityIdx = static_cast<usize>(EntityIndex(entity));
                    if (entityIdx < m_EntityToCullIndex.size()) {
                        u32 cullIdx = m_EntityToCullIndex[entityIdx];
                        if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                            continue;
                        }
                    }
                }
                // Skip 2D sprites — rendered in sorted pass after 3D geometry
                if (spriteStorageVP && spriteStorageVP->Has(entity)) continue;
                RenderEntity(entity);
            }
            }

            // Geometry outline pass (after 3D geometry)
            RenderOutlinePass();

            // Per-entity wireframe overlay (after outlines, before sprites)
            RenderWireframeOverlayPass();

            // Sorted 2D sprite rendering pass (after 3D geometry)
            RenderSprites();

            // Render effects for this viewport
            u32 vpW = static_cast<u32>(pixelW);
            u32 vpH = static_cast<u32>(pixelH);
            RenderGrass(vpW, vpH);
            RenderShrubs(vpW, vpH);
            RenderTrees(vpW, vpH);
            RenderParticles(vpW, vpH);
            RenderFluid(vpW, vpH);
        }

        m_Camera = prevCamera;
        m_ActiveDescriptorSets = &m_DescriptorSets;
        m_ActiveUniformBuffers = &m_UniformBuffers;
        m_ActiveLightingBuffers = &m_LightingBuffers;
        m_OffscreenMode = false;
        m_CurrentViewportIndex = 0;
        return;
    }

    // Single-camera main pass (default)

    // Upload frame-level uniforms once (view/proj + lighting)
    UpdateFrameUniforms();
    BuildMaterialSSBO();

    // Update bindless descriptor set if any textures were registered/changed
    if (m_BindlessManager) m_BindlessManager->UpdateDescriptorSet();

    // Build the sorted render list every frame (needed by RenderToTarget() offscreen path too)
    {
        m_SortedRenderList.clear();

        // Multi-component view: iterates the smallest set (Transform or Mesh),
        // filters to entities that have both. Excludes 2D sprites (separate pass).
        // All storage pointers are cached in the View — no per-entity type-ID hash lookups.
        ECS::View<TransformComponent, MeshComponent> view(*m_World);
        view.Exclude(m_World->GetComponentStorage<Sprite2DComponent>());

        // Also cache material storage for sort key computation (optional component)
        auto* matStorage = m_World->GetComponentStorage<MaterialComponent>();

        // Compute camera position for depth-aware sort key
        Math::Vector3 camPos;
        bool haveCam = (m_Camera != nullptr);
        if (haveCam) camPos = m_Camera->GetPosition();

        m_SortedRenderList.reserve(view.UpperBound());

        for (Entity entity : view) {
            auto* xform = view.Get<TransformComponent>(entity);
            if (!xform->visible) continue;

            // Skip GPU-culled entities (frustum culling — disabled in editor mode)
            if (m_GPUCullingEnabled && !m_IsEditorMode && m_GPUCulling && !m_CullableObjects.empty()) {
                usize entityIdx = static_cast<usize>(EntityIndex(entity));
                if (entityIdx < m_EntityToCullIndex.size()) {
                    u32 cullIdx = m_EntityToCullIndex[entityIdx];
                    if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                        continue;
                    }
                }
            }

            // Compute 64-bit sort key (pipeline | material/texture hash | depth)
            auto* mat = matStorage ? matStorage->Get(entity) : nullptr;
            if (mat) {
                f32 depth = haveCam ? (xform->position - camPos).Length() : 0.0f;
                mat->ComputeSortKey(depth);
            }

            m_SortedRenderList.push_back(entity);
        }

        // Sort by 64-bit material sort key: groups by pipeline (opaque→mask→blend),
        // then by material/texture hash (minimizes descriptor set updates),
        // then by depth (front-to-back for opaque, back-to-front for blend).
        std::sort(m_SortedRenderList.begin(), m_SortedRenderList.end(),
            [matStorage](Entity a, Entity b) {
                auto* matA = matStorage ? matStorage->Get(a) : nullptr;
                auto* matB = matStorage ? matStorage->Get(b) : nullptr;
                u64 keyA = matA ? matA->cachedSortKey : 0;
                u64 keyB = matB ? matB->cachedSortKey : 0;
                return keyA < keyB;
            });
    }

    // GPU timestamp: main geometry begin
    {
        VkQueryPool tsPool = m_VulkanRenderer->GetTimestampPool(m_VulkanRenderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_MAIN_BEGIN);
        }
    }

    // Render skybox first (behind all geometry)
    RenderSkybox(commandBuffer);

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Bind pipeline, descriptor set, viewport, and scissor once for all entities
    m_Pipeline->Bind(commandBuffer);
    m_BoundSpecKey.bits = 0xFFFFFFFF; // Reset variant tracking for main render path
    u32 matDynOffset = 0;
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &matDynOffset);

    // Bind bindless texture array at set 1 (persists for entire pass)
    if (m_BindlessManager) {
        VkDescriptorSet bindlessSet = m_BindlessManager->GetDescriptorSet();
        if (bindlessSet) {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_Pipeline->GetLayout(), 1, 1, &bindlessSet, 0, nullptr);
        }
    }

    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Single-pass LOD update + render — iterates renderable entities once
    // Reset last-bound state so no stale descriptor data carries from a previous pass
    m_LastBound.Reset(); m_GeometryPoolBound = false;

    // GPU-driven rendering: DGC (if enabled) or multi-draw indirect fallback.
    // Skipped in editor mode (editor always uses per-entity draws for full control).
    if (m_GPUCullingEnabled && !m_IsEditorMode && !m_CullableObjects.empty() && m_GeometryPool) {
        // Device Generated Commands: GPU generates push constants + draw calls directly.
        // Eliminates ALL CPU-side draw call submission for non-textured pool entities.
        if (m_DGC && m_DGC->IsEnabled()) {
            DrawDGC(commandBuffer);
        } else {
            // Fallback: multi-draw indirect with parallaxScale = -1.0 sentinel
            DrawIndirect(commandBuffer);
        }

        // Texture-grouped indirect draws: batch textured pool entities by texture set.
        // Each batch shares the same descriptor state, reducing draw calls for textured meshes.
        if (m_IndirectDrawBatcher && m_IndirectDrawBatcher->HasBatches()) {
            DrawTexturedIndirect(commandBuffer);
        }
    }

    // If RT effects were dispatched on async compute, wait for completion and composite.
    // The compute queue ran RT dispatches + denoising while the main geometry was rasterizing.
    if (m_AsyncComputeScheduler && m_AsyncComputeScheduler->WasComputeSubmitted() &&
        m_AsyncComputeScheduler->GetLastWorkType() == Renderer::AsyncComputeWorkType::RTDispatch) {
        m_AsyncComputeScheduler->WaitForCompute(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        CompositeRTResults(commandBuffer);
    }

    {
        Math::Vector3 camPos;
        bool doLOD = (m_Camera != nullptr);
        if (doLOD) camPos = m_Camera->GetPosition();

        // Cache component storage pointers for the hot loop (avoids per-entity type-ID lookups)
        auto* lodStorage = m_World->GetComponentStorage<LODComponent>();
        auto* xformStorageLoop = m_World->GetComponentStorage<TransformComponent>();
        auto* meshStorageLoop = m_World->GetComponentStorage<MeshComponent>();
        auto* matStorageLoop = m_World->GetComponentStorage<MaterialComponent>();

        usize renderCount = m_SortedRenderList.size();
        constexpr usize PREFETCH_AHEAD = 4;

        for (usize ri = 0; ri < renderCount; ++ri) {
            // Prefetch components for entities 4 ahead — loads into L1 cache
            // before they're needed, hiding memory latency
            if (ri + PREFETCH_AHEAD < renderCount) {
                Entity ahead = m_SortedRenderList[ri + PREFETCH_AHEAD];
                if (xformStorageLoop) xformStorageLoop->Prefetch(ahead);
                if (meshStorageLoop) meshStorageLoop->Prefetch(ahead);
                if (matStorageLoop) matStorageLoop->Prefetch(ahead);
            }

            Entity entity = m_SortedRenderList[ri];
            // LOD selection with hysteresis (if camera is available)
            if (doLOD) {
                auto* lod = lodStorage ? lodStorage->Get(entity) : nullptr;
                if (lod && lod->enabled && lod->levelCount > 1) {
                    auto* transform = xformStorageLoop ? xformStorageLoop->Get(entity) : nullptr;
                    if (transform) {
                        f32 metric;
                        if (lod->useScreenSize) {
                            // Screen-space projected size: accounts for object scale.
                            // Approximation: bounding sphere diameter / distance.
                            auto* mesh = meshStorageLoop ? meshStorageLoop->Get(entity) : nullptr;
                            f32 dist = Math::Max((transform->position - camPos).Length(), 0.001f);
                            f32 scale = Math::Max(Math::Max(
                                Math::Abs(transform->scale.x),
                                Math::Abs(transform->scale.y)),
                                Math::Abs(transform->scale.z));
                            // Use cached AABB extent as size estimate (if available)
                            f32 objectSize = scale;
                            if (mesh) {
                                Math::Vector3 extent = mesh->cachedAABBMax - mesh->cachedAABBMin;
                                objectSize = scale * Math::Max(Math::Max(extent.x, extent.y), extent.z);
                            }
                            // Screen metric: larger = closer/bigger = more detail needed
                            // Invert so that larger metric means further away (matches distance thresholds)
                            metric = dist / Math::Max(objectSize, 0.01f);
                        } else {
                            metric = (transform->position - camPos).Length();
                        }

                        // Unified LOD selection (see ECS::SelectLOD) with directional hysteresis.
                        i32 newLOD = SelectLOD(*lod, metric);

                        if (newLOD != lod->activeLOD && newLOD < lod->levelCount) {
                            auto* mesh = meshStorageLoop ? meshStorageLoop->Get(entity) : nullptr;
                            if (mesh && lod->levels[newLOD].mesh.IsValid()) {
                                *mesh = lod->levels[newLOD].mesh;
                                if (static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size())
                                    m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].Invalidate();
                                lod->activeLOD = newLOD;
                            }
                        }
                    }
                }
            }

            RenderEntity(entity);
        }
    }

    // Geometry outline pass (inverted-hull backface extrusion, after main geometry)
    RenderOutlinePass();

    // Per-entity wireframe overlay
    RenderWireframeOverlayPass();

    // Render onion skin ghosts (editor viewport only, before sprites)
    RenderOnionSkinGhosts();

    // Sorted 2D sprite rendering pass (after 3D geometry)
    RenderSprites();

    // Render effect passes (grass, shrubs, trees, particles, fluid)
    RenderGrass(0, 0);
    RenderShrubs(0, 0);
    RenderTrees(0, 0);
    RenderParticles(0, 0);
    RenderFluid(0, 0);

    // Render weather particles in main pass if set (editor viewport)
    if (m_MainPassWeather) {
        RenderWeatherParticles(*m_MainPassWeather, m_MainPassWeatherIsRain, 0, 0);
    }

    // GPU timestamp: main geometry end
    {
        VkQueryPool tsPool = m_VulkanRenderer->GetTimestampPool(m_VulkanRenderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_MAIN_END);
        }
    }
}

void RenderSystem::RenderToTarget(Renderer::RenderTarget* target, Renderer::Camera* camera, u32 viewportIndex) {
    if (!target || !target->IsValid() || !camera || !m_Renderer || !m_Initialized || !m_Pipeline) {
        return;
    }

    // NOTE: The caller (EditorLayer::RenderOffscreen) has already started the render target's
    // render pass via sceneTarget->Begin(). We must NOT start another render pass here
    // (e.g. shadow pass). Water meshes and shadows are handled by Update() for the main pass.

    // Temporarily swap the camera so UpdateFrameUniforms uses the game camera
    Renderer::Camera* prevCamera = m_Camera;
    m_Camera = camera;

    // Switch to offscreen buffers and descriptor sets so the main pass
    // doesn't overwrite the game-camera uniform data
    m_ActiveDescriptorSets = &m_OffscreenDescriptorSets;
    m_ActiveUniformBuffers = &m_OffscreenUniformBuffers;
    m_ActiveLightingBuffers = &m_OffscreenLightingBuffers;
    m_OffscreenMode = true;
    m_CurrentViewportIndex = viewportIndex;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        m_Camera = prevCamera;
        m_ActiveDescriptorSets = &m_DescriptorSets;
        m_ActiveUniformBuffers = &m_UniformBuffers;
        m_ActiveLightingBuffers = &m_LightingBuffers;
        m_OffscreenMode = false;
        return;
    }

    // The render target's Begin/End are handled by the caller (EditorLayer::RenderOffscreen)
    // We just need to bind our pipeline and draw within the active render pass

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    u32 activeIdx = GetActiveBufferIndex(currentFrame);
    // Clamp shadow distance to match game camera far plane (keeps cascade splits consistent)
    f32 prevShadowDist = m_ShadowDistance;
    f32 camFar = camera->GetFarPlane();
    if (m_ShadowDistance > camFar) {
        m_ShadowDistance = camFar;
    }

    // Ensure light list is fresh (RenderOffscreen runs before Update where it's normally rebuilt)
    if (m_LightListDirty && m_World) {
        m_CachedLightEntities.clear();
        auto* lightStorage = m_World->GetComponentStorage<LightComponent>();
        if (lightStorage) {
            const auto& liveLights = m_World->GetEntitiesWithComponent<LightComponent>();
            m_CachedLightEntities.assign(liveLights.begin(), liveLights.end());
        }
        m_LightListDirty = false;
    }

    // Upload frame-level uniforms to offscreen buffers (game camera view/proj + lighting)
    UpdateFrameUniforms();
    BuildMaterialSSBO();

    m_ShadowDistance = prevShadowDist;

    // Set viewport and scissor to match render target size
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(target->GetWidth());
    viewport.height = static_cast<f32>(target->GetHeight());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { target->GetWidth(), target->GetHeight() };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Render skybox in game view (pass viewport/scissor for render target dimensions)
    RenderSkybox(commandBuffer, &viewport, &scissor);

    // Reset descriptor cache for this render pass
    m_LastBound.Reset(); m_GeometryPoolBound = false;

    // Use offscreen pipeline (created for offscreen UNORM render pass) to avoid
    // Vulkan spec violation from binding SRGB pipeline in UNORM render pass
    auto* targetPipeline = m_OffscreenPipeline ? m_OffscreenPipeline.get() : m_Pipeline.get();

    // Bind pipeline and descriptor set ONCE before the entity loop (not per-entity)
    targetPipeline->Bind(commandBuffer);
    m_BoundSpecKey.bits = 0xFFFFFFFF; // Reset variant tracking — force rebind on first entity
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            targetPipeline->GetLayout(), 0, 1, &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
        // Bind bindless set 1 for offscreen render target
        if (m_BindlessManager) {
            VkDescriptorSet bs = m_BindlessManager->GetDescriptorSet();
            if (bs) vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                targetPipeline->GetLayout(), 1, 1, &bs, 0, nullptr);
        }
    }

    // Use the sorted render list (sorted by cachedTextureKey) to maximize descriptor cache hits.
    // The main pass builds m_SortedRenderList each frame; reuse it for the offscreen path.
    // If the list is empty (e.g. first frame), fall back to unsorted iteration.
    const auto& renderList = m_SortedRenderList.empty()
        ? m_World->GetEntitiesWithComponent<MeshComponent>()
        : m_SortedRenderList;

    // Cache storage pointers for the RenderToTarget entity loop
    auto* spriteStorageRT = m_World->GetComponentStorage<Sprite2DComponent>();

    for (Entity entity : renderList) {
        {
            // Skip invisible entities or entities without transform (cached storage)
            auto* xformRT = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (!xformRT || !xformRT->visible) continue;

            // Skip GPU-culled entities (frustum culling — disabled in editor mode)
            if (m_GPUCullingEnabled && !m_IsEditorMode && m_GPUCulling && !m_CullableObjects.empty()) {
                usize entityIdx = static_cast<usize>(EntityIndex(entity));
                if (entityIdx < m_EntityToCullIndex.size()) {
                    u32 cullIdx = m_EntityToCullIndex[entityIdx];
                    if (cullIdx != UINT32_MAX && !m_GPUCulling->IsVisible(cullIdx)) {
                        continue;
                    }
                }
            }

            // Skip 2D sprites — rendered in sorted pass after 3D geometry
            if (spriteStorageRT && spriteStorageRT->Has(entity)) continue;

            EntityRenderData* pRD = GetOrCreateRenderData(entity);
            if (!pRD) continue;
            EntityRenderData& renderData = *pRD;

            // Bind material SSBO at this entity's dynamic offset
            {
                u32 matIdx = GetMaterialIndex(entity);
                u32 dynOffset = matIdx * m_MaterialSSBOStride;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    targetPipeline->GetLayout(), 0, 1, &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &dynOffset);
            }

            // Push constants (world matrix includes parent chain).
            // For skinned meshes: use identity model matrix because skinning matrices
            // already transform vertices from bone-local → world space. Applying the
            // entity's parent hierarchy on top would double-transform the mesh.
            Renderer::PushConstants pushConstants{};
            AnimatorComponent* preCheckAnim = ResolveAnimator(entity);
            pushConstants.model = ECS::ComputeWorldMatrix(m_World, entity);

            MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
            Renderer::Texture* boundTexture = nullptr;
            Renderer::Texture* texHeight = nullptr;
            Renderer::Texture* texNormal = nullptr;
            Renderer::Texture* texMR = nullptr;
            Renderer::Texture* texEmissive = nullptr;
            Renderer::Texture* texMatcap = nullptr;

            if (material) {
                pushConstants.baseColor = material->baseColor;
                pushConstants.metallic = material->metallic;
                pushConstants.emissiveColor = material->emissiveColor;
                pushConstants.roughness = material->roughness;
                pushConstants.emissiveStrength = material->emissiveStrength;
                pushConstants.opacity = material->opacity;
                pushConstants.alphaCutoff = material->alphaCutoff;

                // Resolve textures using cache (avoids per-frame string hash lookups)
                if (material->textureCacheDirty) {
                    if (!material->baseColorTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->baseColorTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedBaseColorTexture = tex.get();
                            material->baseColorTexture = 1;
                        }
                    }
                    if (!material->heightTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->heightTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedHeightTexture = tex.get();
                            material->heightTexture = 1;
                        }
                    }
                    if (!material->normalTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->normalTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedNormalTexture = tex.get();
                            material->normalTexture = 1;
                        }
                    }
                    if (!material->metallicRoughnessTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->metallicRoughnessTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    // Specular map overrides metallic-roughness slot for pre-PBR shading
                    if (!material->specularTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->specularTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    if (!material->emissiveTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedEmissiveTexture = tex.get();
                            material->emissiveTexture = 1;
                        }
                    }
                    if (!material->matcapTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->matcapTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMatcapTexture = tex.get();
                            material->matcapTexture = 1;
                        }
                    }
                    material->textureCacheDirty = false;
                    material->cachedTextureKey = { material->cachedBaseColorTexture,
                        material->cachedHeightTexture, material->cachedNormalTexture,
                        material->cachedMetallicRoughnessTexture, material->cachedEmissiveTexture,
                        material->cachedMatcapTexture };
                }

                // Use cached texture pointers
                boundTexture = material->cachedBaseColorTexture;
                texHeight = material->cachedHeightTexture;
                texNormal = material->cachedNormalTexture;
                texMR = material->cachedMetallicRoughnessTexture;
                texEmissive = material->cachedEmissiveTexture;
                texMatcap = material->cachedMatcapTexture;

                pushConstants.flags = 0;
                if (material->doubleSided) pushConstants.flags |= 1;
                if (material->castShadows) pushConstants.flags |= 2;
                if (material->receiveShadows) pushConstants.flags |= 4;
                pushConstants.flags |= (static_cast<i32>(material->alphaMode) << 8);
                if (boundTexture != nullptr) pushConstants.flags |= (1 << 16);
                if (material->normalTexture >= 0) pushConstants.flags |= (1 << 17);
                if (material->metallicRoughnessTexture >= 0) pushConstants.flags |= (1 << 18);
                if (material->emissiveTexture >= 0) pushConstants.flags |= (1 << 19);
                // Height texture flag
                if (material->heightTexture >= 0) pushConstants.flags |= (1 << 10);
                // Retro flags (per-material)
                if (material->flatShading) pushConstants.flags |= (1 << 20);
                if (material->affineTexturing) pushConstants.flags |= (1 << 21);
                if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
                if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
                if (material->uvQuantize) pushConstants.flags |= (1 << 12);
                if (material->gouraudOnly) pushConstants.flags |= (1 << 13);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
                pushConstants.flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
                pushConstants.parallaxScale = material->parallaxScale;
                // Artistic surface params (reused push constant slots)
                pushConstants.surfaceParam1 = material->reflectivity;
                pushConstants.surfaceParam2 = material->fresnelPower;
                pushConstants.surfaceParam3 = material->rimLightStrength;
                // Dithered gradient: encode bands + pattern into surfaceParam1
                if (material->ditherGradient) {
                    pushConstants.flags |= (1 << 20); // Force flat shading
                    pushConstants.surfaceParam1 = 100.0f + static_cast<f32>(material->ditherGradientBands)
                        + static_cast<f32>(material->ditherGradientPattern) * 0.1f;
                }
                // Dithered transparency: encode pattern + opacity + blend color into surfaceParams
                if (material->ditherTransparency) {
                    pushConstants.surfaceParam1 = 200.0f + static_cast<f32>(material->ditherTransPattern);
                    pushConstants.surfaceParam2 = material->ditherTransOpacity;
                    u32 r = static_cast<u32>(material->ditherTransBlendColor.x * 1023.0f) & 0x3FF;
                    u32 g = static_cast<u32>(material->ditherTransBlendColor.y * 1023.0f) & 0x3FF;
                    u32 b = static_cast<u32>(material->ditherTransBlendColor.z * 1023.0f) & 0x3FF;
                    u32 packed = (r << 20) | (g << 10) | b;
                    pushConstants.surfaceParam3 = *reinterpret_cast<f32*>(&packed);
                }
                // Elemental surface effects: encode char/wet/snow/frost into surfaceParams
                if (!material->ditherGradient && !material->ditherTransparency) {
                    auto* elemSurface = m_World->GetComponent<ECS::ElementalSurfaceComponent>(entity);
                    if (elemSurface && (elemSurface->charAmount > 0.01f || elemSurface->wetness > 0.01f ||
                                        elemSurface->snowCoverage > 0.01f || elemSurface->frostAmount > 0.01f)) {
                        pushConstants.surfaceParam1 = 300.0f + elemSurface->charAmount;
                        pushConstants.surfaceParam2 = elemSurface->wetness + std::floor(elemSurface->snowCoverage * 256.0f);
                        pushConstants.surfaceParam3 = elemSurface->frostAmount;
                    }
                }
                // Procedural surface noise: encode scale/strength into surfaceParams (range 400+)
                // Only when no other effect has claimed the surfaceParam slots
                if (!material->ditherGradient && !material->ditherTransparency &&
                    material->surfaceNoiseScale > 0.0f && pushConstants.surfaceParam1 < 100.0f) {
                    pushConstants.surfaceParam1 = 400.0f + material->surfaceNoiseScale;
                    pushConstants.surfaceParam2 = material->surfaceNoiseStrength;
                }
            } else {
                pushConstants.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
                pushConstants.metallic = 0.0f;
                pushConstants.emissiveColor = Math::Vector3(0.0f);
                pushConstants.roughness = 0.5f;
                pushConstants.emissiveStrength = 0.0f;
                pushConstants.opacity = 1.0f;
                pushConstants.alphaCutoff = 0.5f;
                pushConstants.flags = 0;
                pushConstants.parallaxScale = 0.0f;
            }

            // Global retro overrides (OR with per-material — global forces on)
            if (m_GlobalFlatShading) pushConstants.flags |= (1 << 20);
            if (m_GlobalAffineTexturing) pushConstants.flags |= (1 << 21);
            if (m_GlobalVertexSnapping) pushConstants.flags |= (1 << 22);
            if (m_GlobalStippleTransparency) pushConstants.flags |= (1 << 23);
            if (m_GlobalUVQuantize) pushConstants.flags |= (1 << 12);
            if (m_GlobalGouraudOnly) pushConstants.flags |= (1 << 13);
            if (m_GlobalVertexSnapping && m_GlobalVertexSnapResolution > 0) {
                pushConstants.flags = (pushConstants.flags & ~(0x1F << 24)) | (static_cast<i32>((m_GlobalVertexSnapResolution / 8) & 0x1F) << 24);
            }

            // Per-entity art style override (ArtStyleComponent)
            ArtStyleComponent* artStyle = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
            if (artStyle && artStyle->style != ArtStyleType::Inherit) {
                switch (artStyle->style) {
                case ArtStyleType::PrePBR:
                    if (artStyle->prePBR_flatShading) pushConstants.flags |= (1 << 20);
                    if (artStyle->prePBR_gouraudOnly) pushConstants.flags |= (1 << 13);
                    break;
                case ArtStyleType::HandPainted:
                    // Hand-painted uses light ramp (handled at scene level),
                    // per-entity just force-enables half-Lambert via gouraud mode
                    break;
                case ArtStyleType::CelToon:
                    // Per-entity cel: rim strength via push constants (outline handled in outline pass)
                    pushConstants.surfaceParam3 = artStyle->cel_rimStrength;
                    break;
                case ArtStyleType::Retro:
                    if (artStyle->retro_flatShading) pushConstants.flags |= (1 << 20);
                    if (artStyle->retro_affineTexturing) pushConstants.flags |= (1 << 21);
                    if (artStyle->retro_vertexSnapping) pushConstants.flags |= (1 << 22);
                    if (artStyle->retro_uvQuantize) pushConstants.flags |= (1 << 12);
                    if (artStyle->retro_vertexSnapping && artStyle->retro_snapResolution > 0) {
                        pushConstants.flags = (pushConstants.flags & ~(0x1F << 24))
                            | (static_cast<i32>((artStyle->retro_snapResolution / 8) & 0x1F) << 24);
                    }
                    break;
                case ArtStyleType::MaterialExpression:
                    // Override surface noise from art style component (SSS handled in SSBO build)
                    if (material && artStyle->matExpr_surfaceNoiseScale > 0.0f &&
                        pushConstants.surfaceParam1 < 100.0f) {
                        pushConstants.surfaceParam1 = 400.0f + artStyle->matExpr_surfaceNoiseScale;
                        pushConstants.surfaceParam2 = artStyle->matExpr_surfaceNoiseStrength;
                    }
                    break;
                case ArtStyleType::NPR:
                case ArtStyleType::PixelArt:
                case ArtStyleType::Analog:
                    // These styles are primarily post-process driven (outlines, palettes,
                    // film effects). The per-entity component stores parameters but the
                    // actual rendering happens in the post-process pass, which queries
                    // ArtStyleComponent on the camera entity or scene default.
                    break;
                default:
                    break;
                }
            }

            // Set wind sway flag for vegetation entities
            if (m_World->HasComponent<VegetationComponent>(entity)) {
                pushConstants.flags |= (1 << 4); // FLAG_WIND_SWAY
            }

            // Set water surface flag for water volume entities
            WaterVolumeComponent* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
            if (waterVol) {
                pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE — always set, shader handles freeze
                pushConstants.parallaxScale = waterVol->freezeProgress; // repurpose for water (POM skips water)

                if (m_RainActive && waterVol->freezeProgress < 0.5f) {
                    pushConstants.flags |= (1 << 6); // FLAG_RAIN_RIPPLES (no ripples on ice)
                }
                if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
                    pushConstants.flags |= (1 << 7); // FLAG_WATER_SHORE
                    pushConstants.surfaceParam1 = waterVol->shoreWidth;
                    pushConstants.surfaceParam2 = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
                    pushConstants.surfaceParam3 = waterVol->foamScale;
                }
                if (waterVol->waterType == WaterType::Ocean) {
                    pushConstants.flags |= (1 << 11); // FLAG_WATER_OCEAN
                }
                // Lerp base color toward ice color as water freezes
                f32 fp = waterVol->freezeProgress;
                pushConstants.baseColor = Math::Vector3(
                    pushConstants.baseColor.x * (1.0f - fp) + waterVol->iceColor.x * fp,
                    pushConstants.baseColor.y * (1.0f - fp) + waterVol->iceColor.y * fp,
                    pushConstants.baseColor.z * (1.0f - fp) + waterVol->iceColor.z * fp
                );
                pushConstants.opacity = pushConstants.opacity * (1.0f - fp) + waterVol->iceOpacity * fp;
            } else if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) {
                auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
                pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE for Water3D
                pushConstants.parallaxScale = 0.0f; // no freeze — shader reads this as freezeProgress
                if (water3d) {
                    pushConstants.baseColor = water3d->settings.shallowColor;
                    pushConstants.opacity = water3d->settings.opacity;
                }
            }

            // Rasterize text texture if entity has a TextComponent (cached storage)
            TextComponent* textComp = m_CachedTextStorage ? m_CachedTextStorage->Get(entity) : nullptr;
            if (textComp && textComp->dirty && !textComp->fontPath.empty() && !textComp->text.empty()) {
                auto pixels = m_TextRasterizer.Rasterize(*textComp);
                if (!pixels.empty()) {
                    auto textTex = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
                    if (textTex->CreateFromData(pixels.data(), textComp->textureWidth, textComp->textureHeight, 4)) {
                        m_TextTextureCache[entity] = textTex;
                    }
                }
                textComp->dirty = false;
            }

            // If entity has a text texture, override the base color texture
            auto textTexIt = m_TextTextureCache.find(entity);
            if (textComp && textTexIt != m_TextTextureCache.end() && textTexIt->second && textTexIt->second->IsValid()) {
                boundTexture = textTexIt->second.get();
                pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
            }

            // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 6)
            UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive, texMatcap);

            // Pipeline variant selection disabled pending proper CI caching.
            // Specialization constants remain in shader with default values (all features enabled).
            // TODO: Re-enable once VkGraphicsPipelineCreateInfo sub-structs are cached as members.

            // Upload bone matrices for skinned meshes.
            // Set FLAG_SKINNED whenever the entity has valid skinning matrices —
            // not just when an animation is playing. Imported FBX models start in
            // bind pose (not playing), but their vertices are in bone-local space
            // and MUST be transformed by the bind pose skinning matrices to look
            // correct. Without FLAG_SKINNED, raw bone-local vertices render as a
            // distorted mess (the Mixamo import bug).
            // Find AnimatorComponent — may be on this entity or anywhere in the
            // scene (Mixamo FBX puts mesh and skeleton on different entities).
            // If this entity has bone weights, find the first AnimatorComponent
            // in the world to use its skinning matrices.
            AnimatorComponent* animComp = ResolveAnimator(entity);
            if (!animComp && renderData.indexCount > 0) {
                // This entity has bone weights but no animator — use cached fallback
                // (computed once per frame in Update, not per-entity)
                animComp = m_CachedFallbackAnimator;
            }

            // Upload skinning matrices. If this entity doesn't have its own bone
            // buffer but we found an animator in the hierarchy, create a temporary
            // bone buffer or use the animator entity's buffer.
            if (animComp && !renderData.boneBuffer) {
                // This mesh entity has bone weights but no bone buffer — create one
                // using the animator we found in the hierarchy.
                usize boneCount = animComp->animator.GetSkeleton()
                    ? animComp->animator.GetSkeleton()->bones.size() : 0;
                if (boneCount > 0) {
                    renderData.boneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
                    if (!renderData.boneBuffer->Create(boneCount * sizeof(Math::Matrix4),
                                                        Renderer::BufferUsage::Storage, true)) {
                        renderData.boneBuffer.reset();
                    }
                }
            }

            // Compute skinning (ADR-0002): if the pre-pass already deformed this mesh, draw the
            // deformed buffer with FLAG_SKINNED cleared (the vertex shader must NOT re-skin) and
            // keep binding 7 valid with the default bone buffer (the VS won't read it).
            const bool computeSkinned = m_ComputeSkinningEnabled && renderData.skinnedThisFrame
                                        && renderData.skinnedVertexBuffer;
            if (computeSkinned) {
                if (m_DefaultBoneBuffer) UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
            } else if (animComp && renderData.boneBuffer) {
                const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
                if (!skinningMatrices.empty()) {
                    renderData.boneBuffer->UploadData(skinningMatrices.data(),
                        skinningMatrices.size() * sizeof(Math::Matrix4));
                    UpdateBoneDescriptor(renderData.boneBuffer.get());
                    pushConstants.flags |= (1 << 3); // FLAG_SKINNED
                }
            } else {
                if (m_DefaultBoneBuffer) {
                    UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
                }
            }

            // Morph target SSBO upload + descriptor binding
            auto* morphComp = m_World->GetComponent<MorphTargetComponent>(entity);
            if (morphComp && !morphComp->targets.empty()) {
                UploadMorphTargetSSBO(entity, *morphComp, renderData);
                if (renderData.morphBuffer) {
                    UpdateMorphDescriptor(renderData.morphBuffer.get());
                }
            } else {
                if (m_DefaultMorphBuffer) {
                    UpdateMorphDescriptor(m_DefaultMorphBuffer.get());
                }
            }

            // Multi-material sub-mesh rendering: if entity has MaterialSlotsComponent
            // and the mesh has sub-meshes, draw each sub-mesh with its own material.
            MeshComponent* meshForSubMesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
            MaterialSlotsComponent* matSlots = m_CachedMaterialSlotsStorage ? m_CachedMaterialSlotsStorage->Get(entity) : nullptr;
            bool useSubMeshes = meshForSubMesh && meshForSubMesh->HasSubMeshes() && matSlots && !matSlots->slots.empty();

            bool poolPath = renderData.poolAlloc.valid && m_GeometryPool;
            if (useSubMeshes) {
                // Bind vertex/index buffers once for all sub-meshes
                if (poolPath) {
                    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
                } else if (renderData.vertexBuffer && renderData.indexBuffer) {
                    VkBuffer vertexBuffers[] = { computeSkinned
                        ? renderData.skinnedVertexBuffer->GetBuffer()
                        : renderData.vertexBuffer->GetBuffer() };
                    VkDeviceSize vbOffsets[] = { 0 };
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vbOffsets);
                    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    m_GeometryPoolBound = false;
                } else {
                    useSubMeshes = false;  // No valid buffers, fall through to single-material path
                }
            }

            if (useSubMeshes) {
                // Total index count for bounds validation
                u32 totalIndexCount = poolPath ? renderData.poolAlloc.indexCount : renderData.indexCount;

                for (const auto& subMesh : meshForSubMesh->subMeshes) {
                    // Validate sub-mesh bounds: skip if index range exceeds buffer
                    if (subMesh.indexCount == 0) continue;
                    if (subMesh.indexOffset + subMesh.indexCount > totalIndexCount) continue;

                    // Validate material slot index
                    if (subMesh.materialSlot < 0 || subMesh.materialSlot >= static_cast<i32>(matSlots->slots.size())) continue;

                    MaterialComponent* slotMat = matSlots->GetSlot(subMesh.materialSlot);
                    if (!slotMat) continue;

                    // Build push constants for this sub-mesh's material
                    Renderer::PushConstants subPC = pushConstants;  // Copy shared state (model, flags base)
                    subPC.baseColor = slotMat->baseColor;
                    subPC.metallic = slotMat->metallic;
                    subPC.emissiveColor = slotMat->emissiveColor;
                    subPC.roughness = slotMat->roughness;
                    subPC.emissiveStrength = slotMat->emissiveStrength;
                    subPC.opacity = slotMat->opacity;
                    subPC.alphaCutoff = slotMat->alphaCutoff;
                    subPC.parallaxScale = slotMat->parallaxScale;

                    // Rebuild flags from this slot's material
                    subPC.flags = pushConstants.flags & ((1 << 3) | (1 << 4) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 11)); // Preserve skinned, wind, water flags
                    if (slotMat->doubleSided) subPC.flags |= 1;
                    if (slotMat->castShadows) subPC.flags |= 2;
                    if (slotMat->receiveShadows) subPC.flags |= 4;
                    subPC.flags |= (static_cast<i32>(slotMat->alphaMode) << 8);
                    if (slotMat->flatShading) subPC.flags |= (1 << 20);
                    if (slotMat->affineTexturing) subPC.flags |= (1 << 21);
                    if (slotMat->vertexSnapping) subPC.flags |= (1 << 22);
                    if (slotMat->stippleTransparency) subPC.flags |= (1 << 23);
                    if (slotMat->uvQuantize) subPC.flags |= (1 << 12);
                    if (slotMat->gouraudOnly) subPC.flags |= (1 << 13);
                    subPC.flags |= (static_cast<i32>(slotMat->shadowDitherMode & 0x3) << 14);
                    subPC.flags |= (static_cast<i32>((slotMat->vertexSnapResolution / 8) & 0x1F) << 24);
                    subPC.flags |= (static_cast<i32>(slotMat->shadowDitherPattern & 0x7) << 29);

                    // Resolve textures for this slot's material
                    Renderer::Texture* subBoundTex = nullptr;
                    Renderer::Texture* subTexHeight = nullptr;
                    Renderer::Texture* subTexNormal = nullptr;
                    Renderer::Texture* subTexMR = nullptr;
                    Renderer::Texture* subTexEmissive = nullptr;
                    Renderer::Texture* subTexMatcap = nullptr;
                    if (slotMat->textureCacheDirty) {
                        if (!slotMat->baseColorTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->baseColorTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedBaseColorTexture = tex.get(); slotMat->baseColorTexture = 1; }
                        }
                        if (!slotMat->heightTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->heightTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedHeightTexture = tex.get(); slotMat->heightTexture = 1; }
                        }
                        if (!slotMat->normalTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->normalTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedNormalTexture = tex.get(); slotMat->normalTexture = 1; }
                        }
                        if (!slotMat->metallicRoughnessTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->metallicRoughnessTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedMetallicRoughnessTexture = tex.get(); slotMat->metallicRoughnessTexture = 1; }
                        }
                        if (!slotMat->specularTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->specularTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedMetallicRoughnessTexture = tex.get(); slotMat->metallicRoughnessTexture = 1; }
                        }
                        if (!slotMat->emissiveTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->emissiveTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedEmissiveTexture = tex.get(); slotMat->emissiveTexture = 1; }
                        }
                        if (!slotMat->matcapTexturePath.empty()) {
                            auto tex = GetOrLoadTexture(slotMat->matcapTexturePath);
                            if (tex && tex->IsValid()) { slotMat->cachedMatcapTexture = tex.get(); slotMat->matcapTexture = 1; }
                        }
                        slotMat->textureCacheDirty = false;
                        slotMat->cachedTextureKey = { slotMat->cachedBaseColorTexture,
                            slotMat->cachedHeightTexture, slotMat->cachedNormalTexture,
                            slotMat->cachedMetallicRoughnessTexture, slotMat->cachedEmissiveTexture,
                            slotMat->cachedMatcapTexture };
                    }
                    subBoundTex = slotMat->cachedBaseColorTexture;
                    subTexHeight = slotMat->cachedHeightTexture;
                    subTexNormal = slotMat->cachedNormalTexture;
                    subTexMR = slotMat->cachedMetallicRoughnessTexture;
                    subTexEmissive = slotMat->cachedEmissiveTexture;
                    subTexMatcap = slotMat->cachedMatcapTexture;

                    // Set texture flags in push constants
                    if (subBoundTex) subPC.flags |= (1 << 16);
                    if (slotMat->normalTexture >= 0) subPC.flags |= (1 << 17);
                    if (slotMat->metallicRoughnessTexture >= 0) subPC.flags |= (1 << 18);
                    if (slotMat->emissiveTexture >= 0) subPC.flags |= (1 << 19);
                    if (slotMat->heightTexture >= 0) subPC.flags |= (1 << 10);

                    // Global retro overrides
                    if (m_GlobalFlatShading) subPC.flags |= (1 << 20);
                    if (m_GlobalAffineTexturing) subPC.flags |= (1 << 21);
                    if (m_GlobalVertexSnapping) subPC.flags |= (1 << 22);
                    if (m_GlobalStippleTransparency) subPC.flags |= (1 << 23);
                    if (m_GlobalUVQuantize) subPC.flags |= (1 << 12);
                    if (m_GlobalGouraudOnly) subPC.flags |= (1 << 13);

                    UpdateEntityTextureDescriptors(subBoundTex, subTexHeight, subTexNormal, subTexMR, subTexEmissive, subTexMatcap);

                    vkCmdPushConstants(commandBuffer, targetPipeline->GetLayout(),
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                        sizeof(Renderer::PushConstants), &subPC);

                    // Draw this sub-mesh range
                    if (poolPath) {
                        vkCmdDrawIndexed(commandBuffer, subMesh.indexCount, 1,
                                         renderData.poolAlloc.indexOffset + subMesh.indexOffset,
                                         renderData.poolAlloc.vertexOffset, 0);
                    } else {
                        vkCmdDrawIndexed(commandBuffer, subMesh.indexCount, 1, subMesh.indexOffset, 0, 0);
                    }
                    m_DrawCallCount++;
                    m_TriangleCount += subMesh.indexCount / 3;
                }
            } else {
                // Single-material path (original behavior)
                vkCmdPushConstants(commandBuffer, targetPipeline->GetLayout(),
                    VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                    sizeof(Renderer::PushConstants), &pushConstants);

                if (renderData.poolAlloc.valid && m_GeometryPool) {
                    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
                    vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                                     renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
                } else if (renderData.vertexBuffer && renderData.indexBuffer) {
                    VkBuffer vertexBuffers[] = { computeSkinned
                        ? renderData.skinnedVertexBuffer->GetBuffer()
                        : renderData.vertexBuffer->GetBuffer() };
                    VkDeviceSize offsets[] = { 0 };
                    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                    vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
                    m_GeometryPoolBound = false;
                }
                m_DrawCallCount++;
                m_TriangleCount += renderData.indexCount / 3;
            }
        }
    }

    // Geometry outline pass (inverted-hull backface extrusion, after main geometry)
    RenderOutlinePassForTarget();

    // Sorted 2D sprite rendering pass (after 3D geometry)
    RenderSprites();

    // Render effect passes (grass, shrubs, trees, particles, fluid)
    // Pass render target dimensions so vegetation renderers use the correct viewport
    u32 targetW = target->GetWidth();
    u32 targetH = target->GetHeight();
    RenderGrass(targetW, targetH);
    RenderShrubs(targetW, targetH);
    RenderTrees(targetW, targetH);
    RenderParticles(targetW, targetH);
    RenderFluid(targetW, targetH);

    // Restore main pass camera, buffers, and descriptor sets
    m_Camera = prevCamera;
    m_ActiveDescriptorSets = &m_DescriptorSets;
    m_ActiveUniformBuffers = &m_UniformBuffers;
    m_ActiveLightingBuffers = &m_LightingBuffers;
    m_OffscreenMode = false;
    m_CurrentViewportIndex = 0;
}

void RenderSystem::RenderSplitscreen(Renderer::RenderTarget* target, const std::vector<ViewportCamera>& viewports) {
    if (!target || !target->IsValid() || viewports.empty() || !m_Renderer || !m_Initialized || !m_Pipeline) {
        return;
    }

    u32 viewportCount = static_cast<u32>(viewports.size());
    if (viewportCount > MAX_SPLITSCREEN_VIEWPORTS) {
        viewportCount = MAX_SPLITSCREEN_VIEWPORTS;
    }

    Renderer::Camera* prevCamera = m_Camera;

    // Switch to offscreen buffers
    m_ActiveDescriptorSets = &m_OffscreenDescriptorSets;
    m_ActiveUniformBuffers = &m_OffscreenUniformBuffers;
    m_ActiveLightingBuffers = &m_OffscreenLightingBuffers;
    m_OffscreenMode = true;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        m_Camera = prevCamera;
        m_ActiveDescriptorSets = &m_DescriptorSets;
        m_ActiveUniformBuffers = &m_UniformBuffers;
        m_ActiveLightingBuffers = &m_LightingBuffers;
        m_OffscreenMode = false;
        return;
    }

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    u32 targetW = target->GetWidth();
    u32 targetH = target->GetHeight();

    for (u32 v = 0; v < viewportCount; ++v) {
        const ViewportCamera& vc = viewports[v];
        m_CurrentViewportIndex = v;

        // Build a Camera from the entity's CameraComponent + TransformComponent
        auto* cameraComp = m_World->GetComponent<CameraComponent>(vc.entity);
        auto* cameraTransform = m_World->GetComponent<TransformComponent>(vc.entity);
        if (!cameraComp || !cameraTransform) continue;

        Renderer::Camera viewCamera;
        f32 pixelW = vc.viewportWidth * static_cast<f32>(targetW);
        f32 pixelH = vc.viewportHeight * static_cast<f32>(targetH);
        f32 aspect = (pixelH > 0.0f) ? (pixelW / pixelH) : 1.0f;

        if (cameraComp->projectionType == ProjectionType::Perspective) {
            viewCamera.SetPerspective(cameraComp->fieldOfView, aspect,
                                       cameraComp->nearPlane, cameraComp->farPlane);
        } else {
            f32 halfH = cameraComp->orthoSize;
            f32 halfW = halfH * aspect;
            viewCamera.SetOrthographic(-halfW, halfW, -halfH, halfH,
                                        cameraComp->nearPlane, cameraComp->farPlane);
        }

        viewCamera.SetPosition(cameraTransform->position);
        Math::Vector3 forward = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 0.0f, -1.0f));
        Math::Vector3 up = cameraTransform->rotation.Rotate(Math::Vector3(0.0f, 1.0f, 0.0f));
        Math::Vector3 lookTarget = cameraTransform->position + forward;
        viewCamera.SetLookAt(cameraTransform->position, lookTarget, up);

        m_Camera = &viewCamera;

        // Upload frame-level uniforms to this viewport's offscreen buffers
        UpdateFrameUniforms();
        BuildMaterialSSBO();

        // Compute pixel viewport rect
        VkViewport vkViewport{};
        vkViewport.x = vc.viewportX * static_cast<f32>(targetW);
        vkViewport.y = vc.viewportY * static_cast<f32>(targetH);
        vkViewport.width = pixelW;
        vkViewport.height = pixelH;
        vkViewport.minDepth = 0.0f;
        vkViewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);

        VkRect2D scissor{};
        scissor.offset = { static_cast<i32>(vkViewport.x), static_cast<i32>(vkViewport.y) };
        scissor.extent = { static_cast<u32>(pixelW), static_cast<u32>(pixelH) };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Render skybox for this viewport
        RenderSkybox(commandBuffer, &vkViewport, &scissor);

        // Reset descriptor cache for each viewport
        m_LastBound.Reset(); m_GeometryPoolBound = false;

        // Use offscreen pipeline (created for offscreen UNORM render pass)
        auto* ssPipeline = m_OffscreenPipeline ? m_OffscreenPipeline.get() : m_Pipeline.get();

        // Bind pipeline, descriptor set, viewport, and scissor once per viewport
        ssPipeline->Bind(commandBuffer);
        {
            u32 zeroOff = 0;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                ssPipeline->GetLayout(), 0, 1,
                &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
        }
        vkCmdSetViewport(commandBuffer, 0, 1, &vkViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // Render all entities using sorted render list (skip sprites — drawn in sorted pass)
        // Cache sprite storage pointer outside the loop to avoid per-entity type-ID hash
        auto* spriteStorageSS = m_World->GetComponentStorage<Sprite2DComponent>();
        for (Entity entity : m_SortedRenderList) {
            // Skip invisible entities or entities without transform (cached storage)
            {
                auto* xformSS = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                if (!xformSS || !xformSS->visible) continue;
            }

            // Skip 2D sprites — rendered in sorted pass after 3D geometry
            if (spriteStorageSS && spriteStorageSS->Has(entity)) continue;

            EntityRenderData* pRD = GetOrCreateRenderData(entity);
            if (!pRD) continue;
            EntityRenderData& renderData = *pRD;

            // Bind material SSBO at this entity's dynamic offset
            {
                u32 matIdx = GetMaterialIndex(entity);
                u32 dynOffset = matIdx * m_MaterialSSBOStride;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    ssPipeline->GetLayout(), 0, 1,
                    &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &dynOffset);
            }

            // Build push constants — skinned meshes use identity model matrix (cached storage)
            Renderer::PushConstants pushConstants{};
            {
                AnimatorComponent* ac = ResolveAnimator(entity);
                pushConstants.model = (ac && ac->animator.GetSkeleton())
                    ? Math::Matrix4::Identity()
                    : ECS::ComputeWorldMatrix(m_World, entity);
            }

            MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
            Renderer::Texture* boundTexture = nullptr;
            Renderer::Texture* texHeight = nullptr;
            Renderer::Texture* texNormal = nullptr;
            Renderer::Texture* texMR = nullptr;
            Renderer::Texture* texEmissive = nullptr;
            Renderer::Texture* texMatcap = nullptr;

            if (material) {
                pushConstants.baseColor = material->baseColor;
                pushConstants.metallic = material->metallic;
                pushConstants.emissiveColor = material->emissiveColor;
                pushConstants.roughness = material->roughness;
                pushConstants.emissiveStrength = material->emissiveStrength;
                pushConstants.opacity = material->opacity;
                pushConstants.alphaCutoff = material->alphaCutoff;

                // Resolve textures using cache (avoids per-frame string hash lookups)
                if (material->textureCacheDirty) {
                    if (!material->baseColorTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->baseColorTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedBaseColorTexture = tex.get();
                            material->baseColorTexture = 1;
                        }
                    }
                    if (!material->heightTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->heightTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedHeightTexture = tex.get();
                            material->heightTexture = 1;
                        }
                    }
                    if (!material->normalTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->normalTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedNormalTexture = tex.get();
                            material->normalTexture = 1;
                        }
                    }
                    if (!material->metallicRoughnessTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->metallicRoughnessTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    // Specular map overrides metallic-roughness slot for pre-PBR shading
                    if (!material->specularTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->specularTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMetallicRoughnessTexture = tex.get();
                            material->metallicRoughnessTexture = 1;
                        }
                    }
                    if (!material->emissiveTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedEmissiveTexture = tex.get();
                            material->emissiveTexture = 1;
                        }
                    }
                    if (!material->matcapTexturePath.empty()) {
                        auto tex = GetOrLoadTexture(material->matcapTexturePath);
                        if (tex && tex->IsValid()) {
                            material->cachedMatcapTexture = tex.get();
                            material->matcapTexture = 1;
                        }
                    }
                    material->textureCacheDirty = false;
                    material->cachedTextureKey = { material->cachedBaseColorTexture,
                        material->cachedHeightTexture, material->cachedNormalTexture,
                        material->cachedMetallicRoughnessTexture, material->cachedEmissiveTexture,
                        material->cachedMatcapTexture };
                }

                // Use cached texture pointers
                boundTexture = material->cachedBaseColorTexture;
                texHeight = material->cachedHeightTexture;
                texNormal = material->cachedNormalTexture;
                texMR = material->cachedMetallicRoughnessTexture;
                texEmissive = material->cachedEmissiveTexture;
                texMatcap = material->cachedMatcapTexture;

                pushConstants.flags = 0;
                if (material->doubleSided) pushConstants.flags |= 1;
                if (material->castShadows) pushConstants.flags |= 2;
                if (material->receiveShadows) pushConstants.flags |= 4;
                pushConstants.flags |= (static_cast<i32>(material->alphaMode) << 8);
                if (boundTexture != nullptr) pushConstants.flags |= (1 << 16);
                if (material->normalTexture >= 0) pushConstants.flags |= (1 << 17);
                if (material->metallicRoughnessTexture >= 0) pushConstants.flags |= (1 << 18);
                if (material->emissiveTexture >= 0) pushConstants.flags |= (1 << 19);
                if (material->heightTexture >= 0) pushConstants.flags |= (1 << 10);
                if (material->flatShading) pushConstants.flags |= (1 << 20);
                if (material->affineTexturing) pushConstants.flags |= (1 << 21);
                if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
                if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
                if (material->uvQuantize) pushConstants.flags |= (1 << 12);
                if (material->gouraudOnly) pushConstants.flags |= (1 << 13);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
                pushConstants.flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
                pushConstants.flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
                pushConstants.parallaxScale = material->parallaxScale;
                // Artistic surface params (reused push constant slots)
                pushConstants.surfaceParam1 = material->reflectivity;
                pushConstants.surfaceParam2 = material->fresnelPower;
                pushConstants.surfaceParam3 = material->rimLightStrength;
                // Dithered gradient: encode bands + pattern into surfaceParam1
                if (material->ditherGradient) {
                    pushConstants.flags |= (1 << 20); // Force flat shading
                    pushConstants.surfaceParam1 = 100.0f + static_cast<f32>(material->ditherGradientBands)
                        + static_cast<f32>(material->ditherGradientPattern) * 0.1f;
                }
                // Dithered transparency: encode pattern + opacity + blend color into surfaceParams
                if (material->ditherTransparency) {
                    pushConstants.surfaceParam1 = 200.0f + static_cast<f32>(material->ditherTransPattern);
                    pushConstants.surfaceParam2 = material->ditherTransOpacity;
                    u32 r = static_cast<u32>(material->ditherTransBlendColor.x * 1023.0f) & 0x3FF;
                    u32 g = static_cast<u32>(material->ditherTransBlendColor.y * 1023.0f) & 0x3FF;
                    u32 b = static_cast<u32>(material->ditherTransBlendColor.z * 1023.0f) & 0x3FF;
                    u32 packed = (r << 20) | (g << 10) | b;
                    pushConstants.surfaceParam3 = *reinterpret_cast<f32*>(&packed);
                }
                // Elemental surface effects: encode char/wet/snow/frost into surfaceParams
                if (!material->ditherGradient && !material->ditherTransparency) {
                    auto* elemSurface = m_World->GetComponent<ECS::ElementalSurfaceComponent>(entity);
                    if (elemSurface && (elemSurface->charAmount > 0.01f || elemSurface->wetness > 0.01f ||
                                        elemSurface->snowCoverage > 0.01f || elemSurface->frostAmount > 0.01f)) {
                        pushConstants.surfaceParam1 = 300.0f + elemSurface->charAmount;
                        pushConstants.surfaceParam2 = elemSurface->wetness + std::floor(elemSurface->snowCoverage * 256.0f);
                        pushConstants.surfaceParam3 = elemSurface->frostAmount;
                    }
                }
                // Procedural surface noise: encode scale/strength into surfaceParams (range 400+)
                // Only when no other effect has claimed the surfaceParam slots
                if (!material->ditherGradient && !material->ditherTransparency &&
                    material->surfaceNoiseScale > 0.0f && pushConstants.surfaceParam1 < 100.0f) {
                    pushConstants.surfaceParam1 = 400.0f + material->surfaceNoiseScale;
                    pushConstants.surfaceParam2 = material->surfaceNoiseStrength;
                }
            } else {
                pushConstants.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
                pushConstants.metallic = 0.0f;
                pushConstants.emissiveColor = Math::Vector3(0.0f);
                pushConstants.roughness = 0.5f;
                pushConstants.emissiveStrength = 0.0f;
                pushConstants.opacity = 1.0f;
                pushConstants.alphaCutoff = 0.5f;
                pushConstants.flags = 0;
                pushConstants.parallaxScale = 0.0f;
            }

            // Global retro overrides (OR with per-material — global forces on)
            if (m_GlobalFlatShading) pushConstants.flags |= (1 << 20);
            if (m_GlobalAffineTexturing) pushConstants.flags |= (1 << 21);
            if (m_GlobalVertexSnapping) pushConstants.flags |= (1 << 22);
            if (m_GlobalStippleTransparency) pushConstants.flags |= (1 << 23);
            if (m_GlobalUVQuantize) pushConstants.flags |= (1 << 12);
            if (m_GlobalGouraudOnly) pushConstants.flags |= (1 << 13);
            if (m_GlobalVertexSnapping && m_GlobalVertexSnapResolution > 0) {
                pushConstants.flags = (pushConstants.flags & ~(0x1F << 24)) | (static_cast<i32>((m_GlobalVertexSnapResolution / 8) & 0x1F) << 24);
            }

            if (m_World->HasComponent<VegetationComponent>(entity)) {
                pushConstants.flags |= (1 << 4);
            }

            WaterVolumeComponent* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
            if (waterVol) {
                pushConstants.flags |= (1 << 5);
                pushConstants.parallaxScale = waterVol->freezeProgress;
                if (m_RainActive && waterVol->freezeProgress < 0.5f) {
                    pushConstants.flags |= (1 << 6);
                }
                if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
                    pushConstants.flags |= (1 << 7);
                    pushConstants.surfaceParam1 = waterVol->shoreWidth;
                    pushConstants.surfaceParam2 = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
                    pushConstants.surfaceParam3 = waterVol->foamScale;
                }
                if (waterVol->waterType == WaterType::Ocean) {
                    pushConstants.flags |= (1 << 11);
                }
                f32 fp = waterVol->freezeProgress;
                pushConstants.baseColor = Math::Vector3(
                    pushConstants.baseColor.x * (1.0f - fp) + waterVol->iceColor.x * fp,
                    pushConstants.baseColor.y * (1.0f - fp) + waterVol->iceColor.y * fp,
                    pushConstants.baseColor.z * (1.0f - fp) + waterVol->iceColor.z * fp
                );
                pushConstants.opacity = pushConstants.opacity * (1.0f - fp) + waterVol->iceOpacity * fp;
            } else if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) {
                auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
                pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE for Water3D
                pushConstants.parallaxScale = 0.0f; // no freeze
                if (water3d) {
                    pushConstants.baseColor = water3d->settings.shallowColor;
                    pushConstants.opacity = water3d->settings.opacity;
                }
            }

            // Text rendering (cached storage)
            TextComponent* textComp = m_CachedTextStorage ? m_CachedTextStorage->Get(entity) : nullptr;
            if (textComp && textComp->dirty && !textComp->fontPath.empty() && !textComp->text.empty()) {
                auto pixels = m_TextRasterizer.Rasterize(*textComp);
                if (!pixels.empty()) {
                    auto textTex = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
                    if (textTex->CreateFromData(pixels.data(), textComp->textureWidth, textComp->textureHeight, 4)) {
                        m_TextTextureCache[entity] = textTex;
                    }
                }
                textComp->dirty = false;
            }
            auto textTexIt = m_TextTextureCache.find(entity);
            if (textComp && textTexIt != m_TextTextureCache.end() && textTexIt->second && textTexIt->second->IsValid()) {
                boundTexture = textTexIt->second.get();
                pushConstants.flags |= (1 << 16);
            }

            // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 6)
            UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive, texMatcap);

            // Upload bone matrices for skinned meshes (bind pose or animation, cached storage)
            AnimatorComponent* animComp = ResolveAnimator(entity);
            if (animComp && renderData.boneBuffer) {
                const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
                if (!skinningMatrices.empty()) {
                    renderData.boneBuffer->UploadData(skinningMatrices.data(),
                        skinningMatrices.size() * sizeof(Math::Matrix4));
                    UpdateBoneDescriptor(renderData.boneBuffer.get());
                    pushConstants.flags |= (1 << 3);
                }
            } else {
                if (m_DefaultBoneBuffer) {
                    UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
                }
            }

            vkCmdPushConstants(commandBuffer, ssPipeline->GetLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                sizeof(Renderer::PushConstants), &pushConstants);

            if (renderData.poolAlloc.valid && m_GeometryPool) {
                if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
                vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                                 renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
            } else if (renderData.vertexBuffer && renderData.indexBuffer) {
                VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
                VkDeviceSize offsets[] = { 0 };
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
                m_GeometryPoolBound = false;
            }
            m_DrawCallCount++;
            m_TriangleCount += renderData.indexCount / 3;
        }

        // Sorted 2D sprite rendering pass (after 3D geometry)
        RenderSprites();

        // Render effects for this viewport
        RenderGrass(targetW, targetH);
        RenderShrubs(targetW, targetH);
        RenderTrees(targetW, targetH);
        RenderParticles(targetW, targetH);
        RenderFluid(targetW, targetH);
    }

    // Restore main pass state
    m_Camera = prevCamera;
    m_ActiveDescriptorSets = &m_DescriptorSets;
    m_ActiveUniformBuffers = &m_UniformBuffers;
    m_ActiveLightingBuffers = &m_LightingBuffers;
    m_OffscreenMode = false;
    m_CurrentViewportIndex = 0;
}

void RenderSystem::OnEntityAdded(Entity entity) {
    SetupEntityBuffers(entity);

    // Invalidate scene composition cache (new entity may change 2D/3D classification)
    m_SceneComposition.dirty = true;

    // Invalidate material SSBO (new entity needs to be included in the buffer)
    m_MaterialSSBODirty = true;

    // Invalidate shadow caster cache (new entity may be a shadow caster)
    m_ShadowCastersDirty = true;

    // Invalidate light entity cache (new entity may have a LightComponent)
    if (m_World && m_World->HasComponent<LightComponent>(entity)) {
        m_LightListDirty = true;
    }

    // Cache player entity (first entity with any CharacterController)
    if (m_CachedPlayerEntity == INVALID_ENTITY && m_World) {
        bool hasController = m_World->HasComponent<ThirdPersonController>(entity) ||
                             m_World->HasComponent<FirstPersonController>(entity) ||
                             m_World->HasComponent<Platformer2DController>(entity) ||
                             m_World->HasComponent<TopDown2DController>(entity) ||
                             m_World->HasComponent<TopDown3DController>(entity);
        if (hasController) {
            m_CachedPlayerEntity = entity;
        }
    }
}

void RenderSystem::OnEntityRemoved(Entity entity) {
    // Invalidate this entity's BLAS cache entry BEFORE freeing the pool allocation.
    // Without this, freeing the pool region allows a future entity to reuse the same
    // offsets, producing an identical address-based mesh hash that returns a stale BLAS
    // built for the old (now-freed) geometry — causing a GPU crash on ray trace dispatch.
    if (m_ASManager && m_RTEnabled &&
        static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].valid) {
        const auto& rd = m_EntityRenderData[static_cast<usize>(EntityIndex(entity))];
        VkDeviceAddress vertAddr = 0, idxAddr = 0;
        if (rd.vertexBuffer && rd.indexBuffer) {
            vertAddr = rd.vertexBuffer->GetDeviceAddress();
            idxAddr = rd.indexBuffer->GetDeviceAddress();
        } else if (rd.poolAlloc.valid && m_GeometryPool &&
                   m_GeometryPool->GetVertexBuffer() && m_GeometryPool->GetIndexBuffer()) {
            vertAddr = m_GeometryPool->GetVertexBuffer()->GetDeviceAddress()
                     + static_cast<VkDeviceAddress>(rd.poolAlloc.vertexOffset) * sizeof(MeshComponent::Vertex);
            idxAddr = m_GeometryPool->GetIndexBuffer()->GetDeviceAddress()
                    + static_cast<VkDeviceAddress>(rd.poolAlloc.indexOffset) * sizeof(u32);
        }
        if (vertAddr != 0 && idxAddr != 0) {
            u64 meshHash = vertAddr ^ (idxAddr << 32) ^ (idxAddr >> 32);
            m_ASManager->InvalidateMesh(meshHash);
        }
    }

    // Free merged geometry pool allocation before erasing render data
    if (static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size() && m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].valid) {
        auto& rd = m_EntityRenderData[static_cast<usize>(EntityIndex(entity))];
        if (rd.poolAlloc.valid && m_GeometryPool) {
            m_GeometryPool->Free(rd.poolAlloc);
        }
        rd.Invalidate();
    }
    m_TextTextureCache.erase(entity);
    m_PrevModelMatrices.erase(static_cast<u64>(entity));

    // Invalidate scene composition cache (removed entity may change 2D/3D classification)
    m_SceneComposition.dirty = true;

    // Invalidate material SSBO (removed entity changes the buffer layout)
    m_MaterialSSBODirty = true;

    // Invalidate shadow caster cache (removed entity may have been a shadow caster)
    m_ShadowCastersDirty = true;

    // Invalidate light entity cache (removed entity may have had a LightComponent)
    if (m_World && m_World->HasComponent<LightComponent>(entity)) {
        m_LightListDirty = true;
    }

    // Invalidate cached player entity — will be re-discovered lazily
    if (entity == m_CachedPlayerEntity) {
        m_CachedPlayerEntity = INVALID_ENTITY;
    }
}

void RenderSystem::ClassifySceneComposition() {
    if (!m_SceneComposition.dirty || !m_World) return;

    m_SceneComposition.spriteCount = 0;
    m_SceneComposition.tilemapCount = 0;
    m_SceneComposition.mesh3DCount = 0;
    m_SceneComposition.hasShadowCastingLights = false;

    // Count sprites and tilemaps using direct container size (avoids iteration)
    m_SceneComposition.spriteCount = static_cast<u32>(m_World->GetEntitiesWithComponent<Sprite2DComponent>().size());
    m_SceneComposition.tilemapCount = static_cast<u32>(m_World->GetEntitiesWithComponent<TilemapComponent>().size());

    // Count 3D meshes: total MeshComponent entities minus sprites and tilemaps
    // (sprites and tilemaps also have MeshComponent, so subtract them)
    {
        u32 totalMesh = static_cast<u32>(m_World->GetEntitiesWithComponent<MeshComponent>().size());
        m_SceneComposition.mesh3DCount = (totalMesh > m_SceneComposition.spriteCount + m_SceneComposition.tilemapCount)
            ? totalMesh - m_SceneComposition.spriteCount - m_SceneComposition.tilemapCount : 0;
    }

    // Check for shadow-casting directional lights and any lights at all
    bool hasAnyLights = !m_CachedLightEntities.empty();
    for (Entity entity : m_CachedLightEntities) {
        auto* light = m_World->GetComponent<LightComponent>(entity);
        if (light && light->type == LightType::Directional && light->castShadows) {
            m_SceneComposition.hasShadowCastingLights = true;
            break;
        }
    }

    // Classify scene mode
    // Scene3D: 3D meshes present — full pipeline (shadows, lighting, normal maps)
    // Scene2_5D: sprites only but lights exist — skip shadows, populate full lighting UBO
    // Scene2D: sprites only, no lights — minimal UBO (ambient/fog only)
    if (m_SceneComposition.mesh3DCount > 0) {
        m_SceneComposition.mode = SceneRenderMode::Scene3D;
    } else if (hasAnyLights) {
        m_SceneComposition.mode = SceneRenderMode::Scene2_5D;
    } else {
        m_SceneComposition.mode = SceneRenderMode::Scene2D;
    }

    m_SceneComposition.dirty = false;

    // Diagnostic warnings (every 300 frames to avoid log spam)
    if (++m_DiagnosticFrameCounter >= 300) {
        m_DiagnosticFrameCounter = 0;

        // Warn if many unbatched sprites
        if (m_SceneComposition.spriteCount > 100 && !m_SpriteBatchRenderer) {
            ENJIN_LOG_WARN(Renderer, "%u sprites without batching - consider enabling SpriteBatchRenderer",
                m_SceneComposition.spriteCount);
        }

        // Log mixed 2D/3D scene info for debugging
        if (m_SceneComposition.spriteCount > 0 && m_SceneComposition.mesh3DCount > 0) {
            ENJIN_LOG_INFO(Renderer, "Mixed 2D/3D scene: %u sprites, %u meshes, %u shadow casters",
                m_SceneComposition.spriteCount, m_SceneComposition.mesh3DCount,
                static_cast<u32>(m_ShadowCasters.size()));
        }
    }
}

void RenderSystem::RebuildShadowCasterCache() {
    m_ShadowCasters.clear();
    if (!m_World) {
        m_ShadowCastersDirty = false;
        return;
    }

    // Reserve approximate capacity based on mesh count
    m_ShadowCasters.reserve(m_SceneComposition.mesh3DCount > 0 ? m_SceneComposition.mesh3DCount : 64);

    // Cache storage pointers to avoid per-entity type-ID hash lookups
    auto* spriteStorageSC = m_World->GetComponentStorage<Sprite2DComponent>();
    auto* tilemapStorageSC = m_World->GetComponentStorage<TilemapComponent>();

    // Iterate only entities with MeshComponent
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        // Skip entities without transform
        auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xform) continue;

        // Skip 2D sprites — they never cast shadows
        if (spriteStorageSC && spriteStorageSC->Has(entity)) continue;

        // Skip tilemaps
        if (tilemapStorageSC && tilemapStorageSC->Has(entity)) continue;

        // Skip invisible entities
        if (!xform->visible) continue;

        // Check if material casts shadows (default: yes)
        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material && !material->castShadows) continue;

        m_ShadowCasters.push_back(entity);
    }

    m_ShadowCastersDirty = false;
}

void RenderSystem::BuildCullableObjectList() {
    m_CullableObjects.clear();
    m_EntityToCullIndex.clear();

    if (!m_World || !m_GPUCulling) return;

    // Reserve capacity
    usize meshCount = m_SceneComposition.mesh3DCount;
    if (meshCount == 0) return;

    m_CullableObjects.reserve(meshCount);
    m_EntityToCullIndex.resize(m_World->GetEntityCount(), UINT32_MAX);

    // Track which entities are drawn via indirect (pool-eligible, non-textured or textured-batched)
    m_IndirectDrawn.clear();
    m_IndirectDrawn.resize(m_World->GetEntityCount(), false);

    // Reset texture-grouped indirect draw batcher for this frame
    if (m_IndirectDrawBatcher) m_IndirectDrawBatcher->Reset();

    u32 cullIndex = 0;

    // Cache storage pointers to avoid per-entity type-ID hash lookups
    auto* spriteStorageBCO = m_World->GetComponentStorage<Sprite2DComponent>();
    auto* tilemapStorageBCO = m_World->GetComponentStorage<TilemapComponent>();

    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xform || !xform->visible) continue;
        if (spriteStorageBCO && spriteStorageBCO->Has(entity)) continue;
        if (tilemapStorageBCO && tilemapStorageBCO->Has(entity)) continue;

        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!mesh || !mesh->IsValid()) continue;

        // Compute AABB from mesh vertices (cached on MeshComponent to avoid per-frame recomputation)
        if (mesh->aabbDirty) {
            Math::Vector3 bMin(1e30f, 1e30f, 1e30f);
            Math::Vector3 bMax(-1e30f, -1e30f, -1e30f);
            for (const auto& vertex : mesh->vertices) {
                bMin.x = Math::Min(bMin.x, vertex.position.x);
                bMin.y = Math::Min(bMin.y, vertex.position.y);
                bMin.z = Math::Min(bMin.z, vertex.position.z);
                bMax.x = Math::Max(bMax.x, vertex.position.x);
                bMax.y = Math::Max(bMax.y, vertex.position.y);
                bMax.z = Math::Max(bMax.z, vertex.position.z);
            }
            if (bMin.x > bMax.x) {
                bMin = Math::Vector3(-0.5f);
                bMax = Math::Vector3(0.5f);
            }
            mesh->cachedAABBMin = bMin;
            mesh->cachedAABBMax = bMax;
            mesh->aabbDirty = false;
        }

        Renderer::BoundingBox bounds;
        bounds.min = mesh->cachedAABBMin;
        bounds.max = mesh->cachedAABBMax;

        Renderer::CullableObject obj;
        obj.SetBounds(bounds);
        obj.transform = ECS::ComputeWorldMatrix(m_World, entity);
        obj.meshIndex = EntityIndex(entity); // entity slot index doubles as mesh index for now
        obj.indexCount = static_cast<u32>(mesh->indices.size());

        // Index the per-entity tracking vectors by EntityIndex (slot), NOT the
        // raw generational handle — a recycled handle's raw value is billions
        // and would silently fall outside these vectors. Grow-to-fit because
        // EntityIndex can exceed GetEntityCount() after recycling.
        const usize entIdx = static_cast<usize>(EntityIndex(entity));

        // Use pool offsets if entity has a merged geometry allocation
        bool hasPoolAlloc = false;
        if (static_cast<usize>(EntityIndex(entity)) < m_EntityRenderData.size() &&
            m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].valid &&
            m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].poolAlloc.valid) {
            obj.indexOffset = m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].poolAlloc.indexOffset;
            obj.vertexOffset = m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].poolAlloc.vertexOffset;
            hasPoolAlloc = true;
        } else {
            obj.indexOffset = 0;
            obj.vertexOffset = 0;
        }

        // Mark entity for indirect draw if pool-eligible and has no per-entity textures.
        // Entities with textures (flags bits 16-19) need per-entity descriptor updates
        // and must remain on the per-entity draw path. The indirectEligible flag on the
        // CullableObject controls whether the GPU cull shader emits an indirect draw command.
        if (hasPoolAlloc) {
            auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
            bool hasTextures = false;
            if (material) {
                hasTextures = material->cachedBaseColorTexture != nullptr
                    || material->cachedNormalTexture != nullptr
                    || material->cachedMetallicRoughnessTexture != nullptr
                    || material->cachedEmissiveTexture != nullptr
                    || material->cachedMatcapTexture != nullptr;
            }
            if (!hasTextures) {
                // Non-textured: GPU culling emits indirect draw commands directly
                obj.indirectEligible = 1;
                if (entIdx >= m_IndirectDrawn.size()) m_IndirectDrawn.resize(entIdx + 1, false);
                m_IndirectDrawn[entIdx] = true;
            } else if (m_IndirectDrawBatcher && material) {
                // Textured pool entity: add to texture-grouped indirect draw batcher.
                // The batcher groups entities by texture set hash and issues one
                // vkCmdDrawIndexedIndirect per group instead of per entity.
                material->ComputeSortKey(0.0f);
                u64 texHash = (material->cachedSortKey >> 16) & 0xFFFFFFFFFFULL; // 40 texture bits
                m_IndirectDrawBatcher->AddEntity(
                    EntityIndex(entity), texHash,
                    obj.indexCount, obj.indexOffset, obj.vertexOffset,
                    cullIndex);
                if (entIdx >= m_IndirectDrawn.size()) m_IndirectDrawn.resize(entIdx + 1, false);
                m_IndirectDrawn[entIdx] = true;
            }
        }

        // Map entity to cull index
        if (entIdx >= m_EntityToCullIndex.size()) m_EntityToCullIndex.resize(entIdx + 1, UINT32_MAX);
        m_EntityToCullIndex[entIdx] = cullIndex;

        m_CullableObjects.push_back(obj);
        cullIndex++;
    }

    // Build texture-grouped indirect draw batches from accumulated textured entities
    if (m_IndirectDrawBatcher) {
        if (m_IndirectDrawBatcher->BuildBatches()) {
            m_IndirectDrawBatcher->UploadCommands();
        }
    }
}

void RenderSystem::PerformGPUCulling() {
    if (!m_GPUCulling || !m_GPUCullingEnabled || !m_Camera) return;
    if (m_CullableObjects.empty()) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Submit objects for culling
    m_GPUCulling->SubmitObjects(m_CullableObjects);

    VkBuffer indirectBuffer;
    u32 drawCount;

    // Use two-phase HiZ occlusion culling when Hi-Z pyramid is available
    if (m_HiZPyramid && m_GPUCulling->HasHiZ()) {
        if (m_GPUCulling->ExecuteTwoPhase(
                m_Camera->GetViewMatrix(),
                m_Camera->GetProjectionMatrix(),
                commandBuffer,
                indirectBuffer,
                drawCount)) {
            auto stats = m_GPUCulling->GetStats();
            (void)stats;
            return;
        }
    }

    // Fallback to single-phase culling
    if (m_GPUCulling->ExecuteCulling(
            m_Camera->GetViewMatrix(),
            m_Camera->GetProjectionMatrix(),
            commandBuffer,
            indirectBuffer,
            drawCount)) {
        auto stats = m_GPUCulling->GetStats();
        (void)stats;
    }
}

void RenderSystem::PerformGPUCullingAsync() {
    if (!m_GPUCulling || !m_GPUCullingEnabled || !m_Camera) return;
    if (m_CullableObjects.empty()) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentComputeCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    m_GPUCulling->SubmitObjects(m_CullableObjects);

    VkBuffer indirectBuffer;
    u32 drawCount;
    m_GPUCulling->ExecuteCulling(
        m_Camera->GetViewMatrix(),
        m_Camera->GetProjectionMatrix(),
        commandBuffer,
        indirectBuffer,
        drawCount);
}

void RenderSystem::UploadObjectData() {
    if (!m_GPUCulling || m_CullableObjects.empty()) return;

    // Build ObjectData array matching cullable objects 1:1
    m_ObjectDataCPU.resize(m_CullableObjects.size());

    // Cache storage pointers to avoid per-entity type-ID hash lookups
    auto* spriteStorageUOD = m_World->GetComponentStorage<Sprite2DComponent>();
    auto* tilemapStorageUOD = m_World->GetComponentStorage<TilemapComponent>();

    u32 idx = 0;
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xform || !xform->visible) continue;
        if (spriteStorageUOD && spriteStorageUOD->Has(entity)) continue;
        if (tilemapStorageUOD && tilemapStorageUOD->Has(entity)) continue;

        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!mesh || !mesh->IsValid()) continue;

        if (idx >= m_ObjectDataCPU.size()) break;

        ObjectDataGPU& obj = m_ObjectDataCPU[idx];
        obj.model = ECS::ComputeWorldMatrix(m_World, entity);

        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material) {
            obj.baseColor = material->baseColor;
            obj.metallic = material->metallic;
            obj.emissiveColor = material->emissiveColor;
            obj.roughness = material->roughness;
            obj.emissiveStrength = material->emissiveStrength;
            obj.opacity = material->opacity;
            obj.alphaCutoff = material->alphaCutoff;

            i32 flags = 0;
            if (material->doubleSided) flags |= 1;
            if (material->castShadows) flags |= 2;
            if (material->receiveShadows) flags |= 4;
            flags |= (static_cast<i32>(material->alphaMode) << 8);
            if (material->baseColorTexture >= 0) flags |= (1 << 16);
            if (material->normalTexture >= 0) flags |= (1 << 17);
            if (material->metallicRoughnessTexture >= 0) flags |= (1 << 18);
            if (material->emissiveTexture >= 0) flags |= (1 << 19);
            if (material->heightTexture >= 0) flags |= (1 << 10);
            if (material->flatShading) flags |= (1 << 20);
            if (material->affineTexturing) flags |= (1 << 21);
            if (material->vertexSnapping) flags |= (1 << 22);
            if (material->stippleTransparency) flags |= (1 << 23);
            if (material->uvQuantize) flags |= (1 << 12);
            if (material->gouraudOnly) flags |= (1 << 13);
            flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
            flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
            flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
            obj.flags = flags;
            obj.parallaxScale = material->parallaxScale;
        } else {
            obj.baseColor = Math::Vector3(1.0f);
            obj.metallic = 0.0f;
            obj.emissiveColor = Math::Vector3(0.0f);
            obj.roughness = 0.5f;
            obj.emissiveStrength = 0.0f;
            obj.opacity = 1.0f;
            obj.alphaCutoff = 0.5f;
            obj.flags = 0;
            obj.parallaxScale = 0.0f;
        }

        if (m_GlobalFlatShading) obj.flags |= (1 << 20);

        // Populate previous-frame model matrix for motion vector computation.
        // If no previous matrix exists (first frame for this entity), use current
        // model and mark as teleported so the shader outputs zero velocity.
        u64 entityId = static_cast<u64>(entity);
        auto prevIt = m_PrevModelMatrices.find(entityId);
        if (prevIt != m_PrevModelMatrices.end()) {
            obj.prevModel = prevIt->second;
            obj.teleported = 0;
        } else {
            obj.prevModel = obj.model;
            obj.teleported = 1;
        }

        // Network teleport detection: if the transform was flagged as teleported
        // this frame (large position snap, spawn, or respawn), force teleported = 1
        // so the shader zeroes the motion vector and TAA doesn't ghost.
        if (xform->teleportedThisFrame) {
            obj.teleported = 1;
            obj.prevModel = obj.model;  // Ensure prevModel matches current to zero velocity
            xform->teleportedThisFrame = false;  // Consume the flag (once per frame)
        }

        // Store current model matrix for next frame's previous-model lookup (eliminates second pass)
        m_PrevModelMatrices[entityId] = obj.model;

        obj._pad[0] = obj._pad[1] = 0.0f;
        idx++;
    }

    // Upload to GPU via GPUCulling's ObjectData buffer
    usize uploadSize = idx * sizeof(ObjectDataGPU);
    if (uploadSize > 0) {
        m_GPUCulling->UploadObjectData(m_ObjectDataCPU.data(), uploadSize);
    }
}

void RenderSystem::DrawIndirect(VkCommandBuffer commandBuffer) {
    if (!m_GPUCulling || !m_GPUCullingEnabled || !m_GeometryPool) return;
    if (m_CullableObjects.empty()) return;

    VkBuffer indirectBuffer = m_GPUCulling->GetIndirectDrawBuffer();
    VkBuffer drawCountBuffer = m_GPUCulling->GetDrawCountBuffer();
    if (indirectBuffer == VK_NULL_HANDLE || drawCountBuffer == VK_NULL_HANDLE) return;

    // Bind the merged geometry pool (single VB + IB for all static meshes)
    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }

    // Push sentinel constants to signal indirect mode to shaders.
    // parallaxScale = -1.0 tells the vertex/fragment shaders to read per-object data
    // from the ObjectData SSBO (binding 13) indexed by gl_InstanceIndex instead of push constants.
    Renderer::PushConstants indirectPC{};
    indirectPC.parallaxScale = -1.0f;
    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(Renderer::PushConstants), &indirectPC);

    // Issue a single indirect draw call for all visible static meshes.
    // The GPU culling compute shader has compacted visible objects into a contiguous
    // VkDrawIndexedIndirectCommand array. Each command's firstInstance encodes the
    // original object index for SSBO lookup via gl_InstanceIndex.
    vkCmdDrawIndexedIndirectCount(
        commandBuffer,
        indirectBuffer,             // VkDrawIndexedIndirectCommand array
        0,                          // offset
        drawCountBuffer,            // buffer containing actual draw count
        0,                          // count buffer offset
        m_GPUCulling->GetMaxObjects(), // maxDrawCount
        sizeof(VkDrawIndexedIndirectCommand) // stride
    );

    m_DrawCallCount++;  // Single draw call for all indirect objects
}

void RenderSystem::BuildTexturedIndirectBatches() {
    // This is now done in BuildCullableObjectList � entities are added to the batcher
    // during the main entity iteration loop, and batches are built/uploaded at the end.
}

void RenderSystem::DrawTexturedIndirect(VkCommandBuffer commandBuffer) {
    if (!m_IndirectDrawBatcher || !m_IndirectDrawBatcher->HasBatches()) return;
    if (!m_GeometryPool || !m_Pipeline) return;

    VkBuffer indirectBuffer = m_IndirectDrawBatcher->GetIndirectBuffer();
    if (indirectBuffer == VK_NULL_HANDLE) return;

    // Bind the merged geometry pool (single VB + IB for all static meshes)
    if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }

    // Push sentinel constants for indirect mode (same as non-textured path)
    Renderer::PushConstants indirectPC{};
    indirectPC.parallaxScale = -1.0f;
    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
        sizeof(Renderer::PushConstants), &indirectPC);

    // Iterate texture draw batches: one indirect draw per texture group.
    // Each batch shares the same texture set, so we bind textures once per batch.
    for (const auto& batch : m_IndirectDrawBatcher->GetBatches()) {
        // Bind textures from the representative entity of this batch
        Entity representative = static_cast<Entity>(batch.representativeEntity);
        auto* material = m_World->GetComponent<MaterialComponent>(representative);
        if (material) {
            UpdateEntityTextureDescriptors(
                material->cachedBaseColorTexture,
                material->cachedHeightTexture,
                material->cachedNormalTexture,
                material->cachedMetallicRoughnessTexture,
                material->cachedEmissiveTexture,
                material->cachedMatcapTexture);
        }

        // Issue indirect draw for all entities in this texture group
        VkDeviceSize offset = static_cast<VkDeviceSize>(batch.firstCommand) * sizeof(VkDrawIndexedIndirectCommand);
        vkCmdDrawIndexedIndirect(
            commandBuffer,
            indirectBuffer,
            offset,
            batch.commandCount,
            sizeof(VkDrawIndexedIndirectCommand)
        );

        m_DrawCallCount++;
    }
}

void RenderSystem::DrawDGC(VkCommandBuffer commandBuffer) {
    if (!m_DGC || !m_DGC->IsEnabled()) return;
    if (!m_GPUCulling || !m_GeometryPool || !m_Pipeline) return;

    // Bind the merged geometry pool (single VB + IB for all static meshes)
    if (!m_GeometryPoolBound) {
        m_GeometryPool->BindBuffers(commandBuffer);
        m_GeometryPoolBound = true;
    }

    // Verify required buffers are available
    VkBuffer objectBuffer = m_GPUCulling->GetObjectBuffer();
    VkBuffer objectDataBuffer = m_GPUCulling->GetObjectDataBuffer();
    VkBuffer visibilityBuffer = m_GPUCulling->GetVisibilityBuffer();
    if (objectBuffer == VK_NULL_HANDLE || objectDataBuffer == VK_NULL_HANDLE ||
        visibilityBuffer == VK_NULL_HANDLE) {
        DrawIndirect(commandBuffer); // Fallback
        return;
    }

    // Step 1: Generate DGC command sequences from culling results.
    // The compute shader reads CullableObjects, ObjectData, and visibility flags,
    // then writes push constant + draw indexed sequences into the DGC sequence buffer.
    m_DGC->GenerateCommands(
        commandBuffer,
        objectBuffer,
        objectDataBuffer,
        visibilityBuffer,
        static_cast<u32>(m_CullableObjects.size())
    );

    // Step 2: Preprocess the generated command stream
    m_DGC->Preprocess(commandBuffer);

    // Step 3: Bind graphics pipeline and execute DGC
    m_Pipeline->Bind(commandBuffer);
    m_DGC->Execute(commandBuffer);

    m_DrawCallCount++; // Single DGC execution replaces all indirect draws
}

void RenderSystem::DispatchRTEffectsAsync(u32 frameIndex) {
    if (!m_AsyncComputeScheduler || !m_RTEnabled) return;

    // Record RT dispatches + denoising on async compute command buffer.
    // BeginFrame was already called at the start of Update().
    VkCommandBuffer computeCmd = m_AsyncComputeScheduler->BeginComputeWork(
        frameIndex, Renderer::AsyncComputeWorkType::RTDispatch);
    if (computeCmd == VK_NULL_HANDLE) {
        // Fallback: run on graphics queue
        VkCommandBuffer graphicsCmd = m_VulkanRenderer->GetCurrentCommandBuffer();
        if (graphicsCmd != VK_NULL_HANDLE) {
            DispatchRTEffects(graphicsCmd);
            TemporalReuseRTOutputs(graphicsCmd);
            DenoiseRTOutputs(graphicsCmd);
            CompositeRTResults(graphicsCmd);
        }
        return;
    }

    // Record RT dispatches, temporal reuse, and denoising on the compute command buffer
    DispatchRTEffects(computeCmd);
    TemporalReuseRTOutputs(computeCmd);
    DenoiseRTOutputs(computeCmd);

    // Submit compute work � signal semaphore for graphics queue to wait on
    m_AsyncComputeScheduler->SubmitComputeWork(frameIndex, false, true);

    // Register the compute-finished semaphore with VulkanRenderer so the graphics
    // queue submit waits for compute completion before present.
    VkSemaphore computeSem = m_AsyncComputeScheduler->GetComputeFinishedSemaphore(frameIndex);
    if (computeSem != VK_NULL_HANDLE) {
        m_VulkanRenderer->AddExternalWaitSemaphore(computeSem,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
    }
}

void RenderSystem::DenoiseRTOutputsAsync(u32 frameIndex) {
    // Denoising is included in DispatchRTEffectsAsync (same compute command buffer).
    // This method is available for future use if we want to split denoise into a
    // separate compute submission that overlaps with post-processing.
    (void)frameIndex;
}

void RenderSystem::CreatePipeline() {
    Renderer::PipelineConfig config;
    config.renderPass = m_VulkanRenderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = m_BackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    // Main pipeline always uses fill mode — wireframe only affects the offscreen
    // pipeline (scene view). Game view and Player always render filled.
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.msaaSamples = m_VulkanRenderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // Swapchain MRT: color + velocity (main pass only; offscreen uses 1)

    m_Pipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_Pipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_Pipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create graphics pipeline");
        m_Pipeline.reset();
    }
}

void RenderSystem::CreateShadowPipeline() {
    if (!m_ShadowMap || !m_Pipeline || !m_ShadowVertexShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_ShadowMap->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_NONE;  // Render all faces — front-only culling causes shadow holes under convex objects (capsule bottom)
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.depthBiasEnable = true;
    config.depthBiasConstant = 1.25f;  // Balanced: tight contact shadows without acne rings
    config.depthBiasSlope = 1.75f;     // Slope-scaled bias for angled surfaces
    config.hasColorAttachment = false;  // Depth-only pass

    m_ShadowPipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_ShadowPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    // Shadow shader uses push constants for MVP (avoids HOST_COHERENT UBO race).
    // Share descriptor set layout with main pipeline for compatibility.
    if (!m_ShadowPipeline->CreateWithLayout(config, m_ShadowVertexShader.get(), nullptr,
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create shadow pipeline");
        m_ShadowPipeline.reset();
        m_ShadowsEnabled = false;
    }
}

void RenderSystem::CreateLinePipeline() {
    if (!m_Pipeline) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_VulkanRenderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    config.depthTest = true;
    config.depthWrite = false;
    config.cullMode = VK_CULL_MODE_NONE;
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.alphaBlend = true;
    config.msaaSamples = m_VulkanRenderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // MRT: must match render pass

    m_LinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_LinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_LinePipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(),
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create line pipeline");
        m_LinePipeline.reset();
    }
}

void RenderSystem::CreateOutlinePipeline() {
    if (!m_Pipeline || !m_OutlineVertexShader || !m_OutlineFragmentShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_VulkanRenderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_FRONT_BIT;  // Front-face culling: renders backfaces only (inverted hull)
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.msaaSamples = m_VulkanRenderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // Swapchain MRT: color + velocity (main pass only; offscreen uses 1) (must match render pass)

    m_OutlinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_OutlinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_OutlinePipeline->CreateWithLayout(config, m_OutlineVertexShader.get(), m_OutlineFragmentShader.get(),
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create outline pipeline");
        m_OutlinePipeline.reset();
    }
}

// ============================================================================
// Per-entity wireframe overlay pipeline + pass
// Renders entities with MeshRendererComponent::wireframe=true as LINE overlay
// on top of solid geometry. Uses the main vertex shader (for skinning support)
// and the outline fragment shader (flat color output).
// ============================================================================

void RenderSystem::CreateWireframeOverlayPipeline() {
    if (!m_Pipeline || !m_VertexShader || !m_OutlineFragmentShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_VulkanRenderer->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = false;   // Don't write depth — overlay on top of existing geometry
    config.cullMode = VK_CULL_MODE_NONE;  // Show all edges
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_LINE;
    config.msaaSamples = m_VulkanRenderer->GetMSAASamples();
    config.colorAttachmentCount = 2; // Match main render pass MRT

    m_WireframeOverlayPipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_WireframeOverlayPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_WireframeOverlayPipeline->CreateWithLayout(config, m_VertexShader.get(), m_OutlineFragmentShader.get(),
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create wireframe overlay pipeline");
        m_WireframeOverlayPipeline.reset();
    }
}

void RenderSystem::RenderWireframeOverlayPass() {
    if (!m_WireframeOverlayPipeline || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Check if any entity has wireframe enabled — skip pass if none
    auto* meshRendererStorage = m_World->GetComponentStorage<MeshRendererComponent>();
    if (!meshRendererStorage) return;

    bool anyWireframe = false;
    for (Entity entity : m_SortedRenderList) {
        auto* mr = meshRendererStorage->Get(entity);
        if (mr && mr->wireframe) { anyWireframe = true; break; }
    }
    if (!anyWireframe) return;

    m_WireframeOverlayPipeline->Bind(commandBuffer);
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_WireframeOverlayPipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
    }

    for (Entity entity : m_SortedRenderList) {
        auto* mr = meshRendererStorage->Get(entity);
        if (!mr || !mr->wireframe) continue;

        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!transform || !transform->visible) continue;

        EntityRenderData* pRD = GetRenderData(entity);
        if (!pRD || !pRD->valid) continue;
        EntityRenderData& renderData = *pRD;

        // Build push constants — use wireframe color/opacity from component
        Renderer::PushConstants pc{};
        pc.baseColor = mr->wireframeColor;
        pc.opacity = mr->wireframeOpacity;
        pc.flags = 0;

        // Handle skinned meshes
        auto* animComp = ResolveAnimator(entity);
        if (animComp && renderData.boneBuffer) {
            pc.flags |= (1 << 3); // FLAG_SKINNED
            pc.model = Math::Matrix4::Identity();
            const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
            if (!skinningMatrices.empty()) {
                renderData.boneBuffer->UploadData(skinningMatrices.data(),
                    skinningMatrices.size() * sizeof(Math::Matrix4));
            }
            UpdateBoneDescriptor(renderData.boneBuffer.get());
        } else {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) {
                UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
            }
        }

        vkCmdPushConstants(commandBuffer, m_WireframeOverlayPipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        // Draw
        if (renderData.poolAlloc.valid && m_GeometryPool) {
            if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
            vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                             renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
        } else if (renderData.vertexBuffer && renderData.indexCount > 0) {
            VkBuffer buffers[] = {renderData.vertexBuffer->GetBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
            if (renderData.indexBuffer) {
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            }
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
            m_GeometryPoolBound = false;
        }
    }
}

void RenderSystem::RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                                    u32 firstVertex, const Math::Vector3& color, f32 opacity) {
    if (!m_LinePipeline || !m_Renderer || !vertexBuffer || vertexCount == 0) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    m_LinePipeline->Bind(commandBuffer);

    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_LinePipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
    }

    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(extent.width);
    viewport.height = static_cast<f32>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    pc.baseColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    pc.metallic = 0.0f;
    pc.emissiveColor = color;
    pc.roughness = 1.0f;
    pc.emissiveStrength = 1.0f;
    pc.opacity = opacity;
    pc.alphaCutoff = 0.0f;
    pc.flags = 0;
    pc.parallaxScale = 0.0f;

    vkCmdPushConstants(commandBuffer, m_LinePipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    VkBuffer buffers[] = {vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    vkCmdDraw(commandBuffer, vertexCount, 1, firstVertex, 0);
}

void RenderSystem::RenderGridLines(Renderer::VulkanBuffer* vertexBuffer, u32 vertexCount,
                                    u32 firstVertex, const Math::Vector3& color, f32 opacity,
                                    u32 targetWidth, u32 targetHeight) {
    if (!m_LinePipeline || !m_Renderer || !vertexBuffer || vertexCount == 0) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Use offscreen line pipeline (matches offscreen UNORM render pass)
    auto* linePL = m_OffscreenLinePipeline ? m_OffscreenLinePipeline.get() : m_LinePipeline.get();
    linePL->Bind(commandBuffer);

    // Use offscreen descriptor sets (camera matrices match the editor viewport camera)
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            linePL->GetLayout(), 0, 1, &m_OffscreenDescriptorSets[currentFrame], 1, &zeroOff);
    }

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<f32>(targetWidth);
    viewport.height = static_cast<f32>(targetHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {targetWidth, targetHeight};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    Renderer::PushConstants pc{};
    pc.model = Math::Matrix4::Identity();
    pc.baseColor = Math::Vector3(0.0f, 0.0f, 0.0f);
    pc.metallic = 0.0f;
    pc.emissiveColor = color;
    pc.roughness = 1.0f;
    pc.emissiveStrength = 1.0f;
    pc.opacity = opacity;
    pc.alphaCutoff = 0.0f;
    pc.flags = 0;
    pc.parallaxScale = 0.0f;

    vkCmdPushConstants(commandBuffer, linePL->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

    VkBuffer buffers[] = {vertexBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

    vkCmdDraw(commandBuffer, vertexCount, 1, firstVertex, 0);
}

void RenderSystem::CreateUniformBuffers() {
    constexpr u32 framesInFlight = 2;
    const u32 offscreenCount = framesInFlight * MAX_SPLITSCREEN_VIEWPORTS;

    m_UniformBuffers.resize(framesInFlight);
    m_LightingBuffers.resize(framesInFlight);
    m_MaterialBuffers.resize(framesInFlight);
    m_OffscreenUniformBuffers.resize(offscreenCount);
    m_OffscreenLightingBuffers.resize(offscreenCount);

    for (u32 i = 0; i < framesInFlight; ++i) {
        // View/Projection uniform buffer (model matrix uses push constants now)
        m_UniformBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
        if (!m_UniformBuffers[i]->Create(sizeof(Renderer::UniformBufferObject), Renderer::BufferUsage::Uniform, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create uniform buffer %u", i);
            return;
        }

        // Lighting uniform buffer (multi-light support)
        m_LightingBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
        if (!m_LightingBuffers[i]->Create(sizeof(LightingUBO), Renderer::BufferUsage::Uniform, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create lighting buffer %u", i);
            return;
        }

        // Material SSBO (batched) — query device alignment and allocate for initial capacity
        {
            if (m_MaterialSSBOStride == 0) {
                VkPhysicalDeviceProperties devProps;
                vkGetPhysicalDeviceProperties(m_VulkanRenderer->GetContext()->GetPhysicalDevice(), &devProps);
                u32 minAlign = static_cast<u32>(devProps.limits.minStorageBufferOffsetAlignment);
                if (minAlign == 0) minAlign = 16;
                // Round sizeof(MaterialGPU) up to the required alignment
                m_MaterialSSBOStride = ((static_cast<u32>(sizeof(MaterialGPU)) + minAlign - 1) / minAlign) * minAlign;
                m_MaterialSSBOCapacity = 256;  // Initial capacity (grows as needed)
            }
            usize bufferSize = static_cast<usize>(m_MaterialSSBOStride) * m_MaterialSSBOCapacity;
            m_MaterialBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
            if (!m_MaterialBuffers[i]->Create(bufferSize, Renderer::BufferUsage::Storage, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create material SSBO %u", i);
                return;
            }
        }

        // Offscreen (game view) uniform buffers — one per viewport per frame for splitscreen
        for (u32 v = 0; v < MAX_SPLITSCREEN_VIEWPORTS; ++v) {
            u32 idx = GetOffscreenBufferIndex(i, v);

            m_OffscreenUniformBuffers[idx] = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
            if (!m_OffscreenUniformBuffers[idx]->Create(sizeof(Renderer::UniformBufferObject), Renderer::BufferUsage::Uniform, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create offscreen uniform buffer %u (viewport %u)", i, v);
                return;
            }

            m_OffscreenLightingBuffers[idx] = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
            if (!m_OffscreenLightingBuffers[idx]->Create(sizeof(LightingUBO), Renderer::BufferUsage::Uniform, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create offscreen lighting buffer %u (viewport %u)", i, v);
                return;
            }
        }
    }
}

void RenderSystem::RefreshDescriptorsIfDirty() {
    // No-op now: the shadow descriptor bindings (4/10/11) always bind the real
    // shadow-map views regardless of shadow state, and shadows are enabled/disabled
    // via the LightingUBO `shadowEnabled` flag (set from m_ShadowsEnabled each frame),
    // not by swapping descriptors. Recreating the whole VkDescriptorPool here used to
    // run mid-loop on the first frame (editor "Lit" default vs shadows-on default) and
    // invalidate command buffers that had the old sets bound -- the startup cascade of
    // "commandBuffer not in recording state" validation errors. Just clear the flag.
    m_ShadowDescriptorsDirty = false;
}

void RenderSystem::CreateDescriptorSets() {
    constexpr u32 framesInFlight = 2;
    const u32 offscreenSets = framesInFlight * MAX_SPLITSCREEN_VIEWPORTS;
    const u32 totalSets = framesInFlight + offscreenSets; // main + splitscreen offscreen

    // If a descriptor pool already exists, destroy it before allocating a new one.
    // RefreshDescriptorsIfDirty() can call this every frame (e.g. when the editor
    // toggles shadows for its scene view), and without this cleanup the old pool
    // is leaked AND m_OffscreenDescriptorSets is left dangling pointing into it,
    // which causes use-after-free crashes in the offscreen render path.
    if (m_DescriptorPool != VK_NULL_HANDLE && m_Renderer && m_VulkanRenderer->GetContext()) {
        m_VulkanRenderer->GetContext()->WaitForGPU();
        vkDestroyDescriptorPool(m_VulkanRenderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
        m_DescriptorSets.clear();
        m_OffscreenDescriptorSets.clear();
    }

    // Create descriptor pool — must account for ALL bindings in the layout (24 total)
    // UBOs: bindings 0, 1 = 2
    // Combined image samplers: bindings 3,4,5,6,8,9,10,11,16,17,18,19,21,22,23 = 15
    // SSBOs: bindings 7,12,13,14,15,20 = 6
    // SSBO dynamic: binding 2 = 1
    std::array<VkDescriptorPoolSize, 4> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = totalSets * 2;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = totalSets * 15;  // 12 original + 3 new (21,22,23)
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[2].descriptorCount = totalSets * 6;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    poolSizes[3].descriptorCount = totalSets * 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = totalSets;

    VkResult result = vkCreateDescriptorPool(
        m_VulkanRenderer->GetContext()->GetDevice(), &poolInfo, nullptr, &m_DescriptorPool);
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create descriptor pool: %d", result);
        return;
    }

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_Pipeline->GetDescriptorSetLayout());
    m_DescriptorSets.resize(framesInFlight);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_DescriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    result = vkAllocateDescriptorSets(
        m_VulkanRenderer->GetContext()->GetDevice(), &allocInfo, m_DescriptorSets.data());
    if (result != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate descriptor sets: %d", result);
        return;
    }

    // Update descriptor sets with all UBOs and default texture
    for (u32 i = 0; i < framesInFlight; ++i) {
        std::array<VkDescriptorBufferInfo, 3> bufferInfos{};

        // MVP UBO
        bufferInfos[0].buffer = m_UniformBuffers[i]->GetBuffer();
        bufferInfos[0].offset = 0;
        bufferInfos[0].range = sizeof(Renderer::UniformBufferObject);

        // Lighting UBO (multi-light)
        bufferInfos[1].buffer = m_LightingBuffers[i]->GetBuffer();
        bufferInfos[1].offset = 0;
        bufferInfos[1].range = sizeof(LightingUBO);

        // Material SSBO (dynamic offset — range = one material entry stride)
        bufferInfos[2].buffer = m_MaterialBuffers[i]->GetBuffer();
        bufferInfos[2].offset = 0;
        bufferInfos[2].range = m_MaterialSSBOStride;

        // Default texture (binding 3)
        VkDescriptorImageInfo imageInfo{};
        if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
            imageInfo = m_DefaultWhiteTexture->GetDescriptorInfo();
        } else {
            ENJIN_LOG_ERROR(Renderer, "Default white texture unavailable - descriptor sets will be invalid");
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = VK_NULL_HANDLE;
            imageInfo.sampler = VK_NULL_HANDLE;
        }

        // Shadow map (binding 4) - 2D array for cascaded shadows. Bind the real
        // depth-array view whenever the map exists (it lives for the whole session),
        // matching point/spot bindings 10/11. The shader is sampler2DArrayShadow, so
        // the old white-texture fallback was the wrong type/format/layout entirely
        // (validation 07752/06479/09600). Shadow disable is handled by the lighting
        // UBO flag, not by swapping the descriptor to a color texture.
        VkDescriptorImageInfo shadowImageInfo{};
        if (m_ShadowMap) {
            shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            shadowImageInfo.imageView = m_ShadowMap->GetDepthArrayView();
            shadowImageInfo.sampler = m_ShadowMap->GetShadowSampler();
        } else {
            shadowImageInfo = imageInfo;
        }

        // Height map (binding 5) - default to white texture (no displacement)
        VkDescriptorImageInfo heightImageInfo = imageInfo;

        // Normal map (binding 6) - default to flat normal (white = (0.5,0.5,1) encoded)
        VkDescriptorImageInfo normalMapInfo = imageInfo;

        // Bone matrices SSBO (binding 7) - default identity
        VkDescriptorBufferInfo boneBufferInfo{};
        if (m_DefaultBoneBuffer) {
            boneBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            boneBufferInfo.offset = 0;
            boneBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Metallic-roughness texture (binding 8) - default to white
        VkDescriptorImageInfo metallicRoughnessImageInfo = imageInfo;

        // Emissive texture (binding 9) - default to white
        VkDescriptorImageInfo emissiveImageInfo = imageInfo;

        // Point light shadow cubemap array (binding 10) - default to white
        VkDescriptorImageInfo pointShadowImageInfo{};
        if (m_PointShadowMap) {
            pointShadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            pointShadowImageInfo.imageView = m_PointShadowMap->GetCubeArrayView();
            pointShadowImageInfo.sampler = m_PointShadowMap->GetShadowSampler();
        } else {
            pointShadowImageInfo = imageInfo;
        }

        // Spot light shadow 2D array (binding 11) - default to white
        VkDescriptorImageInfo spotShadowImageInfo{};
        if (m_SpotShadowMap) {
            spotShadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            spotShadowImageInfo.imageView = m_SpotShadowMap->GetArrayView();
            spotShadowImageInfo.sampler = m_SpotShadowMap->GetShadowSampler();
        } else {
            spotShadowImageInfo = imageInfo;
        }

        // Shadow data SSBO (binding 12)
        VkDescriptorBufferInfo shadowDataBufferInfo{};
        if (m_ShadowDataBuffer) {
            shadowDataBufferInfo.buffer = m_ShadowDataBuffer->GetBuffer();
            shadowDataBufferInfo.offset = 0;
            shadowDataBufferInfo.range = m_ShadowDataBuffer->GetSize();
        }

        // ObjectData SSBO (binding 13) - per-object material/transform for indirect draws
        VkDescriptorBufferInfo objectDataBufferInfo{};
        if (m_GPUCulling && m_GPUCulling->GetObjectDataBuffer() != VK_NULL_HANDLE) {
            objectDataBufferInfo.buffer = m_GPUCulling->GetObjectDataBuffer();
            objectDataBufferInfo.offset = 0;
            objectDataBufferInfo.range = VK_WHOLE_SIZE;
        } else if (m_DefaultBoneBuffer) {
            // Fallback: bind any valid SSBO so the descriptor isn't null
            objectDataBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            objectDataBufferInfo.offset = 0;
            objectDataBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Clustered lighting grid SSBO (binding 14) - fallback to default bone buffer
        VkDescriptorBufferInfo clusterGridBufferInfo{};
#ifdef ENJIN_CLUSTERED_LIGHTING
        if (m_ClusteredLighting && m_ClusteredLighting->GetLightGridBuffer() != VK_NULL_HANDLE) {
            clusterGridBufferInfo.buffer = m_ClusteredLighting->GetLightGridBuffer();
            clusterGridBufferInfo.offset = 0;
            clusterGridBufferInfo.range = VK_WHOLE_SIZE;
        } else
#endif
        if (m_DefaultBoneBuffer) {
            clusterGridBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            clusterGridBufferInfo.offset = 0;
            clusterGridBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Clustered lighting index SSBO (binding 15) - fallback to default bone buffer
        VkDescriptorBufferInfo clusterIndexBufferInfo{};
#ifdef ENJIN_CLUSTERED_LIGHTING
        if (m_ClusteredLighting && m_ClusteredLighting->GetLightIndexBuffer() != VK_NULL_HANDLE) {
            clusterIndexBufferInfo.buffer = m_ClusteredLighting->GetLightIndexBuffer();
            clusterIndexBufferInfo.offset = 0;
            clusterIndexBufferInfo.range = VK_WHOLE_SIZE;
        } else
#endif
        if (m_DefaultBoneBuffer) {
            clusterIndexBufferInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
            clusterIndexBufferInfo.offset = 0;
            clusterIndexBufferInfo.range = m_DefaultBoneBuffer->GetSize();
        }

        // Virtual texturing indirection texture (binding 16) - fallback to white
        VkDescriptorImageInfo vtIndirectionImageInfo = imageInfo;
        // Virtual texturing physical atlas (binding 17) - fallback to white
        VkDescriptorImageInfo vtAtlasImageInfo = imageInfo;

        // Matcap texture (binding 18) - fallback to white (procedural matcap used when white)
        VkDescriptorImageInfo matcapImageInfo = imageInfo;

        // Baked reflection probe cubemap (binding 19) - fallback to a 1x1 cube.
        // The shader declares this samplerCube, so the fallback MUST be a CUBE
        // view, not the 2D white texture (else VUID-vkCmdDrawIndexed-viewType-07752).
        // When a probe is baked, this is updated via UpdateProbeCubemapDescriptor().
        VkDescriptorImageInfo probeCubemapImageInfo = imageInfo;
        if (m_DummyCubeImageView != VK_NULL_HANDLE) {
            probeCubemapImageInfo.imageView = m_DummyCubeImageView;
        }

        std::array<VkWriteDescriptorSet, 24> descriptorWrites{};

        // MVP descriptor
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfos[0];

        // Lighting descriptor
        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_DescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &bufferInfos[1];

        // Material SSBO descriptor (dynamic offset)
        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = m_DescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].dstArrayElement = 0;
        descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &bufferInfos[2];

        // Base color texture descriptor
        descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[3].dstSet = m_DescriptorSets[i];
        descriptorWrites[3].dstBinding = 3;
        descriptorWrites[3].dstArrayElement = 0;
        descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[3].descriptorCount = 1;
        descriptorWrites[3].pImageInfo = &imageInfo;

        // Shadow map descriptor
        descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[4].dstSet = m_DescriptorSets[i];
        descriptorWrites[4].dstBinding = 4;
        descriptorWrites[4].dstArrayElement = 0;
        descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[4].descriptorCount = 1;
        descriptorWrites[4].pImageInfo = &shadowImageInfo;

        // Height map descriptor
        descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[5].dstSet = m_DescriptorSets[i];
        descriptorWrites[5].dstBinding = 5;
        descriptorWrites[5].dstArrayElement = 0;
        descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[5].descriptorCount = 1;
        descriptorWrites[5].pImageInfo = &heightImageInfo;

        // Normal map descriptor
        descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[6].dstSet = m_DescriptorSets[i];
        descriptorWrites[6].dstBinding = 6;
        descriptorWrites[6].dstArrayElement = 0;
        descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[6].descriptorCount = 1;
        descriptorWrites[6].pImageInfo = &normalMapInfo;

        // Bone matrices SSBO descriptor
        descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[7].dstSet = m_DescriptorSets[i];
        descriptorWrites[7].dstBinding = 7;
        descriptorWrites[7].dstArrayElement = 0;
        descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[7].descriptorCount = 1;
        descriptorWrites[7].pBufferInfo = &boneBufferInfo;

        // Metallic-roughness texture descriptor
        descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[8].dstSet = m_DescriptorSets[i];
        descriptorWrites[8].dstBinding = 8;
        descriptorWrites[8].dstArrayElement = 0;
        descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[8].descriptorCount = 1;
        descriptorWrites[8].pImageInfo = &metallicRoughnessImageInfo;

        // Emissive texture descriptor
        descriptorWrites[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[9].dstSet = m_DescriptorSets[i];
        descriptorWrites[9].dstBinding = 9;
        descriptorWrites[9].dstArrayElement = 0;
        descriptorWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[9].descriptorCount = 1;
        descriptorWrites[9].pImageInfo = &emissiveImageInfo;

        // Point light shadow cubemap array descriptor
        descriptorWrites[10].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[10].dstSet = m_DescriptorSets[i];
        descriptorWrites[10].dstBinding = 10;
        descriptorWrites[10].dstArrayElement = 0;
        descriptorWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[10].descriptorCount = 1;
        descriptorWrites[10].pImageInfo = &pointShadowImageInfo;

        // Spot light shadow 2D array descriptor
        descriptorWrites[11].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[11].dstSet = m_DescriptorSets[i];
        descriptorWrites[11].dstBinding = 11;
        descriptorWrites[11].dstArrayElement = 0;
        descriptorWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[11].descriptorCount = 1;
        descriptorWrites[11].pImageInfo = &spotShadowImageInfo;

        // Shadow data SSBO descriptor
        descriptorWrites[12].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[12].dstSet = m_DescriptorSets[i];
        descriptorWrites[12].dstBinding = 12;
        descriptorWrites[12].dstArrayElement = 0;
        descriptorWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[12].descriptorCount = 1;
        descriptorWrites[12].pBufferInfo = &shadowDataBufferInfo;

        // ObjectData SSBO descriptor (binding 13)
        descriptorWrites[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[13].dstSet = m_DescriptorSets[i];
        descriptorWrites[13].dstBinding = 13;
        descriptorWrites[13].dstArrayElement = 0;
        descriptorWrites[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[13].descriptorCount = 1;
        descriptorWrites[13].pBufferInfo = &objectDataBufferInfo;

        // Cluster grid SSBO descriptor (binding 14)
        descriptorWrites[14].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[14].dstSet = m_DescriptorSets[i];
        descriptorWrites[14].dstBinding = 14;
        descriptorWrites[14].dstArrayElement = 0;
        descriptorWrites[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[14].descriptorCount = 1;
        descriptorWrites[14].pBufferInfo = &clusterGridBufferInfo;

        // Cluster light index SSBO descriptor (binding 15)
        descriptorWrites[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[15].dstSet = m_DescriptorSets[i];
        descriptorWrites[15].dstBinding = 15;
        descriptorWrites[15].dstArrayElement = 0;
        descriptorWrites[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[15].descriptorCount = 1;
        descriptorWrites[15].pBufferInfo = &clusterIndexBufferInfo;

        // VT indirection texture descriptor (binding 16)
        descriptorWrites[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[16].dstSet = m_DescriptorSets[i];
        descriptorWrites[16].dstBinding = 16;
        descriptorWrites[16].dstArrayElement = 0;
        descriptorWrites[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[16].descriptorCount = 1;
        descriptorWrites[16].pImageInfo = &vtIndirectionImageInfo;

        // VT physical atlas descriptor (binding 17)
        descriptorWrites[17].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[17].dstSet = m_DescriptorSets[i];
        descriptorWrites[17].dstBinding = 17;
        descriptorWrites[17].dstArrayElement = 0;
        descriptorWrites[17].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[17].descriptorCount = 1;
        descriptorWrites[17].pImageInfo = &vtAtlasImageInfo;

        // Matcap texture descriptor (binding 18)
        descriptorWrites[18].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[18].dstSet = m_DescriptorSets[i];
        descriptorWrites[18].dstBinding = 18;
        descriptorWrites[18].dstArrayElement = 0;
        descriptorWrites[18].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[18].descriptorCount = 1;
        descriptorWrites[18].pImageInfo = &matcapImageInfo;

        // Baked reflection probe cubemap descriptor (binding 19)
        descriptorWrites[19].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[19].dstSet = m_DescriptorSets[i];
        descriptorWrites[19].dstBinding = 19;
        descriptorWrites[19].dstArrayElement = 0;
        descriptorWrites[19].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[19].descriptorCount = 1;
        descriptorWrites[19].pImageInfo = &probeCubemapImageInfo;

        // Binding 20: morph target SSBO (default = empty, targetCount=0)
        VkDescriptorBufferInfo morphBufferInfo{};
        if (m_DefaultMorphBuffer) {
            morphBufferInfo.buffer = m_DefaultMorphBuffer->GetBuffer();
            morphBufferInfo.offset = 0;
            morphBufferInfo.range = m_DefaultMorphBuffer->GetSize();
        } else {
            morphBufferInfo = boneBufferInfo; // Fallback to bone buffer as dummy
        }
        descriptorWrites[20].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[20].dstSet = m_DescriptorSets[i];
        descriptorWrites[20].dstBinding = 20;
        descriptorWrites[20].dstArrayElement = 0;
        descriptorWrites[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[20].descriptorCount = 1;
        descriptorWrites[20].pBufferInfo = &morphBufferInfo;

        // Bindings 21-23: DDGI/froxel dummy textures (only written if RT dummy resources exist)
        // RT dummy resources are created in InitializeRayTracing() which runs after CreateDescriptorSets().
        // On first call, these are null — we write only 21 descriptors. Later calls (e.g., resize)
        // have the dummy resources available and write all 24.
        u32 writeCount = 21;
        VkDescriptorImageInfo dummyImageInfo2D{};
        VkDescriptorImageInfo dummyImageInfo3D{};

        if (m_RTDummyImageView && m_RTDummySampler) {
            dummyImageInfo2D.imageView = m_RTDummyImageView;
            dummyImageInfo2D.sampler = m_RTDummySampler;
            dummyImageInfo2D.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            dummyImageInfo3D.imageView = m_RTDummy3DImageView ? m_RTDummy3DImageView : m_RTDummyImageView;
            dummyImageInfo3D.sampler = m_RTDummySampler;
            dummyImageInfo3D.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            descriptorWrites[21].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[21].dstSet = m_DescriptorSets[i];
            descriptorWrites[21].dstBinding = 21;
            descriptorWrites[21].dstArrayElement = 0;
            descriptorWrites[21].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[21].descriptorCount = 1;
            descriptorWrites[21].pImageInfo = &dummyImageInfo2D;

            descriptorWrites[22].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[22].dstSet = m_DescriptorSets[i];
            descriptorWrites[22].dstBinding = 22;
            descriptorWrites[22].dstArrayElement = 0;
            descriptorWrites[22].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[22].descriptorCount = 1;
            descriptorWrites[22].pImageInfo = &dummyImageInfo2D;

            descriptorWrites[23].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[23].dstSet = m_DescriptorSets[i];
            descriptorWrites[23].dstBinding = 23;
            descriptorWrites[23].dstArrayElement = 0;
            descriptorWrites[23].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[23].descriptorCount = 1;
            descriptorWrites[23].pImageInfo = &dummyImageInfo3D;

            writeCount = 24;
        }

        vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(),
            writeCount, descriptorWrites.data(), 0, nullptr);
    }

    // Allocate offscreen descriptor sets (one per viewport per frame for splitscreen)
    {
        std::vector<VkDescriptorSetLayout> offscreenLayouts(offscreenSets, m_Pipeline->GetDescriptorSetLayout());
        m_OffscreenDescriptorSets.resize(offscreenSets);

        VkDescriptorSetAllocateInfo offAllocInfo{};
        offAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        offAllocInfo.descriptorPool = m_DescriptorPool;
        offAllocInfo.descriptorSetCount = offscreenSets;
        offAllocInfo.pSetLayouts = offscreenLayouts.data();

        result = vkAllocateDescriptorSets(
            m_VulkanRenderer->GetContext()->GetDevice(), &offAllocInfo, m_OffscreenDescriptorSets.data());
        if (result != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate offscreen descriptor sets: %d", result);
            return;
        }

        for (u32 i = 0; i < framesInFlight; ++i) {
            for (u32 v = 0; v < MAX_SPLITSCREEN_VIEWPORTS; ++v) {
                u32 idx = GetOffscreenBufferIndex(i, v);

                std::array<VkDescriptorBufferInfo, 3> offBufInfos{};
                offBufInfos[0].buffer = m_OffscreenUniformBuffers[idx]->GetBuffer();
                offBufInfos[0].offset = 0;
                offBufInfos[0].range = sizeof(Renderer::UniformBufferObject);
                offBufInfos[1].buffer = m_OffscreenLightingBuffers[idx]->GetBuffer();
                offBufInfos[1].offset = 0;
                offBufInfos[1].range = sizeof(LightingUBO);
                // Share the material SSBO — batched per-frame, indexed via dynamic offset
                offBufInfos[2].buffer = m_MaterialBuffers[i]->GetBuffer();
                offBufInfos[2].offset = 0;
                offBufInfos[2].range = m_MaterialSSBOStride;

                VkDescriptorImageInfo offImageInfo{};
                if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                    offImageInfo = m_DefaultWhiteTexture->GetDescriptorInfo();
                } else {
                    offImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    offImageInfo.imageView = VK_NULL_HANDLE;
                    offImageInfo.sampler = VK_NULL_HANDLE;
                }
                VkDescriptorImageInfo offShadowInfo{};
                if (m_ShadowMap) {
                    offShadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    offShadowInfo.imageView = m_ShadowMap->GetDepthArrayView();
                    offShadowInfo.sampler = m_ShadowMap->GetShadowSampler();
                } else {
                    offShadowInfo = offImageInfo;
                }
                VkDescriptorImageInfo offHeightInfo = offImageInfo;
                VkDescriptorImageInfo offNormalInfo = offImageInfo;
                VkDescriptorImageInfo offMetRoughInfo = offImageInfo;
                VkDescriptorImageInfo offEmissiveInfo = offImageInfo;

                VkDescriptorBufferInfo offBoneInfo{};
                if (m_DefaultBoneBuffer) {
                    offBoneInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offBoneInfo.offset = 0;
                    offBoneInfo.range = m_DefaultBoneBuffer->GetSize();
                }

                VkDescriptorImageInfo offPointShadowInfo{};
                if (m_PointShadowMap) {
                    offPointShadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    offPointShadowInfo.imageView = m_PointShadowMap->GetCubeArrayView();
                    offPointShadowInfo.sampler = m_PointShadowMap->GetShadowSampler();
                } else {
                    offPointShadowInfo = offImageInfo;
                }
                VkDescriptorImageInfo offSpotShadowInfo{};
                if (m_SpotShadowMap) {
                    offSpotShadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                    offSpotShadowInfo.imageView = m_SpotShadowMap->GetArrayView();
                    offSpotShadowInfo.sampler = m_SpotShadowMap->GetShadowSampler();
                } else {
                    offSpotShadowInfo = offImageInfo;
                }
                VkDescriptorBufferInfo offShadowDataInfo{};
                if (m_ShadowDataBuffer) {
                    offShadowDataInfo.buffer = m_ShadowDataBuffer->GetBuffer();
                    offShadowDataInfo.offset = 0;
                    offShadowDataInfo.range = m_ShadowDataBuffer->GetSize();
                }

                VkDescriptorBufferInfo offObjectDataInfo{};
                if (m_GPUCulling && m_GPUCulling->GetObjectDataBuffer() != VK_NULL_HANDLE) {
                    offObjectDataInfo.buffer = m_GPUCulling->GetObjectDataBuffer();
                    offObjectDataInfo.offset = 0;
                    offObjectDataInfo.range = VK_WHOLE_SIZE;
                } else if (m_DefaultBoneBuffer) {
                    offObjectDataInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offObjectDataInfo.offset = 0;
                    offObjectDataInfo.range = m_DefaultBoneBuffer->GetSize();
                }

                // Offscreen cluster/VT fallback data
                VkDescriptorBufferInfo offClusterGridInfo{};
                VkDescriptorBufferInfo offClusterIdxInfo{};
#ifdef ENJIN_CLUSTERED_LIGHTING
                if (m_ClusteredLighting && m_ClusteredLighting->GetLightGridBuffer() != VK_NULL_HANDLE) {
                    offClusterGridInfo.buffer = m_ClusteredLighting->GetLightGridBuffer();
                    offClusterGridInfo.range = VK_WHOLE_SIZE;
                    offClusterIdxInfo.buffer = m_ClusteredLighting->GetLightIndexBuffer();
                    offClusterIdxInfo.range = VK_WHOLE_SIZE;
                } else
#endif
                if (m_DefaultBoneBuffer) {
                    offClusterGridInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offClusterGridInfo.range = m_DefaultBoneBuffer->GetSize();
                    offClusterIdxInfo.buffer = m_DefaultBoneBuffer->GetBuffer();
                    offClusterIdxInfo.range = m_DefaultBoneBuffer->GetSize();
                }
                VkDescriptorImageInfo offVtIndInfo = offImageInfo;
                VkDescriptorImageInfo offVtAtlasInfo = offImageInfo;
                VkDescriptorImageInfo offMatcapInfo = offImageInfo;
                // Binding 19 is samplerCube: the fallback MUST be a cube view, not
                // the 2D white texture (else VUID-vkCmdDrawIndexed-viewType-07752).
                VkDescriptorImageInfo offProbeCubemapInfo = offImageInfo;
                if (m_DummyCubeImageView != VK_NULL_HANDLE) {
                    offProbeCubemapInfo.imageView = m_DummyCubeImageView;
                }

                // Bindings 21-23 (DDGI/froxel) are statically sampled by the PBR
                // shader; if left unwritten the offscreen pass trips
                // VUID-vkCmdDrawIndexed-None-08114. Fill them with the RT dummies.
                const bool offHaveDummies = (m_RTDummyImageView != VK_NULL_HANDLE && m_RTDummySampler != VK_NULL_HANDLE);
                VkDescriptorImageInfo offDummy2D{};
                VkDescriptorImageInfo offDummy3D{};
                if (offHaveDummies) {
                    offDummy2D.imageView = m_RTDummyImageView;
                    offDummy2D.sampler = m_RTDummySampler;
                    offDummy2D.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    offDummy3D.imageView = m_RTDummy3DImageView ? m_RTDummy3DImageView : m_RTDummyImageView;
                    offDummy3D.sampler = m_RTDummySampler;
                    offDummy3D.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }

                std::array<VkWriteDescriptorSet, 24> offWrites{};
                const u32 offWriteCount = offHaveDummies ? 24u : 21u;
                for (u32 w = 0; w < offWriteCount; ++w) {
                    offWrites[w].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    offWrites[w].dstSet = m_OffscreenDescriptorSets[idx];
                    offWrites[w].dstBinding = w;
                    offWrites[w].dstArrayElement = 0;
                    offWrites[w].descriptorCount = 1;
                }
                offWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                offWrites[0].pBufferInfo = &offBufInfos[0];
                offWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                offWrites[1].pBufferInfo = &offBufInfos[1];
                offWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                offWrites[2].pBufferInfo = &offBufInfos[2];
                offWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[3].pImageInfo = &offImageInfo;
                offWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[4].pImageInfo = &offShadowInfo;
                offWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[5].pImageInfo = &offHeightInfo;
                offWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[6].pImageInfo = &offNormalInfo;
                offWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[7].pBufferInfo = &offBoneInfo;
                offWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[8].pImageInfo = &offMetRoughInfo;
                offWrites[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[9].pImageInfo = &offEmissiveInfo;
                offWrites[10].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[10].pImageInfo = &offPointShadowInfo;
                offWrites[11].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[11].pImageInfo = &offSpotShadowInfo;
                offWrites[12].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[12].pBufferInfo = &offShadowDataInfo;
                offWrites[13].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[13].pBufferInfo = &offObjectDataInfo;
                offWrites[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[14].pBufferInfo = &offClusterGridInfo;
                offWrites[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[15].pBufferInfo = &offClusterIdxInfo;
                offWrites[16].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[16].pImageInfo = &offVtIndInfo;
                offWrites[17].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[17].pImageInfo = &offVtAtlasInfo;
                offWrites[18].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[18].pImageInfo = &offMatcapInfo;
                offWrites[19].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                offWrites[19].pImageInfo = &offProbeCubemapInfo;

                // Binding 20: morph target SSBO
                VkDescriptorBufferInfo offMorphInfo{};
                if (m_DefaultMorphBuffer) {
                    offMorphInfo.buffer = m_DefaultMorphBuffer->GetBuffer();
                    offMorphInfo.range = m_DefaultMorphBuffer->GetSize();
                } else {
                    offMorphInfo = offBoneInfo;
                }
                offWrites[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                offWrites[20].pBufferInfo = &offMorphInfo;

                // Bindings 21-23: DDGI irradiance (2D), froxel/scatter (2D), volume (3D).
                if (offHaveDummies) {
                    offWrites[21].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    offWrites[21].pImageInfo = &offDummy2D;
                    offWrites[22].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    offWrites[22].pImageInfo = &offDummy2D;
                    offWrites[23].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    offWrites[23].pImageInfo = &offDummy3D;
                }

                vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(),
                    offWriteCount, offWrites.data(), 0, nullptr);
            }
        }
    }
}

// The geometry pool packs vertices at MergedGeometryBuffer::VERTEX_STRIDE and the
// graphics pipeline reads them back at sizeof(MeshComponent::Vertex). If these ever
// drift, pooled (static) meshes render as scattered garbage. Keep them locked.
static_assert(Renderer::MergedGeometryBuffer::VERTEX_STRIDE == sizeof(MeshComponent::Vertex),
              "GeometryPool vertex stride must match MeshComponent::Vertex size");

AnimatorComponent* RenderSystem::ResolveAnimator(Entity entity) {
    // 1. The entity's own animator (a single-mesh skinned model, or the leader mesh).
    AnimatorComponent* own = m_CachedAnimatorStorage ? m_CachedAnimatorStorage->Get(entity) : nullptr;
    if (own) return own;
    // 2. Follower mesh: resolve the animator driving its SHARED skeleton, so every mesh in
    //    one imported model skins from a single clock (fixes pause desync + slow drift between
    //    co-skeleton meshes like a body + its joints/clothing).
    if (SkeletonComponent* sk = m_World->GetComponent<SkeletonComponent>(entity)) {
        if (sk->skeleton) {
            auto it = m_SkeletonToAnimator.find(sk->skeleton.get());
            if (it != m_SkeletonToAnimator.end()) return it->second;
        }
    }
    // Non-skinned entity (or no driving animator found). Callers that want the legacy
    // orphan fallback (m_CachedFallbackAnimator) apply it themselves.
    return nullptr;
}

bool RenderSystem::IsPoolEligible(Entity entity) const {
    if (!m_GeometryPool) return false;
    // Dynamic meshes: terrain, jelly, sprites, tilemaps, water — keep per-entity buffers
    if (m_World->HasComponent<Sprite2DComponent>(entity)) return false;
    if (m_World->HasComponent<TilemapComponent>(entity)) return false;
    if (m_World->HasComponent<TerrainComponent>(entity)) return false;
    if (m_World->HasComponent<Terrain2DComponent>(entity)) return false;
    if (m_World->HasComponent<JellyMeshComponent>(entity)) return false;
    if (m_World->HasComponent<WaterVolumeComponent>(entity)) return false;
    if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) return false;
    // Skinned meshes stay per-entity (bone deformation updates vertex data)
    if (m_World->HasComponent<AnimatorComponent>(entity)) return false;
    return true;
}

EntityRenderData* RenderSystem::SetupEntityBuffers(Entity entity) {
    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);
    if (!mesh || !mesh->IsValid()) {
        return nullptr;
    }

    // Ensure dense vector is large enough for this entity ID
    if (static_cast<usize>(EntityIndex(entity)) >= m_EntityRenderData.size()) {
        m_EntityRenderData.resize(static_cast<usize>(EntityIndex(entity)) + 1);
    }
    EntityRenderData& renderData = m_EntityRenderData[static_cast<usize>(EntityIndex(entity))];
    renderData.Invalidate();
    renderData.valid = true;
    renderData.owner = entity;  // full generational handle — see EntityRenderData::owner

    // Try merged geometry pool for static 3D meshes
    if (IsPoolEligible(entity)) {
        auto alloc = m_GeometryPool->Upload(
            mesh->vertices.data(),
            static_cast<u32>(mesh->vertices.size()),
            mesh->indices.data(),
            static_cast<u32>(mesh->indices.size()));
        if (alloc.valid) {
            renderData.poolAlloc = alloc;
            renderData.indexCount = alloc.indexCount;
            // No per-entity VB/IB needed — pool owns the memory
            return &renderData;
        }
        // Pool allocation failed (overflow) — fall through to per-entity buffers
    }

    // Per-entity buffers (dynamic meshes, pool overflow fallback)
    // Add ShaderDeviceAddress usage when RT is supported for BLAS building
    VkBufferUsageFlags vertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkBufferUsageFlags indexUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    if (IsRayTracingSupported()) {
        vertexUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                       VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
        indexUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    }

    usize vertexBufferSize = mesh->vertices.size() * sizeof(MeshComponent::Vertex);
    renderData.vertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
    if (!renderData.vertexBuffer->Create(vertexBufferSize, vertexUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create vertex buffer for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }

    if (!renderData.vertexBuffer->UploadData(mesh->vertices.data(), vertexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload vertex data for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }
    renderData.vertexCount = static_cast<u32>(mesh->vertices.size());  // for compute skinning (ADR-0002)

    // Create index buffer
    usize indexBufferSize = mesh->indices.size() * sizeof(u32);
    renderData.indexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
    if (!renderData.indexBuffer->Create(indexBufferSize, indexUsage, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create index buffer for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }

    if (!renderData.indexBuffer->UploadData(mesh->indices.data(), indexBufferSize)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to upload index data for entity %llu", entity);
        renderData.Invalidate();
        return nullptr;
    }

    renderData.indexCount = static_cast<u32>(mesh->indices.size());

    // Create bone SSBO if entity has a skeleton for animation.
    // The AnimatorComponent may be on this entity or on a sibling (Mixamo FBX
    // puts mesh and skeleton on different entities). Search globally if needed.
    AnimatorComponent* animComp = ResolveAnimator(entity);
    if (!animComp) {
        // Check if this mesh has bone weights — if so, find any AnimatorComponent
        bool hasBoneWeights = false;
        for (const auto& v : mesh->vertices) {
            if (v.boneWeights.x > 0.0f) { hasBoneWeights = true; break; }
        }
        ENJIN_LOG_INFO(Renderer, "BONE CHECK: entity %llu, %zu verts, hasBoneWeights=%d",
            (unsigned long long)entity, mesh->vertices.size(), hasBoneWeights);
        if (hasBoneWeights) {
            for (auto animEntity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
                auto* ac = m_World->GetComponent<AnimatorComponent>(animEntity);
                if (ac && ac->animator.GetSkeleton()) {
                    animComp = ac;
                    break;
                }
            }
        }
    }
    if (animComp && animComp->animator.GetSkeleton()) {
        usize boneCount = animComp->animator.GetSkeleton()->bones.size();
        if (boneCount > 0) {
            usize boneBufferSize = boneCount * sizeof(Math::Matrix4);
            renderData.boneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
            if (!renderData.boneBuffer->Create(boneBufferSize, Renderer::BufferUsage::Storage, true)) {
                ENJIN_LOG_ERROR(Renderer, "Failed to create bone buffer for entity %llu", entity);
                renderData.boneBuffer.reset();
            } else {
                // Initialize bone buffer with identity matrices so bind-pose
                // skinning is a no-op. Without this, the buffer contains garbage
                // until the first animation frame uploads real matrices.
                std::vector<Math::Matrix4> identityMatrices(boneCount, Math::Matrix4::Identity());
                renderData.boneBuffer->UploadData(identityMatrices.data(), boneBufferSize);
            }
        }
    }

    return &renderData;
}

void RenderSystem::UpdateProbeCubemapDescriptor() {
    if (!m_ReflectionProbes || !m_ReflectionProbes->HasActiveBakedCubemap()) return;
    if (!m_Renderer || !m_Pipeline) return;

    VkDescriptorImageInfo cubemapInfo = m_ReflectionProbes->GetActiveBakedCubemapDescriptor();
    if (cubemapInfo.imageView == VK_NULL_HANDLE) return;

    VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();

    // Update binding 19 in all active descriptor sets (main + offscreen)
    // Main pass descriptor sets
    for (auto& descSet : m_DescriptorSets) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSet;
        write.dstBinding = 19;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &cubemapInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Offscreen descriptor sets
    for (auto& descSet : m_OffscreenDescriptorSets) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descSet;
        write.dstBinding = 19;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &cubemapInfo;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
}

void RenderSystem::UpdateFrameUniforms() {
    if (!m_Camera) {
        // No camera set — frame uniforms not uploaded. Scene will render with
        // stale view/projection matrices. This is expected briefly during
        // initialization or scene transitions.
        return;
    }

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Update View/Projection UBO (shared across all objects)
    Renderer::UniformBufferObject ubo{};
    ubo.view = m_Camera->GetViewMatrix();
    ubo.proj = m_Camera->GetProjectionMatrix();
    ubo.prevViewProj = m_PrevViewProj;

    // TAA / Upscaler jitter injection: apply sub-pixel Halton offset to the projection
    // matrix so each frame samples a slightly different sub-pixel position. Both TAA
    // and temporal upscalers (FSR 2, DLSS, XeSS) require jittered input.
    if (m_AAMode == 2 || m_UpscalerType > 0) { // TAA or temporal upscaler
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        // When an upscaler is active, compute jitter relative to the lower render resolution
        // so that sub-pixel offsets are correctly sized for the internal rendering target.
        u32 jitterW = extent.width;
        u32 jitterH = extent.height;
        if (m_UpscalerType > 0 && m_Upscaler) {
            Renderer::IUpscaler::GetRenderResolution(
                extent.width, extent.height,
                static_cast<Renderer::UpscalerQuality>(m_UpscalerQuality),
                jitterW, jitterH);
        }
        if (jitterW > 0 && jitterH > 0) {
            Math::Vector2 jitter = Renderer::HaltonJitter(m_TAAFrameCounter, jitterW, jitterH);
            // Offset the projection matrix: translation in clip space X/Y
            ubo.proj.m[8]  += jitter.x;  // m[2][0] in column-major
            ubo.proj.m[9]  += jitter.y;  // m[2][1] in column-major
            ubo.jitterOffset = Math::Vector4(jitter.x, jitter.y, m_PrevJitter.x, m_PrevJitter.y);
            m_PrevJitter = jitter;
        } else {
            ubo.jitterOffset = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }
        m_TAAFrameCounter++;
    } else {
        ubo.jitterOffset = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        m_PrevJitter = Math::Vector2(0.0f, 0.0f);
        m_TAAFrameCounter = 0;
    }

    (*m_ActiveUniformBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&ubo, sizeof(ubo));

    // Store current viewProj for next frame's velocity computation
    m_PrevViewProj = ubo.proj * ubo.view;

    // Update Lighting UBO with all lights in the scene
    LightingUBO lighting{};
    lighting.ambientColor = m_AmbientColor;
    lighting.ambientIntensity = m_AmbientIntensity;
    lighting.cameraPosition = m_Camera->GetPosition();
    lighting._pad0 = 0.0f;
    lighting.directionalLightCount = 0;
    lighting.pointLightCount = 0;
    lighting.spotLightCount = 0;
    lighting._pad1 = 0;

    // Scene2D fast path: skip light iteration, shadow data, wind — no consumers
    if (m_SceneComposition.mode == SceneRenderMode::Scene2D) {
        lighting.directionalLightCount = 0;
        lighting.pointLightCount = 0;
        lighting.spotLightCount = 0;
        lighting.shadowEnabled = 0;
        lighting.pointShadowCount = 0;
        lighting.spotShadowCount = 0;
        lighting.fogParams = Math::Vector4(m_FogDensity, m_FogStart, m_FogEnd, m_FogHeightFalloff);
        lighting.fogColorSnow = Math::Vector4(m_FogColor.x, m_FogColor.y, m_FogColor.z, m_SnowIntensity);
        (*m_ActiveLightingBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&lighting, sizeof(lighting));
        m_CachedLightingData = lighting;
        return;
    }

    // Editor "Solid" view mode: render with flat ambient only, no lights
    if (m_EditorUnlit) {
        lighting.ambientColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        lighting.ambientIntensity = 1.0f;
        lighting.directionalLightCount = 0;
        lighting.pointLightCount = 0;
        lighting.spotLightCount = 0;
        lighting.shadowEnabled = 0;
        lighting.fogParams = Math::Vector4(0, 0, 0, 0); // No fog in solid view
        (*m_ActiveLightingBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&lighting, sizeof(lighting));
        m_CachedLightingData = lighting;
        return;
    }

    bool hasAnyLight = false;

    auto* lightStorageFU = m_World->GetComponentStorage<LightComponent>();
    for (Entity lightEntity : m_CachedLightEntities) {
        LightComponent* light = lightStorageFU ? lightStorageFU->Get(lightEntity) : nullptr;
        TransformComponent* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(lightEntity) : nullptr;
        if (!light) continue;

        hasAnyLight = true;

        switch (light->type) {
            case LightType::Directional: {
                if (lighting.directionalLightCount < MAX_DIRECTIONAL_LIGHTS) {
                    auto& dirLight = lighting.directionalLights[lighting.directionalLightCount];
                    if (lightTransform) {
                        Math::Vector3 forward(0.0f, 0.0f, -1.0f);
                        dirLight.direction = lightTransform->rotation.Rotate(forward).Normalized();
                    } else {
                        dirLight.direction = Math::Vector3(-0.5f, -0.8f, -0.3f).Normalized();
                    }
                    dirLight.color = light->color;
                    dirLight.intensity = light->intensity;
                    lighting.directionalLightCount++;
                }
                break;
            }
            case LightType::Point: {
                if (lighting.pointLightCount < MAX_POINT_LIGHTS) {
                    auto& pointLight = lighting.pointLights[lighting.pointLightCount];
                    pointLight.position = lightTransform ? lightTransform->position : Math::Vector3(0.0f);
                    pointLight.range = light->range;
                    pointLight.color = light->color;
                    pointLight.intensity = light->intensity;
                    pointLight.constantAttenuation = light->constantAttenuation;
                    pointLight.linearAttenuation = light->linearAttenuation;
                    pointLight.quadraticAttenuation = light->quadraticAttenuation;
                    lighting.pointLightCount++;
                }
                break;
            }
            case LightType::Spot: {
                if (lighting.spotLightCount < MAX_SPOT_LIGHTS) {
                    auto& spotLight = lighting.spotLights[lighting.spotLightCount];
                    spotLight.position = lightTransform ? lightTransform->position : Math::Vector3(0.0f);
                    spotLight.range = light->range;
                    if (lightTransform) {
                        Math::Vector3 forward(0.0f, 0.0f, -1.0f);
                        spotLight.direction = lightTransform->rotation.Rotate(forward).Normalized();
                    } else {
                        spotLight.direction = Math::Vector3(0.0f, -1.0f, 0.0f);
                    }
                    spotLight.color = light->color;
                    spotLight.intensity = light->intensity;
                    spotLight.innerCutoff = std::cos(light->innerConeAngle * 3.14159265f / 180.0f);
                    spotLight.outerCutoff = std::cos(light->outerConeAngle * 3.14159265f / 180.0f);
                    spotLight.constantAttenuation = light->constantAttenuation;
                    spotLight.linearAttenuation = light->linearAttenuation;
                    spotLight.quadraticAttenuation = light->quadraticAttenuation;
                    lighting.spotLightCount++;
                }
                break;
            }
        }
    }

    // Inject transient point lights (fire, muzzle flashes, spells) supplied by
    // gameplay systems this frame. Same UBO path as LightComponent point lights,
    // so they light surfaces here and participating media via clustered lighting.
    for (const auto& tpl : m_TransientPointLights) {
        if (lighting.pointLightCount >= MAX_POINT_LIGHTS) break;
        auto& pointLight = lighting.pointLights[lighting.pointLightCount];
        pointLight.position = tpl.position;
        pointLight.range = tpl.range;
        pointLight.color = tpl.color;
        pointLight.intensity = tpl.intensity;
        pointLight.constantAttenuation = 1.0f;
        pointLight.linearAttenuation = 0.09f;
        pointLight.quadraticAttenuation = 0.032f;
        lighting.pointLightCount++;
        hasAnyLight = true;
    }

    if (!hasAnyLight) {
        lighting.directionalLights[0].direction = Math::Vector3(-0.5f, -0.8f, -0.3f).Normalized();
        lighting.directionalLights[0].color = Math::Vector3(1.0f, 0.95f, 0.9f);
        lighting.directionalLights[0].intensity = 1.2f;
        lighting.directionalLightCount = 1;
    }

    if (m_ShadowsEnabled && m_ShadowMap) {
        for (u32 i = 0; i < Renderer::MAX_SHADOW_CASCADES; ++i) {
            lighting.cascadeViewProj[i] = m_ShadowMap->GetCascadeViewProj(i);
        }
        lighting.cascadeSplits = Math::Vector4(
            m_ShadowMap->GetCascadeSplit(0),
            m_ShadowMap->GetCascadeSplit(1),
            m_ShadowMap->GetCascadeSplit(2),
            m_ShadowMap->GetCascadeSplit(3));
        lighting.shadowSoftness = m_ShadowMap->GetShadowSoftness();
        lighting.shadowEnabled = 1;
        lighting.shadowStrength = m_ShadowMap->GetShadowStrength();
        lighting.shadowMaxDistance = m_ShadowDistance;
    } else {
        for (u32 i = 0; i < Renderer::MAX_SHADOW_CASCADES; ++i) {
            lighting.cascadeViewProj[i] = Math::Matrix4::Identity();
        }
        lighting.cascadeSplits = Math::Vector4(25.0f, 50.0f, 75.0f, 100.0f);
        lighting.shadowSoftness = 0.0f;
        lighting.shadowEnabled = 0;
        lighting.shadowStrength = 1.0f;
        lighting.shadowMaxDistance = 100.0f;
    }

    // Sort shadow-casting point/spot lights to front of UBO arrays
    // so indices 0..N-1 correspond to shadow slots 0..N-1
    lighting.pointShadowCount = static_cast<i32>(m_ActivePointShadowCount);
    lighting.spotShadowCount = static_cast<i32>(m_ActiveSpotShadowCount);
    lighting.celDiffuseBands = m_CelShadingEnabled ? m_CelDiffuseBands : 0.0f;
    lighting.celSpecularCutoff = m_CelShadingEnabled ? m_CelSpecularCutoff : 0.0f;

    // Pack shading model flags: bit0=GGX, bit1=Fresnel, bit2=EnergyConserv, bit3=GeometryTerm, bit4=SphereEnvMap
    lighting.shadingFlags = (m_ShadingModel & 1u)
        | (m_FresnelEnabled ? 2u : 0u)
        | (m_EnergyConservation ? 4u : 0u)
        | (m_GeometryTerm ? 8u : 0u)
        | (m_SphereEnvMapEnabled ? 16u : 0u)
        | (m_HalfLambert ? 32u : 0u);
    lighting.sphereEnvStrength = m_SphereEnvStrength;
    lighting.posterizeLevels = m_PosterizeLevels;
    lighting.texturePageSize = m_TexturePageSize;

    // Pack retro settings into worldCurvature reserved fields
    lighting.worldCurvature.y = m_DepthSortJitter;
    lighting.worldCurvature.z = m_NormalQuantizeSteps;
    lighting.worldCurvature.w = m_CelShadowMode;

    if (m_ActivePointShadowCount > 0 && lighting.pointLightCount > 1) {
        // Move shadow-casting lights to front: swap with non-shadow lights
        for (u32 s = 0; s < m_ActivePointShadowCount && s < lighting.pointLightCount; ++s) {
            auto& shadowLight = m_ShadowPointLights[s];
            // Find this light in the UBO array by matching position
            for (u32 j = s; j < lighting.pointLightCount; ++j) {
                auto& uboLight = lighting.pointLights[j];
                f32 dx = uboLight.position.x - shadowLight.position.x;
                f32 dy = uboLight.position.y - shadowLight.position.y;
                f32 dz = uboLight.position.z - shadowLight.position.z;
                if (dx*dx + dy*dy + dz*dz < 0.001f) {
                    if (j != s) {
                        std::swap(lighting.pointLights[s], lighting.pointLights[j]);
                    }
                    break;
                }
            }
        }
    }

    if (m_ActiveSpotShadowCount > 0 && lighting.spotLightCount > 1) {
        for (u32 s = 0; s < m_ActiveSpotShadowCount && s < lighting.spotLightCount; ++s) {
            auto& shadowLight = m_ShadowSpotLights[s];
            for (u32 j = s; j < lighting.spotLightCount; ++j) {
                auto& uboLight = lighting.spotLights[j];
                f32 dx = uboLight.position.x - shadowLight.position.x;
                f32 dy = uboLight.position.y - shadowLight.position.y;
                f32 dz = uboLight.position.z - shadowLight.position.z;
                if (dx*dx + dy*dy + dz*dz < 0.001f) {
                    if (j != s) {
                        std::swap(lighting.spotLights[s], lighting.spotLights[j]);
                    }
                    break;
                }
            }
        }
    }

    if (m_WindSystem) {
        lighting.windData = m_WindSystem->GetWindVector();
    } else {
        lighting.windData = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    lighting.fogParams = Math::Vector4(m_FogDensity, m_FogStart, m_FogEnd, m_FogHeightFalloff);
    lighting.fogColorSnow = Math::Vector4(m_FogColor.x, m_FogColor.y, m_FogColor.z, m_SnowIntensity);

    // Look up cached player entity position for vegetation stepping (O(1) instead of linear scan)
    lighting.playerPosition = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    if (m_CachedPlayerEntity != INVALID_ENTITY) {
        auto* transform = m_World->GetComponent<TransformComponent>(m_CachedPlayerEntity);
        if (transform) {
            lighting.playerPosition = Math::Vector4(
                transform->position.x, transform->position.y, transform->position.z,
                1.5f);  // w = step radius
        }
    }

    // World curvature
    lighting.worldCurvature = Math::Vector4(m_WorldCurvature, 0.0f, 0.0f, 0.0f);

    // Sky reflection color for water/ice fresnel
    {
        const auto& skyConfig = m_Skybox.GetConfig();
        Math::Vector3 skyCol(0.4f, 0.5f, 0.7f); // fallback matches old hardcoded value
        if (skyConfig.type == Renderer::SkyboxType::Procedural) {
            skyCol = Math::Vector3(
                skyConfig.horizonColor.x * 0.6f + skyConfig.topColor.x * 0.4f,
                skyConfig.horizonColor.y * 0.6f + skyConfig.topColor.y * 0.4f,
                skyConfig.horizonColor.z * 0.6f + skyConfig.topColor.z * 0.4f);
        } else if (skyConfig.type == Renderer::SkyboxType::SolidColor) {
            skyCol = skyConfig.solidColor;
        }
        lighting.skyReflectColor = Math::Vector4(skyCol.x, skyCol.y, skyCol.z, m_LightRampMode);
    }

    // Query SH light probe irradiance at camera position
    if (m_SHLighting && m_SceneComposition.mode == SceneRenderMode::Scene3D && m_Camera) {
        auto irr = m_SHLighting->GetIrradiance(m_Camera->GetPosition(), Math::Vector3(0.0f, 1.0f, 0.0f));
        lighting.shProbeIrradiance = Math::Vector4(irr.x, irr.y, irr.z, 1.0f);
    } else {
        lighting.shProbeIrradiance = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Query reflection probe at camera position for box-projected reflections
    if (m_ReflectionProbes && m_SceneComposition.mode == SceneRenderMode::Scene3D && m_Camera) {
        auto probe = m_ReflectionProbes->FindNearestProbe(m_World, m_Camera->GetPosition());
        if (probe.intensity > 0.0f) {
            lighting.reflectionProbePosition = Math::Vector4(
                probe.probePosition.x, probe.probePosition.y, probe.probePosition.z, probe.intensity);
            lighting.reflectionProbeBoxMin = Math::Vector4(
                probe.boxMin.x, probe.boxMin.y, probe.boxMin.z, probe.blendDistance);
            // w = isBaked (1.0 = baked cubemap at binding 19, 0.0 = skybox gradient fallback)
            lighting.reflectionProbeBoxMax = Math::Vector4(
                probe.boxMax.x, probe.boxMax.y, probe.boxMax.z, probe.isBaked);

            // When the active probe has a baked cubemap, update the descriptor for binding 19
            if (probe.isBaked > 0.5f && m_ReflectionProbes->HasActiveBakedCubemap()) {
                UpdateProbeCubemapDescriptor();
            }
        } else {
            lighting.reflectionProbePosition = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            lighting.reflectionProbeBoxMin = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
            lighting.reflectionProbeBoxMax = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }
    } else {
        lighting.reflectionProbePosition = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        lighting.reflectionProbeBoxMin = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        lighting.reflectionProbeBoxMax = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    (*m_ActiveLightingBuffers)[GetActiveBufferIndex(currentFrame)]->UploadData(&lighting, sizeof(lighting));

    // Cache lighting data for RT/path tracer NEE access
    m_CachedLightingData = lighting;

    // Upload shadow data SSBO for point/spot light shadows
    if (m_ShadowDataBuffer && m_ShadowsEnabled) {
        ShadowDataSSBO shadowData{};
        shadowData.pointShadowCount = static_cast<i32>(m_ActivePointShadowCount);
        shadowData.spotShadowCount = static_cast<i32>(m_ActiveSpotShadowCount);

        for (u32 i = 0; i < m_ActivePointShadowCount; ++i) {
            auto& sl = m_ShadowPointLights[i];
            shadowData.pointLightParams[i] = Math::Vector4(sl.position.x, sl.position.y, sl.position.z, sl.range);
            for (u32 f = 0; f < 6; ++f) {
                shadowData.pointFaceViewProj[i * 6 + f] =
                    Renderer::PointLightShadowMap::ComputeFaceViewProj(sl.position, sl.range, f);
            }
        }

        for (u32 i = 0; i < m_ActiveSpotShadowCount; ++i) {
            auto& sl = m_ShadowSpotLights[i];
            shadowData.spotViewProj[i] =
                Renderer::SpotLightShadowMap::ComputeViewProj(sl.position, sl.direction, sl.outerConeAngle, sl.range);
        }

        m_ShadowDataBuffer->UploadData(&shadowData, sizeof(shadowData));
    }
}

void RenderSystem::BuildMaterialSSBO() {
    if (m_MaterialSSBOBuilt) return;
    m_MaterialSSBOBuilt = true;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Collect all renderable entities and build material data
    const auto& meshEntities = m_World->GetEntitiesWithComponent<MeshComponent>();
    u32 entityCount = static_cast<u32>(meshEntities.size());
    if (entityCount == 0) {
        // Upload a single default material so the SSBO is never empty
        MaterialComponent defaultMat;
        defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        MaterialGPU gpu = MaterialGPU::FromComponent(defaultMat);
        m_MaterialBuffers[currentFrame]->UploadData(&gpu, sizeof(gpu), 0);
        m_MaterialSSBOCount = 1;
        m_MaterialSSBODirty = false;
        return;
    }

    // Fast path: if materials aren't dirty and entity count hasn't changed,
    // skip the per-entity iteration and just re-upload the cached staging buffer
    // to this frame's GPU buffer (needed because each frame-in-flight has its own buffer).
    if (!m_MaterialSSBODirty && m_MaterialSSBOCount > 0 && entityCount == m_MaterialSSBOCount) {
        usize uploadSize = static_cast<usize>(m_MaterialSSBOStride) * m_MaterialSSBOCount;
        m_MaterialBuffers[currentFrame]->UploadData(m_MaterialSSBOData.data(), uploadSize, 0);
        return;
    }

    // Full rebuild path: entity count changed or materials are dirty
    m_EntityMaterialIndex.clear();
    m_MaterialSSBOCount = 0;

    // Grow GPU buffer if needed (recreate with larger capacity)
    if (entityCount > m_MaterialSSBOCapacity) {
        u32 newCapacity = entityCount + (entityCount / 2);  // 1.5x growth
        if (newCapacity < 256) newCapacity = 256;
        usize bufferSize = static_cast<usize>(m_MaterialSSBOStride) * newCapacity;

        m_VulkanRenderer->GetContext()->WaitForGPU();
        m_MaterialBuffers[currentFrame] = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
        if (!m_MaterialBuffers[currentFrame]->Create(bufferSize, Renderer::BufferUsage::Storage, true)) {
            ENJIN_LOG_ERROR(Renderer, "Failed to grow material SSBO to %u entries", newCapacity);
            return;
        }
        m_MaterialSSBOCapacity = newCapacity;

        // Must update the descriptor set to point to the new buffer
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_MaterialBuffers[currentFrame]->GetBuffer();
        bufInfo.offset = 0;
        bufInfo.range = m_MaterialSSBOStride;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_DescriptorSets[currentFrame];
        write.dstBinding = 2;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufInfo;
        vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &write, 0, nullptr);

        // Also update offscreen descriptor sets that share this material buffer
        for (u32 v = 0; v < MAX_SPLITSCREEN_VIEWPORTS; ++v) {
            u32 offIdx = GetOffscreenBufferIndex(currentFrame, v);
            if (offIdx < m_OffscreenDescriptorSets.size()) {
                VkWriteDescriptorSet offWrite = write;
                offWrite.dstSet = m_OffscreenDescriptorSets[offIdx];
                vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &offWrite, 0, nullptr);
            }
        }
    }

    // Resize CPU staging buffer
    usize totalBytes = static_cast<usize>(m_MaterialSSBOStride) * entityCount;
    if (m_MaterialSSBOData.size() < totalBytes) {
        m_MaterialSSBOData.resize(totalBytes, 0);
    }

    // Helper: look up bindless texture handle for a cached texture pointer
    auto lookupBindless = [this](Renderer::Texture* tex) -> u32 {
        if (!tex || !m_BindlessManager) return m_DefaultBindlessHandle;
        auto it = m_TextureBindlessHandles.find(tex);
        return (it != m_TextureBindlessHandles.end()) ? it->second : m_DefaultBindlessHandle;
    };

    // Fill material data for each entity
    u32 index = 0;
    for (Entity entity : meshEntities) {
        MaterialGPU materialGPU;
        MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material) {
            materialGPU = MaterialGPU::FromComponent(*material);
            // Populate bindless texture indices from cached texture pointers
            materialGPU.baseColorTexIdx         = lookupBindless(material->cachedBaseColorTexture);
            materialGPU.heightTexIdx            = lookupBindless(material->cachedHeightTexture);
            materialGPU.normalTexIdx            = lookupBindless(material->cachedNormalTexture);
            materialGPU.metallicRoughnessTexIdx = lookupBindless(material->cachedMetallicRoughnessTexture);
            materialGPU.emissiveTexIdx          = lookupBindless(material->cachedEmissiveTexture);
            materialGPU.matcapTexIdx            = lookupBindless(material->cachedMatcapTexture);
        } else {
            MaterialComponent defaultMat;
            defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
            defaultMat.metallic = 0.0f;
            defaultMat.roughness = 0.5f;
            materialGPU = MaterialGPU::FromComponent(defaultMat);
        }

        // Apply per-entity ArtStyleComponent overrides to MaterialGPU
        ArtStyleComponent* artStyle = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
        if (artStyle && artStyle->style != ArtStyleType::Inherit) {
            if (artStyle->style == ArtStyleType::MaterialExpression) {
                if (artStyle->matExpr_sssIntensity > 0.0f) {
                    materialGPU.sssIntensity = artStyle->matExpr_sssIntensity;
                    materialGPU.sssRadius = artStyle->matExpr_sssRadius;
                    materialGPU.sssColor = artStyle->matExpr_sssColor;
                }
            }
            if (artStyle->style == ArtStyleType::CelToon && material) {
                // Force outline via MaterialGPU (outlineWidth/color are render-time only,
                // handled in the outline pass, not in the SSBO)
            }
        }

        // Write to aligned offset in staging buffer
        usize offset = static_cast<usize>(m_MaterialSSBOStride) * index;
        std::memcpy(m_MaterialSSBOData.data() + offset, &materialGPU, sizeof(MaterialGPU));

        m_EntityMaterialIndex[static_cast<u64>(entity)] = index;
        ++index;
    }

    m_MaterialSSBOCount = index;

    // Single batched upload to GPU
    usize uploadSize = static_cast<usize>(m_MaterialSSBOStride) * m_MaterialSSBOCount;
    m_MaterialBuffers[currentFrame]->UploadData(m_MaterialSSBOData.data(), uploadSize, 0);

    m_MaterialSSBODirty = false;
}

u32 RenderSystem::GetMaterialIndex(Entity entity) const {
    auto it = m_EntityMaterialIndex.find(static_cast<u64>(entity));
    return (it != m_EntityMaterialIndex.end()) ? it->second : 0;
}

void RenderSystem::UpdateMaterialBuffer(Entity entity) {
    // Legacy path: if BuildMaterialSSBO has already run, this is a no-op.
    // For entities not in the SSBO (e.g., RenderToTarget one-off draws),
    // write directly to index 0 as a fallback.
    if (m_MaterialSSBOBuilt) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    MaterialGPU materialGPU;
    MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
    if (material) {
        materialGPU = MaterialGPU::FromComponent(*material);
    } else {
        MaterialComponent defaultMat;
        defaultMat.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        defaultMat.metallic = 0.0f;
        defaultMat.roughness = 0.5f;
        materialGPU = MaterialGPU::FromComponent(defaultMat);
    }
    m_MaterialBuffers[currentFrame]->UploadData(&materialGPU, sizeof(materialGPU), 0);
}

// Legacy wrapper for RenderToTarget which needs both frame + material updates
void RenderSystem::UpdateUniformBuffer(Entity entity) {
    UpdateFrameUniforms();
    UpdateMaterialBuffer(entity);
}

void RenderSystem::SetBackfaceCullingEnabled(bool enabled) {
    if (m_BackfaceCulling == enabled) return;
    m_BackfaceCulling = enabled;
    m_PendingRecreation = PendingRecreationType::PipelineOnly;
}

void RenderSystem::SetWireframeEnabled(bool enabled) {
    if (m_WireframeMode == enabled) return;
    m_WireframeMode = enabled;
    m_PendingRecreation = PendingRecreationType::PipelineOnly;
}

void RenderSystem::SetShadowDistance(f32 d) {
    m_ShadowDistance = d;
    if (m_ShadowMap) m_ShadowMap->SetShadowDistance(d);
}

f32 RenderSystem::GetShadowStrength() const {
    return m_ShadowMap ? m_ShadowMap->GetShadowStrength() : 1.0f;
}

void RenderSystem::SetShadowStrength(f32 s) {
    if (m_ShadowMap) m_ShadowMap->SetShadowStrength(s);
}

f32 RenderSystem::GetShadowSoftness() const {
    return m_ShadowMap ? m_ShadowMap->GetShadowSoftness() : 0.0f;
}

void RenderSystem::SetShadowSoftness(f32 s) {
    if (m_ShadowMap) m_ShadowMap->SetShadowSoftness(s);
}

u32 RenderSystem::GetShadowResolution() const {
    return m_ShadowMap ? m_ShadowMap->GetResolution() : 2048;
}

void RenderSystem::SetShadowResolution(u32 r) {
    if (!m_ShadowMap) return;
    if (r < 512) r = 512;
    if (r > 4096) r = 4096;
    // Skip if resolution hasn't actually changed
    if (r == m_ShadowMap->GetResolution()) return;
    // Defer the actual resize to FlushPendingChanges() where the GPU is already idle
    m_PendingShadowResolution = r;
    m_PendingRecreation = PendingRecreationType::PipelineOnly;
}

void RenderSystem::SetHDREnabled(bool enabled) {
    if (!m_Renderer) return;
    if (m_VulkanRenderer->IsHDREnabled() == enabled) return;

    // VulkanRenderer::SetHDREnabled handles: swapchain recreate, render pass recreate,
    // framebuffer recreate, and notifies resize callbacks. After that, our pipelines
    // (which reference the render pass) must be recreated too.
    m_VulkanRenderer->SetHDREnabled(enabled);
    RecreatePipelines(true);  // GPU already idle from VulkanRenderer::SetHDREnabled
}

void RenderSystem::SetAAMode(u32 mode) {
    if (mode == m_AAMode) return;

    u32 oldMode = m_AAMode;
    m_AAMode = mode;

    // MSAA modes (4=2x, 5=4x, 6=8x) require render pass + framebuffer + pipeline recreation.
    // This is unsafe mid-frame (crashes NVIDIA driver). Defer to the start of the next
    // RenderSystem::Update() via m_PendingMSAAChange flag.
    bool oldIsMSAA = oldMode >= 4 && oldMode <= 6;
    bool newIsMSAA = mode >= 4 && mode <= 6;

    if (newIsMSAA || oldIsMSAA) {
        m_PendingMSAAChange = true;
    }
}

void RenderSystem::ApplyPendingMSAAChange() {
    m_PendingMSAAChange = false;
    if (!m_Renderer) return;

    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    if (m_AAMode == 4) samples = VK_SAMPLE_COUNT_2_BIT;
    else if (m_AAMode == 5) samples = VK_SAMPLE_COUNT_4_BIT;
    else if (m_AAMode == 6) samples = VK_SAMPLE_COUNT_8_BIT;

    if (!m_VulkanRenderer->SetMSAASamples(samples)) {
        ENJIN_LOG_WARN(Renderer, "MSAA %dx not supported, reverting to no MSAA", static_cast<int>(samples));
        m_AAMode = 0;  // Fall back to no AA
        m_VulkanRenderer->SetMSAASamples(VK_SAMPLE_COUNT_1_BIT);
    }

    // Render pass changed — all pipelines must be recreated
    RecreatePipelines(true);  // GPU already idle from SetMSAASamples

    // Invalidate all entity render data so buffers are re-created
    for (auto& rd : m_EntityRenderData) {
        if (rd.valid) rd.Invalidate();
    }
}

u32 RenderSystem::GetMaxMSAASamples() const {
    if (!m_Renderer || !m_VulkanRenderer->GetContext()) return 1;
    return static_cast<u32>(m_VulkanRenderer->GetContext()->GetMaxUsableSampleCount());
}

void RenderSystem::RecreatePipelines(bool gpuAlreadyIdle) {
    if (!m_Pipeline || !m_Initialized) return;

    // Wait for GPU to finish all in-flight work before destroying pipelines
    // Skip if caller guarantees GPU is already idle (e.g., deferred recreation already waited)
    if (!gpuAlreadyIdle && m_Renderer) {
        m_VulkanRenderer->WaitForAllFrames();
    }

    // Destroy all pipelines that share the descriptor set layout
    m_OffscreenWireframeOverlayPipeline.reset();
    m_OffscreenOutlinePipeline.reset();
    m_OffscreenLinePipeline.reset();
    m_OffscreenPipeline.reset();
    m_WireframeOverlayPipeline.reset();
    m_OutlinePipeline.reset();
    m_LinePipeline.reset();
    m_ShadowPipeline.reset();
    m_Pipeline.reset();

    // Destroy old descriptor pool (implicitly frees all descriptor sets allocated from it)
    if (m_DescriptorPool != VK_NULL_HANDLE && m_Renderer && m_VulkanRenderer->GetContext()) {
        vkDestroyDescriptorPool(m_VulkanRenderer->GetContext()->GetDevice(), m_DescriptorPool, nullptr);
        m_DescriptorPool = VK_NULL_HANDLE;
        m_DescriptorSets.clear();
    }

    // Recreate main pipeline (creates new descriptor set layout)
    CreatePipeline();

    // Recreate descriptor sets with the new pipeline's layout, then dependent pipelines
    if (m_Pipeline) {
        CreateDescriptorSets();
        CreateLinePipeline();
        CreateOutlinePipeline();
        CreateWireframeOverlayPipeline();
        CreateShadowPipeline();
        CreatePointShadowPipeline();
        CreateSpotShadowPipeline();

        // Recreate offscreen pipelines if we have a cached offscreen render pass
        if (m_OffscreenRenderPass != VK_NULL_HANDLE) {
            RecreateEffectPipelinesForRenderPass(m_OffscreenRenderPass);
        }
    }
}

// ─── Shader Hot-Reload ────────────────────────────────────────────────────────

void RenderSystem::FindShaderDirectory() {
    namespace fs = std::filesystem;
    // Probe common relative paths from the working directory
    const char* candidates[] = {
        "Engine/shaders",
        "../Engine/shaders",
        "../../Engine/shaders",
        "../../../Engine/shaders",
    };
    for (const auto& candidate : candidates) {
        std::error_code ec;
        fs::path p(candidate);
        if (fs::is_directory(p, ec) && !ec) {
            m_ShaderDir = fs::canonical(p, ec).string();
            if (!ec) {
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: found shader directory at %s", m_ShaderDir.c_str());
                return;
            }
        }
    }
    ENJIN_LOG_INFO(Renderer, "Shader hot-reload: shader source directory not found, hot-reload disabled");
    m_ShaderDir.clear();
}

static bool ReadFileToString(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) return false;
    out.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return true;
}

void RenderSystem::SetupShaderWatchers() {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty()) return;

    auto shaderPath = [&](const char* name) -> std::string {
        return (fs::path(m_ShaderDir) / name).string();
    };

    // Main pipeline shaders (triangle.vert/frag)
    auto mainReload = [this](const std::string& changedFile) { ReloadMainShaders(changedFile); };
    m_ShaderWatcher.Watch(shaderPath("triangle.vert"), mainReload);
    m_ShaderWatcher.Watch(shaderPath("triangle.frag"), mainReload);

    // Shadow pipeline (vertex-only)
    m_ShaderWatcher.Watch(shaderPath("shadow.vert"), [this](const std::string&) { ReloadShadowShaders(); });

    // Skybox pipeline
    auto skyboxReload = [this](const std::string&) { ReloadSkyboxShaders(); };
    m_ShaderWatcher.Watch(shaderPath("skybox.vert"), skyboxReload);
    m_ShaderWatcher.Watch(shaderPath("skybox.frag"), skyboxReload);

    // Sub-renderer shaders — each gets a lambda that calls ReloadShaders on the sub-renderer
    VkDescriptorSetLayout layout = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
    std::string dir = m_ShaderDir;

    // Grass
    auto grassReload = [this, dir, layout](const std::string&) {
        if (m_GrassRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_GrassRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: grass shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("grass.vert"), grassReload);
    m_ShaderWatcher.Watch(shaderPath("grass.frag"), grassReload);

    // Shrub
    auto shrubReload = [this, dir](const std::string&) {
        if (m_ShrubRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_ShrubRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: shrub shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("shrub.vert"), shrubReload);
    m_ShaderWatcher.Watch(shaderPath("shrub.frag"), shrubReload);

    // Tree
    auto treeReload = [this, dir](const std::string&) {
        if (m_TreeRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_TreeRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: tree shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("tree.vert"), treeReload);
    m_ShaderWatcher.Watch(shaderPath("tree.frag"), treeReload);

    // Particle + Weather + Fluid (shared shaders)
    auto particleReload = [this, dir](const std::string&) {
        VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
        bool any = false;
        if (m_ParticleRenderer && m_ParticleRenderer->ReloadShaders(dir, l)) any = true;
        if (m_WeatherRenderer && m_WeatherRenderer->ReloadShaders(dir, l)) any = true;
        if (m_FluidRenderer && m_FluidRenderer->ReloadShaders(dir, l)) any = true;
        if (any) ENJIN_LOG_INFO(Renderer, "Shader hot-reload: particle/weather/fluid shaders reloaded");
    };
    m_ShaderWatcher.Watch(shaderPath("particle.vert"), particleReload);
    m_ShaderWatcher.Watch(shaderPath("particle.frag"), particleReload);

    // Sprite (unlit + lit)
    auto spriteReload = [this, dir](const std::string&) {
        if (m_SpriteBatchRenderer) {
            VkDescriptorSetLayout l = m_Pipeline ? m_Pipeline->GetDescriptorSetLayout() : VK_NULL_HANDLE;
            if (m_SpriteBatchRenderer->ReloadShaders(dir, l))
                ENJIN_LOG_INFO(Renderer, "Shader hot-reload: sprite shaders reloaded");
        }
    };
    m_ShaderWatcher.Watch(shaderPath("sprite.vert"), spriteReload);
    m_ShaderWatcher.Watch(shaderPath("sprite.frag"), spriteReload);
    m_ShaderWatcher.Watch(shaderPath("sprite_lit.vert"), spriteReload);
    m_ShaderWatcher.Watch(shaderPath("sprite_lit.frag"), spriteReload);

    ENJIN_LOG_INFO(Renderer, "Shader hot-reload: watching %zu shader files", m_ShaderWatcher.GetWatchCount());
}

void RenderSystem::ReloadMainShaders(const std::string& changedFile) {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty() || !m_Renderer || !m_Initialized) return;

    std::string vertPath = (fs::path(m_ShaderDir) / "triangle.vert").string();
    std::string fragPath = (fs::path(m_ShaderDir) / "triangle.frag").string();

    // Read GLSL source
    std::string vertSource, fragSource;
    if (!ReadFileToString(vertPath, vertSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read %s", vertPath.c_str());
        return;
    }
    if (!ReadFileToString(fragPath, fragSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read %s", fragPath.c_str());
        return;
    }

    // Compile to temporary shaders — if either fails, keep existing
    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSource, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: triangle.vert compilation failed, keeping old shader");
        return;
    }

    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSource, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: triangle.frag compilation failed, keeping old shader");
        return;
    }

    // Both compiled — defer swap and pipeline recreation to next frame start
    m_PendingVertexShader = std::move(tempVert);
    m_PendingFragmentShader = std::move(tempFrag);
    m_PendingRecreation = PendingRecreationType::MainShader;
}

void RenderSystem::ReloadSkyboxShaders() {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty() || !m_Renderer || !m_Initialized) return;
    if (m_SkyboxPipelineHandle == VK_NULL_HANDLE) return; // skybox not initialized

    std::string vertPath = (fs::path(m_ShaderDir) / "skybox.vert").string();
    std::string fragPath = (fs::path(m_ShaderDir) / "skybox.frag").string();

    std::string vertSource, fragSource;
    if (!ReadFileToString(vertPath, vertSource) || !ReadFileToString(fragPath, fragSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read skybox shader files");
        return;
    }

    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSource, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: skybox.vert compilation failed");
        return;
    }

    auto tempFrag = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!tempFrag->CompileFromGLSL(fragSource, VK_SHADER_STAGE_FRAGMENT_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: skybox.frag compilation failed");
        return;
    }

    // Shaders validated — defer pipeline recreation to next frame start
    m_PendingRecreation = PendingRecreationType::SkyboxShader;
}

void RenderSystem::ReloadShadowShaders() {
    namespace fs = std::filesystem;
    if (m_ShaderDir.empty() || !m_Renderer || !m_Initialized) return;
    if (!m_ShadowMap || !m_ShadowPipeline) return;

    std::string vertPath = (fs::path(m_ShaderDir) / "shadow.vert").string();
    std::string vertSource;
    if (!ReadFileToString(vertPath, vertSource)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: failed to read shadow.vert");
        return;
    }

    // Shadow pipeline uses the main vertex shader — compile and test
    auto tempVert = std::make_unique<Renderer::VulkanShader>(m_VulkanRenderer->GetContext());
    if (!tempVert->CompileFromGLSL(vertSource, VK_SHADER_STAGE_VERTEX_BIT)) {
        ENJIN_LOG_ERROR(Renderer, "Shader hot-reload: shadow.vert compilation failed");
        return;
    }

    // Defer swap to m_ShadowVertexShader and pipeline recreation to next frame start
    m_PendingVertexShader = std::move(tempVert);
    m_PendingRecreation = PendingRecreationType::ShadowShader;
}

// ─── End Shader Hot-Reload ───────────────────────────────────────────────────

void RenderSystem::CreateDefaultMesh() {
    m_DefaultEntity = m_World->CreateEntity();

    // Add name
    NameComponent& name = m_World->AddComponent<NameComponent>(m_DefaultEntity);
    name.name = "Sphere";

    // Add transform at origin
    TransformComponent& transform = m_World->AddComponent<TransformComponent>(m_DefaultEntity);
    transform.position = Math::Vector3(0.0f, 1.0f, 0.0f);
    transform.scale = Math::Vector3(1.0f);

    // Add sphere mesh using MeshFactory
    MeshComponent& mesh = m_World->AddComponent<MeshComponent>(m_DefaultEntity);
    mesh = Renderer::MeshFactory::CreateSphere(0.5f, 32, 16);

    // Add a nice default material
    MaterialComponent& material = m_World->AddComponent<MaterialComponent>(m_DefaultEntity);
    material.baseColor = Math::Vector3(0.7f, 0.7f, 0.8f);
    material.metallic = 0.1f;
    material.roughness = 0.4f;

    SetupEntityBuffers(m_DefaultEntity);
    ENJIN_LOG_INFO(Renderer, "Created default sphere entity: %llu", m_DefaultEntity);
}

void RenderSystem::RenderEntity(Entity entity) {
    if (!m_Pipeline || !m_Renderer) {
        return;
    }

    // Skip entities already drawn by the multi-draw indirect batch
    if (m_GPUCullingEnabled && !m_IsEditorMode &&
        static_cast<usize>(EntityIndex(entity)) < m_IndirectDrawn.size() &&
        m_IndirectDrawn[static_cast<usize>(EntityIndex(entity))]) {
        return;
    }

    // Use cached storage pointers (refreshed once per frame in RefreshStorageCache)
    // to skip the type-ID hash map lookup that GetComponent<T>() does on every call.
    TransformComponent* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
    MeshComponent* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;

    if (!transform || !mesh || !mesh->IsValid()) {
        return;
    }

    // Skip invisible entities (safety net — callers should also check)
    if (!transform->visible) return;

    EntityRenderData* pRD = GetOrCreateRenderData(entity);
    if (!pRD) return;
    EntityRenderData& renderData = *pRD;

    // Get command buffer
    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) {
        return;
    }

    // Bind material SSBO at this entity's dynamic offset (or fallback to legacy upload)
    if (m_MaterialSSBOBuilt) {
        u32 matIdx = GetMaterialIndex(entity);
        u32 dynOffset = matIdx * m_MaterialSSBOStride;
        u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &dynOffset);
    } else {
        UpdateMaterialBuffer(entity);
        u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
        u32 zeroOffset = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_Pipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOffset);
    }

    // Push model matrix — skinned meshes use identity (skinning already transforms to world space)
    Renderer::PushConstants pushConstants{};
    {
        AnimatorComponent* ac = ResolveAnimator(entity);
        pushConstants.model = (ac && ac->animator.GetSkeleton())
            ? Math::Matrix4::Identity()
            : ECS::ComputeWorldMatrix(m_World, entity);
    }

    // Set material data (cached storage avoids type-ID lookup)
    MaterialComponent* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
    Renderer::Texture* boundTexture = nullptr;

    // Cached texture pointers for batched descriptor update
    Renderer::Texture* texHeight = nullptr;
    Renderer::Texture* texNormal = nullptr;
    Renderer::Texture* texMR = nullptr;
    Renderer::Texture* texEmissive = nullptr;
    Renderer::Texture* texMatcap = nullptr;

    if (material) {
        pushConstants.baseColor = material->baseColor;
        pushConstants.metallic = material->metallic;
        pushConstants.emissiveColor = material->emissiveColor;
        pushConstants.roughness = material->roughness;
        pushConstants.emissiveStrength = material->emissiveStrength;
        pushConstants.opacity = material->opacity;
        pushConstants.alphaCutoff = material->alphaCutoff;

        // Resolve textures using cache (avoids per-frame string hash lookups)
        if (material->textureCacheDirty) {
            // Cache miss - load all textures and cache pointers
            if (!material->baseColorTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->baseColorTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedBaseColorTexture = tex.get();
                    material->baseColorTexture = 1;
                }
            }
            if (!material->heightTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->heightTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedHeightTexture = tex.get();
                    material->heightTexture = 1;
                }
            }
            if (!material->normalTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->normalTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedNormalTexture = tex.get();
                    material->normalTexture = 1;
                }
            }
            if (!material->metallicRoughnessTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->metallicRoughnessTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedMetallicRoughnessTexture = tex.get();
                    material->metallicRoughnessTexture = 1;
                }
            }
            if (!material->emissiveTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->emissiveTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedEmissiveTexture = tex.get();
                    material->emissiveTexture = 1;
                }
            }
            if (!material->matcapTexturePath.empty()) {
                auto tex = GetOrLoadTexture(material->matcapTexturePath);
                if (tex && tex->IsValid()) {
                    material->cachedMatcapTexture = tex.get();
                    material->matcapTexture = 1;
                }
            }
            material->textureCacheDirty = false;
            material->cachedTextureKey = { material->cachedBaseColorTexture,
                material->cachedHeightTexture, material->cachedNormalTexture,
                material->cachedMetallicRoughnessTexture, material->cachedEmissiveTexture,
                material->cachedMatcapTexture };
        }

        // Use cached texture pointers
        boundTexture = material->cachedBaseColorTexture;
        texHeight = material->cachedHeightTexture;
        texNormal = material->cachedNormalTexture;
        texMR = material->cachedMetallicRoughnessTexture;
        texEmissive = material->cachedEmissiveTexture;
        texMatcap = material->cachedMatcapTexture;

        // Compute flags same as MaterialGPU::FromComponent
        pushConstants.flags = 0;
        if (material->doubleSided) pushConstants.flags |= 1;
        if (material->castShadows) pushConstants.flags |= 2;
        if (material->receiveShadows) pushConstants.flags |= 4;
        pushConstants.flags |= (static_cast<i32>(material->alphaMode) << 8);
        if (boundTexture != nullptr) pushConstants.flags |= (1 << 16);
        if (material->normalTexture >= 0) pushConstants.flags |= (1 << 17);
        if (material->metallicRoughnessTexture >= 0) pushConstants.flags |= (1 << 18);
        if (material->emissiveTexture >= 0) pushConstants.flags |= (1 << 19);
        if (material->heightTexture >= 0) pushConstants.flags |= (1 << 10);
        // Retro flags (per-material)
        if (material->flatShading) pushConstants.flags |= (1 << 20);
        if (material->affineTexturing) pushConstants.flags |= (1 << 21);
        if (material->vertexSnapping) pushConstants.flags |= (1 << 22);
        if (material->stippleTransparency) pushConstants.flags |= (1 << 23);
        if (material->uvQuantize) pushConstants.flags |= (1 << 12);
        if (material->gouraudOnly) pushConstants.flags |= (1 << 13);
        pushConstants.flags |= (static_cast<i32>(material->shadowDitherMode & 0x3) << 14);
        pushConstants.flags |= (static_cast<i32>((material->vertexSnapResolution / 8) & 0x1F) << 24);
        pushConstants.flags |= (static_cast<i32>(material->shadowDitherPattern & 0x7) << 29);
        pushConstants.parallaxScale = material->parallaxScale;
        // Artistic surface params (reused push constant slots)
        pushConstants.surfaceParam1 = material->reflectivity;
        pushConstants.surfaceParam2 = material->fresnelPower;
        pushConstants.surfaceParam3 = material->rimLightStrength;
        // Dithered gradient: encode bands + pattern into surfaceParam1
        if (material->ditherGradient) {
            pushConstants.flags |= (1 << 20); // Force flat shading
            pushConstants.surfaceParam1 = 100.0f + static_cast<f32>(material->ditherGradientBands)
                + static_cast<f32>(material->ditherGradientPattern) * 0.1f;
        }
        // Dithered transparency: encode pattern + opacity + blend color into surfaceParams
        if (material->ditherTransparency) {
            pushConstants.surfaceParam1 = 200.0f + static_cast<f32>(material->ditherTransPattern);
            pushConstants.surfaceParam2 = material->ditherTransOpacity;
            u32 r = static_cast<u32>(material->ditherTransBlendColor.x * 1023.0f) & 0x3FF;
            u32 g = static_cast<u32>(material->ditherTransBlendColor.y * 1023.0f) & 0x3FF;
            u32 b = static_cast<u32>(material->ditherTransBlendColor.z * 1023.0f) & 0x3FF;
            u32 packed = (r << 20) | (g << 10) | b;
            pushConstants.surfaceParam3 = *reinterpret_cast<f32*>(&packed);
        }
        // Elemental surface effects: encode char/wet/snow/frost into surfaceParams
        if (!material->ditherGradient && !material->ditherTransparency) {
            auto* elemSurface = m_World->GetComponent<ECS::ElementalSurfaceComponent>(entity);
            if (elemSurface && (elemSurface->charAmount > 0.01f || elemSurface->wetness > 0.01f ||
                                elemSurface->snowCoverage > 0.01f || elemSurface->frostAmount > 0.01f)) {
                pushConstants.surfaceParam1 = 300.0f + elemSurface->charAmount;
                pushConstants.surfaceParam2 = elemSurface->wetness + std::floor(elemSurface->snowCoverage * 256.0f);
                pushConstants.surfaceParam3 = elemSurface->frostAmount;
            }
        }
        // Procedural surface noise: encode scale/strength into surfaceParams (range 400+)
        // Only when no other effect has claimed the surfaceParam slots
        if (!material->ditherGradient && !material->ditherTransparency &&
            material->surfaceNoiseScale > 0.0f && pushConstants.surfaceParam1 < 100.0f) {
            pushConstants.surfaceParam1 = 400.0f + material->surfaceNoiseScale;
            pushConstants.surfaceParam2 = material->surfaceNoiseStrength;
        }
    } else {
        // Default material (light gray, non-metallic)
        pushConstants.baseColor = Math::Vector3(0.8f, 0.8f, 0.8f);
        pushConstants.metallic = 0.0f;
        pushConstants.emissiveColor = Math::Vector3(0.0f, 0.0f, 0.0f);
        pushConstants.roughness = 0.5f;
        pushConstants.emissiveStrength = 0.0f;
        pushConstants.opacity = 1.0f;
        pushConstants.alphaCutoff = 0.5f;
        pushConstants.flags = 0;
        pushConstants.parallaxScale = 0.0f;
    }

    // Global retro overrides (OR with per-material — global forces on)
    if (m_GlobalFlatShading) pushConstants.flags |= (1 << 20);
    if (m_GlobalAffineTexturing) pushConstants.flags |= (1 << 21);
    if (m_GlobalVertexSnapping) pushConstants.flags |= (1 << 22);
    if (m_GlobalStippleTransparency) pushConstants.flags |= (1 << 23);
    if (m_GlobalUVQuantize) pushConstants.flags |= (1 << 12);
    if (m_GlobalGouraudOnly) pushConstants.flags |= (1 << 13);
    if (m_GlobalVertexSnapping && m_GlobalVertexSnapResolution > 0) {
        pushConstants.flags = (pushConstants.flags & ~(0x1F << 24)) | (static_cast<i32>((m_GlobalVertexSnapResolution / 8) & 0x1F) << 24);
    }

    // Sprite texture override — use sprite's texturePath instead of material's
    Sprite2DComponent* spriteComp = m_World->GetComponent<Sprite2DComponent>(entity);
    if (spriteComp && !spriteComp->texturePath.empty()) {
        auto tex = GetOrLoadTexture(spriteComp->texturePath);
        if (tex && tex->IsValid()) {
            boundTexture = tex.get();
            pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
        }
        pushConstants.baseColor = spriteComp->tint;
        pushConstants.opacity = spriteComp->alpha;
    }

    // Tilemap texture override — use tileset texture path
    TilemapComponent* tilemapComp = m_World->GetComponent<TilemapComponent>(entity);
    if (tilemapComp && !tilemapComp->tilesetPath.empty()) {
        auto tex = GetOrLoadTexture(tilemapComp->tilesetPath);
        if (tex && tex->IsValid()) {
            boundTexture = tex.get();
            pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
        }
    }

    // Set wind sway flag for vegetation entities
    VegetationComponent* vegComp = m_World->GetComponent<VegetationComponent>(entity);
    if (vegComp) {
        pushConstants.flags |= (1 << 4); // FLAG_WIND_SWAY
    }

    // Set water surface flag for water volume entities
    WaterVolumeComponent* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
    if (waterVol) {
        pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE — always set, shader handles freeze
        pushConstants.parallaxScale = waterVol->freezeProgress; // repurpose for water (POM skips water)

        if (m_RainActive && waterVol->freezeProgress < 0.5f) {
            pushConstants.flags |= (1 << 6); // FLAG_RAIN_RIPPLES (no ripples on ice)
        }
        if (waterVol->enableShore && waterVol->freezeProgress < 0.8f) {
            pushConstants.flags |= (1 << 7); // FLAG_WATER_SHORE
            pushConstants.surfaceParam1 = waterVol->shoreWidth;
            pushConstants.surfaceParam2 = waterVol->foamIntensity * (1.0f - waterVol->freezeProgress);
            pushConstants.surfaceParam3 = waterVol->foamScale;
        }
        if (waterVol->waterType == WaterType::Ocean) {
            pushConstants.flags |= (1 << 11); // FLAG_WATER_OCEAN
        }
        // Lerp base color toward ice color as water freezes
        f32 fp = waterVol->freezeProgress;
        pushConstants.baseColor = Math::Vector3(
            pushConstants.baseColor.x * (1.0f - fp) + waterVol->iceColor.x * fp,
            pushConstants.baseColor.y * (1.0f - fp) + waterVol->iceColor.y * fp,
            pushConstants.baseColor.z * (1.0f - fp) + waterVol->iceColor.z * fp
        );
        pushConstants.opacity = pushConstants.opacity * (1.0f - fp) + waterVol->iceOpacity * fp;
    } else if ((m_CachedWater3DStorage ? m_CachedWater3DStorage->Has(entity) : m_World->HasComponent<Water3DComponent>(entity))) {
        auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
        pushConstants.flags |= (1 << 5); // FLAG_WATER_SURFACE for Water3D
        pushConstants.parallaxScale = 0.0f; // no freeze
        if (water3d) {
            pushConstants.baseColor = water3d->settings.shallowColor;
            pushConstants.opacity = water3d->settings.opacity;
        }
    }

    // Rasterize text texture if entity has a TextComponent (cached storage)
    TextComponent* textComp = m_CachedTextStorage ? m_CachedTextStorage->Get(entity) : nullptr;
    if (textComp && textComp->dirty && !textComp->fontPath.empty() && !textComp->text.empty()) {
        auto pixels = m_TextRasterizer.Rasterize(*textComp);
        if (!pixels.empty()) {
            auto textTex = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
            if (textTex->CreateFromData(pixels.data(), textComp->textureWidth, textComp->textureHeight, 4)) {
                m_TextTextureCache[entity] = textTex;
            }
        }
        textComp->dirty = false;
    }

    // If entity has a text texture, override the base color texture
    auto textTexIt = m_TextTextureCache.find(entity);
    if (textComp && textTexIt != m_TextTextureCache.end() && textTexIt->second && textTexIt->second->IsValid()) {
        boundTexture = textTexIt->second.get();
        pushConstants.flags |= (1 << 16); // HAS_BASE_COLOR_TEXTURE
    }

    // Update texture descriptor if entity has a texture
    // Batched texture descriptor update (1 vkUpdateDescriptorSets call instead of 6)
    UpdateEntityTextureDescriptors(boundTexture, texHeight, texNormal, texMR, texEmissive, texMatcap);

    // Upload bone matrices for skinned meshes (cached storage avoids type-ID lookup).
    // Always upload when skeleton exists, not just when animation is playing.
    AnimatorComponent* animComp = ResolveAnimator(entity);
    if (animComp && renderData.boneBuffer) {
        const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
        if (!skinningMatrices.empty()) {
            renderData.boneBuffer->UploadData(skinningMatrices.data(),
                skinningMatrices.size() * sizeof(Math::Matrix4));
            UpdateBoneDescriptor(renderData.boneBuffer.get());
            pushConstants.flags |= (1 << 3); // FLAG_SKINNED
        }
    } else {
        // Bind default bone buffer for static meshes
        if (m_DefaultBoneBuffer) {
            UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
        }
    }

    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::PushConstants), &pushConstants);

    // Bind and draw — pool-allocated entities use merged buffer with offsets
    if (renderData.poolAlloc.valid && m_GeometryPool) {
        if (!m_GeometryPoolBound) {
            m_GeometryPool->BindBuffers(commandBuffer);
            m_GeometryPoolBound = true;
        }
        vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                         renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
    } else if (renderData.vertexBuffer && renderData.indexBuffer) {
        VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
        m_GeometryPoolBound = false;  // Non-pool entity invalidates pool binding
    }
    m_DrawCallCount++;
    m_TriangleCount += renderData.indexCount / 3;
}

void RenderSystem::RenderEntityGhost(Entity entity, const Math::Matrix4& modelMatrix,
                                      const Math::Vector3& tint, f32 opacity,
                                      const std::vector<Math::Matrix4>* skinningMatrices) {
    if (!m_Pipeline || !m_Renderer) return;

    MeshComponent* mesh = m_World->GetComponent<MeshComponent>(entity);
    if (!mesh || !mesh->IsValid()) return;

    EntityRenderData* pRD = GetOrCreateRenderData(entity);
    if (!pRD) return;
    EntityRenderData& renderData = *pRD;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // Build ghost push constants — override baseColor with tint, set opacity for transparency
    Renderer::PushConstants pushConstants{};
    pushConstants.model = modelMatrix;
    pushConstants.baseColor = tint;
    pushConstants.metallic = 0.0f;
    pushConstants.emissiveColor = Math::Vector3(0, 0, 0);
    pushConstants.roughness = 1.0f;
    pushConstants.emissiveStrength = 0.0f;
    pushConstants.opacity = opacity;
    pushConstants.alphaCutoff = 0.0f;
    // Set alpha blend flag (bit 9 = ALPHA_MODE_BLEND)
    pushConstants.flags = (2 << 8);
    pushConstants.parallaxScale = 0.0f;

    // Bind default white texture for ghost (no texture lookups needed)
    if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
        UpdateEntityTextureDescriptors(m_DefaultWhiteTexture.get(), nullptr, nullptr, nullptr, nullptr);
    }

    // Upload skeletal skinning matrices for 3D onion skin ghosts
    bool hasSkinning = skinningMatrices && !skinningMatrices->empty();
    if (hasSkinning) {
        usize requiredSize = skinningMatrices->size() * sizeof(Math::Matrix4);

        // Lazily create or grow the reusable ghost bone buffer
        if (!m_GhostBoneBuffer || m_GhostBoneBufferCapacity < requiredSize) {
            m_GhostBoneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
            if (m_GhostBoneBuffer->Create(requiredSize, Renderer::BufferUsage::Storage, true)) {
                m_GhostBoneBufferCapacity = requiredSize;
            } else {
                m_GhostBoneBuffer.reset();
                m_GhostBoneBufferCapacity = 0;
                hasSkinning = false;
            }
        }

        if (hasSkinning && m_GhostBoneBuffer) {
            m_GhostBoneBuffer->UploadData(skinningMatrices->data(), requiredSize);
            UpdateBoneDescriptor(m_GhostBoneBuffer.get());
            pushConstants.flags |= (1 << 3); // FLAG_SKINNED
        }
    }

    if (!hasSkinning) {
        // Bind default bone buffer for non-skinned ghosts
        if (m_DefaultBoneBuffer) {
            UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
        }
    }

    vkCmdPushConstants(commandBuffer, m_Pipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::PushConstants), &pushConstants);

    // Bind and draw
    if (renderData.poolAlloc.valid && m_GeometryPool) {
        if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
        vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                         renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
    } else if (renderData.vertexBuffer && renderData.indexBuffer) {
        VkBuffer vertexBuffers[] = { renderData.vertexBuffer->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
    }
    m_DrawCallCount++;
    m_TriangleCount += renderData.indexCount / 3;
}

void RenderSystem::RenderOnionSkinGhosts() {
    if (m_OnionSkinGhosts.empty() || !m_IsEditorMode) return;

    m_LastBound.Reset(); m_GeometryPoolBound = false;  // Reset descriptor cache for ghost pass

    for (const auto& ghost : m_OnionSkinGhosts) {
        // Build model matrix from ghost transform
        TransformComponent ghostTransform;
        ghostTransform.position = ghost.position;
        ghostTransform.rotation = Math::Quaternion::FromEuler(ghost.rotation);
        ghostTransform.scale = ghost.scale;
        Math::Matrix4 modelMatrix = ghostTransform.ToMatrix();

        // Pass skinning matrices for 3D skeletal onion skin ghosts (empty = non-skinned)
        const std::vector<Math::Matrix4>* bones = ghost.skinningMatrices.empty() ? nullptr : &ghost.skinningMatrices;
        RenderEntityGhost(ghost.entity, modelMatrix, ghost.tint, ghost.ghostOpacity * ghost.alpha, bones);
    }
}

void RenderSystem::RenderOutlinePass() {
    if (!m_OutlinePipeline || !m_GeometryOutlinesEnabled || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    m_OutlinePipeline->Bind(commandBuffer);
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_OutlinePipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
    }

    // Cache storage pointers for the outline pass hot loop
    auto* spriteStorageOP = m_World->GetComponentStorage<Sprite2DComponent>();

    for (Entity entity : m_SortedRenderList) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!transform || !transform->visible) continue;

        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        // Skip entities excluded from cel shading (they don't get outlines)
        if (material && material->excludeFromCelShading) continue;
        // Skip 2D sprites
        if (spriteStorageOP && spriteStorageOP->Has(entity)) continue;
        // Skip transparent objects
        if (material && material->alphaMode == MaterialComponent::AlphaMode::Blend) continue;

        EntityRenderData* pRD = GetRenderData(entity);
        if (!pRD || !pRD->valid) continue;
        EntityRenderData& renderData = *pRD;

        // Use per-material outline settings if set, then art style, then global
        f32 outlineWidth = m_GeometryOutlineWidth;
        Math::Vector3 outlineColor = m_GeometryOutlineColor;
        if (material && material->outlineWidth > 0.0f) {
            outlineWidth = material->outlineWidth;
            outlineColor = material->outlineColor;
        }
        // Per-entity ArtStyleComponent cel/toon outline override
        auto* artStyleOutline = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
        if (artStyleOutline && artStyleOutline->style == ArtStyleType::CelToon && artStyleOutline->cel_outlineWidth > 0.0f) {
            outlineWidth = artStyleOutline->cel_outlineWidth;
            outlineColor = artStyleOutline->cel_outlineColor;
        }

        // Build push constants — repurpose baseColor for outlineColor, metallic for outlineWidth
        Renderer::PushConstants pc{};
        pc.baseColor = outlineColor;
        pc.metallic = outlineWidth;
        pc.flags = 0;

        // Propagate skinned flag so outline follows skeleton (playing or bind pose)
        auto* animComp = ResolveAnimator(entity);
        // Compute skinning (ADR-0002): draw the deformed (model-space) buffer with FLAG_SKINNED
        // cleared, applying the world matrix via model (identical to the VS path at origin).
        const bool computeSkinned = m_ComputeSkinningEnabled && renderData.skinnedThisFrame
                                    && renderData.skinnedVertexBuffer;
        if (computeSkinned) {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
        } else if (animComp && renderData.boneBuffer) {
            pc.flags |= (1 << 3); // FLAG_SKINNED
            pc.model = Math::Matrix4::Identity(); // Skinned: identity, bone matrices handle positioning
            const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
            if (!skinningMatrices.empty()) {
                renderData.boneBuffer->UploadData(skinningMatrices.data(),
                    skinningMatrices.size() * sizeof(Math::Matrix4));
            }
            UpdateBoneDescriptor(renderData.boneBuffer.get());
        } else {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) {
                UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
            }
        }

        vkCmdPushConstants(commandBuffer, m_OutlinePipeline->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        // Bind vertex/index buffers and draw
        if (renderData.poolAlloc.valid && m_GeometryPool) {
            if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
            vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                             renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
        } else if (renderData.vertexBuffer && renderData.indexCount > 0) {
            VkBuffer buffers[] = { computeSkinned
                ? renderData.skinnedVertexBuffer->GetBuffer()
                : renderData.vertexBuffer->GetBuffer() };
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
            if (renderData.indexBuffer) {
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            }
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
            m_GeometryPoolBound = false;
        }
    }
}

void RenderSystem::RenderOutlinePassForTarget() {
    if (!m_OutlinePipeline || !m_GeometryOutlinesEnabled || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Use offscreen outline pipeline (matches offscreen UNORM render pass)
    auto* outlinePL = m_OffscreenOutlinePipeline ? m_OffscreenOutlinePipeline.get() : m_OutlinePipeline.get();
    outlinePL->Bind(commandBuffer);
    {
        u32 zeroOff = 0;
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            outlinePL->GetLayout(), 0, 1, &(*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)], 1, &zeroOff);
    }

    const auto& renderList = m_SortedRenderList.empty()
        ? m_World->GetEntitiesWithComponent<MeshComponent>()
        : m_SortedRenderList;

    // Cache storage pointers for the offscreen outline pass hot loop
    auto* spriteStorageOPT = m_World->GetComponentStorage<Sprite2DComponent>();

    for (Entity entity : renderList) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!transform || !transform->visible) continue;

        auto* material = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (material && material->excludeFromCelShading) continue;
        if (spriteStorageOPT && spriteStorageOPT->Has(entity)) continue;
        if (material && material->alphaMode == MaterialComponent::AlphaMode::Blend) continue;

        EntityRenderData* pRD = GetRenderData(entity);
        if (!pRD || !pRD->valid) continue;
        EntityRenderData& renderData = *pRD;

        f32 outlineWidth = m_GeometryOutlineWidth;
        Math::Vector3 outlineColor = m_GeometryOutlineColor;
        if (material && material->outlineWidth > 0.0f) {
            outlineWidth = material->outlineWidth;
            outlineColor = material->outlineColor;
        }
        auto* artStyleOPT = m_CachedArtStyleStorage ? m_CachedArtStyleStorage->Get(entity) : nullptr;
        if (artStyleOPT && artStyleOPT->style == ArtStyleType::CelToon && artStyleOPT->cel_outlineWidth > 0.0f) {
            outlineWidth = artStyleOPT->cel_outlineWidth;
            outlineColor = artStyleOPT->cel_outlineColor;
        }

        Renderer::PushConstants pc{};
        pc.baseColor = outlineColor;
        pc.metallic = outlineWidth;
        pc.flags = 0;

        auto* animComp = ResolveAnimator(entity);
        // Compute skinning (ADR-0002): deformed buffer + cleared flag, world via model.
        const bool computeSkinned = m_ComputeSkinningEnabled && renderData.skinnedThisFrame
                                    && renderData.skinnedVertexBuffer;
        if (computeSkinned) {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
        } else if (animComp && renderData.boneBuffer) {
            pc.flags |= (1 << 3);
            pc.model = Math::Matrix4::Identity(); // Skinned: identity, bone matrices handle positioning
            const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
            if (!skinningMatrices.empty()) {
                renderData.boneBuffer->UploadData(skinningMatrices.data(),
                    skinningMatrices.size() * sizeof(Math::Matrix4));
            }
            UpdateBoneDescriptor(renderData.boneBuffer.get());
        } else {
            pc.model = ECS::ComputeWorldMatrix(m_World, entity);
            if (m_DefaultBoneBuffer) {
                UpdateBoneDescriptor(m_DefaultBoneBuffer.get());
            }
        }

        vkCmdPushConstants(commandBuffer, outlinePL->GetLayout(),
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);

        if (renderData.poolAlloc.valid && m_GeometryPool) {
            if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
            vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                             renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
        } else if (renderData.vertexBuffer && renderData.indexCount > 0) {
            VkBuffer buffers[] = { computeSkinned
                ? renderData.skinnedVertexBuffer->GetBuffer()
                : renderData.vertexBuffer->GetBuffer() };
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
            if (renderData.indexBuffer) {
                vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
            }
            vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
            m_GeometryPoolBound = false;
        }
    }
}

void RenderSystem::RenderSprites() {
    if (!m_Pipeline || !m_Renderer || !m_World) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Render tilemaps first (layer -1000, behind sprites) via the per-entity path
    // Tilemaps are complex meshes that don't benefit from instance batching — render directly
    for (Entity entity : m_World->GetEntitiesWithComponent<TilemapComponent>()) {
        auto* xformTM = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        if (!xformTM || !xformTM->visible) continue;
        if (!(m_CachedMeshStorage && m_CachedMeshStorage->Has(entity))) continue;
        RenderEntity(entity);
    }

    // Render sprites via batch renderer (instanced draw calls grouped by texture)
    if (m_SpriteBatchRenderer) {
        // Populate sprite texture atlas with all sprite textures before rendering
        if (m_SpriteAtlas) {
            auto* spriteAtlasStorage = m_World->GetComponentStorage<Sprite2DComponent>();
            for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
                auto* sprite = spriteAtlasStorage ? spriteAtlasStorage->Get(entity) : nullptr;
                if (sprite && !sprite->texturePath.empty())
                    m_SpriteAtlas->RequestTexture(sprite->texturePath);
            }
            if (m_SpriteAtlas->IsDirty()) m_SpriteAtlas->Build();
        }

        // Determine lit mode: Scene2D = unlit, Scene2_5D/Scene3D = lit (sprites respond to lights)
        bool litMode = (m_SceneComposition.mode != SceneRenderMode::Scene2D);

        auto textureBindCallback = [this, litMode](const std::string& texturePath, const std::string& normalMapPath) {
            // Handle atlas sentinel — bind the packed atlas texture
            if (texturePath == "__atlas__" && m_SpriteAtlas && m_SpriteAtlas->IsValid()) {
                UpdateTextureDescriptor(m_SpriteAtlas->GetAtlasTexture());
            } else if (!texturePath.empty()) {
                auto tex = GetOrLoadTexture(texturePath);
                if (tex && tex->IsValid()) {
                    UpdateTextureDescriptor(tex.get());
                }
            }
            // Bind normal map for lit sprites (binding 6)
            if (litMode) {
                if (!normalMapPath.empty()) {
                    auto normalTex = GetOrLoadTexture(normalMapPath);
                    if (normalTex && normalTex->IsValid()) {
                        UpdateNormalMapDescriptor(normalTex.get());
                    } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                        UpdateNormalMapDescriptor(m_DefaultWhiteTexture.get());
                    }
                } else if (m_DefaultWhiteTexture && m_DefaultWhiteTexture->IsValid()) {
                    UpdateNormalMapDescriptor(m_DefaultWhiteTexture.get());
                }
            }
        };

        m_SpriteBatchRenderer->Render(
            commandBuffer,
            *m_ActiveDescriptorSets,
            GetActiveBufferIndex(currentFrame),
            m_World,
            textureBindCallback,
            0, 0,
            litMode);
    } else {
        // Fallback: per-entity sprite rendering (no batching)
        struct SpriteEntry {
            Entity entity;
            i32 sortingLayer;
            i32 orderInLayer;
        };

        std::vector<SpriteEntry> sprites;
        sprites.reserve(64);
        auto* spriteFBStorage = m_World->GetComponentStorage<Sprite2DComponent>();
        for (Entity entity : m_World->GetEntitiesWithComponent<Sprite2DComponent>()) {
            auto* sprite = spriteFBStorage ? spriteFBStorage->Get(entity) : nullptr;
            if (!sprite || !sprite->visible) continue;
            auto* xformSprite = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (!xformSprite || !xformSprite->visible) continue;
            if (!(m_CachedMeshStorage && m_CachedMeshStorage->Has(entity))) continue;

            sprites.push_back({ entity, sprite->sortingLayer, sprite->orderInLayer });
        }

        if (!sprites.empty()) {
            std::sort(sprites.begin(), sprites.end(), [](const SpriteEntry& a, const SpriteEntry& b) {
                if (a.sortingLayer != b.sortingLayer) return a.sortingLayer < b.sortingLayer;
                return a.orderInLayer < b.orderInLayer;
            });

            for (const auto& entry : sprites) {
                RenderEntity(entry.entity);
            }
        }
    }
}

bool RenderSystem::ShouldUpdateCascade(u32 cascade) const {
    if (!m_CascadeProgressiveUpdate) return true;
    u32 interval = (cascade <= 1) ? 1 : m_CascadeFarUpdateInterval;
    // Stagger far cascades so they don't all update on the same frame
    u32 offset = (cascade <= 1) ? 0 : (cascade - 1);
    return ((m_ShadowFrameCounter + offset) % interval) == 0;
}

void RenderSystem::RenderShadowPass() {
    if (!m_ShadowMap || !m_ShadowPipeline) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    // GPU timestamp: shadow pass begin
    {
        VkQueryPool tsPool = m_VulkanRenderer->GetTimestampPool(m_VulkanRenderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_SHADOW_BEGIN);
        }
    }

    // Progressive cascade updates: increment frame counter and detect camera teleport
    m_ShadowFrameCounter++;
    bool forceFullUpdate = false;
    if (m_Camera) {
        auto pos = m_Camera->GetPosition();
        f32 dist = (pos - m_PrevShadowCameraPos).Length();
        if (dist > 5.0f) forceFullUpdate = true;
        m_PrevShadowCameraPos = pos;
    }
    // When the shadow caster set changes (e.g. an object was deleted) every cascade
    // must re-render this frame. Progressive updates otherwise skip far cascades, so
    // the cascade holding a deleted object's shadow keeps it until it happens to
    // refresh -- the "shadow with no object" lingering after a delete.
    if (m_ShadowCastersDirty) forceFullUpdate = true;

    // Find the first directional light for shadow casting
    bool foundShadowLight = false;

    Math::Vector3 shadowLightDir(0.5f, 0.8f, 0.3f);

    auto* lightStorageSP = m_World->GetComponentStorage<LightComponent>();
    for (Entity lightEntity : m_CachedLightEntities) {
        LightComponent* light = lightStorageSP ? lightStorageSP->Get(lightEntity) : nullptr;
        if (!light || light->type != LightType::Directional || !light->castShadows) continue;

        TransformComponent* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(lightEntity) : nullptr;
        if (lightTransform) {
            Math::Vector3 forward(0.0f, 0.0f, -1.0f);
            shadowLightDir = lightTransform->rotation.Rotate(forward).Normalized();
        }
        foundShadowLight = true;
        break;
    }

    // Update cascade frustums from camera
    if (m_Camera) {
        Math::Vector3 lightDir = foundShadowLight ? shadowLightDir : Math::Vector3(0.5f, 0.8f, 0.3f).Normalized();
        m_ShadowMap->UpdateCascades(
            m_Camera->GetViewMatrix(), m_Camera->GetProjectionMatrix(),
            m_Camera->GetNearPlane(), m_Camera->GetFarPlane(),
            lightDir);
    }

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Rebuild the caster cache BEFORE the cascade loop so the parallel-vs-inline
    // decision (caster count >= 32) is computed once on a stable count. Rebuilding
    // mid-loop changed the count between BeginCascadePass (which picks INLINE vs
    // SECONDARY contents) and the vkCmdExecuteCommands path, producing a contents
    // mismatch (validation 09680) and re-recorded-secondary cascades.
    if (m_ShadowCastersDirty) {
        RebuildShadowCasterCache();
    }

    // Render each cascade
    for (u32 cascade = 0; cascade < m_ShadowMap->GetCascadeCount(); ++cascade) {
        // Progressive update: skip far cascades on non-update frames
        if (!forceFullUpdate && !ShouldUpdateCascade(cascade)) continue;

        // Store cascade VP for RenderEntityShadow to pre-multiply with model matrix.
        // Push constants are embedded in the command buffer, so they're immune to
        // the HOST_COHERENT UBO race that was causing empty shadow maps.
        m_CurrentCascadeVP = m_ShadowMap->GetCascadeViewProj(cascade);

        u32 numCasters = static_cast<u32>(m_ShadowCasters.size());
        bool parallelShadow = (numCasters >= 32 && m_CmdBufferPool && m_ThreadPool.GetThreadCount() > 0);
        m_ShadowMap->BeginCascadePass(commandBuffer, cascade, parallelShadow);

        // Bind shadow pipeline (only for inline mode — parallel mode binds per-thread)
        if (!parallelShadow) m_ShadowPipeline->Bind(commandBuffer);

        // Bind descriptor set (pipeline layout requires it even though shadow shader doesn't use material)
        {
            u32 zeroOff = 0;
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_ShadowPipeline->GetLayout(),
                0, 1, &m_DescriptorSets[currentFrame],
                1, &zeroOff
            );
        }

        // Caster cache was rebuilt before the loop (above) so the count is stable.
        // Parallel shadow caster rendering (if enough entities to justify overhead)
        u32 casterCount = static_cast<u32>(m_ShadowCasters.size());
        bool useParallelShadow = (casterCount >= 32 && m_CmdBufferPool && m_ThreadPool.GetThreadCount() > 0);

        if (useParallelShadow) {
            u32 threadCount = m_ThreadPool.GetThreadCount();
            u32 chunkSize = (casterCount + threadCount - 1) / threadCount;
            u32 frameIdx = m_VulkanRenderer->GetCurrentFrameIndex();

            std::vector<std::future<void>> futures;
            std::vector<VkCommandBuffer> secondaryBuffers(threadCount, VK_NULL_HANDLE);

            VkCommandBufferInheritanceInfo inheritInfo{};
            inheritInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
            inheritInfo.renderPass = m_ShadowMap->GetRenderPass();
            inheritInfo.framebuffer = m_ShadowMap->GetCurrentFramebuffer();
            inheritInfo.subpass = 0;

            for (u32 t = 0; t < threadCount; ++t) {
                u32 start = t * chunkSize;
                u32 end = std::min(start + chunkSize, casterCount);
                if (start >= end) break;

                futures.push_back(m_ThreadPool.Submit([this, t, start, end, frameIdx, &inheritInfo, &secondaryBuffers]() {
                    VkCommandBuffer secCmd = m_CmdBufferPool->Allocate(t, frameIdx);
                    if (!secCmd) return;

                    VkCommandBufferBeginInfo beginInfo{};
                    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT |
                                     VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                    beginInfo.pInheritanceInfo = &inheritInfo;
                    vkBeginCommandBuffer(secCmd, &beginInfo);

                    // Dynamic viewport/scissor must be set in the secondary itself —
                    // state set on the primary does not carry into executed secondaries.
                    m_ShadowMap->ApplyCascadeViewportScissor(secCmd);
                    m_ShadowPipeline->Bind(secCmd);
                    for (u32 i = start; i < end; ++i) {
                        Entity entity = m_ShadowCasters[i];
                        auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                        if (xform && !xform->visible) continue;
                        RenderEntityShadow(entity, secCmd);
                    }

                    vkEndCommandBuffer(secCmd);
                    secondaryBuffers[t] = secCmd;
                }));
            }

            // Wait for all threads
            for (auto& f : futures) f.get();

            // Collect valid secondary buffers and execute
            std::vector<VkCommandBuffer> validBuffers;
            for (auto buf : secondaryBuffers) {
                if (buf != VK_NULL_HANDLE) validBuffers.push_back(buf);
            }
            if (!validBuffers.empty()) {
                vkCmdExecuteCommands(commandBuffer, static_cast<u32>(validBuffers.size()), validBuffers.data());
            }
        } else {
            // Single-threaded fallback (< 32 entities) with prefetching
            for (usize si = 0; si < m_ShadowCasters.size(); ++si) {
                if (si + 4 < m_ShadowCasters.size() && m_CachedTransformStorage) {
                    m_CachedTransformStorage->Prefetch(m_ShadowCasters[si + 4]);
                }
                Entity entity = m_ShadowCasters[si];
                auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                if (xform && !xform->visible) continue;
                RenderEntityShadow(entity, commandBuffer);
            }
        }

        m_ShadowMap->EndCascadePass(commandBuffer);
    }

    // GPU timestamp: shadow pass end
    {
        VkQueryPool tsPool = m_VulkanRenderer->GetTimestampPool(m_VulkanRenderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_SHADOW_END);
        }
    }
}

void RenderSystem::RenderShadowPassForCamera(Renderer::Camera* camera) {
    if (!camera) { ENJIN_LOG_WARN(Renderer, "ShadowPassForCamera: no camera"); return; }
    if (!m_ShadowsEnabled) return;
    if (!m_ShadowMap) { ENJIN_LOG_WARN(Renderer, "ShadowPassForCamera: no shadow map"); return; }
    if (!m_ShadowPipeline) { ENJIN_LOG_WARN(Renderer, "ShadowPassForCamera: no shadow pipeline"); return; }

    ClassifySceneComposition();
    if (m_SceneComposition.mode != SceneRenderMode::Scene3D) return;
    if (!m_SceneComposition.hasShadowCastingLights) return;

    Renderer::Camera* prevCamera = m_Camera;
    m_Camera = camera;

    // Clamp shadow distance to game camera far plane to align cascade splits
    f32 prevShadowDist = m_ShadowDistance;
    f32 camFar = camera->GetFarPlane();
    if (m_ShadowDistance > camFar) {
        m_ShadowDistance = camFar;
    }

    // Re-select shadow lights relative to game camera position
    SelectShadowLights();

    RenderShadowPass();

    // Also render point and spot shadow passes
    if (m_PointShadowMap && m_PointShadowPipeline && m_ActivePointShadowCount > 0) {
        RenderPointShadowPass();
    }
    if (m_SpotShadowMap && m_SpotShadowPipeline && m_ActiveSpotShadowCount > 0) {
        RenderSpotShadowPass();
    }

    m_ShadowDistance = prevShadowDist;
    m_Camera = prevCamera;
}

void RenderSystem::RenderEntityShadow(Entity entity, VkCommandBuffer commandBuffer) {
    TransformComponent* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
    MeshComponent* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;

    if (!transform || !mesh || !mesh->IsValid()) return;

    EntityRenderData* pRD = GetOrCreateRenderData(entity);
    if (!pRD) return;
    EntityRenderData& renderData = *pRD;

    // Push pre-multiplied cascadeVP * model as the MVP matrix.
    // The shadow vertex shader reads this from push constants (first 64 bytes),
    // avoiding the HOST_COHERENT UBO race condition.
    Renderer::PushConstants pushConstants{};

    // Skinned mesh handling: upload bone matrices and use identity model matrix
    // so shadow geometry matches the main pass's skinned positions.
    AnimatorComponent* animComp = ResolveAnimator(entity);
    if (!animComp) {
        // Mesh entity may not have animator — search globally (same as main pass)
        for (auto animEntity : m_World->GetEntitiesWithComponent<AnimatorComponent>()) {
            auto* ac = m_World->GetComponent<AnimatorComponent>(animEntity);
            if (ac && ac->animator.GetSkeleton()) { animComp = ac; break; }
        }
    }
    // Compute skinning (ADR-0002): if the pre-pass already deformed this mesh, draw the deformed
    // (model-space) buffer with FLAG_SKINNED cleared. The model matrix still applies world+cascade,
    // exactly as the vertex-shader path does, so shadow geometry matches the main pass.
    const bool computeSkinned = m_ComputeSkinningEnabled && renderData.skinnedThisFrame
                                && renderData.skinnedVertexBuffer;
    if (computeSkinned) {
        pushConstants.model = m_CurrentCascadeVP * ECS::ComputeWorldMatrix(m_World, entity);
    } else if (animComp && renderData.boneBuffer) {
        const auto& skinningMatrices = animComp->animator.GetSkinningMatrices();
        if (!skinningMatrices.empty()) {
            renderData.boneBuffer->UploadData(skinningMatrices.data(),
                skinningMatrices.size() * sizeof(Math::Matrix4));
            UpdateBoneDescriptor(renderData.boneBuffer.get());
            pushConstants.flags |= (1 << 3); // FLAG_SKINNED
            // Include entity's world matrix for parent scale (cm→m conversion)
            pushConstants.model = m_CurrentCascadeVP * ECS::ComputeWorldMatrix(m_World, entity);
        } else {
            pushConstants.model = m_CurrentCascadeVP * ECS::ComputeWorldMatrix(m_World, entity);
        }
    } else {
        pushConstants.model = m_CurrentCascadeVP * ECS::ComputeWorldMatrix(m_World, entity);
    }

    vkCmdPushConstants(commandBuffer, m_ShadowPipeline->GetLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Renderer::PushConstants), &pushConstants);

    // Bind and draw — pool-allocated entities use merged buffer with offsets
    if (renderData.poolAlloc.valid && m_GeometryPool) {
        if (!m_GeometryPoolBound) { m_GeometryPool->BindBuffers(commandBuffer); m_GeometryPoolBound = true; }
        vkCmdDrawIndexed(commandBuffer, renderData.poolAlloc.indexCount, 1,
                         renderData.poolAlloc.indexOffset, renderData.poolAlloc.vertexOffset, 0);
    } else if (renderData.vertexBuffer && renderData.indexBuffer) {
        VkBuffer vertexBuffers[] = { computeSkinned
            ? renderData.skinnedVertexBuffer->GetBuffer()
            : renderData.vertexBuffer->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, renderData.indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, renderData.indexCount, 1, 0, 0, 0);
        m_GeometryPoolBound = false;
    }
}

void RenderSystem::CreatePointShadowPipeline() {
    if (!m_PointShadowMap || !m_Pipeline || !m_ShadowVertexShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_PointShadowMap->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_NONE;  // All faces for correct contact shadows
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.depthBiasEnable = true;
    config.depthBiasConstant = 1.25f;
    config.depthBiasSlope = 1.75f;
    config.hasColorAttachment = false;

    m_PointShadowPipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_PointShadowPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_PointShadowPipeline->CreateWithLayout(config, m_ShadowVertexShader.get(), nullptr,
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create point shadow pipeline");
        m_PointShadowPipeline.reset();
    }
}

void RenderSystem::CreateSpotShadowPipeline() {
    if (!m_SpotShadowMap || !m_Pipeline || !m_ShadowVertexShader) return;

    Renderer::PipelineConfig config;
    config.renderPass = m_SpotShadowMap->GetRenderPass();
    config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.depthTest = true;
    config.depthWrite = true;
    config.cullMode = VK_CULL_MODE_FRONT_BIT;  // Render back faces to eliminate self-shadowing on curved geometry
    config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    config.polygonMode = VK_POLYGON_MODE_FILL;
    config.depthBiasEnable = true;
    config.depthBiasConstant = 1.5f;
    config.depthBiasSlope = 1.5f;
    config.hasColorAttachment = false;

    m_SpotShadowPipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_SpotShadowPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
    if (!m_SpotShadowPipeline->CreateWithLayout(config, m_ShadowVertexShader.get(), nullptr,
            m_Pipeline->GetDescriptorSetLayout())) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create spot shadow pipeline");
        m_SpotShadowPipeline.reset();
    }
}

void RenderSystem::SelectShadowLights() {
    m_ShadowPointLights.clear();
    m_ShadowSpotLights.clear();
    m_ActivePointShadowCount = 0;
    m_ActiveSpotShadowCount = 0;

    if (!m_Camera) return;
    Math::Vector3 camPos = m_Camera->GetPosition();

    auto* lightStorageSL = m_World->GetComponentStorage<LightComponent>();
    for (Entity lightEntity : m_CachedLightEntities) {
        LightComponent* light = lightStorageSL ? lightStorageSL->Get(lightEntity) : nullptr;
        TransformComponent* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(lightEntity) : nullptr;
        if (!light || !light->castShadows || !lightTransform) continue;

        Math::Vector3 pos = lightTransform->position;
        Math::Vector3 diff = pos - camPos;
        f32 distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
        f32 score = light->intensity / std::max(distSq, 1.0f);

        if (light->type == LightType::Point) {
            m_ShadowPointLights.push_back({lightEntity, pos, light->range, score});
        } else if (light->type == LightType::Spot) {
            Math::Vector3 forward(0.0f, 0.0f, -1.0f);
            Math::Vector3 dir = lightTransform->rotation.Rotate(forward).Normalized();
            m_ShadowSpotLights.push_back({lightEntity, pos, dir, light->outerConeAngle, light->range, score});
        }
    }

    // Sort by score descending, take top N
    std::sort(m_ShadowPointLights.begin(), m_ShadowPointLights.end(),
        [](const ShadowPointLight& a, const ShadowPointLight& b) { return a.score > b.score; });
    std::sort(m_ShadowSpotLights.begin(), m_ShadowSpotLights.end(),
        [](const ShadowSpotLight& a, const ShadowSpotLight& b) { return a.score > b.score; });

    m_ActivePointShadowCount = static_cast<u32>(std::min(m_ShadowPointLights.size(),
        static_cast<size_t>(MAX_SHADOW_POINT_LIGHTS)));
    m_ActiveSpotShadowCount = static_cast<u32>(std::min(m_ShadowSpotLights.size(),
        static_cast<size_t>(MAX_SHADOW_SPOT_LIGHTS)));

    m_ShadowPointLights.resize(m_ActivePointShadowCount);
    m_ShadowSpotLights.resize(m_ActiveSpotShadowCount);
}

void RenderSystem::RenderPointShadowPass() {
    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    if (m_ShadowCastersDirty) {
        RebuildShadowCasterCache();
    }

    for (u32 lightIdx = 0; lightIdx < m_ActivePointShadowCount; ++lightIdx) {
        auto& sl = m_ShadowPointLights[lightIdx];

        for (u32 face = 0; face < 6; ++face) {
            m_CurrentCascadeVP = Renderer::PointLightShadowMap::ComputeFaceViewProj(
                sl.position, sl.range, face);

            m_PointShadowMap->BeginFacePass(commandBuffer, lightIdx, face);
            m_PointShadowPipeline->Bind(commandBuffer);

            {
                u32 zeroOff = 0;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    m_PointShadowPipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
            }

            for (Entity entity : m_ShadowCasters) {
                auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
                if (xform && !xform->visible) continue;
                RenderEntityShadow(entity, commandBuffer);
            }

            m_PointShadowMap->EndFacePass(commandBuffer);
        }
    }
}

void RenderSystem::RenderSpotShadowPass() {
    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    if (m_ShadowCastersDirty) {
        RebuildShadowCasterCache();
    }

    for (u32 spotIdx = 0; spotIdx < m_ActiveSpotShadowCount; ++spotIdx) {
        auto& sl = m_ShadowSpotLights[spotIdx];

        m_CurrentCascadeVP = Renderer::SpotLightShadowMap::ComputeViewProj(
            sl.position, sl.direction, sl.outerConeAngle, sl.range);

        m_SpotShadowMap->BeginPass(commandBuffer, spotIdx);
        m_SpotShadowPipeline->Bind(commandBuffer);

        {
            u32 zeroOff = 0;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_SpotShadowPipeline->GetLayout(), 0, 1, &m_DescriptorSets[currentFrame], 1, &zeroOff);
        }

        for (Entity entity : m_ShadowCasters) {
            auto* xform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (xform && !xform->visible) continue;
            RenderEntityShadow(entity, commandBuffer);
        }

        m_SpotShadowMap->EndPass(commandBuffer);
    }
}

std::shared_ptr<Renderer::Texture> RenderSystem::GetOrLoadTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    // Check integer-keyed cache first (O(1) after initial load — no string hashing)
    auto idIt = m_TexturePathToId.find(path);
    if (idIt != m_TexturePathToId.end()) {
        return m_TextureById[idIt->second];
    }

    // Check failed cache (avoid retrying broken paths every frame)
    if (m_FailedTextures.count(path)) {
        return nullptr;
    }

    // Load new texture (SVG or raster)
    std::shared_ptr<Renderer::Texture> texture;
    if (Renderer::SVGLoader::IsSVGFile(path)) {
        texture = Renderer::SVGLoader::LoadAsTexture(m_VulkanRenderer->GetContext(), path);
    } else {
        texture = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
        if (!texture->LoadFromFile(path)) {
            texture = nullptr;
        }
    }

    if (!texture) {
        ENJIN_LOG_WARN(Renderer, "Failed to load texture (will not retry): %s", path.c_str());
        m_FailedTextures.insert(path);
        return nullptr;
    }

    ENJIN_LOG_INFO(Renderer, "Loaded texture: %s (%dx%d)",
        path.c_str(), texture->GetWidth(), texture->GetHeight());

    // Cache with integer ID and watch for hot-reload
    u32 texId = static_cast<u32>(m_TextureById.size());
    m_TexturePathToId[path] = texId;
    m_TextureById.push_back(texture);
    m_TextureIdToPath.push_back(path);

    // Register in bindless descriptor set for indexed texture access
    if (m_BindlessManager && texture->IsValid()) {
        auto handle = m_BindlessManager->RegisterTexture(
            texture->GetImageView(), texture->GetSampler());
        if (handle != UINT32_MAX) {
            m_TextureBindlessHandles[texture.get()] = handle;
        }
    }

    m_TextureWatcher.Watch(path, [this](const std::string& changedPath) {
        ENJIN_LOG_INFO(Renderer, "Texture changed, reloading: %s", changedPath.c_str());
        auto pathIt = m_TexturePathToId.find(changedPath);
        if (pathIt != m_TexturePathToId.end()) {
            auto newTex = std::make_shared<Renderer::Texture>(m_VulkanRenderer->GetContext());
            if (newTex->LoadFromFile(changedPath)) {
                m_TextureById[pathIt->second] = newTex;
                // Invalidate cached raw pointers on all materials referencing this texture
                if (m_World) {
                    for (ECS::Entity e : m_World->GetEntitiesWithComponent<ECS::MaterialComponent>()) {
                        auto* mat = m_World->GetComponent<ECS::MaterialComponent>(e);
                        if (!mat) continue;
                        if (mat->baseColorTexturePath == changedPath ||
                            mat->normalTexturePath == changedPath ||
                            mat->heightTexturePath == changedPath ||
                            mat->metallicRoughnessTexturePath == changedPath ||
                            mat->emissiveTexturePath == changedPath) {
                            mat->textureCacheDirty = true;
                        }
                    }
                }
                // Invalidate sprite atlas so it rebuilds with the new texture data
                if (m_SpriteAtlas) m_SpriteAtlas->Invalidate();
            }
        }
    });
    return texture;
}

void RenderSystem::UpdateTextureDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) {
        return;
    }

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Update binding 3 (base color texture) with the new texture
    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 3;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateHeightTextureDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) {
        return;
    }

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 5;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateNormalMapDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) {
        return;
    }

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 6;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateMetallicRoughnessDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 8;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateEmissiveDescriptor(Renderer::Texture* texture) {
    if (!texture || !texture->IsValid()) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    VkDescriptorImageInfo imageInfo = texture->GetDescriptorInfo();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 9;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateEntityTextureDescriptors(
    Renderer::Texture* baseColor,
    Renderer::Texture* height,
    Renderer::Texture* normal,
    Renderer::Texture* metallicRoughness,
    Renderer::Texture* emissive,
    Renderer::Texture* matcap)
{
    // Skip per-entity descriptor writes when bindless is active —
    // textures are indexed via MaterialSSBO handles instead.
    if (m_BindlessManager) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    VkDescriptorSet dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();

    // Use default white texture for any nullptr slots
    Renderer::Texture* defaultTex = m_DefaultWhiteTexture.get();
    if (!defaultTex || !defaultTex->IsValid()) return;

    Renderer::Texture* texBase = (baseColor && baseColor->IsValid()) ? baseColor : defaultTex;
    Renderer::Texture* texHeight = (height && height->IsValid()) ? height : defaultTex;
    Renderer::Texture* texNormal = (normal && normal->IsValid()) ? normal : defaultTex;
    Renderer::Texture* texMR = (metallicRoughness && metallicRoughness->IsValid()) ? metallicRoughness : defaultTex;
    Renderer::Texture* texEmissive = (emissive && emissive->IsValid()) ? emissive : defaultTex;
    Renderer::Texture* texMatcap = (matcap && matcap->IsValid()) ? matcap : defaultTex;

    // Skip vkUpdateDescriptorSets if these textures are already bound
    MaterialComponent::TextureKey currentKey{ texBase, texHeight, texNormal, texMR, texEmissive, texMatcap };
    if (currentKey == m_LastBound.textureKey) { ++m_DescriptorCacheHits; return; }
    m_LastBound.textureKey = currentKey;
    ++m_DescriptorCacheWrites;

    // Collect image infos (must persist until vkUpdateDescriptorSets returns)
    VkDescriptorImageInfo imageInfos[6];
    imageInfos[0] = texBase->GetDescriptorInfo();
    imageInfos[1] = texHeight->GetDescriptorInfo();
    imageInfos[2] = texNormal->GetDescriptorInfo();
    imageInfos[3] = texMR->GetDescriptorInfo();
    imageInfos[4] = texEmissive->GetDescriptorInfo();
    imageInfos[5] = texMatcap->GetDescriptorInfo();

    // Bindings: 3=baseColor, 5=height, 6=normal, 8=metallicRoughness, 9=emissive, 18=matcap
    VkWriteDescriptorSet writes[6] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = dstSet;
    writes[0].dstBinding = 3;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &imageInfos[0];

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = dstSet;
    writes[1].dstBinding = 5;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageInfos[1];

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = dstSet;
    writes[2].dstBinding = 6;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &imageInfos[2];

    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = dstSet;
    writes[3].dstBinding = 8;
    writes[3].dstArrayElement = 0;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[3].descriptorCount = 1;
    writes[3].pImageInfo = &imageInfos[3];

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = dstSet;
    writes[4].dstBinding = 9;
    writes[4].dstArrayElement = 0;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].descriptorCount = 1;
    writes[4].pImageInfo = &imageInfos[4];

    writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[5].dstSet = dstSet;
    writes[5].dstBinding = 18;
    writes[5].dstArrayElement = 0;
    writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[5].descriptorCount = 1;
    writes[5].pImageInfo = &imageInfos[5];

    // Single batched call instead of 6 individual calls
    vkUpdateDescriptorSets(device, 6, writes, 0, nullptr);
}

#if !ENJIN_RENDERER_WEBGPU
// ============================================================================
// GPU compute skinning (ADR-0002 Phase 1). Skins bind-pose vertices to world space once per
// frame into a per-entity output buffer; raster passes then bind that buffer with FLAG_SKINNED
// cleared. Pipeline is created lazily from embedded SPIR-V; descriptor sets are allocated
// per-dispatch from a per-frame-in-flight pool that is reset at frame start.
// ============================================================================

bool RenderSystem::InitComputeSkinning() {
    if (m_SkinningPipeline != VK_NULL_HANDLE) return true;  // already initialized
    if (!m_VulkanRenderer || !m_VulkanRenderer->GetContext()) return false;
    VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();
    if (device == VK_NULL_HANDLE) return false;

    // Descriptor set layout: bindings 0=in verts, 1=bone matrices, 2=out verts (all storage).
    VkDescriptorSetLayoutBinding bindings[3]{};
    for (u32 i = 0; i < 3; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo dslInfo{};
    dslInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslInfo.bindingCount = 3;
    dslInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &dslInfo, nullptr, &m_SkinningDescSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Compute skinning: failed to create descriptor set layout");
        return false;
    }

    // Pipeline layout with a single push constant: uint vertexCount.
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(u32);
    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &m_SkinningDescSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pcRange;
    if (vkCreatePipelineLayout(device, &plInfo, nullptr, &m_SkinningPipelineLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Compute skinning: failed to create pipeline layout");
        return false;
    }

    // Compute pipeline from embedded SPIR-V.
    VkShaderModuleCreateInfo smInfo{};
    smInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smInfo.codeSize = Renderer::SkinningComputeShaderDataSize;
    smInfo.pCode = reinterpret_cast<const u32*>(Renderer::SkinningComputeShaderData);
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &smInfo, nullptr, &module) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Compute skinning: failed to create shader module");
        return false;
    }
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = module;
    stage.pName = "main";
    VkComputePipelineCreateInfo cpInfo{};
    cpInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpInfo.stage = stage;
    cpInfo.layout = m_SkinningPipelineLayout;
    VkResult pr = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &cpInfo, nullptr, &m_SkinningPipeline);
    vkDestroyShaderModule(device, module, nullptr);
    if (pr != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Compute skinning: failed to create compute pipeline");
        return false;
    }

    // One descriptor pool per frame-in-flight, each holding room for many per-dispatch sets.
    constexpr u32 kMaxSkinnedPerFrame = 512;
    u32 framesInFlight = m_VulkanRenderer->GetFramesInFlight();
    m_SkinningDescPools.assign(framesInFlight, VK_NULL_HANDLE);
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = kMaxSkinnedPerFrame * 3;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = kMaxSkinnedPerFrame;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    for (u32 i = 0; i < framesInFlight; ++i) {
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_SkinningDescPools[i]) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Compute skinning: failed to create descriptor pool");
            return false;
        }
    }

    ENJIN_LOG_INFO(Renderer, "Compute skinning pipeline initialized (%u frames in flight)", framesInFlight);
    return true;
}

void RenderSystem::ShutdownComputeSkinning() {
    if (!m_VulkanRenderer || !m_VulkanRenderer->GetContext()) return;
    VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();
    if (device == VK_NULL_HANDLE) return;
    for (VkDescriptorPool& pool : m_SkinningDescPools) {
        if (pool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(device, pool, nullptr); pool = VK_NULL_HANDLE; }
    }
    m_SkinningDescPools.clear();
    if (m_SkinningPipeline != VK_NULL_HANDLE)       { vkDestroyPipeline(device, m_SkinningPipeline, nullptr); m_SkinningPipeline = VK_NULL_HANDLE; }
    if (m_SkinningPipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(device, m_SkinningPipelineLayout, nullptr); m_SkinningPipelineLayout = VK_NULL_HANDLE; }
    if (m_SkinningDescSetLayout != VK_NULL_HANDLE)  { vkDestroyDescriptorSetLayout(device, m_SkinningDescSetLayout, nullptr); m_SkinningDescSetLayout = VK_NULL_HANDLE; }
}

void RenderSystem::BeginComputeSkinningFrame() {
    // Reset only the current frame-in-flight's pool. Safe because the engine has already waited
    // on this frame index's fence before recording, so the GPU is done with its prior sets.
    if (m_SkinningDescPools.empty() || !m_VulkanRenderer || !m_VulkanRenderer->GetContext()) return;
    u32 frame = m_VulkanRenderer->GetCurrentFrameIndex() % static_cast<u32>(m_SkinningDescPools.size());
    if (m_SkinningDescPools[frame] != VK_NULL_HANDLE) {
        vkResetDescriptorPool(m_VulkanRenderer->GetContext()->GetDevice(), m_SkinningDescPools[frame], 0);
    }
}

bool RenderSystem::DispatchComputeSkinning(VkCommandBuffer cmd, EntityRenderData& renderData,
                                           Renderer::VulkanBuffer* boneBuffer) {
    if (!InitComputeSkinning()) return false;
    if (!cmd || !renderData.vertexBuffer || !boneBuffer || renderData.vertexCount == 0) return false;
    VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();

    // Allocate the deformed output buffer on first use (VERTEX so raster can draw it, STORAGE so
    // the compute shader can write it). Same 136-byte stride as the bind-pose vertices.
    const VkDeviceSize outSize = static_cast<VkDeviceSize>(renderData.vertexCount) * sizeof(MeshComponent::Vertex);
    if (!renderData.skinnedVertexBuffer || renderData.skinnedVertexBuffer->GetSize() < outSize) {
        renderData.skinnedVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
        if (!renderData.skinnedVertexBuffer->Create(outSize,
                static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),
                false)) {
            renderData.skinnedVertexBuffer.reset();
            return false;
        }
    }

    // Allocate a descriptor set from this frame's pool and point it at in/bone/out buffers.
    u32 frame = m_VulkanRenderer->GetCurrentFrameIndex() % static_cast<u32>(m_SkinningDescPools.size());
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_SkinningDescPools[frame];
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_SkinningDescSetLayout;
    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &allocInfo, &set) != VK_SUCCESS) {
        // Pool exhausted this frame (more than kMaxSkinnedPerFrame skinned meshes) — skip; this
        // mesh keeps its bind-pose buffer and the raster fallback skins it. Logged sparsely.
        static u32 s_warned = 0;
        if ((s_warned++ % 240) == 0) ENJIN_LOG_WARN(Renderer, "Compute skinning: descriptor pool exhausted this frame");
        return false;
    }

    VkDescriptorBufferInfo inInfo{ renderData.vertexBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo boneInfo{ boneBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo outInfo{ renderData.skinnedVertexBuffer->GetBuffer(), 0, VK_WHOLE_SIZE };
    VkWriteDescriptorSet writes[3]{};
    const VkDescriptorBufferInfo* infos[3] = { &inInfo, &boneInfo, &outInfo };
    for (u32 i = 0; i < 3; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = set;
        writes[i].dstBinding = i;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].descriptorCount = 1;
        writes[i].pBufferInfo = infos[i];
    }
    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_SkinningPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_SkinningPipelineLayout, 0, 1, &set, 0, nullptr);
    u32 vtxCount = renderData.vertexCount;
    vkCmdPushConstants(cmd, m_SkinningPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(u32), &vtxCount);
    const u32 groups = (renderData.vertexCount + 63u) / 64u;
    vkCmdDispatch(cmd, groups, 1, 1);
    return true;
}

void RenderSystem::RunComputeSkinningPass(VkCommandBuffer cmd) {
    if (!m_ComputeSkinningEnabled || !cmd || !m_World) return;
    if (!InitComputeSkinning()) return;
    BeginComputeSkinningFrame();

    bool anySkinned = false;
    const auto& meshEntities = m_World->GetEntitiesWithComponent<MeshComponent>();
    for (Entity entity : meshEntities) {
        if (!m_World->IsValid(entity)) continue;

        // Skinned meshes only: must resolve to an animator with non-empty skinning matrices.
        AnimatorComponent* animComp = ResolveAnimator(entity);
        if (!animComp) continue;
        const auto& mats = animComp->animator.GetSkinningMatrices();
        if (mats.empty()) continue;

        // Get-or-create per-entity buffers (same pattern as the draw loop; created once, reused).
        EntityRenderData* pRD = GetOrCreateRenderData(entity);
        if (!pRD) continue;
        EntityRenderData& rd = *pRD;
        rd.skinnedThisFrame = false;

        // Pooled meshes have no per-entity vertex buffer; Phase 1 also skips morph-target meshes
        // (the compute shader doesn't apply morph yet — they fall back to vertex-shader skinning).
        if (!rd.vertexBuffer || rd.vertexCount == 0) continue;
        if (m_World->HasComponent<MorphTargetComponent>(entity)) continue;

        // Ensure a bone buffer and upload this frame's matrices for the dispatch.
        if (!rd.boneBuffer) {
            rd.boneBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
            if (!rd.boneBuffer->Create(mats.size() * sizeof(Math::Matrix4),
                                       Renderer::BufferUsage::Storage, true)) {
                rd.boneBuffer.reset();
                continue;
            }
        }
        rd.boneBuffer->UploadData(mats.data(), mats.size() * sizeof(Math::Matrix4));

        if (DispatchComputeSkinning(cmd, rd, rd.boneBuffer.get())) {
            rd.skinnedThisFrame = true;
            anySkinned = true;
        }
    }

    if (anySkinned) {
        // Compute writes -> vertex-input reads: the deformed buffer is drawn as a vertex buffer.
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
}
#endif // !ENJIN_RENDERER_WEBGPU

void RenderSystem::UpdateBoneDescriptor(Renderer::VulkanBuffer* boneBuffer) {
    if (!boneBuffer) return;

    // Skip if this bone buffer is already bound
    if (boneBuffer == m_LastBound.boneBuffer) return;
    m_LastBound.boneBuffer = boneBuffer;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = boneBuffer->GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = boneBuffer->GetSize();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    descriptorWrite.dstBinding = 7;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}

void RenderSystem::UpdateMorphDescriptor(Renderer::VulkanBuffer* morphBuffer) {
    if (!morphBuffer) return;
    if (morphBuffer == m_LastBound.morphBuffer) return;
    m_LastBound.morphBuffer = morphBuffer;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = morphBuffer->GetBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = morphBuffer->GetSize();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = (*m_ActiveDescriptorSets)[GetActiveBufferIndex(currentFrame)];
    write.dstBinding = 20;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &write, 0, nullptr);
}

void RenderSystem::UploadMorphTargetSSBO(Entity entity, ECS::MorphTargetComponent& morph, EntityRenderData& rd) {
    auto* meshComp = m_World->GetComponent<MeshComponent>(entity);
    if (!meshComp) return;
    u32 vertexCount = static_cast<u32>(meshComp->vertices.size());
    u32 targetCount = static_cast<u32>(morph.targets.size());
    if (targetCount == 0 || vertexCount == 0) return;

    u32 headerSize = 2 + targetCount;
    usize totalFloats = headerSize + static_cast<usize>(targetCount) * vertexCount * 6;
    usize totalBytes = totalFloats * sizeof(f32);

    if (!rd.morphBuffer || rd.morphBuffer->GetSize() < totalBytes) {
        rd.morphBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
        if (!rd.morphBuffer->Create(totalBytes, Renderer::BufferUsage::Storage, true)) {
            rd.morphBuffer.reset();
            return;
        }
        morph.deltasDirty = true;
    }

    if (morph.deltasDirty) {
        std::vector<f32> ssbo(totalFloats, 0.0f);
        u32 vc = vertexCount, tc = targetCount;
        std::memcpy(&ssbo[0], &vc, 4);
        std::memcpy(&ssbo[1], &tc, 4);
        for (u32 t = 0; t < targetCount; ++t) ssbo[2 + t] = morph.weights[t];
        for (u32 t = 0; t < targetCount; ++t) {
            const auto& tgt = morph.targets[t];
            for (u32 v = 0; v < vertexCount && v < static_cast<u32>(tgt.deltas.size()); ++v) {
                u32 idx = headerSize + t * vertexCount * 6 + v * 6;
                ssbo[idx+0] = tgt.deltas[v].positionDelta.x;
                ssbo[idx+1] = tgt.deltas[v].positionDelta.y;
                ssbo[idx+2] = tgt.deltas[v].positionDelta.z;
                ssbo[idx+3] = tgt.deltas[v].normalDelta.x;
                ssbo[idx+4] = tgt.deltas[v].normalDelta.y;
                ssbo[idx+5] = tgt.deltas[v].normalDelta.z;
            }
        }
        rd.morphBuffer->UploadData(ssbo.data(), totalBytes);
        morph.deltasDirty = false;
        morph.weightsDirty = false;
    } else if (morph.weightsDirty) {
        std::vector<f32> hdr(headerSize);
        u32 vc = vertexCount, tc = targetCount;
        std::memcpy(&hdr[0], &vc, 4);
        std::memcpy(&hdr[1], &tc, 4);
        for (u32 t = 0; t < targetCount; ++t) hdr[2 + t] = morph.weights[t];
        rd.morphBuffer->UploadData(hdr.data(), headerSize * sizeof(f32));
        morph.weightsDirty = false;
    }
}

void RenderSystem::EnsureWaterMeshes() {
    for (Entity entity : m_World->GetEntitiesWithComponent<WaterVolumeComponent>()) {
        auto* waterVol = m_CachedWaterVolumeStorage ? m_CachedWaterVolumeStorage->Get(entity) : m_World->GetComponent<WaterVolumeComponent>(entity);
        if (!waterVol) continue;
        if (waterVol->meshCreated && m_World->GetComponent<MeshComponent>(entity)) continue;

        // Create a subdivided plane mesh for the water surface
        MeshComponent mesh;
        f32 hx = waterVol->halfExtents.x;
        f32 hz = waterVol->halfExtents.z;

        const u32 segsX = 20;
        const u32 segsZ = 20;
        mesh.vertices.reserve((segsX + 1) * (segsZ + 1));
        mesh.indices.reserve(segsX * segsZ * 6);

        for (u32 zi = 0; zi <= segsZ; ++zi) {
            for (u32 xi = 0; xi <= segsX; ++xi) {
                MeshComponent::Vertex v;
                f32 u = static_cast<f32>(xi) / segsX;
                f32 vt = static_cast<f32>(zi) / segsZ;
                v.position = Math::Vector3(
                    -hx + u * 2.0f * hx,
                    0.0f,
                    -hz + vt * 2.0f * hz
                );
                v.normal = Math::Vector3(0.0f, 1.0f, 0.0f);
                v.uv = Math::Vector2(u, vt);

                // Compute minimum distance to any edge (normalized 0=edge, 1=center)
                f32 distLeft = u;
                f32 distRight = 1.0f - u;
                f32 distTop = vt;
                f32 distBottom = 1.0f - vt;
                f32 minEdgeDist = std::min(std::min(distLeft, distRight), std::min(distTop, distBottom));
                // Normalize so center = 1.0 (max edge dist is 0.5)
                f32 edgeDist = std::min(minEdgeDist * 2.0f, 1.0f);

                // R = water color red (unused by shader for water), G = edge distance, B = unused, A = opacity
                v.color = Math::Vector4(
                    waterVol->waterColor.x,
                    edgeDist,
                    waterVol->waterColor.z,
                    waterVol->opacity
                );
                mesh.vertices.push_back(v);
            }
        }

        for (u32 zi = 0; zi < segsZ; ++zi) {
            for (u32 xi = 0; xi < segsX; ++xi) {
                u32 tl = zi * (segsX + 1) + xi;
                u32 tr = tl + 1;
                u32 bl = (zi + 1) * (segsX + 1) + xi;
                u32 br = bl + 1;

                mesh.indices.push_back(tl);
                mesh.indices.push_back(bl);
                mesh.indices.push_back(tr);

                mesh.indices.push_back(tr);
                mesh.indices.push_back(bl);
                mesh.indices.push_back(br);
            }
        }

        m_World->AddComponent<MeshComponent>(entity, std::move(mesh));

        // Add material with water visual properties based on water type
        MaterialComponent material;
        material.baseColor = waterVol->waterColor;
        material.opacity = waterVol->opacity;
        material.doubleSided = true;
        material.castShadows = false;
        material.alphaMode = static_cast<MaterialComponent::AlphaMode>(0);  // Opaque — writes depth

        // Water type presets for material properties
        switch (waterVol->waterType) {
            case WaterType::Ocean:
                material.metallic = 0.4f;
                material.roughness = 0.05f;
                break;
            case WaterType::River:
                material.metallic = 0.25f;
                material.roughness = 0.15f;
                break;
            case WaterType::Pond:
                material.metallic = 0.2f;
                material.roughness = 0.2f;
                break;
            case WaterType::Lake:
            default:
                material.metallic = 0.3f;
                material.roughness = 0.1f;
                break;
        }

        m_World->AddComponent<MaterialComponent>(entity, material);

        SetupEntityBuffers(entity);
        waterVol->meshCreated = true;

        ENJIN_LOG_INFO(Renderer, "Created water surface mesh for entity %llu (%.0f x %.0f)",
            entity, hx * 2.0f, hz * 2.0f);
    }
}

void RenderSystem::EnsureWater3DMeshes() {
    if (!m_World) return;
    for (Entity entity : m_World->GetEntitiesWithComponent<Water3DComponent>()) {
        if (!m_World->IsValid(entity)) continue;
        auto* water3d = m_CachedWater3DStorage ? m_CachedWater3DStorage->Get(entity) : m_World->GetComponent<Water3DComponent>(entity);
        if (!water3d) continue;
        if (water3d->meshCreated && m_World->GetComponent<MeshComponent>(entity)) continue;
        // Guard against zero/negative dimensions
        if (water3d->settings.width < 0.01f || water3d->settings.depth < 0.01f) continue;

        // Use Water3D to build the initial mesh on this entity
        Effects::Water3D builder;
        builder.Initialize(water3d->settings);
        builder.BuildEntityMesh(m_World, entity);

        SetupEntityBuffers(entity);
        water3d->meshCreated = true;

        ENJIN_LOG_INFO(Renderer, "Created Water3D surface mesh for entity %llu (%.0f x %.0f)",
            entity, water3d->settings.width, water3d->settings.depth);
    }
}

void RenderSystem::RenderWeatherParticles(const Effects::WeatherSystem& weather, bool isRain,
                                           u32 viewportWidth, u32 viewportHeight) {
    if (!m_WeatherRenderer || !m_Renderer || !m_Initialized || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    m_WeatherRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                              GetActiveBufferIndex(currentFrame), weather, isRain,
                              viewportWidth, viewportHeight);
}

void RenderSystem::RenderParticles(u32 viewportWidth, u32 viewportHeight) {
    if (!m_ParticleRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    m_ParticleRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                               GetActiveBufferIndex(currentFrame), m_World,
                               viewportWidth, viewportHeight);
}

void RenderSystem::RenderElementalParticles(const Effects::ElementalSystem& elementalSystem,
                                             u32 viewportWidth, u32 viewportHeight) {
    if (!m_ParticleRenderer || !m_Renderer || !m_Initialized || !m_ActiveDescriptorSets) return;
    if (elementalSystem.GetActiveCount() == 0) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    m_ParticleRenderer->RenderElementalParticles(commandBuffer, *m_ActiveDescriptorSets,
                                                  GetActiveBufferIndex(currentFrame), elementalSystem,
                                                  viewportWidth, viewportHeight);
}

void RenderSystem::RenderFluid(u32 viewportWidth, u32 viewportHeight) {
    if (!m_FluidRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    m_FluidRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                             GetActiveBufferIndex(currentFrame), m_World,
                             viewportWidth, viewportHeight);
}

void RenderSystem::SetFluidSimulation(Effects::FluidSimulation* sim) {
    if (m_FluidRenderer) {
        m_FluidRenderer->SetFluidSimulation(sim);
    }
}

void RenderSystem::RenderGrass(u32 viewportWidth, u32 viewportHeight) {
    if (!m_GrassRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    m_GrassRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                            GetActiveBufferIndex(currentFrame), m_World,
                            viewportWidth, viewportHeight);
}

void RenderSystem::RenderShrubs(u32 viewportWidth, u32 viewportHeight) {
    if (!m_ShrubRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    m_ShrubRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                            GetActiveBufferIndex(currentFrame), m_World,
                            viewportWidth, viewportHeight);
}

void RenderSystem::RenderTrees(u32 viewportWidth, u32 viewportHeight) {
    if (!m_TreeRenderer || !m_Renderer || !m_Initialized || !m_World || !m_ActiveDescriptorSets) return;

    VkCommandBuffer commandBuffer = m_VulkanRenderer->GetCurrentCommandBuffer();
    if (commandBuffer == VK_NULL_HANDLE) return;

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();
    m_TreeRenderer->Render(commandBuffer, *m_ActiveDescriptorSets,
                           GetActiveBufferIndex(currentFrame), m_World,
                           viewportWidth, viewportHeight);
}

void RenderSystem::RecreateEffectPipelinesForRenderPass(VkRenderPass renderPass) {
    if (!m_Pipeline) return;
    m_OffscreenRenderPass = renderPass;  // Cache for RecreatePipelines
    VkDescriptorSetLayout layout = m_Pipeline->GetDescriptorSetLayout();

    if (m_WeatherRenderer) {
        m_WeatherRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_GrassRenderer) {
        m_GrassRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_ShrubRenderer) {
        m_ShrubRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_ParticleRenderer) {
        m_ParticleRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_FluidRenderer) {
        m_FluidRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_TreeRenderer) {
        m_TreeRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }
    if (m_SpriteBatchRenderer) {
        m_SpriteBatchRenderer->RecreateForRenderPass(renderPass, layout, 1);
    }

    // Recreate main pipeline for offscreen render pass (fixes SRGB vs UNORM format mismatch
    // that causes NVIDIA driver crash on 2D scenes)
    {
        Renderer::PipelineConfig config;
        config.renderPass = renderPass;
        config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        config.depthTest = true;
        config.depthWrite = true;
        config.cullMode = m_BackfaceCulling ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
        config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        config.polygonMode = m_WireframeMode ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
        config.msaaSamples = VK_SAMPLE_COUNT_1_BIT;  // Offscreen RT is always 1 sample
        config.colorAttachmentCount = 1; // Single color output (no MRT velocity — avoids NVIDIA teal)

        m_OffscreenPipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_OffscreenPipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
        if (!m_OffscreenPipeline->Create(config, m_VertexShader.get(), m_FragmentShader.get())) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create offscreen pipeline");
            m_OffscreenPipeline.reset();
        }
    }

    // Offscreen line pipeline (editor grid inside offscreen render target)
    {
        Renderer::PipelineConfig config;
        config.renderPass = renderPass;
        config.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        config.depthTest = true;
        config.depthWrite = false;
        config.cullMode = VK_CULL_MODE_NONE;
        config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        config.polygonMode = VK_POLYGON_MODE_FILL;
        config.alphaBlend = true;
        config.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        config.colorAttachmentCount = 1;

        m_OffscreenLinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_OffscreenLinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
        if (!m_OffscreenLinePipeline->CreateWithLayout(config, m_VertexShader.get(), m_FragmentShader.get(), layout)) {
            ENJIN_LOG_WARN(Renderer, "Failed to create offscreen line pipeline");
            m_OffscreenLinePipeline.reset();
        }
    }

    // Offscreen outline pipeline (geometry outlines inside offscreen render target)
    if (m_OutlineVertexShader && m_OutlineFragmentShader) {
        Renderer::PipelineConfig config;
        config.renderPass = renderPass;
        config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        config.depthTest = true;
        config.depthWrite = true;
        config.cullMode = VK_CULL_MODE_FRONT_BIT;
        config.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        config.polygonMode = VK_POLYGON_MODE_FILL;
        config.msaaSamples = VK_SAMPLE_COUNT_1_BIT;
        config.colorAttachmentCount = 1;

        m_OffscreenOutlinePipeline = std::make_unique<Renderer::VulkanPipeline>(m_VulkanRenderer->GetContext());
    if (m_BindlessManager) m_OffscreenOutlinePipeline->SetBindlessLayout(m_BindlessManager->GetDescriptorSetLayout());
        if (!m_OffscreenOutlinePipeline->CreateWithLayout(config, m_OutlineVertexShader.get(), m_OutlineFragmentShader.get(), layout)) {
            ENJIN_LOG_WARN(Renderer, "Failed to create offscreen outline pipeline");
            m_OffscreenOutlinePipeline.reset();
        }
    }

    // Note: skybox pipeline is NOT recreated here — it was created for the swapchain
    // render pass in Initialize() and works in both passes via driver-level render pass
    // compatibility (SRGB/UNORM same memory layout). Destroying and recreating it here
    // would invalidate the descriptor set layout and break skybox rendering entirely.
}

void RenderSystem::SetSkybox(const Renderer::SkyboxConfig& config) {
    // Defer skybox config change to the start of the next frame — the old cubemap
    // may still be referenced by the current frame's command buffer which is being
    // recorded. Applying the change at the top of Update() is safe because all
    // in-flight frames have been submitted and can be waited on.
    m_PendingSkybox = config;
    m_PendingSkyboxConfig = true;
}

void RenderSystem::CreateSkyboxCubeVBO() {
    float cubeVertices[] = {
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f,-1.0f,  1.0f, 1.0f,-1.0f,
         1.0f, 1.0f,-1.0f, -1.0f, 1.0f,-1.0f, -1.0f,-1.0f,-1.0f,
        -1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,  1.0f,-1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, -1.0f,-1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f, -1.0f,-1.0f,-1.0f, -1.0f, 1.0f,-1.0f,
        -1.0f,-1.0f,-1.0f, -1.0f, 1.0f, 1.0f, -1.0f,-1.0f, 1.0f,
         1.0f, 1.0f, 1.0f,  1.0f, 1.0f,-1.0f,  1.0f,-1.0f,-1.0f,
         1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,-1.0f,-1.0f,  1.0f,-1.0f, 1.0f,  1.0f,-1.0f,-1.0f,
         1.0f,-1.0f, 1.0f, -1.0f,-1.0f,-1.0f, -1.0f,-1.0f, 1.0f,
        -1.0f, 1.0f,-1.0f,  1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f,-1.0f,
    };

    m_SkyboxVertexBuffer = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
    if (!m_SkyboxVertexBuffer->Create(sizeof(cubeVertices), Renderer::BufferUsage::Vertex, true)) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create skybox VBO");
        m_SkyboxVertexBuffer.reset();
        return;
    }
    m_SkyboxVertexBuffer->UploadData(cubeVertices, sizeof(cubeVertices));
}

void RenderSystem::CreateSkyboxPipeline(VkRenderPass renderPass) {
    ENJIN_LOG_INFO(Renderer, "CreateSkyboxPipeline called");

    if (!m_Renderer || !m_VulkanRenderer->GetContext()) {
        ENJIN_LOG_ERROR(Renderer, "CreateSkyboxPipeline: No renderer or context!");
        return;
    }

    auto* context = m_VulkanRenderer->GetContext();
    VkDevice device = context->GetDevice();

    // Load skybox shaders
    auto skyboxVert = std::make_unique<Renderer::VulkanShader>(context);
    if (!skyboxVert->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SkyboxVertexShaderData),
        Renderer::ShaderData::SkyboxVertexShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Skybox vertex shader not available, skybox disabled");
        return;
    }

    auto skyboxFrag = std::make_unique<Renderer::VulkanShader>(context);
    if (!skyboxFrag->LoadFromSPIRV(
        reinterpret_cast<const u8*>(Renderer::ShaderData::SkyboxFragmentShaderData),
        Renderer::ShaderData::SkyboxFragmentShaderDataSize)) {
        ENJIN_LOG_WARN(Renderer, "Skybox fragment shader not available, skybox disabled");
        return;
    }

    // Create descriptor set layout (binding 0: UBO, binding 1: cubemap sampler)
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_SkyboxDescriptorSetLayoutHandle) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox descriptor set layout");
        return;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_SkyboxDescriptorSetLayoutHandle;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_SkyboxPipelineLayoutHandle) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox pipeline layout");
        return;
    }

    // Vertex input: position only (vec3)
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 3;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc{};
    attrDesc.binding = 0;
    attrDesc.location = 0;
    attrDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDesc.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &attrDesc;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;  // No culling — camera is always inside the skybox cube
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = m_VulkanRenderer->GetMSAASamples();

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    // MRT: second attachment for velocity buffer (write zero velocity for skybox)
    VkPipelineColorBlendAttachmentState velocityBlendAttachment{};
    velocityBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    std::array<VkPipelineColorBlendAttachmentState, 2> skyboxBlendAttachments = { colorBlendAttachment, velocityBlendAttachment };

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 2; // Swapchain MRT: color + velocity (main pass only; offscreen uses 1)
    colorBlending.pAttachments = skyboxBlendAttachments.data();

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = skyboxVert->GetModule();
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = skyboxFrag->GetModule();
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_SkyboxPipelineLayoutHandle;
    pipelineInfo.renderPass = (renderPass != VK_NULL_HANDLE) ? renderPass : m_VulkanRenderer->GetRenderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_SkyboxPipelineHandle) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox pipeline");
        return;
    }

    // Create descriptor pool for skybox
    constexpr u32 framesInFlight = 2;
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = framesInFlight;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = framesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = framesInFlight;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_SkyboxDescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to create skybox descriptor pool");
        return;
    }

    // Allocate descriptor sets
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_SkyboxDescriptorSetLayoutHandle);
    m_SkyboxDescriptorSets.resize(framesInFlight);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_SkyboxDescriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(device, &allocInfo, m_SkyboxDescriptorSets.data()) != VK_SUCCESS) {
        ENJIN_LOG_WARN(Renderer, "Failed to allocate skybox descriptor sets");
        return;
    }

    // Create UBOs for skybox viewProj matrix
    m_SkyboxUniformBuffers.resize(framesInFlight);
    for (u32 i = 0; i < framesInFlight; ++i) {
        m_SkyboxUniformBuffers[i] = std::make_unique<Renderer::VulkanBuffer>(m_VulkanRenderer->GetContext());
        m_SkyboxUniformBuffers[i]->Create(sizeof(Math::Matrix4), Renderer::BufferUsage::Uniform, true);
    }

    ENJIN_LOG_INFO(Renderer, "Skybox pipeline created");
}

void RenderSystem::RenderSkybox(VkCommandBuffer commandBuffer,
                                const VkViewport* viewportOverride,
                                const VkRect2D* scissorOverride) {
    if (!m_Skybox.IsValid() || m_SkyboxPipelineHandle == VK_NULL_HANDLE || !m_SkyboxVertexBuffer || !m_Camera) {
        return;
    }

    u32 currentFrame = m_VulkanRenderer->GetCurrentFrameIndex();

    // Build view-projection matrix with translation removed (keep skybox centered on camera)
    Math::Matrix4 view = m_Camera->GetViewMatrix();
    // Zero out the translation column (column 3, rows 0-2) - flat column-major layout
    view.m[12] = 0.0f;
    view.m[13] = 0.0f;
    view.m[14] = 0.0f;
    Math::Matrix4 viewProj = m_Camera->GetProjectionMatrix() * view;

    // Upload UBO
    m_SkyboxUniformBuffers[currentFrame]->UploadData(&viewProj, sizeof(Math::Matrix4));

    // Update descriptor set with UBO and cubemap
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_SkyboxUniformBuffers[currentFrame]->GetBuffer();
    uboInfo.offset = 0;
    uboInfo.range = sizeof(Math::Matrix4);

    VkDescriptorImageInfo cubemapInfo = m_Skybox.GetDescriptorInfo();

    std::array<VkWriteDescriptorSet, 2> writes{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_SkyboxDescriptorSets[currentFrame];
    writes[0].dstBinding = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &uboInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_SkyboxDescriptorSets[currentFrame];
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &cubemapInfo;

    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

    // Set viewport and scissor — use overrides if provided (offscreen / splitscreen),
    // otherwise fall back to swapchain extent (main pass single-camera)
    if (viewportOverride) {
        vkCmdSetViewport(commandBuffer, 0, 1, viewportOverride);
    } else {
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        VkViewport viewport{};
        viewport.width = static_cast<f32>(extent.width);
        viewport.height = static_cast<f32>(extent.height);
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    }

    if (scissorOverride) {
        vkCmdSetScissor(commandBuffer, 0, 1, scissorOverride);
    } else {
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        VkRect2D scissor{};
        scissor.extent = extent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    // Bind skybox pipeline and draw
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_SkyboxPipelineHandle);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_SkyboxPipelineLayoutHandle, 0, 1, &m_SkyboxDescriptorSets[currentFrame], 0, nullptr);

    VkBuffer vertexBuffers[] = { m_SkyboxVertexBuffer->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}

// ---------------------------------------------------------------------------
// Ray Tracing subsystem lifecycle
// ---------------------------------------------------------------------------

bool RenderSystem::IsRayTracingSupported() const {
    if (!m_Renderer || !m_VulkanRenderer->GetContext()) return false;
    return m_VulkanRenderer->GetContext()->IsRayTracingSupported();
}

void RenderSystem::InitializeRayTracing() {
    if (!m_RTEnabled) {
        ENJIN_LOG_INFO(Renderer, "Ray tracing disabled by configuration");
        return;
    }
    if (!IsRayTracingSupported()) {
        ENJIN_LOG_INFO(Renderer, "Ray tracing not supported on this device, RT features disabled");
        return;
    }

    auto* ctx = m_VulkanRenderer->GetContext();
    ENJIN_LOG_INFO(Renderer, "Initializing ray tracing subsystems...");

    // Create RT descriptor set layout (27 bindings: 0-16 existing + 17 SDF + 18 simplified materials + 19-20 ReSTIR + 21-23 radiance cache + 24-26 surfel cache)
    std::array<VkDescriptorSetLayoutBinding, 27> rtBindings{};

    // Binding 0: TLAS
    rtBindings[0].binding = 0;
    rtBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    rtBindings[0].descriptorCount = 1;
    rtBindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // Binding 1: Scene HDR (storage image)
    rtBindings[1].binding = 1;
    rtBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[1].descriptorCount = 1;
    rtBindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Depth buffer
    rtBindings[2].binding = 2;
    rtBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    rtBindings[2].descriptorCount = 1;
    rtBindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 3: World normals
    rtBindings[3].binding = 3;
    rtBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    rtBindings[3].descriptorCount = 1;
    rtBindings[3].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 4: Motion vectors
    rtBindings[4].binding = 4;
    rtBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    rtBindings[4].descriptorCount = 1;
    rtBindings[4].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 5: RT Shadow output (storage image)
    rtBindings[5].binding = 5;
    rtBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[5].descriptorCount = 1;
    rtBindings[5].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 6: RT Reflection output (storage image)
    rtBindings[6].binding = 6;
    rtBindings[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[6].descriptorCount = 1;
    rtBindings[6].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 7: RT AO output (storage image)
    rtBindings[7].binding = 7;
    rtBindings[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[7].descriptorCount = 1;
    rtBindings[7].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 8: RT GI output (storage image)
    rtBindings[8].binding = 8;
    rtBindings[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[8].descriptorCount = 1;
    rtBindings[8].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Bindings 9-12: Storage buffers (material, vertex, index, transforms)
    for (u32 i = 9; i <= 12; ++i) {
        rtBindings[i].binding = i;
        rtBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        rtBindings[i].descriptorCount = 1;
        rtBindings[i].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    }

    // Binding 13: Light data UBO (used by RT shaders + compute: ReSTIR, radiance cache)
    rtBindings[13].binding = 13;
    rtBindings[13].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    rtBindings[13].descriptorCount = 1;
    rtBindings[13].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 14: RT Translucency output (storage image)
    rtBindings[14].binding = 14;
    rtBindings[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[14].descriptorCount = 1;
    rtBindings[14].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 15: RT Caustics output (storage image)
    rtBindings[15].binding = 15;
    rtBindings[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[15].descriptorCount = 1;
    rtBindings[15].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 16: NEE light SSBO (scene lights for path tracer direct light sampling)
    rtBindings[16].binding = 16;
    rtBindings[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[16].descriptorCount = 1;
    rtBindings[16].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    // Binding 17: SDF scene SSBO (SDF objects for reflection fallback sphere tracing)
    rtBindings[17].binding = 17;
    rtBindings[17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[17].descriptorCount = 1;
    rtBindings[17].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 18: Simplified material SSBO (pre-baked F0/kDiffuse/effectiveRoughness)
    rtBindings[18].binding = 18;
    rtBindings[18].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[18].descriptorCount = 1;
    rtBindings[18].stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 19: ReSTIR reservoir SSBO — current frame (per-pixel light selection results)
    rtBindings[19].binding = 19;
    rtBindings[19].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[19].descriptorCount = 1;
    rtBindings[19].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 20: ReSTIR reservoir SSBO — previous frame (ping-pong for temporal reuse)
    rtBindings[20].binding = 20;
    rtBindings[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[20].descriptorCount = 1;
    rtBindings[20].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 21: Radiance cache tile buffer (per-tile cached irradiance + depth/normal/age)
    rtBindings[21].binding = 21;
    rtBindings[21].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[21].descriptorCount = 1;
    rtBindings[21].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 22: Radiance cache stale tile mask (bitfield — 1 bit per tile)
    rtBindings[22].binding = 22;
    rtBindings[22].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[22].descriptorCount = 1;
    rtBindings[22].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // Binding 23: Radiance cache output image (full-resolution interpolated irradiance)
    rtBindings[23].binding = 23;
    rtBindings[23].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[23].descriptorCount = 1;
    rtBindings[23].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 24: Surfel radiance cache buffer (world-space surfel array)
    rtBindings[24].binding = 24;
    rtBindings[24].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[24].descriptorCount = 1;
    rtBindings[24].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 25: Surfel counter/metadata buffer (active count, free slot, update count)
    rtBindings[25].binding = 25;
    rtBindings[25].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    rtBindings[25].descriptorCount = 1;
    rtBindings[25].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 26: Surfel radiance cache output image (blended surfel + screen-space irradiance)
    rtBindings[26].binding = 26;
    rtBindings[26].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    rtBindings[26].descriptorCount = 1;
    rtBindings[26].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(rtBindings.size());
    layoutInfo.pBindings = rtBindings.data();

    if (vkCreateDescriptorSetLayout(ctx->GetDevice(), &layoutInfo, nullptr, &m_RTDescriptorSetLayout) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT descriptor set layout");
        return;
    }

    // Create RT descriptor pool (includes all descriptor types used by RT bindings)
    std::array<VkDescriptorPoolSize, 5> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 9 };   // 5-8, 14-15, 23, 26 + radiance cache output + surfel output
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3 };
    poolSizes[3] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 13 }; // 9-12, 16-20, 21-22, 24-25 (radiance cache + surfel cache)
    poolSizes[4] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(ctx->GetDevice(), &poolInfo, nullptr, &m_RTDescriptorPool) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT descriptor pool");
        return;
    }

    // Allocate RT descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_RTDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_RTDescriptorSetLayout;

    if (vkAllocateDescriptorSets(ctx->GetDevice(), &allocInfo, &m_RTDescriptorSet) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT descriptor set");
        return;
    }

    // Initialize acceleration structure manager (shader-independent, always safe)
    m_ASManager = std::make_unique<Renderer::AccelerationStructureManager>(ctx);
    m_ASManager->Initialize();

    // Safety net: skip subsystem initialization if RTShaderData.h somehow holds
    // placeholder stubs instead of real compiled SPIR-V. To regenerate the
    // embedded shaders from Engine/shaders/ sources:
    //   cd Engine/shaders
    //   glslc --target-env=vulkan1.2 -I. rt_shadow.rgen -o rt_shadow.rgen.spv
    //   (etc. for all RT/compute shaders, then: python _gen_rt.py)
    {
        bool usingStubs = (sizeof(Renderer::RT_SHADOW_RGEN_SPV) <= 40 * sizeof(u32));
        if (usingStubs) {
            ENJIN_LOG_INFO(Renderer, "RT shaders are placeholder stubs — skipping pipeline creation. "
                           "Compile GLSL shaders in Engine/shaders/ and update RTShaderData.h for full RT support.");
            return;
        }
    }

    // Get render dimensions
    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
    u32 width = extent.width;
    u32 height = extent.height;

    // Initialize RT effect subsystems
    m_RTShadows = std::make_unique<Renderer::RTShadows>(ctx);
    if (!m_RTShadows->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Shadows initialization failed");
        m_RTShadows.reset();
    }

    m_RTReflections = std::make_unique<Renderer::RTReflections>(ctx);
    if (!m_RTReflections->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Reflections initialization failed");
        m_RTReflections.reset();
    }

    m_RTAO = std::make_unique<Renderer::RTAmbientOcclusion>(ctx);
    if (!m_RTAO->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT AO initialization failed");
        m_RTAO.reset();
    }

    m_RTGI = std::make_unique<Renderer::RTGlobalIllumination>(ctx);
    if (!m_RTGI->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT GI initialization failed");
        m_RTGI.reset();
    }

    m_RTTranslucency = std::make_unique<Renderer::RTTranslucency>(ctx);
    if (!m_RTTranslucency->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Translucency initialization failed");
        m_RTTranslucency.reset();
    }

    m_RTCaustics = std::make_unique<Renderer::RTCaustics>(ctx);
    if (!m_RTCaustics->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Caustics initialization failed");
        m_RTCaustics.reset();
    }

    m_PathTracer = std::make_unique<Renderer::PathTracer>(ctx);
    if (!m_PathTracer->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "Path Tracer initialization failed");
        m_PathTracer.reset();
    }

    m_ReSTIR = std::make_unique<Renderer::ReSTIR>(ctx);
    if (!m_ReSTIR->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "ReSTIR initialization failed");
        m_ReSTIR.reset();
    }

    m_RadianceCache = std::make_unique<Renderer::RadianceCache>(ctx);
    if (!m_RadianceCache->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "Radiance cache initialization failed");
        m_RadianceCache.reset();
    }

    m_SurfelRadianceCache = std::make_unique<Renderer::SurfelRadianceCache>(ctx);
    if (!m_SurfelRadianceCache->Initialize(width, height, m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "Surfel radiance cache initialization failed");
        m_SurfelRadianceCache.reset();
    }

    // Initialize SVGF denoiser (always available as fallback)
    m_SVGFDenoiser = std::make_unique<Renderer::SVGFDenoiser>(ctx);
    if (!m_SVGFDenoiser->Initialize(width, height)) {
        ENJIN_LOG_WARN(Renderer, "SVGF Denoiser initialization failed");
        m_SVGFDenoiser.reset();
    }

    // Initialize OIDN denoiser (optional, compile-guarded)
    if (Renderer::OIDNDenoiser::IsAvailable()) {
        m_OIDNDenoiser = std::make_unique<Renderer::OIDNDenoiser>(ctx);
        if (!m_OIDNDenoiser->Initialize(width, height)) {
            ENJIN_LOG_WARN(Renderer, "OIDN Denoiser initialization failed — SVGF will be used");
            m_OIDNDenoiser.reset();
        }
    }

    // Register RT output images with OIDN denoiser so it can perform GPU<->CPU copies
    if (m_OIDNDenoiser) {
        if (m_RTShadows)
            m_OIDNDenoiser->RegisterImageMapping(m_RTShadows->GetOutputView(), m_RTShadows->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTReflections)
            m_OIDNDenoiser->RegisterImageMapping(m_RTReflections->GetOutputView(), m_RTReflections->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTAO)
            m_OIDNDenoiser->RegisterImageMapping(m_RTAO->GetOutputView(), m_RTAO->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTGI)
            m_OIDNDenoiser->RegisterImageMapping(m_RTGI->GetOutputView(), m_RTGI->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTTranslucency)
            m_OIDNDenoiser->RegisterImageMapping(m_RTTranslucency->GetOutputView(), m_RTTranslucency->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTCaustics)
            m_OIDNDenoiser->RegisterImageMapping(m_RTCaustics->GetOutputView(), m_RTCaustics->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    // Initialize OptiX AI Denoiser (optional, compile-guarded)
    if (Renderer::OptiXDenoiser::IsAvailable()) {
        m_OptiXDenoiser = std::make_unique<Renderer::OptiXDenoiser>(ctx);
        if (!m_OptiXDenoiser->Initialize(width, height)) {
            ENJIN_LOG_WARN(Renderer, "OptiX AI Denoiser initialization failed — SVGF/OIDN will be used");
            m_OptiXDenoiser.reset();
        }
    }

    // Register RT output images with OptiX denoiser
    if (m_OptiXDenoiser) {
        if (m_RTShadows)
            m_OptiXDenoiser->RegisterImageMapping(m_RTShadows->GetOutputView(), m_RTShadows->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTReflections)
            m_OptiXDenoiser->RegisterImageMapping(m_RTReflections->GetOutputView(), m_RTReflections->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTAO)
            m_OptiXDenoiser->RegisterImageMapping(m_RTAO->GetOutputView(), m_RTAO->GetOutputImage(), VK_FORMAT_R16_SFLOAT);
        if (m_RTGI)
            m_OptiXDenoiser->RegisterImageMapping(m_RTGI->GetOutputView(), m_RTGI->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTTranslucency)
            m_OptiXDenoiser->RegisterImageMapping(m_RTTranslucency->GetOutputView(), m_RTTranslucency->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
        if (m_RTCaustics)
            m_OptiXDenoiser->RegisterImageMapping(m_RTCaustics->GetOutputView(), m_RTCaustics->GetOutputImage(), VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    // Initialize RT compositor (uses RT descriptor set layout for pipeline compatibility)
    m_RTCompositor = std::make_unique<Renderer::RTCompositor>(ctx);
    if (!m_RTCompositor->Initialize(m_RTDescriptorSetLayout)) {
        ENJIN_LOG_WARN(Renderer, "RT Compositor initialization failed");
        m_RTCompositor.reset();
    }

    // Initialize RT temporal reuse (motion-vector-based history blending for RT outputs)
    m_RTTemporalReuse = std::make_unique<Renderer::RTTemporalReuse>(ctx);
    if (!m_RTTemporalReuse->Initialize(extent.width, extent.height)) {
        ENJIN_LOG_WARN(Renderer, "RT Temporal Reuse initialization failed");
        m_RTTemporalReuse.reset();
    }

    // Create dummy resources and RT light UBOs for descriptor binding
    CreateRTDummyResources();

    // Register dummy image with OIDN/OptiX so depth/normal/motion view lookups resolve
    if (m_OIDNDenoiser && m_RTDummyImageView != VK_NULL_HANDLE) {
        m_OIDNDenoiser->RegisterImageMapping(m_RTDummyImageView, m_RTDummyImage, VK_FORMAT_R8G8B8A8_UNORM);
    }
    if (m_OptiXDenoiser && m_RTDummyImageView != VK_NULL_HANDLE) {
        m_OptiXDenoiser->RegisterImageMapping(m_RTDummyImageView, m_RTDummyImage, VK_FORMAT_R8G8B8A8_UNORM);
    }

    // Register velocity buffer with denoisers for temporal accumulation
    auto* swapchain = m_VulkanRenderer->GetSwapchain();
    if (swapchain && swapchain->GetVelocityImageView() != VK_NULL_HANDLE) {
        VkImageView velView = swapchain->GetVelocityImageView();
        VkImage velImage = swapchain->GetVelocityImage();
        if (m_OIDNDenoiser)
            m_OIDNDenoiser->RegisterImageMapping(velView, velImage, Renderer::VulkanSwapchain::VELOCITY_FORMAT);
        if (m_OptiXDenoiser)
            m_OptiXDenoiser->RegisterImageMapping(velView, velImage, Renderer::VulkanSwapchain::VELOCITY_FORMAT);
    }

    ENJIN_LOG_INFO(Renderer, "Ray tracing subsystems initialized (shadows=%s, reflections=%s, AO=%s, GI=%s, pathtracer=%s, restir=%s, surfel_cache=%s)",
                   m_RTShadows ? "yes" : "no", m_RTReflections ? "yes" : "no",
                   m_RTAO ? "yes" : "no", m_RTGI ? "yes" : "no",
                   m_PathTracer ? "yes" : "no", m_ReSTIR ? "yes" : "no",
                   m_SurfelRadianceCache ? "yes" : "no");
}

void RenderSystem::ShutdownRayTracing() {
    m_SurfelRadianceCache.reset();
    m_RTTemporalReuse.reset();
    m_RTCompositor.reset();
    m_ReSTIR.reset();
    m_OptiXDenoiser.reset();
    m_OIDNDenoiser.reset();
    m_SVGFDenoiser.reset();
    m_PathTracer.reset();
    m_RTGI.reset();
    m_RTAO.reset();
    m_RTReflections.reset();
    m_RTShadows.reset();
    m_ASManager.reset();

    DestroyRTDummyResources();

    if (m_Renderer && m_VulkanRenderer->GetContext()) {
        VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();
        if (m_RTDescriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, m_RTDescriptorPool, nullptr);
            m_RTDescriptorPool = VK_NULL_HANDLE;
        }
        if (m_RTDescriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, m_RTDescriptorSetLayout, nullptr);
            m_RTDescriptorSetLayout = VK_NULL_HANDLE;
        }
    }
    m_RTDescriptorSet = VK_NULL_HANDLE;
    m_RTDescriptorsWritten = false;
}

void RenderSystem::RebuildTLAS(VkCommandBuffer cmd) {
    if (!m_ASManager || !m_RTEnabled) return;

    m_ASManager->ResetInstances();

    // Cache pool buffer device addresses (computed once, reused for all pool entities)
    VkDeviceAddress poolVertBase = 0;
    VkDeviceAddress poolIdxBase = 0;
    if (m_GeometryPool && m_GeometryPool->GetVertexBuffer() && m_GeometryPool->GetIndexBuffer()) {
        poolVertBase = m_GeometryPool->GetVertexBuffer()->GetDeviceAddress();
        poolIdxBase = m_GeometryPool->GetIndexBuffer()->GetDeviceAddress();
    }

    // Add all mesh entities to the TLAS (use cached storage pointers)
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!transform || !mesh || !transform->visible) continue;
        if (mesh->vertices.empty() || mesh->indices.empty()) continue;

        if (static_cast<usize>(EntityIndex(entity)) >= m_EntityRenderData.size()) continue;
        const auto& rd = m_EntityRenderData[static_cast<usize>(EntityIndex(entity))];
        if (!rd.valid) continue;

        VkDeviceAddress vertAddr = 0;
        VkDeviceAddress idxAddr = 0;
        u32 vertexCount = 0;
        u32 indexCount = 0;

        if (rd.vertexBuffer && rd.indexBuffer) {
            // Per-entity buffers (dynamic meshes, skinned, pool overflow)
            vertAddr = rd.vertexBuffer->GetDeviceAddress();
            idxAddr = rd.indexBuffer->GetDeviceAddress();
            vertexCount = static_cast<u32>(mesh->vertices.size());
            indexCount = static_cast<u32>(mesh->indices.size());
        } else if (rd.poolAlloc.valid && poolVertBase != 0 && poolIdxBase != 0) {
            // Pool-allocated: base address + byte offset into merged buffer
            vertAddr = poolVertBase + static_cast<VkDeviceAddress>(rd.poolAlloc.vertexOffset) * sizeof(MeshComponent::Vertex);
            idxAddr = poolIdxBase + static_cast<VkDeviceAddress>(rd.poolAlloc.indexOffset) * sizeof(u32);
            vertexCount = rd.poolAlloc.vertexCount;
            indexCount = rd.poolAlloc.indexCount;
        } else {
            continue;
        }

        if (vertAddr == 0 || idxAddr == 0) continue;

        // Hash based on buffer addresses (unique per mesh region)
        u64 meshHash = vertAddr ^ (idxAddr << 32) ^ (idxAddr >> 32);

        u32 blasId = m_ASManager->RegisterMesh(
            meshHash,
            vertAddr, vertexCount, sizeof(MeshComponent::Vertex),
            idxAddr, indexCount);

        // Build world model matrix (includes parent chain)
        Math::Matrix4 model = ECS::ComputeWorldMatrix(m_World, entity);

        m_ASManager->AddInstance(blasId, model, EntityIndex(entity));
    }

    // Upload per-entity material data to the RT material SSBO (binding 9).
    // Must happen before TLAS build so the buffer is valid when descriptors are written,
    // and every frame thereafter since material properties can change at runtime.
    UploadRTMaterials();

    // Flush BLAS builds and build/update TLAS
    if (m_ASManager->HasPendingBuilds()) {
        m_ASManager->FlushPendingBLASBuilds(cmd);
    }
    m_ASManager->BuildTLAS(cmd);

    // Write all RT descriptors once TLAS is valid (need a real handle for binding 0)
    if (!m_RTDescriptorsWritten && m_ASManager->HasValidTLAS()) {
        WriteRTDescriptors();
        TransitionRTOutputImages(cmd);
        m_RTDescriptorsWritten = true;
    } else if (m_RTDescriptorsWritten && m_RTMaterialBuffer != VK_NULL_HANDLE) {
        // Update bindings 9 and 18 each frame to reflect current material data.
        // The buffer contents are already uploaded via UploadRTMaterials() above;
        // this ensures the descriptors point to the correct buffers after any reallocation.
        VkDescriptorBufferInfo matBufInfo{};
        matBufInfo.buffer = m_RTMaterialBuffer;
        matBufInfo.offset = 0;
        matBufInfo.range = VK_WHOLE_SIZE;

        VkDescriptorBufferInfo simplifiedMatBufInfo{};
        simplifiedMatBufInfo.buffer = (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE)
            ? m_RTSimplifiedMaterialBuffer : m_RTDummyBuffer;
        simplifiedMatBufInfo.offset = 0;
        simplifiedMatBufInfo.range = (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE)
            ? VK_WHOLE_SIZE : static_cast<VkDeviceSize>(256);

        std::array<VkWriteDescriptorSet, 2> matWrites{};

        // Binding 9: Full material SSBO
        matWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        matWrites[0].dstSet = m_RTDescriptorSet;
        matWrites[0].dstBinding = 9;
        matWrites[0].descriptorCount = 1;
        matWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        matWrites[0].pBufferInfo = &matBufInfo;

        // Binding 18: Simplified material SSBO
        matWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        matWrites[1].dstSet = m_RTDescriptorSet;
        matWrites[1].dstBinding = 18;
        matWrites[1].descriptorCount = 1;
        matWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        matWrites[1].pBufferInfo = &simplifiedMatBufInfo;

        vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(),
                               static_cast<u32>(matWrites.size()), matWrites.data(), 0, nullptr);
    }

}

void RenderSystem::DispatchRTEffects(VkCommandBuffer cmd) {
    if (!m_RTEnabled || !m_ASManager || !m_ASManager->HasValidTLAS()) return;
    if (!m_RTDescriptorsWritten) return;

    // GPU timestamp: RT effects begin
    {
        VkQueryPool tsPool = m_VulkanRenderer->GetTimestampPool(m_VulkanRenderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_RT_BEGIN);
        }
    }

    // Compute inverse view-projection and camera position
    Math::Matrix4 view = m_Camera->GetViewMatrix();
    Math::Matrix4 proj = m_Camera->GetProjectionMatrix();
    Math::Matrix4 viewProj = proj * view;
    Math::Matrix4 invViewProj = viewProj.Inverse();
    Math::Vector3 cameraPos = m_Camera->GetPosition();

    // Detect camera changes for path tracer accumulation reset
    bool cameraChanged = false;
    {
        // Compare VP matrices — any significant change resets accumulation
        const f32* a = viewProj.m;
        const f32* b = m_PrevViewProj.m;
        f32 diff = 0.0f;
        for (int i = 0; i < 16; ++i) diff += std::abs(a[i] - b[i]);
        cameraChanged = (diff > 0.001f);
        m_PrevViewProj = viewProj;
    }
    if (cameraChanged && m_PathTracer) {
        m_PathTracer->ResetAccumulation();
        m_RTFrameCount = 0;
    }
    if (cameraChanged && m_RadianceCache) {
        m_RadianceCache->InvalidateAll();
    }
    if (cameraChanged && m_SurfelRadianceCache) {
        // Surfel cache persists across camera movement (world-space), but on
        // major teleport/cut we invalidate to avoid stale data from distant locations.
        m_SurfelRadianceCache->InvalidateAll();
    }
    // Note: temporal reuse does NOT reset on camera change — it handles reprojection
    // via motion vectors and detects disocclusions automatically. Only reset on explicit
    // camera cuts (scene load, teleport) which are handled by ResetHistory() calls.

    // Find primary directional light direction from its transform
    Math::Vector3 lightDir(0.0f, -1.0f, 0.0f);
    f32 lightIntensity = 1.0f;
    f32 lightShadowDistance = 100.0f;
    Math::Vector3 lightColor(1.0f, 1.0f, 1.0f);
    auto* lightStorageRT = m_World->GetComponentStorage<LightComponent>();
    for (Entity entity : m_CachedLightEntities) {
        auto* light = lightStorageRT ? lightStorageRT->Get(entity) : nullptr;
        if (light && light->type == LightType::Directional) {
            auto* lightTransform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
            if (lightTransform) {
                Math::Vector3 forward(0.0f, 0.0f, -1.0f);
                lightDir = lightTransform->rotation.Rotate(forward);
            }
            lightIntensity = light->intensity;
            lightColor = light->color;
            break;
        }
    }

    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
    m_RTFrameCount++;

    // Update RT light UBO with current frame data
    f32 shadowRadius = m_RTShadows ? m_RTShadows->GetConfig().radius : 0.01f;
    if (m_RTShadows) lightShadowDistance = m_RTShadows->GetConfig().maxDistance;

    // Path tracer config for shader upload
    f32 fireflyClamp = 10.0f;
    i32 enableNEE = 1, enableMIS = 1, rrMinBounce = 3;
    f32 rrMinProb = 0.05f;
    u32 ptMaxBounces = 4, ptAccumulatedSamples = 0;
    if (m_PathTracer) {
        const auto& ptCfg = m_PathTracer->GetConfig();
        fireflyClamp = ptCfg.fireflyClampValue;
        enableNEE = ptCfg.enableNEE ? 1 : 0;
        enableMIS = ptCfg.enableMIS ? 1 : 0;
        rrMinBounce = static_cast<i32>(ptCfg.russianRouletteMinBounce);
        rrMinProb = ptCfg.russianRouletteMinProb;
        ptMaxBounces = ptCfg.maxBounces;
        ptAccumulatedSamples = m_PathTracer->GetAccumulatedSamples();
    }

    // Light counts from cached forward renderer data
    u32 dirLightCount = m_CachedLightingData.directionalLightCount;
    u32 ptLightCount = m_CachedLightingData.pointLightCount;
    u32 sptLightCount = m_CachedLightingData.spotLightCount;

    UpdateRTLightUBO(invViewProj, lightDir, lightIntensity, lightShadowDistance,
                     shadowRadius, m_RTFrameCount,
                     fireflyClamp, enableNEE, enableMIS, rrMinBounce, rrMinProb,
                     dirLightCount, ptLightCount, sptLightCount,
                     ptMaxBounces, ptAccumulatedSamples);

    // Update TLAS descriptor (handle may change on rebuild)
    {
        auto* ctx = m_VulkanRenderer->GetContext();
        VkAccelerationStructureKHR tlas = m_ASManager->GetTLAS();
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &tlas;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = &asInfo;
        write.dstSet = m_RTDescriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        vkUpdateDescriptorSets(ctx->GetDevice(), 1, &write, 0, nullptr);
    }

    if (m_RTMode == 1 && m_PathTracer) {
        // Path trace mode — progressive accumulation
        m_PathTracer->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                               lightDir, m_RTFrameCount);
        return;
    }

    // Upload SDF scene data to SSBO (binding 17) for reflection fallback sphere tracing
    if (m_RTSDFMapped && m_SDFScene) {
        auto sdfObjects = m_SDFScene->GetObjectBuffer();
        u32 sdfCount = std::min(static_cast<u32>(sdfObjects.size()), 256u);

        // Buffer layout: [u32 objectCount, u32 pad0, u32 pad1, u32 pad2, SDFObjectGPU objects[...]]
        u32 header[4] = { sdfCount, 0, 0, 0 };
        std::memcpy(m_RTSDFMapped, header, sizeof(header));
        if (sdfCount > 0) {
            std::memcpy(static_cast<u8*>(m_RTSDFMapped) + 16,
                        sdfObjects.data(),
                        sdfCount * sizeof(Renderer::SDFObjectGPU));
        }
    } else if (m_RTSDFMapped) {
        // No SDF scene — zero the object count
        u32 zero = 0;
        std::memcpy(m_RTSDFMapped, &zero, sizeof(zero));
    }

    // ReSTIR — importance-weighted light selection (must run before RT shadows/GI)
    if (m_ReSTIR && m_ReSTIR->GetConfig().enabled) {
        u32 totalLightCount = dirLightCount + ptLightCount + sptLightCount;
        m_ReSTIR->Dispatch(cmd, m_RTDescriptorSet, m_RTFrameCount, totalLightCount);
    }

    // Hybrid mode — dispatch individual effects
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) {
        m_RTShadows->Dispatch(cmd, m_RTDescriptorSet, invViewProj, lightDir,
                              lightIntensity, m_RTFrameCount);
    }
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) {
        m_RTReflections->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                                  m_RTFrameCount);
    }
    if (m_RTAO && m_RTAO->GetConfig().enabled) {
        m_RTAO->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                         m_RTFrameCount);
    }
    if (m_RTGI && m_RTGI->GetConfig().enabled) {
        m_RTGI->Dispatch(cmd, m_RTDescriptorSet, invViewProj, lightDir,
                         cameraPos, m_RTFrameCount);
    }

    // Radiance cache — update tile validity and read cached irradiance.
    // Runs after GI dispatch so it can absorb fresh GI results into cache tiles.
    // The read pass interpolates tile irradiance to per-pixel output.
    if (m_RadianceCache && m_RadianceCache->GetConfig().enabled) {
        m_RadianceCache->DispatchUpdate(cmd, m_RTDescriptorSet, m_RTFrameCount);
        m_RadianceCache->DispatchRead(cmd, m_RTDescriptorSet, m_RTFrameCount);
    }

    // Surfel radiance cache — world-space surfel-based irradiance caching.
    // Runs after screen-space radiance cache so it can blend with its output.
    // Placement spawns/removes surfels, update traces rays, lookup interpolates per-pixel.
    if (m_SurfelRadianceCache && m_SurfelRadianceCache->GetConfig().enabled) {
        m_SurfelRadianceCache->DispatchPlacement(cmd, m_RTDescriptorSet, cameraPos, m_RTFrameCount);
        m_SurfelRadianceCache->DispatchUpdate(cmd, m_RTDescriptorSet, cameraPos, m_RTFrameCount);
        m_SurfelRadianceCache->DispatchLookup(cmd, m_RTDescriptorSet, m_RTFrameCount);
    }

    if (m_RTTranslucency && m_RTTranslucency->GetConfig().enabled) {
        m_RTTranslucency->Dispatch(cmd, m_RTDescriptorSet, invViewProj, cameraPos,
                                    m_RTFrameCount);
    }
    if (m_RTCaustics && m_RTCaustics->GetConfig().enabled) {
        m_RTCaustics->Dispatch(cmd, m_RTDescriptorSet, invViewProj, lightDir,
                                m_RTFrameCount);
    }

    // GPU timestamp: RT effects end
    {
        VkQueryPool tsPool = m_VulkanRenderer->GetTimestampPool(m_VulkanRenderer->GetCurrentFrameIndex());
        if (tsPool != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, tsPool, Renderer::GPU_TS_RT_END);
        }
    }
}

void RenderSystem::TemporalReuseRTOutputs(VkCommandBuffer cmd) {
    if (!m_RTEnabled || m_RTMode == 1 || !m_RTTemporalReuse) return;
    if (!m_RTTemporalReuse->GetConfig().enabled) return;

    // Obtain depth, normal, and motion views (same logic as DenoiseRTOutputs)
    auto* swapchain = m_VulkanRenderer->GetSwapchain();
    VkImageView depthView = (swapchain && swapchain->GetDepthImageView() != VK_NULL_HANDLE)
        ? swapchain->GetDepthImageView() : m_RTDummyImageView;
    VkImageView normalView = m_RTDummyImageView;
    VkImageView motionView = (swapchain && swapchain->GetVelocityImageView() != VK_NULL_HANDLE)
        ? swapchain->GetVelocityImageView() : m_RTDummyImageView;

    // Temporal reuse for each enabled RT buffer (dispatch writes blended result back to output)
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseShadows(cmd, m_RTShadows->GetOutputView(),
            depthView, normalView, motionView, m_RTShadows->GetOutputView());
    }
    if (m_RTAO && m_RTAO->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseAO(cmd, m_RTAO->GetOutputView(),
            depthView, normalView, motionView, m_RTAO->GetOutputView());
    }
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseReflections(cmd, m_RTReflections->GetOutputView(),
            depthView, normalView, motionView, m_RTReflections->GetOutputView());
    }
    if (m_RTGI && m_RTGI->GetConfig().enabled) {
        m_RTTemporalReuse->ReuseGI(cmd, m_RTGI->GetOutputView(),
            depthView, normalView, motionView, m_RTGI->GetOutputView());
    }
}

void RenderSystem::DenoiseRTOutputs(VkCommandBuffer cmd) {
    if (!m_RTEnabled || m_RTMode == 1) return;

    // Select active denoiser based on type setting (0=SVGF, 1=OIDN, 2=OptiX)
    Renderer::IDenoiser* denoiser = nullptr;
    if (m_DenoiserType == 2 && m_OptiXDenoiser) {
        denoiser = m_OptiXDenoiser.get();
    } else if (m_DenoiserType == 1 && m_OIDNDenoiser) {
        denoiser = m_OIDNDenoiser.get();
    } else if (m_SVGFDenoiser) {
        denoiser = m_SVGFDenoiser.get();
    }
    if (!denoiser) return;

    // Use real depth and velocity from swapchain when available; normals still use dummy
    // (no G-buffer MRT normal output yet — will be wired when deferred normals are added)
    auto* swapchain = m_VulkanRenderer->GetSwapchain();
    VkImageView depthView = (swapchain && swapchain->GetDepthImageView() != VK_NULL_HANDLE)
        ? swapchain->GetDepthImageView() : m_RTDummyImageView;
    VkImageView normalView = m_RTDummyImageView;
    VkImageView motionView = (swapchain && swapchain->GetVelocityImageView() != VK_NULL_HANDLE)
        ? swapchain->GetVelocityImageView() : m_RTDummyImageView;

    // Denoise shadow output (single channel R16F)
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) {
        denoiser->DenoiseSingleChannel(cmd,
            m_RTShadows->GetOutputView(), depthView, normalView, motionView,
            m_RTShadows->GetOutputView());
    }
    // Denoise AO output (single channel R16F)
    if (m_RTAO && m_RTAO->GetConfig().enabled) {
        denoiser->DenoiseSingleChannel(cmd,
            m_RTAO->GetOutputView(), depthView, normalView, motionView,
            m_RTAO->GetOutputView());
    }
    // Denoise reflections (RGBA16F)
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTReflections->GetOutputView(), depthView, normalView, motionView,
            m_RTReflections->GetOutputView());
    }
    // Denoise GI (RGBA16F)
    if (m_RTGI && m_RTGI->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTGI->GetOutputView(), depthView, normalView, motionView,
            m_RTGI->GetOutputView());
    }
    // Denoise translucency (RGBA16F)
    if (m_RTTranslucency && m_RTTranslucency->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTTranslucency->GetOutputView(), depthView, normalView, motionView,
            m_RTTranslucency->GetOutputView());
    }
    // Denoise caustics (RGBA16F)
    if (m_RTCaustics && m_RTCaustics->GetConfig().enabled) {
        denoiser->DenoiseColor(cmd,
            m_RTCaustics->GetOutputView(), depthView, normalView, motionView,
            m_RTCaustics->GetOutputView());
    }
}

void RenderSystem::CompositeRTResults(VkCommandBuffer cmd) {
    if (!m_RTEnabled || !m_RTCompositor || m_RTMode == 1) return;

    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();

    // Build enable flags: bit 0=shadow, 1=reflect, 2=ao, 3=gi, 4=translucency, 5=caustics
    u32 enableFlags = 0;
    if (m_RTShadows && m_RTShadows->GetConfig().enabled) enableFlags |= 1;
    if (m_RTReflections && m_RTReflections->GetConfig().enabled) enableFlags |= 2;
    if (m_RTAO && m_RTAO->GetConfig().enabled) enableFlags |= 4;
    if (m_RTGI && m_RTGI->GetConfig().enabled) enableFlags |= 8;
    if (m_RTTranslucency && m_RTTranslucency->GetConfig().enabled) enableFlags |= 16;
    if (m_RTCaustics && m_RTCaustics->GetConfig().enabled) enableFlags |= 32;

    if (enableFlags == 0) return;

    m_RTCompositor->Dispatch(cmd, m_RTDescriptorSet, extent.width, extent.height, enableFlags);
}

void RenderSystem::TransitionDepthImagesToReadable(const std::vector<VkImage>& images) {
    if (images.empty() || !m_VulkanRenderer) return;
    auto* ctx = m_VulkanRenderer->GetContext();
    if (!ctx) return;
    VkDevice device = ctx->GetDevice();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = ctx->GetGraphicsQueueFamily();
    VkCommandPool tempPool = VK_NULL_HANDLE;
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) != VK_SUCCESS) return;

    VkCommandBufferAllocateInfo cbAlloc{};
    cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool = tempPool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &cbAlloc, &cmd) == VK_SUCCESS) {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        for (VkImage img : images) {
            if (img == VK_NULL_HANDLE) continue;
            VkImageMemoryBarrier b{};
            b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = img;
            b.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
            b.srcAccessMask = 0;
            b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &b);
        }

        vkEndCommandBuffer(cmd);
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        if (vkQueueSubmit(ctx->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS) {
            vkQueueWaitIdle(ctx->GetGraphicsQueue());
        }
    }
    vkDestroyCommandPool(device, tempPool, nullptr);
}

void RenderSystem::CreateRTDummyResources() {
    // Idempotent: these placeholders are needed by the main PBR descriptor set
    // (bindings 21-23) whether or not ray tracing initializes, so this is now
    // called unconditionally during init AND from InitializeRayTracing(). Skip
    // if already built.
    if (m_RTDummyImageView != VK_NULL_HANDLE) return;

    auto* ctx = m_VulkanRenderer->GetContext();
    VkDevice device = ctx->GetDevice();

    // Create 1x1 dummy image for placeholder descriptor bindings
    {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgInfo.extent = { 1, 1, 1 };
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &imgInfo, nullptr, &m_RTDummyImage) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy image");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device, m_RTDummyImage, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_RTDummyImageMemory) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT dummy image memory");
            return;
        }
        if (vkBindImageMemory(device, m_RTDummyImage, m_RTDummyImageMemory, 0) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to bind RT dummy image memory");
            return;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_RTDummyImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        if (vkCreateImageView(device, &viewInfo, nullptr, &m_RTDummyImageView) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy image view");
            return;
        }
    }

    // Create 1x1x1 3D dummy image for sampler3D bindings (froxel volume)
    {
        VkImageCreateInfo img3DInfo{};
        img3DInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        img3DInfo.imageType = VK_IMAGE_TYPE_3D;
        img3DInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        img3DInfo.extent = { 1, 1, 1 };
        img3DInfo.mipLevels = 1;
        img3DInfo.arrayLayers = 1;
        img3DInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        img3DInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        img3DInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        img3DInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        img3DInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &img3DInfo, nullptr, &m_RTDummy3DImage) == VK_SUCCESS) {
            VkMemoryRequirements memReqs3D;
            vkGetImageMemoryRequirements(device, m_RTDummy3DImage, &memReqs3D);
            VkMemoryAllocateInfo alloc3D{};
            alloc3D.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc3D.allocationSize = memReqs3D.size;
            alloc3D.memoryTypeIndex = ctx->FindMemoryType(memReqs3D.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (vkAllocateMemory(device, &alloc3D, nullptr, &m_RTDummy3DImageMemory) == VK_SUCCESS) {
                vkBindImageMemory(device, m_RTDummy3DImage, m_RTDummy3DImageMemory, 0);
                VkImageViewCreateInfo view3DInfo{};
                view3DInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                view3DInfo.image = m_RTDummy3DImage;
                view3DInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
                view3DInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
                view3DInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
                vkCreateImageView(device, &view3DInfo, nullptr, &m_RTDummy3DImageView);
            }
        }
    }

    // Create 1x1 cube dummy for the probeCubemap binding (samplerCube). A 6-layer
    // cube image with a CUBE view so binding 19 matches the shader when no
    // reflection probe is baked (otherwise it falls back to a 2D view and trips
    // VUID-vkCmdDrawIndexed-viewType-07752 every draw).
    {
        VkImageCreateInfo cubeInfo{};
        cubeInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        cubeInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        cubeInfo.imageType = VK_IMAGE_TYPE_2D;
        cubeInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        cubeInfo.extent = { 1, 1, 1 };
        cubeInfo.mipLevels = 1;
        cubeInfo.arrayLayers = 6;
        cubeInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        cubeInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        cubeInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
        cubeInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        cubeInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &cubeInfo, nullptr, &m_DummyCubeImage) == VK_SUCCESS) {
            VkMemoryRequirements memReqsCube;
            vkGetImageMemoryRequirements(device, m_DummyCubeImage, &memReqsCube);
            VkMemoryAllocateInfo allocCube{};
            allocCube.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocCube.allocationSize = memReqsCube.size;
            allocCube.memoryTypeIndex = ctx->FindMemoryType(memReqsCube.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (vkAllocateMemory(device, &allocCube, nullptr, &m_DummyCubeImageMemory) == VK_SUCCESS) {
                vkBindImageMemory(device, m_DummyCubeImage, m_DummyCubeImageMemory, 0);
                VkImageViewCreateInfo cubeView{};
                cubeView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                cubeView.image = m_DummyCubeImage;
                cubeView.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
                cubeView.format = VK_FORMAT_R8G8B8A8_UNORM;
                cubeView.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
                vkCreateImageView(device, &cubeView, nullptr, &m_DummyCubeImageView);
            }
        }
    }

    // Create sampler for combined image sampler bindings
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_RTDummySampler) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy sampler");
            return;
        }
    }

    // Create dummy buffer for storage buffer bindings (256 bytes)
    {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = 256;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bufInfo, nullptr, &m_RTDummyBuffer) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to create RT dummy buffer");
            return;
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTDummyBuffer, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_RTDummyBufferMemory) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT dummy buffer memory");
            return;
        }
        if (vkBindBufferMemory(device, m_RTDummyBuffer, m_RTDummyBufferMemory, 0) != VK_SUCCESS) {
            ENJIN_LOG_ERROR(Renderer, "Failed to bind RT dummy buffer memory");
            return;
        }
    }

    // Transition the sampled dummy images UNDEFINED -> SHADER_READ_ONLY_OPTIMAL.
    // They are bound to the PBR descriptor set (bindings 19/21/22/23) and sampled
    // unconditionally, so leaving them UNDEFINED trips the layout check at draw
    // time (validation vkCmdDraw-None-09600).
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = ctx->GetGraphicsQueueFamily();
        VkCommandPool tempPool = VK_NULL_HANDLE;
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &tempPool) == VK_SUCCESS) {
            VkCommandBufferAllocateInfo cbAlloc{};
            cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            cbAlloc.commandPool = tempPool;
            cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            cbAlloc.commandBufferCount = 1;
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            if (vkAllocateCommandBuffers(device, &cbAlloc, &cmd) == VK_SUCCESS) {
                VkCommandBufferBeginInfo begin{};
                begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                vkBeginCommandBuffer(cmd, &begin);

                struct DummyTransition { VkImage image; u32 layers; };
                const DummyTransition dummies[] = {
                    { m_RTDummyImage,   1 },
                    { m_RTDummy3DImage, 1 },
                    { m_DummyCubeImage, 6 },
                };
                for (const auto& d : dummies) {
                    if (d.image == VK_NULL_HANDLE) continue;
                    VkImageMemoryBarrier b{};
                    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                    b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                    b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    b.image = d.image;
                    b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, d.layers };
                    b.srcAccessMask = 0;
                    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    vkCmdPipelineBarrier(cmd,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &b);
                }

                vkEndCommandBuffer(cmd);
                VkSubmitInfo submit{};
                submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submit.commandBufferCount = 1;
                submit.pCommandBuffers = &cmd;
                if (vkQueueSubmit(ctx->GetGraphicsQueue(), 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS) {
                    vkQueueWaitIdle(ctx->GetGraphicsQueue());
                }
            }
            vkDestroyCommandPool(device, tempPool, nullptr);
        }
    }

    // Create RT light UBOs (per frame in flight, host visible + coherent, persistently mapped)
    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = 256;  // Enough for RTLightUBO struct (112 bytes + padding)
        bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufInfo, nullptr, &m_RTLightUBO[i]);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTLightUBO[i], &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &allocInfo, nullptr, &m_RTLightUBOMemory[i]);
        vkBindBufferMemory(device, m_RTLightUBO[i], m_RTLightUBOMemory[i], 0);
        if (vkMapMemory(device, m_RTLightUBOMemory[i], 0, 256, 0, &m_RTLightUBOMapped[i]) != VK_SUCCESS) {
            m_RTLightUBOMapped[i] = nullptr;
            continue;
        }
        std::memset(m_RTLightUBOMapped[i], 0, 256);
    }

    // Create NEE light SSBOs (binding 16, per frame in flight — scene lights for path tracer)
    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = RT_NEE_LIGHT_BUFFER_SIZE;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufInfo, nullptr, &m_RTNEELightBuffer[i]);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTNEELightBuffer[i], &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &allocInfo, nullptr, &m_RTNEELightMemory[i]);
        vkBindBufferMemory(device, m_RTNEELightBuffer[i], m_RTNEELightMemory[i], 0);
        if (vkMapMemory(device, m_RTNEELightMemory[i], 0, RT_NEE_LIGHT_BUFFER_SIZE, 0, &m_RTNEELightMapped[i]) != VK_SUCCESS) {
            m_RTNEELightMapped[i] = nullptr;
            continue;
        }
        std::memset(m_RTNEELightMapped[i], 0, RT_NEE_LIGHT_BUFFER_SIZE);
    }

    // Create RT material SSBO (binding 9) — persistently mapped, host visible + coherent
    EnsureRTMaterialBuffer(RT_MATERIAL_BUFFER_INITIAL_CAPACITY);

    // Create RT simplified material SSBO (binding 18) — pre-baked material properties
    EnsureRTSimplifiedMaterialBuffer(RT_MATERIAL_BUFFER_INITIAL_CAPACITY);

    // Create SDF scene SSBO (binding 17) — persistently mapped for per-frame SDF object upload
    {
        VkBufferCreateInfo bufInfo{};
        bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufInfo.size = RT_SDF_BUFFER_SIZE;
        bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device, &bufInfo, nullptr, &m_RTSDFBuffer);

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device, m_RTSDFBuffer, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device, &allocInfo, nullptr, &m_RTSDFMemory);
        vkBindBufferMemory(device, m_RTSDFBuffer, m_RTSDFMemory, 0);
        if (vkMapMemory(device, m_RTSDFMemory, 0, RT_SDF_BUFFER_SIZE, 0, &m_RTSDFMapped) != VK_SUCCESS) {
            m_RTSDFMapped = nullptr;
        } else {
            std::memset(m_RTSDFMapped, 0, RT_SDF_BUFFER_SIZE);
        }
    }

    ENJIN_LOG_INFO(Renderer, "RT dummy resources, light UBOs, NEE light SSBOs, material SSBO, simplified material SSBO, and SDF SSBO created");
}

void RenderSystem::EnsureRTMaterialBuffer(u32 requiredCapacity) {
    if (requiredCapacity <= m_RTMaterialBufferCapacity && m_RTMaterialBuffer != VK_NULL_HANDLE) return;

    auto* ctx = m_VulkanRenderer->GetContext();
    VkDevice device = ctx->GetDevice();

    // Destroy old buffer if exists
    if (m_RTMaterialMapped) {
        vkUnmapMemory(device, m_RTMaterialMemory);
        m_RTMaterialMapped = nullptr;
    }
    if (m_RTMaterialBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_RTMaterialBuffer, nullptr);
        m_RTMaterialBuffer = VK_NULL_HANDLE;
    }
    if (m_RTMaterialMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_RTMaterialMemory, nullptr);
        m_RTMaterialMemory = VK_NULL_HANDLE;
    }

    // Grow by at least 2x to avoid frequent reallocations
    u32 newCapacity = m_RTMaterialBufferCapacity > 0
        ? m_RTMaterialBufferCapacity * 2
        : RT_MATERIAL_BUFFER_INITIAL_CAPACITY;
    if (newCapacity < requiredCapacity) newCapacity = requiredCapacity;

    VkDeviceSize bufferSize = static_cast<VkDeviceSize>(newCapacity) * sizeof(MaterialGPU);

    VkBufferCreateInfo bufInfo{};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = bufferSize;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufInfo, nullptr, &m_RTMaterialBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT material SSBO (%u entries)", newCapacity);
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, m_RTMaterialBuffer, &memReqs);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = ctx->FindMemoryType(memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_RTMaterialMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT material SSBO memory");
        vkDestroyBuffer(device, m_RTMaterialBuffer, nullptr);
        m_RTMaterialBuffer = VK_NULL_HANDLE;
        return;
    }

    if (vkBindBufferMemory(device, m_RTMaterialBuffer, m_RTMaterialMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind RT material SSBO memory");
        return;
    }

    if (vkMapMemory(device, m_RTMaterialMemory, 0, bufferSize, 0, &m_RTMaterialMapped) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map RT material SSBO");
        m_RTMaterialMapped = nullptr;
        return;
    }

    std::memset(m_RTMaterialMapped, 0, static_cast<size_t>(bufferSize));
    m_RTMaterialBufferCapacity = newCapacity;

    ENJIN_LOG_INFO(Renderer, "RT material SSBO created/resized: %u entries (%llu bytes)",
                   newCapacity, static_cast<unsigned long long>(bufferSize));
}

void RenderSystem::EnsureRTSimplifiedMaterialBuffer(u32 requiredCapacity) {
    if (requiredCapacity <= m_RTSimplifiedMaterialBufferCapacity && m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE) return;

    auto* ctx = m_VulkanRenderer->GetContext();
    VkDevice device = ctx->GetDevice();

    // Destroy old buffer if exists
    if (m_RTSimplifiedMaterialMapped) {
        vkUnmapMemory(device, m_RTSimplifiedMaterialMemory);
        m_RTSimplifiedMaterialMapped = nullptr;
    }
    if (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, m_RTSimplifiedMaterialBuffer, nullptr);
        m_RTSimplifiedMaterialBuffer = VK_NULL_HANDLE;
    }
    if (m_RTSimplifiedMaterialMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_RTSimplifiedMaterialMemory, nullptr);
        m_RTSimplifiedMaterialMemory = VK_NULL_HANDLE;
    }

    // Grow by at least 2x to avoid frequent reallocations
    u32 newCapacity = m_RTSimplifiedMaterialBufferCapacity > 0
        ? m_RTSimplifiedMaterialBufferCapacity * 2
        : RT_MATERIAL_BUFFER_INITIAL_CAPACITY;
    if (newCapacity < requiredCapacity) newCapacity = requiredCapacity;

    // RTSimplifiedMaterialGPU is 64 bytes per entry
    VkDeviceSize simplifiedBufSize = static_cast<VkDeviceSize>(newCapacity) * 64;

    VkBufferCreateInfo simplifiedBufCI{};
    simplifiedBufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    simplifiedBufCI.size = simplifiedBufSize;
    simplifiedBufCI.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    simplifiedBufCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &simplifiedBufCI, nullptr, &m_RTSimplifiedMaterialBuffer) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to create RT simplified material SSBO (%u entries)", newCapacity);
        return;
    }

    VkMemoryRequirements smemReqs;
    vkGetBufferMemoryRequirements(device, m_RTSimplifiedMaterialBuffer, &smemReqs);
    VkMemoryAllocateInfo sallocInfo{};
    sallocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    sallocInfo.allocationSize = smemReqs.size;
    sallocInfo.memoryTypeIndex = ctx->FindMemoryType(smemReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (vkAllocateMemory(device, &sallocInfo, nullptr, &m_RTSimplifiedMaterialMemory) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to allocate RT simplified material SSBO memory");
        vkDestroyBuffer(device, m_RTSimplifiedMaterialBuffer, nullptr);
        m_RTSimplifiedMaterialBuffer = VK_NULL_HANDLE;
        return;
    }

    if (vkBindBufferMemory(device, m_RTSimplifiedMaterialBuffer, m_RTSimplifiedMaterialMemory, 0) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to bind RT simplified material SSBO memory");
        return;
    }

    if (vkMapMemory(device, m_RTSimplifiedMaterialMemory, 0, simplifiedBufSize, 0, &m_RTSimplifiedMaterialMapped) != VK_SUCCESS) {
        ENJIN_LOG_ERROR(Renderer, "Failed to map RT simplified material SSBO");
        m_RTSimplifiedMaterialMapped = nullptr;
        return;
    }

    std::memset(m_RTSimplifiedMaterialMapped, 0, static_cast<size_t>(simplifiedBufSize));
    m_RTSimplifiedMaterialBufferCapacity = newCapacity;

    ENJIN_LOG_INFO(Renderer, "RT simplified material SSBO created/resized: %u entries (%llu bytes)",
                   newCapacity, static_cast<unsigned long long>(simplifiedBufSize));
}

void RenderSystem::DestroyRTDummyResources() {
    if (!m_Renderer || !m_VulkanRenderer->GetContext()) return;
    VkDevice device = m_VulkanRenderer->GetContext()->GetDevice();

    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        if (m_RTLightUBOMapped[i]) {
            vkUnmapMemory(device, m_RTLightUBOMemory[i]);
            m_RTLightUBOMapped[i] = nullptr;
        }
        if (m_RTLightUBO[i]) { vkDestroyBuffer(device, m_RTLightUBO[i], nullptr); m_RTLightUBO[i] = VK_NULL_HANDLE; }
        if (m_RTLightUBOMemory[i]) { vkFreeMemory(device, m_RTLightUBOMemory[i], nullptr); m_RTLightUBOMemory[i] = VK_NULL_HANDLE; }
    }

    if (m_RTDummySampler) { vkDestroySampler(device, m_RTDummySampler, nullptr); m_RTDummySampler = VK_NULL_HANDLE; }
    if (m_RTDummyImageView) { vkDestroyImageView(device, m_RTDummyImageView, nullptr); m_RTDummyImageView = VK_NULL_HANDLE; }
    if (m_RTDummyImage) { vkDestroyImage(device, m_RTDummyImage, nullptr); m_RTDummyImage = VK_NULL_HANDLE; }
    if (m_RTDummyImageMemory) { vkFreeMemory(device, m_RTDummyImageMemory, nullptr); m_RTDummyImageMemory = VK_NULL_HANDLE; }
    if (m_RTDummyBuffer) { vkDestroyBuffer(device, m_RTDummyBuffer, nullptr); m_RTDummyBuffer = VK_NULL_HANDLE; }
    if (m_RTDummyBufferMemory) { vkFreeMemory(device, m_RTDummyBufferMemory, nullptr); m_RTDummyBufferMemory = VK_NULL_HANDLE; }
    if (m_DummyCubeImageView) { vkDestroyImageView(device, m_DummyCubeImageView, nullptr); m_DummyCubeImageView = VK_NULL_HANDLE; }
    if (m_DummyCubeImage) { vkDestroyImage(device, m_DummyCubeImage, nullptr); m_DummyCubeImage = VK_NULL_HANDLE; }
    if (m_DummyCubeImageMemory) { vkFreeMemory(device, m_DummyCubeImageMemory, nullptr); m_DummyCubeImageMemory = VK_NULL_HANDLE; }

    // Destroy NEE light SSBOs
    for (u32 i = 0; i < RT_FRAMES_IN_FLIGHT; ++i) {
        if (m_RTNEELightMapped[i]) {
            vkUnmapMemory(device, m_RTNEELightMemory[i]);
            m_RTNEELightMapped[i] = nullptr;
        }
        if (m_RTNEELightBuffer[i]) { vkDestroyBuffer(device, m_RTNEELightBuffer[i], nullptr); m_RTNEELightBuffer[i] = VK_NULL_HANDLE; }
        if (m_RTNEELightMemory[i]) { vkFreeMemory(device, m_RTNEELightMemory[i], nullptr); m_RTNEELightMemory[i] = VK_NULL_HANDLE; }
    }

    // Destroy RT material SSBO
    if (m_RTMaterialMapped) { vkUnmapMemory(device, m_RTMaterialMemory); m_RTMaterialMapped = nullptr; }
    if (m_RTMaterialBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_RTMaterialBuffer, nullptr); m_RTMaterialBuffer = VK_NULL_HANDLE; }
    if (m_RTMaterialMemory != VK_NULL_HANDLE) { vkFreeMemory(device, m_RTMaterialMemory, nullptr); m_RTMaterialMemory = VK_NULL_HANDLE; }
    m_RTMaterialBufferCapacity = 0;

    // Destroy SDF scene SSBO
    if (m_RTSDFMapped) { vkUnmapMemory(device, m_RTSDFMemory); m_RTSDFMapped = nullptr; }
    if (m_RTSDFBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_RTSDFBuffer, nullptr); m_RTSDFBuffer = VK_NULL_HANDLE; }
    if (m_RTSDFMemory != VK_NULL_HANDLE) { vkFreeMemory(device, m_RTSDFMemory, nullptr); m_RTSDFMemory = VK_NULL_HANDLE; }

    // Destroy RT simplified material SSBO
    if (m_RTSimplifiedMaterialMapped) { vkUnmapMemory(device, m_RTSimplifiedMaterialMemory); m_RTSimplifiedMaterialMapped = nullptr; }
    if (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE) { vkDestroyBuffer(device, m_RTSimplifiedMaterialBuffer, nullptr); m_RTSimplifiedMaterialBuffer = VK_NULL_HANDLE; }
    if (m_RTSimplifiedMaterialMemory != VK_NULL_HANDLE) { vkFreeMemory(device, m_RTSimplifiedMaterialMemory, nullptr); m_RTSimplifiedMaterialMemory = VK_NULL_HANDLE; }
    m_RTSimplifiedMaterialBufferCapacity = 0;
}

void RenderSystem::UploadRTMaterials() {
    if (!m_RTEnabled || !m_ASManager) return;

    // Find the highest entity ID among renderable mesh entities to size the buffer
    u32 maxEntityId = 0;
    u32 entityCount = 0;
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!transform || !mesh || !transform->visible) continue;
        if (mesh->vertices.empty() || mesh->indices.empty()) continue;
        if (static_cast<usize>(EntityIndex(entity)) >= m_EntityRenderData.size()) continue;
        if (!m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].valid) continue;

        u32 eid = EntityIndex(entity);
        if (eid > maxEntityId) maxEntityId = eid;
        entityCount++;
    }

    if (entityCount == 0) return;

    // Ensure buffer is large enough (indexed by entity ID, so need maxEntityId + 1 entries)
    u32 requiredCapacity = maxEntityId + 1;
    bool bufferGrew = (requiredCapacity > m_RTMaterialBufferCapacity);
    bool simplifiedBufferGrew = (requiredCapacity > m_RTSimplifiedMaterialBufferCapacity);
    if (bufferGrew || simplifiedBufferGrew) {
        // GPU must be idle before reallocating a buffer that may be in-flight
        vkDeviceWaitIdle(m_VulkanRenderer->GetContext()->GetDevice());
        if (bufferGrew) EnsureRTMaterialBuffer(requiredCapacity);
        if (simplifiedBufferGrew) EnsureRTSimplifiedMaterialBuffer(requiredCapacity);

        // Force descriptor re-write since a buffer handle changed
        m_RTDescriptorsWritten = false;
    }

    if (!m_RTMaterialMapped) return;

    // GPU struct matching GLSL RTSimplifiedMaterial (std430 layout, 64 bytes)
    struct RTSimplifiedMaterialGPU {
        f32 albedo[3];          f32 effectiveRoughness;  // 16 bytes
        f32 f0[3];              f32 kDiffuse;             // 16 bytes
        f32 emissive[3];        f32 opacity;              // 16 bytes
        f32 transmission;       f32 ior;  f32 _pad0;  f32 _pad1; // 16 bytes
    };
    static_assert(sizeof(RTSimplifiedMaterialGPU) == 64, "RTSimplifiedMaterialGPU must be 64 bytes for std430");

    // Upload MaterialGPU for each renderable entity, indexed by entity ID
    // (matches gl_InstanceCustomIndexEXT set in AddInstance)
    auto* dst = static_cast<MaterialGPU*>(m_RTMaterialMapped);
    auto* sdst = m_RTSimplifiedMaterialMapped
                 ? static_cast<RTSimplifiedMaterialGPU*>(m_RTSimplifiedMaterialMapped)
                 : nullptr;
    for (Entity entity : m_World->GetEntitiesWithComponent<MeshComponent>()) {
        auto* transform = m_CachedTransformStorage ? m_CachedTransformStorage->Get(entity) : nullptr;
        auto* mesh = m_CachedMeshStorage ? m_CachedMeshStorage->Get(entity) : nullptr;
        if (!transform || !mesh || !transform->visible) continue;
        if (mesh->vertices.empty() || mesh->indices.empty()) continue;
        if (static_cast<usize>(EntityIndex(entity)) >= m_EntityRenderData.size()) continue;
        if (!m_EntityRenderData[static_cast<usize>(EntityIndex(entity))].valid) continue;

        u32 eid = EntityIndex(entity);
        auto* mat = m_CachedMaterialStorage ? m_CachedMaterialStorage->Get(entity) : nullptr;
        if (mat) {
            dst[eid] = MaterialGPU::FromComponent(*mat);
        } else {
            // Default material for entities without a MaterialComponent
            dst[eid] = MaterialGPU{};
            dst[eid].baseColor = Math::Vector3(1.0f, 1.0f, 1.0f);
            dst[eid].roughness = 0.5f;
            dst[eid].opacity = 1.0f;
            dst[eid].alphaCutoff = 0.5f;
            dst[eid].ior = 1.5f;
            dst[eid].sssColor = Math::Vector3(1.0f, 0.2f, 0.1f);
            dst[eid].sssRadius = 1.0f;
        }

        // Pre-compute simplified material from the just-written MaterialGPU
        if (sdst) {
            const auto& m = dst[eid];
            auto& s = sdst[eid];

            // Albedo
            s.albedo[0] = m.baseColor.x;
            s.albedo[1] = m.baseColor.y;
            s.albedo[2] = m.baseColor.z;

            // Effective roughness (clamped to avoid singularities, matches fetchMaterial)
            s.effectiveRoughness = std::max(m.roughness, 0.04f);

            // F0: reflectance at normal incidence = mix(0.04, baseColor, metallic)
            f32 oneMinusMetallic = 1.0f - m.metallic;
            s.f0[0] = 0.04f * oneMinusMetallic + m.baseColor.x * m.metallic;
            s.f0[1] = 0.04f * oneMinusMetallic + m.baseColor.y * m.metallic;
            s.f0[2] = 0.04f * oneMinusMetallic + m.baseColor.z * m.metallic;

            // kDiffuse: diffuse weight = (1 - metallic)
            s.kDiffuse = oneMinusMetallic;

            // Emissive: pre-multiply color * strength
            s.emissive[0] = m.emissiveColor.x * m.emissiveStrength;
            s.emissive[1] = m.emissiveColor.y * m.emissiveStrength;
            s.emissive[2] = m.emissiveColor.z * m.emissiveStrength;

            s.opacity = m.opacity;
            s.transmission = m.transmission;
            s.ior = m.ior;
            s._pad0 = 0.0f;
            s._pad1 = 0.0f;
        }
    }
}

void RenderSystem::WriteRTDescriptors() {
    auto* ctx = m_VulkanRenderer->GetContext();
    VkDevice device = ctx->GetDevice();
    u32 frameIdx = m_VulkanRenderer->GetCurrentFrameIndex();

    // Binding 0: TLAS (acceleration structure)
    VkAccelerationStructureKHR tlas = m_ASManager->GetTLAS();
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
    asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &tlas;

    // Binding 1: Scene HDR / Accumulation (storage image) — use path tracer output or dummy
    VkDescriptorImageInfo storageImageInfo1{};
    storageImageInfo1.imageView = m_PathTracer ? m_PathTracer->GetOutputView() : m_RTDummyImageView;
    storageImageInfo1.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Bindings 2, 3, 4: Depth, normals, motion vectors (combined image sampler)
    // Binding 2 uses real swapchain depth when available; bindings 3-4 use dummy (no G-buffer MRT yet)
    VkDescriptorImageInfo samplerInfos[3]{};
    for (auto& si : samplerInfos) {
        si.sampler = m_RTDummySampler;
        si.imageView = m_RTDummyImageView;
        si.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    }
    // Wire real depth buffer from swapchain
    auto* swapchain = m_VulkanRenderer->GetSwapchain();
    if (swapchain && swapchain->GetDepthImageView() != VK_NULL_HANDLE) {
        samplerInfos[0].imageView = swapchain->GetDepthImageView();
        samplerInfos[0].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }

    // Bindings 5-8: RT output images (storage image), 14-15: translucency, caustics
    VkDescriptorImageInfo rtOutputInfos[6]{};
    rtOutputInfos[0].imageView = m_RTShadows ? m_RTShadows->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[1].imageView = m_RTReflections ? m_RTReflections->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[2].imageView = m_RTAO ? m_RTAO->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[3].imageView = m_RTGI ? m_RTGI->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[3].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[4].imageView = m_RTTranslucency ? m_RTTranslucency->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[4].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    rtOutputInfos[5].imageView = m_RTCaustics ? m_RTCaustics->GetOutputView() : m_RTDummyImageView;
    rtOutputInfos[5].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Bindings 9-12: Storage buffers
    // Binding 9 = material SSBO (real buffer), bindings 10-12 = dummy (vertex/index/transforms TBD)
    VkDescriptorBufferInfo dummyBufInfos[4]{};
    // Binding 9: Material SSBO — use real buffer if available, otherwise dummy
    if (m_RTMaterialBuffer != VK_NULL_HANDLE && m_RTMaterialBufferCapacity > 0) {
        dummyBufInfos[0].buffer = m_RTMaterialBuffer;
        dummyBufInfos[0].offset = 0;
        dummyBufInfos[0].range = VK_WHOLE_SIZE;
    } else {
        dummyBufInfos[0].buffer = m_RTDummyBuffer;
        dummyBufInfos[0].offset = 0;
        dummyBufInfos[0].range = 256;
    }
    // Bindings 10-12: Still dummy (vertex/index/transforms — future work)
    for (u32 i = 1; i < 4; ++i) {
        dummyBufInfos[i].buffer = m_RTDummyBuffer;
        dummyBufInfos[i].offset = 0;
        dummyBufInfos[i].range = 256;
    }

    // Binding 13: RT light UBO
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_RTLightUBO[frameIdx];
    uboInfo.offset = 0;
    uboInfo.range = 256;

    // Binding 16: NEE light SSBO
    VkDescriptorBufferInfo neeBufInfo{};
    neeBufInfo.buffer = m_RTNEELightBuffer[frameIdx] ? m_RTNEELightBuffer[frameIdx] : m_RTDummyBuffer;
    neeBufInfo.offset = 0;
    neeBufInfo.range = m_RTNEELightBuffer[frameIdx] ? static_cast<VkDeviceSize>(RT_NEE_LIGHT_BUFFER_SIZE) : 256;

    // Binding 17: SDF scene SSBO
    VkDescriptorBufferInfo sdfBufInfo{};
    sdfBufInfo.buffer = m_RTSDFBuffer ? m_RTSDFBuffer : m_RTDummyBuffer;
    sdfBufInfo.offset = 0;
    sdfBufInfo.range = m_RTSDFBuffer ? static_cast<VkDeviceSize>(RT_SDF_BUFFER_SIZE) : 256;

    // Binding 18: Simplified material SSBO
    VkDescriptorBufferInfo simplifiedMatBufInfo{};
    if (m_RTSimplifiedMaterialBuffer != VK_NULL_HANDLE && m_RTSimplifiedMaterialBufferCapacity > 0) {
        simplifiedMatBufInfo.buffer = m_RTSimplifiedMaterialBuffer;
        simplifiedMatBufInfo.offset = 0;
        simplifiedMatBufInfo.range = VK_WHOLE_SIZE;
    } else {
        simplifiedMatBufInfo.buffer = m_RTDummyBuffer;
        simplifiedMatBufInfo.offset = 0;
        simplifiedMatBufInfo.range = 256;
    }

    // Binding 19: ReSTIR reservoir SSBO -- current frame
    VkDescriptorBufferInfo restirBufInfo{};
    if (m_ReSTIR && m_ReSTIR->GetReservoirBuffer() != VK_NULL_HANDLE) {
        restirBufInfo.buffer = m_ReSTIR->GetReservoirBuffer();
        restirBufInfo.offset = 0;
        restirBufInfo.range = VK_WHOLE_SIZE;
    } else {
        restirBufInfo.buffer = m_RTDummyBuffer;
        restirBufInfo.offset = 0;
        restirBufInfo.range = 256;
    }

    // Binding 20: ReSTIR reservoir SSBO -- previous frame (ping-pong for temporal reuse)
    VkDescriptorBufferInfo restirPrevBufInfo{};
    if (m_ReSTIR && m_ReSTIR->GetPrevReservoirBuffer() != VK_NULL_HANDLE) {
        restirPrevBufInfo.buffer = m_ReSTIR->GetPrevReservoirBuffer();
        restirPrevBufInfo.offset = 0;
        restirPrevBufInfo.range = VK_WHOLE_SIZE;
    } else {
        restirPrevBufInfo.buffer = m_RTDummyBuffer;
        restirPrevBufInfo.offset = 0;
        restirPrevBufInfo.range = 256;
    }

    // Binding 21: Radiance cache tile buffer
    VkDescriptorBufferInfo rcTileBufInfo{};
    if (m_RadianceCache && m_RadianceCache->GetTileBuffer() != VK_NULL_HANDLE) {
        rcTileBufInfo.buffer = m_RadianceCache->GetTileBuffer();
        rcTileBufInfo.offset = 0;
        rcTileBufInfo.range = VK_WHOLE_SIZE;
    } else {
        rcTileBufInfo.buffer = m_RTDummyBuffer;
        rcTileBufInfo.offset = 0;
        rcTileBufInfo.range = 256;
    }

    // Binding 22: Radiance cache stale mask buffer
    VkDescriptorBufferInfo rcStaleMaskBufInfo{};
    if (m_RadianceCache && m_RadianceCache->GetStaleMaskBuffer() != VK_NULL_HANDLE) {
        rcStaleMaskBufInfo.buffer = m_RadianceCache->GetStaleMaskBuffer();
        rcStaleMaskBufInfo.offset = 0;
        rcStaleMaskBufInfo.range = VK_WHOLE_SIZE;
    } else {
        rcStaleMaskBufInfo.buffer = m_RTDummyBuffer;
        rcStaleMaskBufInfo.offset = 0;
        rcStaleMaskBufInfo.range = 256;
    }

    // Binding 23: Radiance cache output image
    VkDescriptorImageInfo rcOutputImgInfo{};
    rcOutputImgInfo.imageView = m_RadianceCache ? m_RadianceCache->GetOutputView() : m_RTDummyImageView;
    rcOutputImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Binding 24: Surfel radiance cache buffer
    VkDescriptorBufferInfo surfelBufInfo{};
    if (m_SurfelRadianceCache && m_SurfelRadianceCache->GetSurfelBuffer() != VK_NULL_HANDLE) {
        surfelBufInfo.buffer = m_SurfelRadianceCache->GetSurfelBuffer();
        surfelBufInfo.offset = 0;
        surfelBufInfo.range = VK_WHOLE_SIZE;
    } else {
        surfelBufInfo.buffer = m_RTDummyBuffer;
        surfelBufInfo.offset = 0;
        surfelBufInfo.range = 256;
    }

    // Binding 25: Surfel counter/metadata buffer
    VkDescriptorBufferInfo surfelCounterBufInfo{};
    if (m_SurfelRadianceCache && m_SurfelRadianceCache->GetSurfelCounterBuffer() != VK_NULL_HANDLE) {
        surfelCounterBufInfo.buffer = m_SurfelRadianceCache->GetSurfelCounterBuffer();
        surfelCounterBufInfo.offset = 0;
        surfelCounterBufInfo.range = VK_WHOLE_SIZE;
    } else {
        surfelCounterBufInfo.buffer = m_RTDummyBuffer;
        surfelCounterBufInfo.offset = 0;
        surfelCounterBufInfo.range = 256;
    }

    // Binding 26: Surfel radiance cache output image
    VkDescriptorImageInfo surfelOutputImgInfo{};
    surfelOutputImgInfo.imageView = m_SurfelRadianceCache ? m_SurfelRadianceCache->GetOutputView() : m_RTDummyImageView;
    surfelOutputImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // Build write array for all 27 bindings
    std::array<VkWriteDescriptorSet, 27> writes{};

    // Binding 0: TLAS
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &asInfo;
    writes[0].dstSet = m_RTDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    // Binding 1: Scene HDR (storage image)
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_RTDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &storageImageInfo1;

    // Bindings 2, 3, 4: Depth, normals, motion vectors
    for (u32 i = 0; i < 3; ++i) {
        writes[2 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2 + i].dstSet = m_RTDescriptorSet;
        writes[2 + i].dstBinding = 2 + i;
        writes[2 + i].descriptorCount = 1;
        writes[2 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2 + i].pImageInfo = &samplerInfos[i];
    }

    // Bindings 5-8: RT output images
    for (u32 i = 0; i < 4; ++i) {
        writes[5 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[5 + i].dstSet = m_RTDescriptorSet;
        writes[5 + i].dstBinding = 5 + i;
        writes[5 + i].descriptorCount = 1;
        writes[5 + i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[5 + i].pImageInfo = &rtOutputInfos[i];
    }

    // Bindings 9-12: Storage buffers
    for (u32 i = 0; i < 4; ++i) {
        writes[9 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[9 + i].dstSet = m_RTDescriptorSet;
        writes[9 + i].dstBinding = 9 + i;
        writes[9 + i].descriptorCount = 1;
        writes[9 + i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[9 + i].pBufferInfo = &dummyBufInfos[i];
    }

    // Binding 13: Light UBO
    writes[13].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[13].dstSet = m_RTDescriptorSet;
    writes[13].dstBinding = 13;
    writes[13].descriptorCount = 1;
    writes[13].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[13].pBufferInfo = &uboInfo;

    // Binding 14: RT Translucency output
    writes[14].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[14].dstSet = m_RTDescriptorSet;
    writes[14].dstBinding = 14;
    writes[14].descriptorCount = 1;
    writes[14].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[14].pImageInfo = &rtOutputInfos[4];

    // Binding 15: RT Caustics output
    writes[15].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[15].dstSet = m_RTDescriptorSet;
    writes[15].dstBinding = 15;
    writes[15].descriptorCount = 1;
    writes[15].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[15].pImageInfo = &rtOutputInfos[5];

    // Binding 16: NEE light SSBO
    writes[16].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[16].dstSet = m_RTDescriptorSet;
    writes[16].dstBinding = 16;
    writes[16].descriptorCount = 1;
    writes[16].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[16].pBufferInfo = &neeBufInfo;

    // Binding 17: SDF scene SSBO
    writes[17].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[17].dstSet = m_RTDescriptorSet;
    writes[17].dstBinding = 17;
    writes[17].descriptorCount = 1;
    writes[17].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[17].pBufferInfo = &sdfBufInfo;

    // Binding 18: Simplified material SSBO
    writes[18].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[18].dstSet = m_RTDescriptorSet;
    writes[18].dstBinding = 18;
    writes[18].descriptorCount = 1;
    writes[18].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[18].pBufferInfo = &simplifiedMatBufInfo;

    // Binding 19: ReSTIR reservoir SSBO
    writes[19].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[19].dstSet = m_RTDescriptorSet;
    writes[19].dstBinding = 19;
    writes[19].descriptorCount = 1;
    writes[19].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[19].pBufferInfo = &restirBufInfo;

    // Binding 20: ReSTIR reservoir SSBO — previous frame (placeholder, temporal reuse not yet active)
    writes[20].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[20].dstSet = m_RTDescriptorSet;
    writes[20].dstBinding = 20;
    writes[20].descriptorCount = 1;
    writes[20].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[20].pBufferInfo = &restirPrevBufInfo;

    // Binding 21: Radiance cache tile buffer
    writes[21].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[21].dstSet = m_RTDescriptorSet;
    writes[21].dstBinding = 21;
    writes[21].descriptorCount = 1;
    writes[21].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[21].pBufferInfo = &rcTileBufInfo;

    // Binding 22: Radiance cache stale mask buffer
    writes[22].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[22].dstSet = m_RTDescriptorSet;
    writes[22].dstBinding = 22;
    writes[22].descriptorCount = 1;
    writes[22].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[22].pBufferInfo = &rcStaleMaskBufInfo;

    // Binding 23: Radiance cache output image
    writes[23].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[23].dstSet = m_RTDescriptorSet;
    writes[23].dstBinding = 23;
    writes[23].descriptorCount = 1;
    writes[23].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[23].pImageInfo = &rcOutputImgInfo;

    // Binding 24: Surfel radiance cache buffer
    writes[24].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[24].dstSet = m_RTDescriptorSet;
    writes[24].dstBinding = 24;
    writes[24].descriptorCount = 1;
    writes[24].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[24].pBufferInfo = &surfelBufInfo;

    // Binding 25: Surfel counter/metadata buffer
    writes[25].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[25].dstSet = m_RTDescriptorSet;
    writes[25].dstBinding = 25;
    writes[25].descriptorCount = 1;
    writes[25].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[25].pBufferInfo = &surfelCounterBufInfo;

    // Binding 26: Surfel radiance cache output image
    writes[26].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[26].dstSet = m_RTDescriptorSet;
    writes[26].dstBinding = 26;
    writes[26].descriptorCount = 1;
    writes[26].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[26].pImageInfo = &surfelOutputImgInfo;

    vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);

    ENJIN_LOG_INFO(Renderer, "RT descriptor set written (all 27 bindings)");
}

void RenderSystem::TransitionRTOutputImages(VkCommandBuffer cmd) {
    // Transition all RT output images from UNDEFINED → GENERAL for storage image usage
    std::vector<VkImageMemoryBarrier> barriers;

    auto addBarrier = [&](VkImage image) {
        if (image == VK_NULL_HANDLE) return;
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        barriers.push_back(barrier);
    };

    if (m_RTShadows) addBarrier(m_RTShadows->GetOutputImage());
    if (m_RTReflections) addBarrier(m_RTReflections->GetOutputImage());
    if (m_RTAO) addBarrier(m_RTAO->GetOutputImage());
    if (m_RTGI) addBarrier(m_RTGI->GetOutputImage());
    if (m_RTTranslucency) addBarrier(m_RTTranslucency->GetOutputImage());
    if (m_RTCaustics) addBarrier(m_RTCaustics->GetOutputImage());
    if (m_PathTracer) addBarrier(m_PathTracer->GetOutputImage());
    if (m_RadianceCache) addBarrier(m_RadianceCache->GetOutputImage());
    if (m_SurfelRadianceCache) addBarrier(m_SurfelRadianceCache->GetOutputImage());

    // Also transition the dummy image
    addBarrier(m_RTDummyImage);

    if (!barriers.empty()) {
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 0, nullptr,
            static_cast<u32>(barriers.size()), barriers.data());
    }

    ENJIN_LOG_INFO(Renderer, "RT output images transitioned to GENERAL layout (%zu images)", barriers.size());
}

void RenderSystem::UpdateRTLightUBO(const Math::Matrix4& invViewProj, const Math::Vector3& lightDir,
                                     f32 lightIntensity, f32 shadowDistance, f32 shadowRadius, u32 frameCount,
                                     f32 fireflyClamp, i32 enableNEE, i32 enableMIS,
                                     i32 rrMinBounce, f32 rrMinProb,
                                     u32 dirLightCount, u32 ptLightCount, u32 sptLightCount,
                                     u32 maxBounces, u32 accumulatedSamples) {
    u32 frameIdx = m_VulkanRenderer->GetCurrentFrameIndex();
    if (!m_RTLightUBOMapped[frameIdx]) return;

    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();

    // RT light UBO layout (std140, matches shader LightData uniform block):
    // vec4 lightDir            (offset 0)
    // vec4 lightColor          (offset 16)
    // mat4 invViewProj         (offset 32)
    // vec2 screenSize          (offset 96)
    // uint frameCount          (offset 104)
    // float shadowRadius       (offset 108)
    // float fireflyClamp       (offset 112)
    // int enableNEE            (offset 116)
    // int enableMIS            (offset 120)
    // int rrMinBounce          (offset 124)
    // float rrMinProb          (offset 128)
    // uint dirLightCount       (offset 132)
    // uint pointLightCount     (offset 136)
    // uint spotLightCount      (offset 140)
    // uint maxBounces          (offset 144)
    // uint accumulatedSamples  (offset 148)
    // uint _pad[2]             (offset 152)  -- pad to 160 bytes
    struct RTLightData {
        f32 lightDir[4];
        f32 lightColor[4];
        f32 invViewProj[16];
        f32 screenSize[2];
        u32 frameCount;
        f32 shadowRadius;
        // Path tracer config
        f32 fireflyClamp;
        i32 enableNEE;
        i32 enableMIS;
        i32 rrMinBounce;
        f32 rrMinProb;
        // NEE light counts
        u32 directionalLightCount;
        u32 pointLightCount;
        u32 spotLightCount;
        // Convergence
        u32 maxBounces;
        u32 accumulatedSamples;
        // SDF fallback (reflection shader reads these at offset 152-160)
        f32 sdfFallbackEnabled;   // >0.5 = SDF sphere-trace fallback active
        f32 sdfMaxDistance;       // Max sphere-trace distance
    };

    RTLightData data{};
    data.lightDir[0] = lightDir.x;
    data.lightDir[1] = lightDir.y;
    data.lightDir[2] = lightDir.z;
    data.lightDir[3] = lightIntensity;
    data.lightColor[0] = 1.0f;
    data.lightColor[1] = 1.0f;
    data.lightColor[2] = 1.0f;
    data.lightColor[3] = shadowDistance;

    // Copy mat4 (our Matrix4 stores as flat f32 m[16])
    for (int i = 0; i < 16; ++i) {
        data.invViewProj[i] = invViewProj.m[i];
    }

    data.screenSize[0] = static_cast<f32>(extent.width);
    data.screenSize[1] = static_cast<f32>(extent.height);
    data.frameCount = frameCount;
    data.shadowRadius = shadowRadius;

    // Path tracer config
    data.fireflyClamp = fireflyClamp;
    data.enableNEE = enableNEE;
    data.enableMIS = enableMIS;
    data.rrMinBounce = rrMinBounce;
    data.rrMinProb = rrMinProb;

    // NEE light counts
    data.directionalLightCount = dirLightCount;
    data.pointLightCount = ptLightCount;
    data.spotLightCount = sptLightCount;

    // Convergence
    data.maxBounces = maxBounces;
    data.accumulatedSamples = accumulatedSamples;

    // SDF fallback settings (read by reflection shader at offsets 152-160)
    if (m_RTReflections && m_SDFScene && m_SDFScene->GetObjectCount() > 0) {
        data.sdfFallbackEnabled = m_RTReflections->GetConfig().sdfFallback ? 1.0f : 0.0f;
        data.sdfMaxDistance = m_RTReflections->GetConfig().sdfMaxDistance;
    } else {
        data.sdfFallbackEnabled = 0.0f;
        data.sdfMaxDistance = 0.0f;
    }

    std::memcpy(m_RTLightUBOMapped[frameIdx], &data, sizeof(data));

    // Update descriptor binding 13 to point to this frame's UBO
    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = m_RTLightUBO[frameIdx];
    uboInfo.offset = 0;
    uboInfo.range = 256;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_RTDescriptorSet;
    write.dstBinding = 13;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &uboInfo;
    vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &write, 0, nullptr);

    // Upload NEE light data to SSBO (binding 16) for path tracer direct light sampling
    if (m_RTNEELightMapped[frameIdx] && (dirLightCount > 0 || ptLightCount > 0 || sptLightCount > 0)) {
        // NEE light SSBO layout: packed RTLight structs (64 bytes each, matches rt_common.glsl)
        // RTLight { vec3 position, float range, vec3 direction, float intensity,
        //           vec3 color, int type, float innerCutoff, float outerCutoff, float _pad0, float _pad1 }
        struct RTLightGPU {
            f32 position[3]; f32 range;
            f32 direction[3]; f32 intensity;
            f32 color[3]; i32 type;
            f32 innerCutoff; f32 outerCutoff;
            f32 _pad0; f32 _pad1;
        };

        u8* dst = static_cast<u8*>(m_RTNEELightMapped[frameIdx]);
        u32 offset = 0;
        u32 maxLights = RT_NEE_LIGHT_BUFFER_SIZE / sizeof(RTLightGPU);
        u32 totalLights = 0;

        // Copy directional lights
        for (u32 i = 0; i < dirLightCount && totalLights < maxLights; ++i) {
            RTLightGPU light{};
            light.position[0] = 0.0f; light.position[1] = 0.0f; light.position[2] = 0.0f;
            light.range = 0.0f;  // Infinite for directional
            light.direction[0] = m_CachedLightingData.directionalLights[i].direction.x;
            light.direction[1] = m_CachedLightingData.directionalLights[i].direction.y;
            light.direction[2] = m_CachedLightingData.directionalLights[i].direction.z;
            light.intensity = m_CachedLightingData.directionalLights[i].intensity;
            light.color[0] = m_CachedLightingData.directionalLights[i].color.x;
            light.color[1] = m_CachedLightingData.directionalLights[i].color.y;
            light.color[2] = m_CachedLightingData.directionalLights[i].color.z;
            light.type = 0;
            light.innerCutoff = 0.0f;
            light.outerCutoff = 0.0f;
            std::memcpy(dst + offset, &light, sizeof(RTLightGPU));
            offset += sizeof(RTLightGPU);
            totalLights++;
        }

        // Copy point lights
        for (u32 i = 0; i < ptLightCount && totalLights < maxLights; ++i) {
            RTLightGPU light{};
            light.position[0] = m_CachedLightingData.pointLights[i].position.x;
            light.position[1] = m_CachedLightingData.pointLights[i].position.y;
            light.position[2] = m_CachedLightingData.pointLights[i].position.z;
            light.range = m_CachedLightingData.pointLights[i].range;
            light.direction[0] = 0.0f; light.direction[1] = 0.0f; light.direction[2] = 0.0f;
            light.intensity = m_CachedLightingData.pointLights[i].intensity;
            light.color[0] = m_CachedLightingData.pointLights[i].color.x;
            light.color[1] = m_CachedLightingData.pointLights[i].color.y;
            light.color[2] = m_CachedLightingData.pointLights[i].color.z;
            light.type = 1;
            light.innerCutoff = 0.0f;
            light.outerCutoff = 0.0f;
            std::memcpy(dst + offset, &light, sizeof(RTLightGPU));
            offset += sizeof(RTLightGPU);
            totalLights++;
        }

        // Copy spot lights
        for (u32 i = 0; i < sptLightCount && totalLights < maxLights; ++i) {
            RTLightGPU light{};
            light.position[0] = m_CachedLightingData.spotLights[i].position.x;
            light.position[1] = m_CachedLightingData.spotLights[i].position.y;
            light.position[2] = m_CachedLightingData.spotLights[i].position.z;
            light.range = m_CachedLightingData.spotLights[i].range;
            light.direction[0] = m_CachedLightingData.spotLights[i].direction.x;
            light.direction[1] = m_CachedLightingData.spotLights[i].direction.y;
            light.direction[2] = m_CachedLightingData.spotLights[i].direction.z;
            light.intensity = m_CachedLightingData.spotLights[i].intensity;
            light.color[0] = m_CachedLightingData.spotLights[i].color.x;
            light.color[1] = m_CachedLightingData.spotLights[i].color.y;
            light.color[2] = m_CachedLightingData.spotLights[i].color.z;
            light.type = 2;
            light.innerCutoff = m_CachedLightingData.spotLights[i].innerCutoff;
            light.outerCutoff = m_CachedLightingData.spotLights[i].outerCutoff;
            std::memcpy(dst + offset, &light, sizeof(RTLightGPU));
            offset += sizeof(RTLightGPU);
            totalLights++;
        }

        // Update descriptor binding 16 to point to this frame's NEE light SSBO
        VkDescriptorBufferInfo neeBufInfo{};
        neeBufInfo.buffer = m_RTNEELightBuffer[frameIdx];
        neeBufInfo.offset = 0;
        neeBufInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet neeWrite{};
        neeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        neeWrite.dstSet = m_RTDescriptorSet;
        neeWrite.dstBinding = 16;
        neeWrite.descriptorCount = 1;
        neeWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        neeWrite.pBufferInfo = &neeBufInfo;
        vkUpdateDescriptorSets(m_VulkanRenderer->GetContext()->GetDevice(), 1, &neeWrite, 0, nullptr);
    }
}

// ============================================================================
// TEMPORAL UPSCALING (FSR 2 / DLSS / XeSS)
// ============================================================================

void RenderSystem::SetUpscalerType(u32 type) {
    if (type == m_UpscalerType) return;

    // Tear down existing upscaler
    if (m_Upscaler) {
        m_Upscaler->Shutdown();
        m_Upscaler.reset();
    }

    m_UpscalerType = type;

    if (type == 0) {
        ENJIN_LOG_INFO(Renderer, "Temporal upscaler disabled");
        return;
    }

    // Create the requested backend
    auto* ctx = m_VulkanRenderer->GetContext();
    switch (type) {
        case 1: // FSR 2
            m_Upscaler = std::make_unique<Renderer::FSR2Upscaler>(ctx);
            break;
        case 2: // DLSS
            m_Upscaler = std::make_unique<Renderer::DLSSUpscaler>(ctx);
            break;
        case 3: // XeSS
            m_Upscaler = std::make_unique<Renderer::XeSSUpscaler>(ctx);
            break;
        default:
            m_UpscalerType = 0;
            return;
    }

    if (!m_Upscaler->IsAvailable()) {
        ENJIN_LOG_WARN(Renderer, "%s upscaler SDK not compiled in", m_Upscaler->GetName());
        m_Upscaler.reset();
        m_UpscalerType = 0;
        return;
    }

    // Initialize with current display resolution
    VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
    u32 renderW, renderH;
    Renderer::IUpscaler::GetRenderResolution(
        extent.width, extent.height,
        static_cast<Renderer::UpscalerQuality>(m_UpscalerQuality),
        renderW, renderH);

    if (!m_Upscaler->Initialize(renderW, renderH, extent.width, extent.height,
                                static_cast<Renderer::UpscalerQuality>(m_UpscalerQuality))) {
        ENJIN_LOG_WARN(Renderer, "Failed to initialize %s upscaler", m_Upscaler->GetName());
        m_Upscaler.reset();
        m_UpscalerType = 0;
        return;
    }

    ENJIN_LOG_INFO(Renderer, "%s upscaler active (%ux%u -> %ux%u)",
                   m_Upscaler->GetName(), renderW, renderH, extent.width, extent.height);
}

void RenderSystem::SetUpscalerQuality(u32 quality) {
    if (quality == m_UpscalerQuality) return;
    m_UpscalerQuality = quality;

    if (m_Upscaler && m_UpscalerType > 0) {
        VkExtent2D extent = m_VulkanRenderer->GetSwapchainExtent();
        u32 renderW, renderH;
        Renderer::IUpscaler::GetRenderResolution(
            extent.width, extent.height,
            static_cast<Renderer::UpscalerQuality>(quality),
            renderW, renderH);
        m_Upscaler->Resize(renderW, renderH, extent.width, extent.height);
        ENJIN_LOG_INFO(Renderer, "Upscaler quality changed: %ux%u -> %ux%u",
                       renderW, renderH, extent.width, extent.height);
    }
}

} // namespace ECS
} // namespace Enjin

#endif // !ENJIN_RENDERER_WEBGPU
