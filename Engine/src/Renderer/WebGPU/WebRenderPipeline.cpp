#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebRenderPipeline.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderCompiler.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipeline.h"
#include "Enjin/Logging/Log.h"
#include <webgpu/webgpu.h>
#include <cstring>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#include "stb_image.h"

namespace Enjin::Renderer {

// Helper: create a WGPUStringView from a C string literal
static WGPUStringView wgpuStr(const char* s) { return {s, WGPU_STRLEN}; }

// ============================================================================
// Embedded PBR shader source (matches Engine/shaders/wgsl/pbr.wgsl)
// Embedded here so the web player works without loading shader files from disk.
// ============================================================================
static const char* PBR_WGSL_SOURCE = R"(
// Enjin Engine — PBR shader (WebGPU / WGSL)

struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};

struct LightingUBO {
    lightDir: array<vec4<f32>, 8>,
    lightColor: array<vec4<f32>, 8>,
    lightParams: array<vec4<f32>, 8>,
    ambientColor: vec4<f32>,
    fogColor: vec4<f32>,
    fogParams: vec4<f32>,
    shadowParams: vec4<f32>,
    lightCount: vec4<f32>,
};

@group(0) @binding(0) var<uniform> viewProj: ViewProjection;
@group(0) @binding(1) var<uniform> lighting: LightingUBO;

struct ObjectData {
    model: mat4x4<f32>,
    baseColor: vec3<f32>,
    metallic: f32,
    emissiveColor: vec3<f32>,
    roughness: f32,
    emissiveStrength: f32,
    opacity: f32,
    alphaCutoff: f32,
    flags: i32,
    parallaxScale: f32,
};
@group(1) @binding(0) var<uniform> object: ObjectData;

@group(2) @binding(0) var baseColorTex: texture_2d<f32>;
@group(2) @binding(1) var baseColorSmp: sampler;
@group(2) @binding(2) var normalTex: texture_2d<f32>;
@group(2) @binding(3) var normalSmp: sampler;
@group(2) @binding(4) var mrTex: texture_2d<f32>;
@group(2) @binding(5) var mrSmp: sampler;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) tangent: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_pos: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) world_tangent: vec3<f32>,
    @location(4) world_bitangent: vec3<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let world_pos = object.model * vec4<f32>(in.position, 1.0);
    out.clip_position = viewProj.proj * viewProj.view * world_pos;
    out.world_pos = world_pos.xyz;

    let normal_mat = mat3x3<f32>(
        object.model[0].xyz,
        object.model[1].xyz,
        object.model[2].xyz
    );
    out.world_normal = normalize(normal_mat * in.normal);
    out.world_tangent = normalize(normal_mat * in.tangent.xyz);
    out.world_bitangent = cross(out.world_normal, out.world_tangent) * in.tangent.w;
    out.uv = in.uv;
    return out;
}

fn distributionGGX(N: vec3<f32>, H: vec3<f32>, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let NdotH = max(dot(N, H), 0.0);
    let NdotH2 = NdotH * NdotH;
    let denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * denom * denom);
}

