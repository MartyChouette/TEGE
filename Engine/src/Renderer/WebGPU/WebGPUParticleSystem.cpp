#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUParticleSystem.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Renderer/GPUBuffer.h"
#include "Enjin/Renderer/GPUShader.h"
#include "Enjin/Renderer/GPUBindGroup.h"
#include "Enjin/Renderer/GPUPipeline.h"
#include "Enjin/Renderer/GPURenderEncoder.h"   // IComputeEncoder
#include "Enjin/Logging/Log.h"

#include <vector>
#include <cstring>
#include <cmath>

namespace Enjin {
namespace Renderer {

// Embedded WGSL. Keep in sync with Engine/shaders/wgsl/particle_sim.wgsl and
// particle_draw.wgsl (LoadShader takes source; there is no on-disk load on web).
static const char* kSimWGSL = R"WGSL(
struct Particle {
    position : vec3<f32>, lifetime : f32,
    velocity : vec3<f32>, age : f32,
    color : vec4<f32>,
    size : f32, rotation : f32, gravityScale : f32, drag : f32,
};
struct EmitterParams {
    emitterPosition : vec3<f32>, deltaTime : f32,
    emitterDirection : vec3<f32>, emitterSpread : f32,
    gravity : vec3<f32>, damping : f32,
    windForce : vec3<f32>, maxParticles : u32,
    startColor : vec4<f32>, endColor : vec4<f32>,
    startSize : f32, endSize : f32, maxLifetime : f32, spawnRate : f32,
    turbulenceStrength : f32, turbulenceFrequency : f32, frameNumber : u32, pad0 : f32,
};
@group(0) @binding(0) var<storage, read_write> particles : array<Particle>;
@group(0) @binding(1) var<uniform> params : EmitterParams;
fn hashU(n0 : u32) -> f32 {
    var n = n0;
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return f32(n & 0x7FFFFFFFu) / f32(0x7FFFFFFF);
}
fn turb(pos : vec3<f32>, freq : f32, strength : f32, seed : u32) -> vec3<f32> {
    let p = pos * freq;
    let nx = hashU(seed + u32(p.x * 73.0 + p.y * 157.0 + p.z * 113.0));
    let ny = hashU(seed + u32(p.x * 97.0 + p.y * 131.0 + p.z * 89.0));
    let nz = hashU(seed + u32(p.x * 61.0 + p.y * 173.0 + p.z * 149.0));
    return (vec3<f32>(nx, ny, nz) * 2.0 - 1.0) * strength;
}
@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
    let idx = gid.x;
    if (idx >= params.maxParticles) { return; }
    var p = particles[idx];
    if (p.lifetime <= 0.0) { return; }
    p.lifetime = p.lifetime - params.deltaTime;
    p.age = p.age + params.deltaTime;
    if (p.lifetime <= 0.0) { particles[idx] = p; return; }
    var acc = params.gravity * p.gravityScale + params.windForce;
    if (params.turbulenceStrength > 0.0) {
        acc = acc + turb(p.position, params.turbulenceFrequency, params.turbulenceStrength, idx + params.frameNumber);
    }
    p.velocity = p.velocity + acc * params.deltaTime;
    p.velocity = p.velocity * (1.0 - (params.damping + p.drag) * params.deltaTime);
    p.position = p.position + p.velocity * params.deltaTime;
    particles[idx] = p;
}
)WGSL";

