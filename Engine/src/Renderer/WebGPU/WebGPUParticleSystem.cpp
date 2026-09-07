#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUParticleSystem.h"
#include "Enjin/Renderer/WebGPU/WebSceneTarget.h"
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

// Embedded WGSL. This IS the shipped source -- the .wgsl tree this used to
// point at is gone; it had drifted and nothing ever read it.
static const char* kSimWGSL = R"WGSL(
struct Particle {
    position : vec3<f32>, lifetime : f32,
    velocity : vec3<f32>, age : f32,
    color : vec4<f32>,
    size : f32, rotation : f32, gravityScale : f32, drag : f32,
    spriteParams : vec4<f32>,   // x=sprite card, y=softness, z=texIndex(unused on web), w=pad
    collision : vec4<f32>,      // x=bounciness, y=slide keep, z=extra radius, w=collide flag
};
struct EmitterParams {
    emitterPosition : vec3<f32>, deltaTime : f32,
    emitterDirection : vec3<f32>, emitterSpread : f32,
    gravity : vec3<f32>, damping : f32,
    windForce : vec3<f32>, maxParticles : u32,
    startColor : vec4<f32>, endColor : vec4<f32>,
    startSize : f32, endSize : f32, maxLifetime : f32, spawnRate : f32,
    turbulenceStrength : f32, turbulenceFrequency : f32, frameNumber : u32, colliderCount : u32,
};
struct ColliderShape {
    posKind : vec4<f32>,   // xyz = center, w = kind (0 box, 1 sphere, 2 capsule)
    rot : vec4<f32>,       // rotation quaternion
    dims : vec4<f32>,      // box: half extents | sphere: x=r | capsule: x=r, y=halfHeight
};
@group(0) @binding(0) var<storage, read_write> particles : array<Particle>;
@group(0) @binding(1) var<uniform> params : EmitterParams;
@group(0) @binding(2) var<storage, read> shapes : array<ColliderShape>;
fn quatRot(q : vec4<f32>, v : vec3<f32>) -> vec3<f32> {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
fn quatRotInv(q : vec4<f32>, v : vec3<f32>) -> vec3<f32> {
    return quatRot(vec4<f32>(-q.xyz, q.w), v);
}
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
    if (p.collision.w > 1.5) {   // stain: ages in place, no physics
        particles[idx] = p;
        return;
    }
    var acc = params.gravity * p.gravityScale + params.windForce;
    if (params.turbulenceStrength > 0.0) {
        acc = acc + turb(p.position, params.turbulenceFrequency, params.turbulenceStrength, idx + params.frameNumber);
    }
    p.velocity = p.velocity + acc * params.deltaTime;
    p.velocity = p.velocity * (1.0 - (params.damping + p.drag) * params.deltaTime);

    // Substep the motion when colliders exist so fast particles can't tunnel
    // through thin shapes in one frame step.
    let substeps = select(1u, 4u, params.colliderCount > 0u && p.collision.w > 0.5);
    let stepDt = params.deltaTime / f32(substeps);
    for (var ss = 0u; ss < substeps; ss = ss + 1u) {
    p.position = p.position + p.velocity * stepDt;

    // Collision: push out of world shapes and bounce (restitution 0.35, 70% slide).
    if (p.collision.w > 0.5) {
    for (var s = 0u; s < params.colliderCount && s < 32u; s = s + 1u) {
        let sc = shapes[s].posKind.xyz;
        let kind = i32(shapes[s].posKind.w + 0.5);
        var n = vec3<f32>(0.0);
        var pen = 0.0;
        let skin = 0.02 + p.collision.z;
        if (kind == 1) {
            let d = p.position - sc;
            let dist = length(d);
            let r = shapes[s].dims.x + skin;
            if (dist < r) {
                if (dist > 1e-6) { n = d / dist; } else { n = vec3<f32>(0.0, 1.0, 0.0); }
                pen = r - dist;
            }
        } else if (kind == 2) {
            let axis = quatRot(shapes[s].rot, vec3<f32>(0.0, 1.0, 0.0));
            let d = p.position - sc;
            let t = clamp(dot(d, axis), -shapes[s].dims.y, shapes[s].dims.y);
            let closest = sc + axis * t;
            let rd = p.position - closest;
            let dist = length(rd);
            let r = shapes[s].dims.x + skin;
            if (dist < r) {
                if (dist > 1e-6) { n = rd / dist; } else { n = vec3<f32>(0.0, 1.0, 0.0); }
                pen = r - dist;
            }
        } else {
            let lp = quatRotInv(shapes[s].rot, p.position - sc);
            let h = shapes[s].dims.xyz + vec3<f32>(skin);
            let a = abs(lp);
            if (a.x < h.x && a.y < h.y && a.z < h.z) {
                let depth = h - a;
                var ln = vec3<f32>(0.0);
                if (depth.x <= depth.y && depth.x <= depth.z) {
                    ln = vec3<f32>(sign(lp.x), 0.0, 0.0); pen = depth.x;
                } else if (depth.y <= depth.x && depth.y <= depth.z) {
                    ln = vec3<f32>(0.0, sign(lp.y), 0.0); pen = depth.y;
                } else {
                    ln = vec3<f32>(0.0, 0.0, sign(lp.z)); pen = depth.z;
                }
                n = quatRot(shapes[s].rot, ln);
            }
        }
        if (pen > 0.0) {
            p.position = p.position + n * pen;
            let vn = dot(p.velocity, n);
            if (vn < 0.0) {
                let vTan = p.velocity - vn * n;
                p.velocity = vTan * p.collision.y - n * (vn * p.collision.x);
            }
        }
    }
    }   // collide flag
    }   // substeps
    particles[idx] = p;
}
)WGSL";