fn geometrySchlickGGX(NdotV: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

fn geometrySmith(N: vec3<f32>, V: vec3<f32>, L: vec3<f32>, roughness: f32) -> f32 {
    let NdotV = max(dot(N, V), 0.0);
    let NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

fn fresnelSchlick(cosTheta: f32, F0: vec3<f32>) -> vec3<f32> {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let albedo = textureSample(baseColorTex, baseColorSmp, in.uv).rgb * object.baseColor;
    let mr = textureSample(mrTex, mrSmp, in.uv);
    let metallic = mr.b * object.metallic;
    let roughness = mr.g * object.roughness;

    let tangentNormal = textureSample(normalTex, normalSmp, in.uv).rgb * 2.0 - 1.0;
    let TBN = mat3x3<f32>(
        normalize(in.world_tangent),
        normalize(in.world_bitangent),
        normalize(in.world_normal)
    );
    let N = normalize(TBN * tangentNormal);
    let V = normalize(viewProj.viewPos - in.world_pos);

    let F0_dielectric = vec3<f32>(0.04);
    let F0 = mix(F0_dielectric, albedo, metallic);

    var Lo = vec3<f32>(0.0);

    // Directional lights
    let dirCount = i32(lighting.lightCount.x);
    for (var i = 0; i < dirCount; i = i + 1) {
        let L = normalize(-lighting.lightDir[i].xyz);
        let H = normalize(V + L);
        let radiance = lighting.lightColor[i].rgb * lighting.lightColor[i].w;

        let NDF = distributionGGX(N, H, roughness);
        let G = geometrySmith(N, V, L, roughness);
        let F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        let numerator = NDF * G * F;
        let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        let specular = numerator / denominator;

        let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
        let NdotL = max(dot(N, L), 0.0);
        Lo = Lo + (kD * albedo / 3.14159265 + specular) * radiance * NdotL;
    }

    // Point lights
    let pointCount = i32(lighting.lightCount.y);
    for (var i = dirCount; i < dirCount + pointCount; i = i + 1) {
        let lightPos = lighting.lightDir[i].xyz;
        let diff = lightPos - in.world_pos;
        let distance = length(diff);
        let range = lighting.lightParams[i].x;
        if (distance > range) { continue; }

        let L = normalize(diff);
        let H = normalize(V + L);
        let attenuation = 1.0 / (1.0 + distance * distance);
        let radiance = lighting.lightColor[i].rgb * lighting.lightColor[i].w * attenuation;

        let NDF = distributionGGX(N, H, roughness);
        let G = geometrySmith(N, V, L, roughness);
        let F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        let numerator = NDF * G * F;
        let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        let specular = numerator / denominator;

        let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
        let NdotL = max(dot(N, L), 0.0);
        Lo = Lo + (kD * albedo / 3.14159265 + specular) * radiance * NdotL;
    }

    let ambient = lighting.ambientColor.rgb * lighting.ambientColor.w * albedo;
    let emissive = object.emissiveColor * object.emissiveStrength;

    var color = ambient + Lo + emissive;

    // Tone mapping (Reinhard)
    color = color / (color + vec3<f32>(1.0));
    // Gamma correction
    color = pow(color, vec3<f32>(1.0 / 2.2));

    return vec4<f32>(color, object.opacity);
}
)";

// ============================================================================
// Lifecycle
// ============================================================================

WebRenderPipeline::WebRenderPipeline(WebGPURenderer* renderer)
    : m_Renderer(renderer) {}

WebRenderPipeline::~WebRenderPipeline() {
    Shutdown();
}

bool WebRenderPipeline::Initialize() {
    if (m_Initialized || !m_Renderer) return false;

    m_CanvasWidth = m_Renderer->GetSwapChainWidth();
    m_CanvasHeight = m_Renderer->GetSwapChainHeight();

    CreateUniformBuffers();
    CreateDefaultTextures();
    CreatePBRPipeline();

    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Core, "WebRenderPipeline: Failed to create PBR pipeline");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Core, "WebRenderPipeline initialized (%ux%u)", m_CanvasWidth, m_CanvasHeight);
    return true;
}