static const char* kDrawWGSL = R"WGSL(
struct Particle {
    position : vec3<f32>, lifetime : f32,
    velocity : vec3<f32>, age : f32,
    color : vec4<f32>,
    size : f32, rotation : f32, gravityScale : f32, drag : f32,
};
struct ViewProj { view : mat4x4<f32>, proj : mat4x4<f32>, };
@group(0) @binding(0) var<uniform> ubo : ViewProj;
@group(0) @binding(1) var<storage, read> particles : array<Particle>;
struct VSOut {
    @builtin(position) pos : vec4<f32>,
    @location(0) uv : vec2<f32>,
    @location(1) color : vec4<f32>,
    @location(2) lifeT : f32,
};
@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) iid : u32) -> VSOut {
    var out : VSOut;
    let p = particles[iid];
    let lifetime = p.lifetime;
    if (!(lifetime > 0.0)) {
        out.pos = vec4<f32>(0.0, 0.0, 2.0, 1.0);
        out.uv = vec2<f32>(0.0); out.color = vec4<f32>(0.0); out.lifeT = 1.0;
        return out;
    }
    var corners = array<vec2<f32>, 6>(
        vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, -0.5), vec2<f32>(0.5, 0.5),
        vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, 0.5), vec2<f32>(-0.5, 0.5));
    var uvs = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0), vec2<f32>(0.0, 1.0));
    var corner = corners[vid];
    let c = cos(p.rotation); let s = sin(p.rotation);
    corner = vec2<f32>(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
    let camRight = vec3<f32>(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    let camUp    = vec3<f32>(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);
    let worldPos = p.position + (camRight * corner.x + camUp * corner.y) * p.size;
    out.pos = ubo.proj * ubo.view * vec4<f32>(worldPos, 1.0);
    out.uv = uvs[vid];
    out.color = p.color;
    let total = p.age + max(lifetime, 0.0001);
    out.lifeT = clamp(p.age / total, 0.0, 1.0);
    return out;
}
@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    let dist = length(in.uv - vec2<f32>(0.5));
    let falloff = smoothstep(0.5, 0.15, dist);
    let fadeIn = smoothstep(0.0, 0.08, in.lifeT);
    let fadeOut = 1.0 - smoothstep(0.55, 1.0, in.lifeT);
    let life = fadeIn * fadeOut;
    let alpha = in.color.a * falloff * life;
    if (alpha < 0.01) { discard; }
    return vec4<f32>(in.color.rgb, alpha);
}
)WGSL";

// std140 mirror of EmitterParams (128B) — same as the Vulkan EmitterParamsUBO.
struct EmitterParamsUBO {
    Math::Vector3 emitterPosition;  f32 deltaTime;
    Math::Vector3 emitterDirection; f32 emitterSpread;
    Math::Vector3 gravity;          f32 damping;
    Math::Vector3 windForce;        u32 maxParticles;
    Math::Vector4 startColor;
    Math::Vector4 endColor;
    f32 startSize; f32 endSize; f32 maxLifetime; f32 spawnRate;
    f32 turbulenceStrength; f32 turbulenceFrequency; u32 frameNumber; f32 pad0;
};
static_assert(sizeof(EmitterParamsUBO) == 128, "must match the WGSL EmitterParams");

static f32 HashUnit(u32 n) {
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return static_cast<f32>(n & 0x7FFFFFFFu) / static_cast<f32>(0x7FFFFFFF);
}

WebGPUParticleSystem::WebGPUParticleSystem(WebGPURenderer* renderer)
    : m_Renderer(renderer) {}

WebGPUParticleSystem::~WebGPUParticleSystem() { Shutdown(); }