static const char* kDrawWGSL = R"WGSL(
struct Particle {
    position : vec3<f32>, lifetime : f32,
    velocity : vec3<f32>, age : f32,
    color : vec4<f32>,
    size : f32, rotation : f32, gravityScale : f32, drag : f32,
    spriteParams : vec4<f32>,   // x=sprite card, y=softness, z=texIndex(unused on web), w=pad
    collision : vec4<f32>,      // x=bounciness, y=slide keep, z=extra radius, w=collide flag
};
struct ViewProj { view : mat4x4<f32>, proj : mat4x4<f32>, };
@group(0) @binding(0) var<uniform> ubo : ViewProj;
@group(0) @binding(1) var<storage, read> particles : array<Particle>;
struct VSOut {
    @builtin(position) pos : vec4<f32>,
    @location(0) uv : vec2<f32>,
    @location(1) color : vec4<f32>,
    @location(2) lifeT : f32,
    @location(3) sprite : vec2<f32>,   // x=card, y=softness
};
@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) iid : u32) -> VSOut {
    var out : VSOut;
    let p = particles[iid];
    let lifetime = p.lifetime;
    if (!(lifetime > 0.0)) {
        out.pos = vec4<f32>(0.0, 0.0, 2.0, 1.0);
        out.uv = vec2<f32>(0.0); out.color = vec4<f32>(0.0); out.lifeT = 1.0;
        out.sprite = vec2<f32>(0.0, 1.0);
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
    var worldPos : vec3<f32>;
    if (p.collision.w > 1.5) {   // stain: lie on the surface (velocity = normal)
        let n = normalize(p.velocity);
        let helper = select(vec3<f32>(1.0, 0.0, 0.0), vec3<f32>(0.0, 1.0, 0.0), abs(n.y) < 0.99);
        let t = normalize(cross(n, helper));
        let b = cross(n, t);
        worldPos = p.position + (t * corner.x + b * corner.y) * p.size;
    } else {
        let camRight = vec3<f32>(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
        let camUp    = vec3<f32>(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);
        worldPos = p.position + (camRight * corner.x + camUp * corner.y) * p.size;
    }
    out.pos = ubo.proj * ubo.view * vec4<f32>(worldPos, 1.0);
    out.uv = uvs[vid];
    out.color = p.color;
    let total = p.age + max(lifetime, 0.0001);
    out.lifeT = clamp(p.age / total, 0.0, 1.0);
    out.sprite = vec2<f32>(p.spriteParams.x, p.spriteParams.y);
    return out;
}
fn spriteMask(card : i32, uv : vec2<f32>, softness : f32) -> f32 {
    let c = uv - vec2<f32>(0.5);
    var w = mix(0.02, 0.35, clamp(softness, 0.0, 1.0));
    var d : f32;
    if (card == 2) {                       // square
        d = max(abs(c.x), abs(c.y));
    } else if (card == 3) {                // streak
        d = length(vec2<f32>(c.x * 3.0, c.y));
    } else if (card == 4) {                // 4-point star
        let ang = atan2(c.y, c.x);
        d = length(c) * (1.0 + 1.2 * pow(abs(sin(2.0 * ang)), 2.0));
    } else {                               // circles (0 soft / 1 hard; 5 texture falls back to soft)
        d = length(c);
        if (card == 1) { w = mix(0.02, 0.10, clamp(softness, 0.0, 1.0)); }
    }
    return 1.0 - smoothstep(0.5 - w, 0.5, d);
}

@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    let fadeIn = smoothstep(0.0, 0.08, in.lifeT);
    let fadeOut = 1.0 - smoothstep(0.55, 1.0, in.lifeT);
    let life = fadeIn * fadeOut;
    let mask = spriteMask(i32(in.sprite.x + 0.5), in.uv, in.sprite.y);
    let alpha = in.color.a * mask * life;
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
    f32 turbulenceStrength; f32 turbulenceFrequency; u32 frameNumber; u32 colliderCount;
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

    GPUBufferDesc cdesc;
    cdesc.size = Effects::kMaxParticleColliders * sizeof(Effects::ParticleColliderShape);
    cdesc.usage = GPUBufferUsage::Storage | GPUBufferUsage::CopyDst;
    cdesc.label = "particle-colliders";
    m_ColliderBuffer = bufMgr->CreateBuffer(cdesc);

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
        ld.entries.push_back({2, GPUBindingType::StorageBufferReadOnly, GPUShaderStage::Compute,
                              Effects::kMaxParticleColliders * sizeof(Effects::ParticleColliderShape)});
        ld.label = "particle-compute-layout";
        m_ComputeLayout = bgMgr->CreateBindGroupLayout(ld);

        GPUBindGroupDesc gd; gd.layout = m_ComputeLayout; gd.label = "particle-compute-bg";
        GPUBindGroupEntry e0; e0.binding = 0; e0.buffer = m_ParticleBuffer; e0.bufferSize = particleBytes;
        GPUBindGroupEntry e1; e1.binding = 1; e1.buffer = m_ParamsUBO; e1.bufferSize = sizeof(EmitterParamsUBO);
        GPUBindGroupEntry e2; e2.binding = 2; e2.buffer = m_ColliderBuffer;
        e2.bufferSize = Effects::kMaxParticleColliders * sizeof(Effects::ParticleColliderShape);
        gd.entries.push_back(e0); gd.entries.push_back(e1); gd.entries.push_back(e2);
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
        // The overlay pass DECLARES a depth attachment (so the pipeline must too, for
        // pass compatibility) — but the web scene renders into its own offscreen chain,
        // so that swapchain depth buffer is never written by the scene and holds stale
        // data. Testing against it silently killed every particle. Compare=Always:
        // particles composite over the finished frame (like the UI). Real scene-depth
        // occlusion on web means drawing inside the scene's offscreen pass — later.
        rp.depthTest = true;                 // declare depth state (pass has the attachment)
        rp.depthWrite = false;
        rp.depthCompare = GPUCompareFunction::Always;
        rp.depthFormat = GPUTextureFormat::Depth24PlusStencil8;
        rp.colorAttachmentCount = 1;
        rp.colorFormat = GPUTextureFormat::BGRA8Unorm;   // web swapchain format
        rp.alphaBlend = true;
        rp.blendState.srcColor = GPUBlendFactor::SrcAlpha;
        rp.blendState.dstColor = GPUBlendFactor::OneMinusSrcAlpha;
        rp.blendState.srcAlpha = GPUBlendFactor::One;
        rp.blendState.dstAlpha = GPUBlendFactor::OneMinusSrcAlpha;
        rp.label = "particle-draw";
        m_DrawPipeline = pipeMgr->CreateRenderPipeline(rp);

        // Scene-pass variant: the web scene renders into an RGBA16Float HDR
        // target with its OWN depth. Drawing there gives particles real depth
        // occlusion and the same tonemapping as the rest of the scene.
        GPURenderPipelineDesc sp = rp;
        sp.colorFormat = GPUTextureFormat::RGBA16Float;
        sp.sampleCount = kWebSceneSampleCount;   // MUST match the scene target (see WebSceneTarget.h)
        sp.depthCompare = GPUCompareFunction::LessEqual;   // real scene depth here
        sp.label = "particle-draw-scene";
        m_ScenePipeline = pipeMgr->CreateRenderPipeline(sp);
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
    if (pipeMgr) { pipeMgr->DestroyPipeline(m_ComputePipeline); pipeMgr->DestroyPipeline(m_DrawPipeline);
                   pipeMgr->DestroyPipeline(m_ScenePipeline); }
    if (bgMgr)  { bgMgr->DestroyBindGroup(m_ComputeBindGroup); bgMgr->DestroyBindGroup(m_DrawBindGroup);
                  bgMgr->DestroyBindGroupLayout(m_ComputeLayout); bgMgr->DestroyBindGroupLayout(m_DrawLayout); }
    if (bufMgr) { bufMgr->DestroyBuffer(m_ParticleBuffer); bufMgr->DestroyBuffer(m_ParamsUBO);
                  bufMgr->DestroyBuffer(m_ViewProjUBO); }
    m_Initialized = false;
}

void WebGPUParticleSystem::SpawnWithParams(u32 count, const Math::Vector3& position,
                                           const Math::Vector3& direction,
                                           const Effects::ParticleSpawnParams& params,
                                           u8 shape, f32 shapeSize,
                                            const Math::Quaternion& orientation) {
    if (!m_Initialized || count == 0) return;
    m_HasSpawned = true;

    std::vector<Effects::GPUParticle> fresh(count);
    for (u32 i = 0; i < count; ++i) {
        Effects::GPUParticle& p = fresh[i];
        p.position = position + orientation.Rotate(Effects::ShapeSpawnOffset(shape, shapeSize, i));
        p.lifetime = params.lifetime * (0.7f + 0.6f * HashUnit(i * 2654435761u + 11u));
        p.age = 0.0f;
        // A real cone around `direction`, shared with the desktop spawn. The
        // hand-written version this replaces added the axial term to world Y,
        // so a rotated emitter still threw particles upward.
        p.velocity = Effects::ConeVelocity(direction, params.spread, i) * params.speed;
        p.color = params.color;
        p.size = params.size * (1.0f - params.sizeJitter * 0.5f + params.sizeJitter * HashUnit(i * 40503u + 7u));
        p.rotation = (params.fixedRotation >= 0.0f) ? params.fixedRotation
                     : HashUnit(i * 22695477u + 3u) * 6.2831853f;
        p.gravityScale = params.gravityScale;
        p.drag = params.drag;
        p.sprite = static_cast<f32>(params.sprite);
        p.softness = params.softness;
        p.texIndex = -1.0f;   // no bindless on web yet; card 5 falls back in-shader
        p.emitterKey = params.emitterKey;
        p.collision = Math::Vector4(params.bounciness,
                                    1.0f - params.friction,
                                    params.collisionRadius,
                                    params.collide ? 1.0f : 0.0f);
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

void WebGPUParticleSystem::SetColliders(const std::vector<Effects::ParticleColliderShape>& shapes) {
    m_ColliderCount = 0;
    if (!m_Initialized || !m_ColliderBuffer.IsValid()) return;
    m_ColliderCount = std::min<u32>(static_cast<u32>(shapes.size()), Effects::kMaxParticleColliders);
    if (m_ColliderCount > 0) {
        m_Renderer->GetBufferManager()->UploadData(m_ColliderBuffer, shapes.data(),
            m_ColliderCount * sizeof(Effects::ParticleColliderShape));
    }
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
    ubo.colliderCount = m_ColliderCount;
    m_Renderer->GetBufferManager()->UploadData(m_ParamsUBO, &ubo, sizeof(ubo));

    IComputeEncoder* enc = m_Renderer->BeginComputePass();
    if (!enc) return;   // a render pass is already open, or no frame
    enc->BindPipeline(m_ComputePipeline);
    enc->SetBindGroup(0, m_ComputeBindGroup, 0, nullptr);
    enc->Dispatch((m_Config.maxParticles + 255u) / 256u, 1, 1);
    m_Renderer->EndComputePass(enc);
}

static void DrawParticles(WebGPURenderer* renderer, GPUPipelineHandle pipeline,
                          GPUBindGroupHandle bindGroup, GPUBufferHandle viewProjUBO,
                          WGPURenderPassEncoder pass, const Math::Matrix4& view,
                          const Math::Matrix4& proj, u32 instances) {
    struct { Math::Matrix4 view; Math::Matrix4 proj; } vp{view, proj};
    // Same convention flip the web scene UBO applies: Vulkan-style proj (Y-down)
    // -> WebGPU (Y-up). Without this the particle layer is vertically mirrored
    // about the view center and appears to swim as the camera pitches.
    vp.proj.m[5] = -vp.proj.m[5];
    renderer->GetBufferManager()->UploadData(viewProjUBO, &vp, sizeof(vp));

    auto* pipeMgr = static_cast<WebGPUPipelineManager*>(renderer->GetPipelineManager());
    auto* bgMgr   = static_cast<WebGPUBindGroupManager*>(renderer->GetBindGroupManager());
    WGPURenderPipeline nativePipe = pipeMgr->GetNativePipeline(pipeline);
    WGPUBindGroup nativeBg = bgMgr->GetNativeGroup(bindGroup);
    if (!nativePipe || !nativeBg) return;

    wgpuRenderPassEncoderSetPipeline(pass, nativePipe);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, nativeBg, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 6, instances, 0, 0);
}

void WebGPUParticleSystem::RenderScene(WGPURenderPassEncoder pass, const Math::Matrix4& view,
                                       const Math::Matrix4& proj) {
    if (!m_Initialized || !m_HasSpawned || !pass || !m_ScenePipeline.IsValid()) return;
    DrawParticles(m_Renderer, m_ScenePipeline, m_DrawBindGroup, m_ViewProjUBO,
                  pass, view, proj, m_Config.maxParticles);
    m_SceneDrewThisFrame = true;   // the overlay fallback skips this frame
}

void WebGPUParticleSystem::Render(WGPURenderPassEncoder pass, const Math::Matrix4& view,
                                  const Math::Matrix4& proj) {
    if (!m_Initialized || !m_HasSpawned || !pass) return;
    if (m_SceneDrewThisFrame) { m_SceneDrewThisFrame = false; return; }
    DrawParticles(m_Renderer, m_DrawPipeline, m_DrawBindGroup, m_ViewProjUBO,
                  pass, view, proj, m_Config.maxParticles);
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