void WebRenderPipeline::Shutdown() {
    if (!m_Initialized) return;

    // Release entity GPU data
    for (auto& [entity, data] : m_EntityGPUData) {
        m_Renderer->DestroyBuffer(data.vertexBuffer);
        m_Renderer->DestroyBuffer(data.indexBuffer);
        if (data.textureBindGroup) wgpuBindGroupRelease(data.textureBindGroup);
    }
    m_EntityGPUData.clear();

    // Release texture cache
    for (auto& [path, handle] : m_TextureCache) {
        m_Renderer->DestroyTexture(handle);
    }
    m_TextureCache.clear();

    // Release default textures
    if (m_DefaultTextureBindGroup) { wgpuBindGroupRelease(m_DefaultTextureBindGroup); m_DefaultTextureBindGroup = nullptr; }
    m_Renderer->DestroyTexture(m_DefaultWhiteTexture);
    m_Renderer->DestroyTexture(m_DefaultNormalTexture);
    m_Renderer->DestroyTexture(m_DefaultBlackTexture);

    // Release bind groups
    if (m_ViewLightBindGroup) { wgpuBindGroupRelease(m_ViewLightBindGroup); m_ViewLightBindGroup = nullptr; }
    if (m_ObjectBindGroup) { wgpuBindGroupRelease(m_ObjectBindGroup); m_ObjectBindGroup = nullptr; }

    // Release uniform buffers
    m_Renderer->DestroyBuffer(m_ViewProjBuffer);
    m_Renderer->DestroyBuffer(m_LightingBuffer);
    m_Renderer->DestroyBuffer(m_ObjectBuffer);

    // Release pipeline objects
    if (m_Pipeline) { wgpuRenderPipelineRelease(m_Pipeline); m_Pipeline = nullptr; }
    if (m_PipelineLayout) { wgpuPipelineLayoutRelease(m_PipelineLayout); m_PipelineLayout = nullptr; }
    if (m_ViewLightLayout) { wgpuBindGroupLayoutRelease(m_ViewLightLayout); m_ViewLightLayout = nullptr; }
    if (m_ObjectLayout) { wgpuBindGroupLayoutRelease(m_ObjectLayout); m_ObjectLayout = nullptr; }
    if (m_TextureLayout) { wgpuBindGroupLayoutRelease(m_TextureLayout); m_TextureLayout = nullptr; }

    m_Initialized = false;
    ENJIN_LOG_INFO(Core, "WebRenderPipeline shut down");
}

// ============================================================================
// Pipeline creation
// ============================================================================

void WebRenderPipeline::CreateUniformBuffers() {
    // ViewProjection UBO — 144 bytes, round up to 256 for alignment safety
    m_ViewProjBuffer = m_Renderer->CreateBuffer(256, WGPUBufferUsage_Uniform, nullptr);

    // Lighting UBO — 464 bytes, round up to 512
    m_LightingBuffer = m_Renderer->CreateBuffer(512, WGPUBufferUsage_Uniform, nullptr);

    // Object UBO — 128 bytes, round up to 256
    m_ObjectBuffer = m_Renderer->CreateBuffer(256, WGPUBufferUsage_Uniform, nullptr);

    m_Stats.gpuBufferBytes += 256 + 512 + 256;
}

void WebRenderPipeline::CreateDefaultTextures() {
    // 1x1 white texture (default base color)
    u8 white[] = { 255, 255, 255, 255 };
    m_DefaultWhiteTexture = m_Renderer->CreateTexture(1, 1, WGPUTextureFormat_RGBA8Unorm,
        WGPUTextureUsage_TextureBinding, white);

    // 1x1 flat normal (0.5, 0.5, 1.0 = tangent-space up)
    u8 normal[] = { 128, 128, 255, 255 };
    m_DefaultNormalTexture = m_Renderer->CreateTexture(1, 1, WGPUTextureFormat_RGBA8Unorm,
        WGPUTextureUsage_TextureBinding, normal);

    // 1x1 black texture (default metallic-roughness: metallic=0 roughness=1)
    // mr.b = metallic (0), mr.g = roughness (255 = 1.0)
    u8 mr[] = { 0, 255, 0, 255 };
    m_DefaultBlackTexture = m_Renderer->CreateTexture(1, 1, WGPUTextureFormat_RGBA8Unorm,
        WGPUTextureUsage_TextureBinding, mr);

    m_Stats.gpuTextureBytes += 4 * 3;  // 3 textures, 4 bytes each
}