bool WebGPUParticleSystem::Initialize(const Effects::GPUEmitterConfig& config) {
    if (m_Initialized) return true;
    m_Config = config;

    auto* bufMgr    = m_Renderer->GetBufferManager();
    auto* shaderMgr = m_Renderer->GetShaderManager();
    auto* bgMgr     = m_Renderer->GetBindGroupManager();
    auto* pipeMgr   = m_Renderer->GetPipelineManager();
    if (!bufMgr || !shaderMgr || !bgMgr || !pipeMgr) return false;

    const u64 particleBytes = static_cast<u64>(m_Config.maxParticles) * sizeof(Effects::GPUParticle);

    // Storage buffer, zero-initialized so every particle starts dead (lifetime 0).
    std::vector<u8> zeros(static_cast<usize>(particleBytes), 0u);
    GPUBufferDesc pdesc;
    pdesc.size = particleBytes;
    pdesc.usage = GPUBufferUsage::Storage | GPUBufferUsage::CopyDst;
    pdesc.label = "particles";
    m_ParticleBuffer = bufMgr->CreateBufferWithData(pdesc, zeros.data());

    GPUBufferDesc udesc;
    udesc.size = sizeof(EmitterParamsUBO);
    udesc.usage = GPUBufferUsage::Uniform | GPUBufferUsage::CopyDst;
    udesc.label = "particle-params";
    m_ParamsUBO = bufMgr->CreateBuffer(udesc);

    GPUBufferDesc vpdesc;
    vpdesc.size = 128;  // two mat4
    vpdesc.usage = GPUBufferUsage::Uniform | GPUBufferUsage::CopyDst;
    vpdesc.label = "particle-viewproj";
    m_ViewProjUBO = bufMgr->CreateBuffer(vpdesc);

    if (!m_ParticleBuffer.IsValid() || !m_ParamsUBO.IsValid() || !m_ViewProjUBO.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "WebGPUParticleSystem: buffer creation failed");
        return false;
    }

    // Shaders (WGSL source is stage-agnostic; the draw module has vs_main + fs_main).
    m_SimShader  = shaderMgr->LoadShader(kSimWGSL, std::strlen(kSimWGSL) + 1,
                                         GPUShaderStage::Compute, "particle-sim");
    m_DrawShader = shaderMgr->LoadShader(kDrawWGSL, std::strlen(kDrawWGSL) + 1,
                                         GPUShaderStage::Vertex, "particle-draw");
    if (!m_SimShader.IsValid() || !m_DrawShader.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "WebGPUParticleSystem: WGSL compile failed");
        return false;
    }

    // Compute: storage particles @0 (read_write), uniform params @1.
    {
        GPUBindGroupLayoutDesc ld;
        ld.entries.push_back({0, GPUBindingType::StorageBuffer, GPUShaderStage::Compute, particleBytes});
        ld.entries.push_back({1, GPUBindingType::UniformBuffer, GPUShaderStage::Compute, sizeof(EmitterParamsUBO)});
        ld.label = "particle-compute-layout";
        m_ComputeLayout = bgMgr->CreateBindGroupLayout(ld);

        GPUBindGroupDesc gd; gd.layout = m_ComputeLayout; gd.label = "particle-compute-bg";
        GPUBindGroupEntry e0; e0.binding = 0; e0.buffer = m_ParticleBuffer; e0.bufferSize = particleBytes;
        GPUBindGroupEntry e1; e1.binding = 1; e1.buffer = m_ParamsUBO; e1.bufferSize = sizeof(EmitterParamsUBO);
        gd.entries.push_back(e0); gd.entries.push_back(e1);
        m_ComputeBindGroup = bgMgr->CreateBindGroup(gd);

        GPUComputePipelineDesc cp;
        cp.computeShader = m_SimShader;
        cp.bindGroupLayouts.push_back(m_ComputeLayout);
        cp.entryPoint = "main"; cp.label = "particle-compute";
        m_ComputePipeline = pipeMgr->CreateComputePipeline(cp);
    }

    // Draw: uniform viewproj @0 (vertex), read-only storage particles @1 (vertex).
    {
        GPUBindGroupLayoutDesc ld;
        ld.entries.push_back({0, GPUBindingType::UniformBuffer, GPUShaderStage::Vertex, 128});
        ld.entries.push_back({1, GPUBindingType::StorageBufferReadOnly, GPUShaderStage::Vertex, particleBytes});
        ld.label = "particle-draw-layout";
        m_DrawLayout = bgMgr->CreateBindGroupLayout(ld);

        GPUBindGroupDesc gd; gd.layout = m_DrawLayout; gd.label = "particle-draw-bg";
        GPUBindGroupEntry e0; e0.binding = 0; e0.buffer = m_ViewProjUBO; e0.bufferSize = 128;
        GPUBindGroupEntry e1; e1.binding = 1; e1.buffer = m_ParticleBuffer; e1.bufferSize = particleBytes;
        gd.entries.push_back(e0); gd.entries.push_back(e1);
        m_DrawBindGroup = bgMgr->CreateBindGroup(gd);

        GPURenderPipelineDesc rp;
        rp.vertexShader = m_DrawShader;
        rp.fragmentShader = m_DrawShader;   // one module, vs_main + fs_main
        rp.bindGroupLayouts.push_back(m_DrawLayout);
        rp.topology = GPUPrimitiveTopology::TriangleList;
        rp.cullMode = GPUCullMode::None;
        rp.depthTest = false;               // overlay pass has no depth (first-cut integration)
        rp.depthWrite = false;
        rp.colorAttachmentCount = 1;
        rp.colorFormat = GPUTextureFormat::BGRA8Unorm;   // web swapchain format
        rp.alphaBlend = true;
        rp.blendState.srcColor = GPUBlendFactor::SrcAlpha;
        rp.blendState.dstColor = GPUBlendFactor::OneMinusSrcAlpha;
        rp.blendState.srcAlpha = GPUBlendFactor::One;
        rp.blendState.dstAlpha = GPUBlendFactor::OneMinusSrcAlpha;
        rp.label = "particle-draw";
        m_DrawPipeline = pipeMgr->CreateRenderPipeline(rp);
    }

    if (!m_ComputePipeline.IsValid() || !m_DrawPipeline.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "WebGPUParticleSystem: pipeline creation failed");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "WebGPUParticleSystem initialized: max %u particles", m_Config.maxParticles);
    return true;
}