void WebRenderPipeline::CreatePBRPipeline() {
    WGPUDevice device = m_Renderer->GetDevice();

    // Compile shader
    auto* compiler = m_Renderer->GetShaderCompiler();
    WGPUShaderModule shaderModule = compiler->CompileWGSL(PBR_WGSL_SOURCE, "web_pbr");
    if (!shaderModule) {
        ENJIN_LOG_ERROR(Core, "WebRenderPipeline: Shader compilation failed: %s",
                        compiler->GetLastError().c_str());
        return;
    }

    // --- Bind group layout 0: ViewProjection + Lighting ---
    WGPUBindGroupLayoutEntry viewLightEntries[2] = {};
    // binding 0: ViewProjection UBO
    viewLightEntries[0].binding = 0;
    viewLightEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    viewLightEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
    viewLightEntries[0].buffer.minBindingSize = sizeof(WebViewProjectionUBO);
    // binding 1: Lighting UBO
    viewLightEntries[1].binding = 1;
    viewLightEntries[1].visibility = WGPUShaderStage_Fragment;
    viewLightEntries[1].buffer.type = WGPUBufferBindingType_Uniform;
    viewLightEntries[1].buffer.minBindingSize = sizeof(WebLightingUBO);

    WGPUBindGroupLayoutDescriptor viewLightLayoutDesc = {};
    viewLightLayoutDesc.entryCount = 2;
    viewLightLayoutDesc.entries = viewLightEntries;
    m_ViewLightLayout = wgpuDeviceCreateBindGroupLayout(device, &viewLightLayoutDesc);

    // --- Bind group layout 1: ObjectData ---
    WGPUBindGroupLayoutEntry objectEntry = {};
    objectEntry.binding = 0;
    objectEntry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    objectEntry.buffer.type = WGPUBufferBindingType_Uniform;
    objectEntry.buffer.minBindingSize = sizeof(WebObjectDataUBO);

    WGPUBindGroupLayoutDescriptor objectLayoutDesc = {};
    objectLayoutDesc.entryCount = 1;
    objectLayoutDesc.entries = &objectEntry;
    m_ObjectLayout = wgpuDeviceCreateBindGroupLayout(device, &objectLayoutDesc);

    // --- Bind group layout 2: Textures (3 tex + 3 sampler) ---
    WGPUBindGroupLayoutEntry texEntries[6] = {};
    for (int i = 0; i < 3; i++) {
        // Texture
        texEntries[i * 2].binding = i * 2;
        texEntries[i * 2].visibility = WGPUShaderStage_Fragment;
        texEntries[i * 2].texture.sampleType = WGPUTextureSampleType_Float;
        texEntries[i * 2].texture.viewDimension = WGPUTextureViewDimension_2D;
        texEntries[i * 2].texture.multisampled = false;
        // Sampler
        texEntries[i * 2 + 1].binding = i * 2 + 1;
        texEntries[i * 2 + 1].visibility = WGPUShaderStage_Fragment;
        texEntries[i * 2 + 1].sampler.type = WGPUSamplerBindingType_Filtering;
    }

    WGPUBindGroupLayoutDescriptor texLayoutDesc = {};
    texLayoutDesc.entryCount = 6;
    texLayoutDesc.entries = texEntries;
    m_TextureLayout = wgpuDeviceCreateBindGroupLayout(device, &texLayoutDesc);

    // --- Pipeline layout ---
    WGPUBindGroupLayout layouts[] = { m_ViewLightLayout, m_ObjectLayout, m_TextureLayout };
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.bindGroupLayoutCount = 3;
    pipelineLayoutDesc.bindGroupLayouts = layouts;
    m_PipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // --- Vertex layout ---
    // Matches ECS::Vertex: position(0), normal(1), uv(2), color(skip), tangent(3)
    WGPUVertexAttribute attributes[4] = {};
    attributes[0].format = WGPUVertexFormat_Float32x3;
    attributes[0].offset = offsetof(ECS::Vertex, position);
    attributes[0].shaderLocation = 0;

    attributes[1].format = WGPUVertexFormat_Float32x3;
    attributes[1].offset = offsetof(ECS::Vertex, normal);
    attributes[1].shaderLocation = 1;

    attributes[2].format = WGPUVertexFormat_Float32x2;
    attributes[2].offset = offsetof(ECS::Vertex, uv);
    attributes[2].shaderLocation = 2;

    attributes[3].format = WGPUVertexFormat_Float32x4;
    attributes[3].offset = offsetof(ECS::Vertex, tangent);
    attributes[3].shaderLocation = 3;

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = sizeof(ECS::Vertex);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = 4;
    vertexBufferLayout.attributes = attributes;

    // --- Render pipeline ---
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = GetPreferredSwapChainFormat();
    colorTarget.writeMask = WGPUColorWriteMask_All;

    // Alpha blending
    WGPUBlendState blend = {};
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.color.operation = WGPUBlendOperation_Add;
    blend.alpha.srcFactor = WGPUBlendFactor_One;
    blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.operation = WGPUBlendOperation_Add;
    colorTarget.blend = &blend;

    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = wgpuStr("fs_main");
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    WGPUDepthStencilState depthStencil = {};
    depthStencil.format = GetDepthStencilFormat();
    depthStencil.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencil.depthCompare = WGPUCompareFunction_Less;

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.layout = m_PipelineLayout;
    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = wgpuStr("vs_main");
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = &depthStencil;
    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;  // Disabled until Phase 3 frustum culling validates mesh winding
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = 0xFFFFFFFF;

    m_Pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
    wgpuShaderModuleRelease(shaderModule);

    if (!m_Pipeline) {
        ENJIN_LOG_ERROR(Core, "WebRenderPipeline: Pipeline creation failed");
        return;
    }

    // --- Create bind groups ---
    // Group 0: ViewProjection + Lighting
    WGPUBindGroupEntry viewLightBGEntries[2] = {};
    viewLightBGEntries[0].binding = 0;
    viewLightBGEntries[0].buffer = m_ViewProjBuffer.buffer;
    viewLightBGEntries[0].offset = 0;
    viewLightBGEntries[0].size = sizeof(WebViewProjectionUBO);
    viewLightBGEntries[1].binding = 1;
    viewLightBGEntries[1].buffer = m_LightingBuffer.buffer;
    viewLightBGEntries[1].offset = 0;
    viewLightBGEntries[1].size = sizeof(WebLightingUBO);
    m_ViewLightBindGroup = m_Renderer->CreateBindGroup(m_ViewLightLayout, {viewLightBGEntries[0], viewLightBGEntries[1]});

    // Group 1: ObjectData
    WGPUBindGroupEntry objectBGEntry = {};
    objectBGEntry.binding = 0;
    objectBGEntry.buffer = m_ObjectBuffer.buffer;
    objectBGEntry.offset = 0;
    objectBGEntry.size = sizeof(WebObjectDataUBO);
    m_ObjectBindGroup = m_Renderer->CreateBindGroup(m_ObjectLayout, {objectBGEntry});

    // Group 2: Default textures
    m_DefaultTextureBindGroup = CreateTextureBindGroup(
        m_DefaultWhiteTexture, m_DefaultNormalTexture, m_DefaultBlackTexture);

    ENJIN_LOG_INFO(Core, "WebRenderPipeline: PBR pipeline created");
}

// ============================================================================
// Per-frame rendering
// ============================================================================

void WebRenderPipeline::BeginFrame(const Camera* camera, ECS::World* world) {
    if (!m_Initialized || !camera || !world) return;

    m_FrameWorld = world;
    m_FrameCamera = camera;
    m_Stats.drawCalls = 0;
    m_Stats.entityCount = 0;
    m_Stats.triangleCount = 0;

    // Sync camera position from the first CameraComponent entity on first frame
    if (!m_CameraSynced) {
        Camera* cam = const_cast<Camera*>(camera);
        bool found = false;
        for (auto entity : world->GetEntitiesWithComponent<ECS::CameraComponent>()) {
            auto* xform = world->GetComponent<ECS::TransformComponent>(entity);
            if (xform) {
                Math::Vector3 target(0.0f, 0.0f, 0.0f);
                cam->SetLookAt(xform->position, target, Math::Vector3(0.0f, 1.0f, 0.0f));
                found = true;
                break;
            }
        }
        if (!found) {
            cam->SetLookAt(
                Math::Vector3(0.0f, 8.0f, 15.0f),
                Math::Vector3(0.0f, 0.0f, 0.0f),
                Math::Vector3(0.0f, 1.0f, 0.0f));
        }
        m_CameraSynced = true;
    }

    UpdateViewProjectionUBO(camera);
    UpdateLightingUBO(world);

    // Periodic cleanup of destroyed entities (every 60 frames)
    static u32 frameCounter = 0;
    if (++frameCounter % 60 == 0) {
        CleanupDestroyedEntities(world);
    }
}