void WebGPUParticleSystem::Shutdown() {
    if (!m_Initialized) return;
    auto* bufMgr = m_Renderer ? m_Renderer->GetBufferManager() : nullptr;
    auto* bgMgr  = m_Renderer ? m_Renderer->GetBindGroupManager() : nullptr;
    auto* pipeMgr = m_Renderer ? m_Renderer->GetPipelineManager() : nullptr;
    if (pipeMgr) { pipeMgr->DestroyPipeline(m_ComputePipeline); pipeMgr->DestroyPipeline(m_DrawPipeline); }
    if (bgMgr)  { bgMgr->DestroyBindGroup(m_ComputeBindGroup); bgMgr->DestroyBindGroup(m_DrawBindGroup);
                  bgMgr->DestroyBindGroupLayout(m_ComputeLayout); bgMgr->DestroyBindGroupLayout(m_DrawLayout); }
    if (bufMgr) { bufMgr->DestroyBuffer(m_ParticleBuffer); bufMgr->DestroyBuffer(m_ParamsUBO);
                  bufMgr->DestroyBuffer(m_ViewProjUBO); }
    m_Initialized = false;
}

void WebGPUParticleSystem::SpawnWithParams(u32 count, const Math::Vector3& position,
                                           const Math::Vector3& direction,
                                           const Effects::ParticleSpawnParams& params) {
    if (!m_Initialized || count == 0) return;
    m_HasSpawned = true;

    std::vector<Effects::GPUParticle> fresh(count);
    for (u32 i = 0; i < count; ++i) {
        Effects::GPUParticle& p = fresh[i];
        p.position = position;
        p.lifetime = params.lifetime * (0.7f + 0.6f * HashUnit(i * 2654435761u + 11u));
        p.age = 0.0f;
        f32 theta = static_cast<f32>(i) * 2.39996f;   // golden angle
        f32 phi = params.spread * static_cast<f32>(i % 16) / 16.0f;
        p.velocity = Math::Vector3(
            direction.x + sinf(phi) * cosf(theta) * params.spread,
            direction.y + cosf(phi),
            direction.z + sinf(phi) * sinf(theta) * params.spread) * params.speed;
        p.color = params.color;
        p.size = params.size * (1.0f - params.sizeJitter * 0.5f + params.sizeJitter * HashUnit(i * 40503u + 7u));
        p.rotation = HashUnit(i * 22695477u + 3u) * 6.2831853f;
        p.gravityScale = params.gravityScale;
        p.drag = params.drag;
    }

    // Ring-buffer the writes into the particle SSBO (wrap at maxParticles).
    auto* bufMgr = m_Renderer->GetBufferManager();
    u32 start = m_NextSpawnIndex % m_Config.maxParticles;
    u32 first = std::min(count, m_Config.maxParticles - start);
    bufMgr->UploadData(m_ParticleBuffer, fresh.data(),
                       static_cast<u64>(first) * sizeof(Effects::GPUParticle),
                       static_cast<u64>(start) * sizeof(Effects::GPUParticle));
    if (first < count) {   // wrapped remainder writes at offset 0
        bufMgr->UploadData(m_ParticleBuffer, fresh.data() + first,
                           static_cast<u64>(count - first) * sizeof(Effects::GPUParticle), 0);
    }
    m_NextSpawnIndex = (m_NextSpawnIndex + count) % m_Config.maxParticles;
}