void WebRenderPipeline::Render() {
    if (!m_Initialized || !m_Pipeline || !m_FrameWorld) return;
    if (!m_Renderer->BeginFrameWebGPU()) return;

    m_Renderer->SetPipeline(m_Pipeline);
    m_Renderer->SetBindGroup(0, m_ViewLightBindGroup);

    for (auto entity : m_FrameWorld->GetEntitiesWithComponent<ECS::MeshComponent>()) {
        auto* mesh = m_FrameWorld->GetComponent<ECS::MeshComponent>(entity);
        auto* xform = m_FrameWorld->GetComponent<ECS::TransformComponent>(entity);
        if (!mesh || !xform || !xform->visible) continue;
        if (mesh->vertices.empty() || mesh->indices.empty()) continue;

        // Ensure GPU buffers exist
        EnsureEntityGPUData(entity, mesh);
        auto& gpuData = m_EntityGPUData[entity];

        // Load textures on first encounter
        auto* mat = m_FrameWorld->GetComponent<ECS::MaterialComponent>(entity);
        LoadEntityTextures(entity, mat);

        // Update per-object uniforms
        UpdateObjectDataUBO(entity, m_FrameWorld);

        // Bind per-object data
        m_Renderer->SetBindGroup(1, m_ObjectBindGroup);
        m_Renderer->SetBindGroup(2, gpuData.textureBindGroup ? gpuData.textureBindGroup : m_DefaultTextureBindGroup);

        // Draw
        m_Renderer->SetVertexBuffer(0, gpuData.vertexBuffer.buffer, 0, 0);
        m_Renderer->SetIndexBuffer(gpuData.indexBuffer.buffer, WGPUIndexFormat_Uint32, 0, 0);
        m_Renderer->DrawIndexed(gpuData.indexCount, 1, 0, 0, 0);

        m_Stats.drawCalls++;
        m_Stats.entityCount++;
        m_Stats.triangleCount += gpuData.indexCount / 3;
    }

    m_Renderer->EndFrame();
}

void WebRenderPipeline::EndFrame() {
    m_FrameWorld = nullptr;
    m_FrameCamera = nullptr;
}

// ============================================================================
// UBO updates
// ============================================================================

void WebRenderPipeline::UpdateViewProjectionUBO(const Camera* camera) {
    WebViewProjectionUBO ubo;
    ubo.view = camera->GetViewMatrix();
    ubo.proj = camera->GetProjectionMatrix();
    ubo.viewPos = camera->GetPosition();
    ubo.time = m_Time;
    m_Renderer->UpdateBuffer(m_ViewProjBuffer, &ubo, sizeof(ubo), 0);
}

void WebRenderPipeline::UpdateLightingUBO(ECS::World* world) {
    WebLightingUBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));

    u32 dirCount = 0;
    u32 pointCount = 0;
    constexpr u32 MAX_DIR_LIGHTS = 4;
    constexpr u32 MAX_POINT_LIGHTS = 4;

    if (world) {
        for (auto entity : world->GetEntitiesWithComponent<ECS::LightComponent>()) {
            auto* light = world->GetComponent<ECS::LightComponent>(entity);
            auto* xform = world->GetComponent<ECS::TransformComponent>(entity);
            if (!light || !xform) continue;

            if (light->type == ECS::LightType::Directional && dirCount < MAX_DIR_LIGHTS) {
                auto dir = (xform->rotation.GetForward() * -1.0f).Normalized();
                ubo.lightDir[dirCount] = Math::Vector4(dir.x, dir.y, dir.z, 0.0f);
                ubo.lightColor[dirCount] = Math::Vector4(light->color.x, light->color.y,
                                                          light->color.z, light->intensity);
                dirCount++;
            } else if (light->type == ECS::LightType::Point && pointCount < MAX_POINT_LIGHTS) {
                u32 idx = MAX_DIR_LIGHTS + pointCount;
                ubo.lightDir[idx] = Math::Vector4(xform->position.x, xform->position.y,
                                                    xform->position.z, 1.0f);
                ubo.lightColor[idx] = Math::Vector4(light->color.x, light->color.y,
                                                      light->color.z, light->intensity);
                ubo.lightParams[idx] = Math::Vector4(light->range, 0.0f, 0.0f, 0.0f);
                pointCount++;
            }
        }
    }

    // Defaults if no lights found
    if (dirCount == 0) {
        auto dir = Math::Vector3(0.5f, 0.8f, 0.3f).Normalized();
        ubo.lightDir[0] = Math::Vector4(dir.x, dir.y, dir.z, 0.0f);
        ubo.lightColor[0] = Math::Vector4(1.0f, 0.95f, 0.9f, 1.2f);
        dirCount = 1;
    }

    ubo.ambientColor = Math::Vector4(0.3f, 0.3f, 0.35f, 0.5f);
    ubo.lightCount = Math::Vector4(static_cast<f32>(dirCount), static_cast<f32>(pointCount), 0.0f, 0.0f);

    m_Renderer->UpdateBuffer(m_LightingBuffer, &ubo, sizeof(ubo), 0);
}