void WebGPUParticleSystem::Simulate(f32 deltaTime, u32 frameNumber, const Math::Vector3& windForce) {
    if (!m_Initialized || !m_HasSpawned) return;

    EmitterParamsUBO ubo{};
    ubo.emitterPosition = m_Config.position;
    ubo.deltaTime = deltaTime;
    ubo.emitterDirection = m_Config.direction;
    ubo.emitterSpread = m_Config.spread;
    ubo.gravity = m_Config.gravity;
    ubo.damping = m_Config.damping;
    ubo.windForce = windForce;
    ubo.maxParticles = m_Config.maxParticles;
    ubo.startColor = m_Config.startColor;
    ubo.endColor = m_Config.endColor;
    ubo.startSize = m_Config.startSize;
    ubo.endSize = m_Config.endSize;
    ubo.maxLifetime = m_Config.maxLifetime;
    ubo.spawnRate = m_Config.spawnRate;
    ubo.turbulenceStrength = m_Config.turbulenceStrength;
    ubo.turbulenceFrequency = m_Config.turbulenceFrequency;
    ubo.frameNumber = frameNumber;
    m_Renderer->GetBufferManager()->UploadData(m_ParamsUBO, &ubo, sizeof(ubo));

    IComputeEncoder* enc = m_Renderer->BeginComputePass();
    if (!enc) return;   // a render pass is already open, or no frame
    enc->BindPipeline(m_ComputePipeline);
    enc->SetBindGroup(0, m_ComputeBindGroup, 0, nullptr);
    enc->Dispatch((m_Config.maxParticles + 255u) / 256u, 1, 1);
    m_Renderer->EndComputePass(enc);
}

void WebGPUParticleSystem::Render(WGPURenderPassEncoder pass, const Math::Matrix4& view,
                                  const Math::Matrix4& proj) {
    if (!m_Initialized || !m_HasSpawned || !pass) return;

    struct { Math::Matrix4 view; Math::Matrix4 proj; } vp{view, proj};
    m_Renderer->GetBufferManager()->UploadData(m_ViewProjUBO, &vp, sizeof(vp));

    auto* pipeMgr = static_cast<WebGPUPipelineManager*>(m_Renderer->GetPipelineManager());
    auto* bgMgr   = static_cast<WebGPUBindGroupManager*>(m_Renderer->GetBindGroupManager());
    WGPURenderPipeline nativePipe = pipeMgr->GetNativePipeline(m_DrawPipeline);
    WGPUBindGroup nativeBg = bgMgr->GetNativeGroup(m_DrawBindGroup);
    if (!nativePipe || !nativeBg) return;

    wgpuRenderPassEncoderSetPipeline(pass, nativePipe);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, nativeBg, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 6, m_Config.maxParticles, 0, 0);
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