void WebRenderPipeline::UpdateObjectDataUBO(ECS::Entity entity, ECS::World* world) {
    auto* xform = world->GetComponent<ECS::TransformComponent>(entity);
    auto* mat = world->GetComponent<ECS::MaterialComponent>(entity);

    WebObjectDataUBO ubo;
    std::memset(&ubo, 0, sizeof(ubo));
    ubo.model = xform ? xform->ToMatrix() : Math::Matrix4::Identity();
    ubo.baseColor = mat ? mat->baseColor : Math::Vector3(0.8f, 0.8f, 0.8f);
    ubo.metallic = mat ? mat->metallic : 0.0f;
    ubo.emissiveColor = mat ? (mat->emissiveColor * mat->emissiveStrength) : Math::Vector3(0.0f);
    ubo.roughness = mat ? mat->roughness : 0.5f;
    ubo.emissiveStrength = mat ? mat->emissiveStrength : 0.0f;
    ubo.opacity = mat ? mat->opacity : 1.0f;
    ubo.alphaCutoff = mat ? mat->alphaCutoff : 0.0f;
    ubo.flags = 0;
    ubo.parallaxScale = 0.0f;

    m_Renderer->UpdateBuffer(m_ObjectBuffer, &ubo, sizeof(ubo), 0);
}

// ============================================================================
// Entity GPU data management
// ============================================================================

void WebRenderPipeline::EnsureEntityGPUData(ECS::Entity entity, const ECS::MeshComponent* mesh) {
    auto& data = m_EntityGPUData[entity];
    if (data.uploaded) return;

    u64 vbSize = mesh->vertices.size() * sizeof(ECS::Vertex);
    u64 ibSize = mesh->indices.size() * sizeof(u32);

    data.vertexBuffer = m_Renderer->CreateBuffer(vbSize, WGPUBufferUsage_Vertex, mesh->vertices.data());
    data.indexBuffer = m_Renderer->CreateBuffer(ibSize, WGPUBufferUsage_Index, mesh->indices.data());
    data.indexCount = static_cast<u32>(mesh->indices.size());
    data.uploaded = true;

    m_Stats.gpuBufferBytes += vbSize + ibSize;
}

void WebRenderPipeline::CleanupDestroyedEntities(ECS::World* world) {
    // Remove GPU data for entities that no longer exist
    for (auto it = m_EntityGPUData.begin(); it != m_EntityGPUData.end(); ) {
        if (!world->IsValid(it->first)) {
            m_Stats.gpuBufferBytes -= it->second.vertexBuffer.size + it->second.indexBuffer.size;
            m_Renderer->DestroyBuffer(it->second.vertexBuffer);
            m_Renderer->DestroyBuffer(it->second.indexBuffer);
            if (it->second.textureBindGroup) wgpuBindGroupRelease(it->second.textureBindGroup);
            it = m_EntityGPUData.erase(it);
        } else {
            ++it;
        }
    }
}

// ============================================================================
// Texture loading
// ============================================================================

WebGPUTextureHandle WebRenderPipeline::LoadTexture(const std::string& path) {
    // Check cache first
    auto it = m_TextureCache.find(path);
    if (it != m_TextureCache.end()) return it->second;

    // Try loading from asset reader
    std::vector<u8> fileData;
    if (m_AssetReader && m_AssetReader->IsOpen()) {
        fileData = m_AssetReader->ReadFile(path);
    }

    if (fileData.empty()) {
        ENJIN_LOG_WARN(Core, "WebRenderPipeline: Texture not found: %s", path.c_str());
        return {};  // Return empty handle — caller should use default
    }

    // Decode with stb_image
    int w, h, channels;
    stbi_uc* pixels = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()),
                                             &w, &h, &channels, 4);
    if (!pixels) {
        ENJIN_LOG_WARN(Core, "WebRenderPipeline: Failed to decode texture: %s", path.c_str());
        return {};
    }

    auto handle = m_Renderer->CreateTexture(static_cast<u32>(w), static_cast<u32>(h),
        WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding, pixels);
    stbi_image_free(pixels);

    m_Stats.gpuTextureBytes += static_cast<u64>(w) * h * 4;
    m_TextureCache[path] = handle;
    ENJIN_LOG_INFO(Core, "WebRenderPipeline: Loaded texture %s (%dx%d)", path.c_str(), w, h);
    return handle;
}

void WebRenderPipeline::LoadEntityTextures(ECS::Entity entity, const ECS::MaterialComponent* mat) {
    if (!mat) return;

    auto& data = m_EntityGPUData[entity];
    if (data.hasTextures) return;  // Already loaded

    // Load textures from material paths
    WebGPUTextureHandle baseColor = m_DefaultWhiteTexture;
    WebGPUTextureHandle normalMap = m_DefaultNormalTexture;
    WebGPUTextureHandle mrMap = m_DefaultBlackTexture;

    if (!mat->baseColorTexturePath.empty()) {
        auto tex = LoadTexture(mat->baseColorTexturePath);
        if (tex.texture) baseColor = tex;
    }
    if (!mat->normalTexturePath.empty()) {
        auto tex = LoadTexture(mat->normalTexturePath);
        if (tex.texture) normalMap = tex;
    }
    if (!mat->metallicRoughnessTexturePath.empty()) {
        auto tex = LoadTexture(mat->metallicRoughnessTexturePath);
        if (tex.texture) mrMap = tex;
    }

    // Only create a custom bind group if at least one texture is non-default
    bool hasCustomTextures = (baseColor.texture != m_DefaultWhiteTexture.texture) ||
                              (normalMap.texture != m_DefaultNormalTexture.texture) ||
                              (mrMap.texture != m_DefaultBlackTexture.texture);

    if (hasCustomTextures) {
        data.textureBindGroup = CreateTextureBindGroup(baseColor, normalMap, mrMap);
    }
    data.hasTextures = true;
}

WGPUBindGroup WebRenderPipeline::CreateTextureBindGroup(const WebGPUTextureHandle& baseColor,
                                                          const WebGPUTextureHandle& normal,
                                                          const WebGPUTextureHandle& metallicRoughness) {
    WGPUBindGroupEntry entries[6] = {};
    // Base color texture + sampler
    entries[0].binding = 0;
    entries[0].textureView = baseColor.view;
    entries[1].binding = 1;
    entries[1].sampler = baseColor.sampler;
    // Normal texture + sampler
    entries[2].binding = 2;
    entries[2].textureView = normal.view;
    entries[3].binding = 3;
    entries[3].sampler = normal.sampler;
    // Metallic-roughness texture + sampler
    entries[4].binding = 4;
    entries[4].textureView = metallicRoughness.view;
    entries[5].binding = 5;
    entries[5].sampler = metallicRoughness.sampler;

    return m_Renderer->CreateBindGroup(m_TextureLayout,
        {entries[0], entries[1], entries[2], entries[3], entries[4], entries[5]});
}

// ============================================================================
// Canvas resize
// ============================================================================

void WebRenderPipeline::OnResize(u32 width, u32 height, f32 dpr) {
    m_CanvasWidth = width;
    m_CanvasHeight = height;
    m_DevicePixelRatio = dpr;
}

} // namespace Enjin::Renderer

#endif // ENJIN_PLATFORM_WEB
