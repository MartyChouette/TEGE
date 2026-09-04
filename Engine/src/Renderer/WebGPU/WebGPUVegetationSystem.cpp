#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

#include "Enjin/Renderer/WebGPU/WebGPUVegetationSystem.h"
#include "Enjin/Renderer/WebGPU/WebSceneTarget.h"
#include "Enjin/Renderer/WebGPU/WebGPURenderer.h"
#include "Enjin/Renderer/WebGPU/WebGPUPipelineManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBindGroupManager.h"
#include "Enjin/Renderer/WebGPU/WebGPUBufferManager.h"
#include "Enjin/Renderer/GPUBuffer.h"
#include "Enjin/Renderer/GPUShader.h"
#include "Enjin/Renderer/WebGPU/WebGPUShaderManager.h"
#include "Enjin/Renderer/GPUBindGroup.h"
#include "Enjin/Renderer/GPUPipeline.h"
#include "Enjin/Effects/VegetationTemplates.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/GrassVolume.h"
#include "Enjin/ECS/Components/ShrubVolume.h"
#include "Enjin/ECS/Components/TreeVolume.h"
#include "Enjin/Logging/Log.h"

#include <vector>
#include <cstring>

namespace Enjin {
namespace Renderer {

// Port of grass.vert's procedural core (hash scatter, per-instance variance,
// two-band wind sway) shared by all three volume kinds; the fragment applies a
// base->tip gradient with a fixed-sun half-lambert. Player-step bend and world
// curvature are desktop-only for now. Volume selection: firstInstance encodes
// the volume slot in the high bits (vol = ii >> 16, blade = ii & 0xFFFF).
static const char* kVegWGSL = R"WGSL(
struct ViewProj {
    view : mat4x4<f32>,
    proj : mat4x4<f32>,
    // The scene's real sun and ambient. This pass has no lighting buffer bound,
    // so without these the shader lit every plant from a hardcoded direction
    // with a hardcoded ambient -- the grove kept the same flat noon light while
    // the sun moved across the sky and everything around it changed.
    sunDir : vec4<f32>,        // xyz = direction TO the light, w unused
    sunColor : vec4<f32>,      // rgb, w = intensity
    ambient : vec4<f32>,       // rgb, w = intensity
};
struct VolumeParams {
    posHalfX : vec4<f32>,        // xyz volume center, w halfExtentX
    baseColorHalfZ : vec4<f32>,  // xyz base color,    w halfExtentZ
    tipColorHeight : vec4<f32>,  // xyz tip color,     w plant height
    misc : vec4<f32>,            // x heightVariance, y width, z windSway, w kind (2 = tree)
    trunkColorIdxOff : vec4<f32>,// xyz trunk color,   w template index offset
    wind : vec4<f32>,            // xyz wind dir*strength, w time
    // Trees only. A tree is not a tall blade of grass: it has four authored
    // dimensions that scale two different parts of the template, and collapsing
    // them into one height and one width is what made web trees giant
    // translucent slabs while the editor drew them correctly.
    treeDims : vec4<f32>,        // trunkWidth, trunkHeight, canopyRadius, canopyOffset
    treeScale : vec4<f32>,       // minHeightScale, maxHeightScale, unused, unused
};
@group(0) @binding(0) var<uniform> ubo : ViewProj;
@group(0) @binding(1) var<storage, read> verts : array<f32>;    // 5 floats per vertex
@group(0) @binding(2) var<storage, read> indices : array<u32>;
@group(0) @binding(3) var<storage, read> volumes : array<VolumeParams>;

fn hashU(n0 : u32) -> f32 {
    var n = n0;
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return f32(n & 0x7fffffffu) / f32(0x7fffffff);
}

struct VSOut {
    @builtin(position) pos : vec4<f32>,
    @location(0) color : vec3<f32>,
    @location(1) heightFrac : f32,
    @location(2) normal : vec3<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) ii : u32) -> VSOut {
    let vol = volumes[ii >> 16u];
    let blade = ii & 0xFFFFu;

    let idx = indices[u32(vol.trunkColorIdxOff.w) + vid];
    let base = idx * 5u;
    var lp = vec3<f32>(verts[base], verts[base + 1u], verts[base + 2u]);
    let uv = vec2<f32>(verts[base + 3u], verts[base + 4u]);

    // Procedural placement inside the volume
    let px = hashU(blade * 3u + 0u) * 2.0 - 1.0;
    let pz = hashU(blade * 3u + 1u) * 2.0 - 1.0;
    let heightVar = hashU(blade * 3u + 2u) * 2.0 - 1.0;
    let rotAngle = hashU(blade * 7u + 5u) * 6.28318;

    let origin = vol.posHalfX.xyz + vec3<f32>(px * vol.posHalfX.w, 0.0, pz * vol.baseColorHalfZ.w);
    let height = vol.tipColorHeight.w + heightVar * vol.misc.x;
    let width = vol.misc.y;

    let isTree = vol.misc.w > 1.5;
    var plantHeight = height;

    if (isTree) {
        // Trunk and canopy scale by DIFFERENT authored dimensions -- the same
        // rule tree.vert uses, so web and desktop draw the same tree.
        //   trunk  (uv.y < 0.5): xz by trunkWidth*2, y by trunkHeight
        //   canopy (uv.y >= 0.5): xz by canopyRadius*2, y about canopyOffset
        // The template's canopy spans y 0.5..1.5, which is why the canopy y term
        // subtracts 0.5 before scaling.
        let sizeVar = mix(vol.treeScale.x, vol.treeScale.y, hashU(blade * 3u + 2u));
        let tW = vol.treeDims.x * sizeVar;
        let tH = vol.treeDims.y * sizeVar;
        let cR = vol.treeDims.z * sizeVar;
        let cO = vol.treeDims.w * sizeVar;
        if (uv.y >= 0.5) {
            lp.x = lp.x * cR * 2.0;
            lp.z = lp.z * cR * 2.0;
            lp.y = (lp.y - 0.5) * cR * 2.0 + cO;
        } else {
            lp.x = lp.x * tW * 2.0;
            lp.z = lp.z * tW * 2.0;
            lp.y = lp.y * tH;
        }
        plantHeight = cO + cR * 2.0;
    } else {
        lp.x = lp.x * width;
        lp.y = lp.y * height;
        lp.z = lp.z * height;
        // Rest tilt so a patch reads as plants, not stamped copies
        lp.z = lp.z + (hashU(blade * 5u) * 0.06 - 0.03) * uv.y * height;
    }

    let c = cos(rotAngle); let s = sin(rotAngle);
    let rp = vec3<f32>(lp.x * c - lp.z * s, lp.y, lp.x * s + lp.z * c);

    // Two-band wind sway, tip-weighted, clamped so gusts can't fold the plant
    let hf = uv.y;
    let sway = vol.misc.z;
    let t = vol.wind.w;
    let w1 = sin(dot(origin.xz, vec2<f32>(0.15, 0.25)) + t * 2.0) * hf * hf * sway;
    let w2 = sin(dot(origin.xz, vec2<f32>(0.4, 0.6)) + t * 4.5) * hf * hf * sway * 0.3;
    let wtot = clamp(w1 + w2, -0.35 * plantHeight, 0.35 * plantHeight);
    let worldPos = origin + rp + vol.wind.xyz * wtot;

    var out : VSOut;
    out.pos = ubo.proj * ubo.view * vec4<f32>(worldPos, 1.0);
    out.heightFrac = hf;
    out.normal = normalize(vec3<f32>(s, 0.5, c));

    // Trees: bark below the canopy line, gradient foliage above. The template
    // splits at uv.y 0.5 (trunk verts carry 0.0 and 0.4), so testing 0.3 left
    // the TOP of every trunk shaded as foliage -- the brown-to-green trunks.
    var col = mix(vol.baseColorHalfZ.xyz, vol.tipColorHeight.xyz, hf);
    if (isTree) {
        if (uv.y < 0.5) {
            col = vol.trunkColorIdxOff.xyz;
        } else {
            // Crown blends base (underneath) to tip (on top) across 0.5..1.0.
            col = mix(vol.baseColorHalfZ.xyz, vol.tipColorHeight.xyz,
                      clamp((uv.y - 0.5) * 2.0, 0.0, 1.0));
        }
    }
    out.color = col;
    return out;
}

@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    // Half-lambert against the SCENE's sun, so foliage darkens at dusk and
    // picks up the light's colour like every other surface.
    let sun = normalize(ubo.sunDir.xyz);
    let ndl = clamp(dot(normalize(in.normal), sun), 0.0, 1.0) * 0.5 + 0.5;
    let ambient = ubo.ambient.rgb * ubo.ambient.w;
    let direct = ubo.sunColor.rgb * ubo.sunColor.w * ndl;
    return vec4<f32>(in.color * (ambient + direct), 1.0);
}
)WGSL";

// Mirrors the volume-params struct above (128 bytes). Keep the two in lockstep:
// nothing else checks them against each other, so this static_assert and the
// field order are the whole contract.
struct VolumeParamsCPU {
    f32 posHalfX[4];
    f32 baseColorHalfZ[4];
    f32 tipColorHeight[4];
    f32 misc[4];
    f32 trunkColorIdxOff[4];
    f32 wind[4];
    f32 treeDims[4];    // trunkWidth, trunkHeight, canopyRadius, canopyOffset
    f32 treeScale[4];   // minHeightScale, maxHeightScale, unused, unused
};
static_assert(sizeof(VolumeParamsCPU) == 128, "must match the WGSL VolumeParams");

bool WebGPUVegetationSystem::Initialize(WebGPURenderer* renderer) {
    if (m_Initialized) return true;
    m_Renderer = renderer;
    if (!m_Renderer) return false;

    auto* bufMgr = m_Renderer->GetBufferManager();
    auto* bgMgr = m_Renderer->GetBindGroupManager();
    auto* pipeMgr = m_Renderer->GetPipelineManager();
    if (!bufMgr || !bgMgr || !pipeMgr) return false;

    // Concatenate the three shared template meshes into one vert + index pool.
    std::vector<Effects::VegTemplates::VegVertex> verts;
    std::vector<u32> indices;
    auto append = [&](TemplateRange& range, void (*build)(std::vector<Effects::VegTemplates::VegVertex>&, std::vector<u32>&)) {
        std::vector<Effects::VegTemplates::VegVertex> v;
        std::vector<u32> i;
        build(v, i);
        const u32 vertBase = static_cast<u32>(verts.size());
        range.indexOffset = static_cast<u32>(indices.size());
        range.indexCount = static_cast<u32>(i.size());
        verts.insert(verts.end(), v.begin(), v.end());
        for (u32 ix : i) indices.push_back(vertBase + ix);
    };
    append(m_Grass, &Effects::VegTemplates::BuildGrassBlade);
    append(m_Shrub, &Effects::VegTemplates::BuildShrub);
    append(m_Tree,  &Effects::VegTemplates::BuildTree);

    GPUBufferDesc vd;
    vd.size = verts.size() * sizeof(Effects::VegTemplates::VegVertex);
    vd.usage = GPUBufferUsage::Storage | GPUBufferUsage::CopyDst;
    vd.label = "veg-template-verts";
    m_TemplateVerts = bufMgr->CreateBuffer(vd);
    bufMgr->UploadData(m_TemplateVerts, verts.data(), vd.size);

    GPUBufferDesc id;
    id.size = indices.size() * sizeof(u32);
    id.usage = GPUBufferUsage::Storage | GPUBufferUsage::CopyDst;
    id.label = "veg-template-indices";
    m_TemplateIndices = bufMgr->CreateBuffer(id);
    bufMgr->UploadData(m_TemplateIndices, indices.data(), id.size);

    GPUBufferDesc pd;
    pd.size = kMaxVolumes * sizeof(VolumeParamsCPU);
    pd.usage = GPUBufferUsage::Storage | GPUBufferUsage::CopyDst;
    pd.label = "veg-volume-params";
    m_VolumeParams = bufMgr->CreateBuffer(pd);

    GPUBufferDesc ud;
    ud.size = 176;   // 2 matrices (128) + sunDir/sunColor/ambient (48)
    ud.usage = GPUBufferUsage::Uniform | GPUBufferUsage::CopyDst;
    ud.label = "veg-viewproj";
    m_ViewProjUBO = bufMgr->CreateBuffer(ud);

    auto* shaderMgr = m_Renderer->GetShaderManager();
    if (!shaderMgr) return false;
    m_Shader = shaderMgr->LoadShader(kVegWGSL, std::strlen(kVegWGSL) + 1,
                                     GPUShaderStage::Vertex, "veg-wgsl");
    if (!m_Shader.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "WebGPUVegetation: shader compile failed");
        return false;
    }

    GPUBindGroupLayoutDesc ld;
    ld.entries.push_back({0, GPUBindingType::UniformBuffer, GPUShaderStage::Vertex, 128});
    ld.entries.push_back({1, GPUBindingType::StorageBufferReadOnly, GPUShaderStage::Vertex, vd.size});
    ld.entries.push_back({2, GPUBindingType::StorageBufferReadOnly, GPUShaderStage::Vertex, id.size});
    ld.entries.push_back({3, GPUBindingType::StorageBufferReadOnly, GPUShaderStage::Vertex, pd.size});
    ld.label = "veg-layout";
    m_Layout = bgMgr->CreateBindGroupLayout(ld);

    GPUBindGroupDesc gd;
    gd.layout = m_Layout;
    GPUBindGroupEntry e0; e0.binding = 0; e0.buffer = m_ViewProjUBO; e0.bufferSize = 128;
    GPUBindGroupEntry e1; e1.binding = 1; e1.buffer = m_TemplateVerts; e1.bufferSize = vd.size;
    GPUBindGroupEntry e2; e2.binding = 2; e2.buffer = m_TemplateIndices; e2.bufferSize = id.size;
    GPUBindGroupEntry e3; e3.binding = 3; e3.buffer = m_VolumeParams; e3.bufferSize = pd.size;
    gd.entries.push_back(e0); gd.entries.push_back(e1);
    gd.entries.push_back(e2); gd.entries.push_back(e3);
    gd.label = "veg-bindgroup";
    m_BindGroup = bgMgr->CreateBindGroup(gd);

    // Scene-pass pipeline only (RGBA16Float MSAA 4x offscreen with real depth,
    // same target the particles' scene path draws into). Vegetation is opaque:
    // depth-write ON so blades occlude each other and the scene correctly.
    GPURenderPipelineDesc rp;
    rp.vertexShader = m_Shader;
    rp.fragmentShader = m_Shader;
    rp.bindGroupLayouts.push_back(m_Layout);
    rp.topology = GPUPrimitiveTopology::TriangleList;
    rp.cullMode = GPUCullMode::None;
    rp.depthTest = true;
    rp.depthWrite = true;
    rp.depthCompare = GPUCompareFunction::LessEqual;
    rp.depthFormat = GPUTextureFormat::Depth24PlusStencil8;
    rp.colorAttachmentCount = 1;
    rp.colorFormat = GPUTextureFormat::RGBA16Float;
    rp.sampleCount = kWebSceneSampleCount;   // MUST match the scene target (see WebSceneTarget.h)
    rp.alphaBlend = false;
    rp.label = "veg-scene";
    m_Pipeline = pipeMgr->CreateRenderPipeline(rp);
    if (!m_Pipeline.IsValid()) {
        ENJIN_LOG_ERROR(Renderer, "WebGPUVegetation: pipeline creation failed");
        return false;
    }

    m_Initialized = true;
    ENJIN_LOG_INFO(Renderer, "WebGPUVegetation initialized (%zu template verts, %zu indices)",
                   verts.size(), indices.size());
    return true;
}

void WebGPUVegetationSystem::Shutdown() {
    m_Initialized = false;
}

void WebGPUVegetationSystem::RenderScene(WGPURenderPassEncoder pass, const Math::Matrix4& view,
                                         const Math::Matrix4& proj, ECS::World* world) {
    if (!m_Initialized || !pass || !world || !m_Pipeline.IsValid()) return;
    using namespace Enjin::ECS;

    // Gather volumes -> params array. Each entry also records its template
    // range + density for the draw loop below.
    struct DrawInfo { u32 indexCount; u32 density; };
    VolumeParamsCPU params[kMaxVolumes]{};
    DrawInfo draws[kMaxVolumes]{};
    u32 count = 0;

    auto put3 = [](f32* dst, const Math::Vector3& v) { dst[0] = v.x; dst[1] = v.y; dst[2] = v.z; };

    for (Entity e : world->GetEntitiesWithComponent<GrassVolumeComponent>()) {
        if (count >= kMaxVolumes) break;
        auto* g = world->GetComponent<GrassVolumeComponent>(e);
        auto* t = world->GetComponent<TransformComponent>(e);
        if (!g || !t || !t->visible) continue;
        VolumeParamsCPU& p = params[count];
        put3(p.posHalfX, t->position); p.posHalfX[3] = g->halfExtents.x;
        put3(p.baseColorHalfZ, g->baseColor); p.baseColorHalfZ[3] = g->halfExtents.z;
        put3(p.tipColorHeight, g->tipColor); p.tipColorHeight[3] = g->bladeHeight;
        p.misc[0] = g->bladeHeightVariance; p.misc[1] = g->bladeWidth;
        p.misc[2] = g->windSwayStrength; p.misc[3] = 0.0f;
        p.trunkColorIdxOff[3] = static_cast<f32>(m_Grass.indexOffset);
        put3(p.wind, m_Wind); p.wind[3] = m_WindTime;
        draws[count] = { m_Grass.indexCount, std::min(g->density, 60000u) };
        ++count;
    }
    for (Entity e : world->GetEntitiesWithComponent<ShrubVolumeComponent>()) {
        if (count >= kMaxVolumes) break;
        auto* g = world->GetComponent<ShrubVolumeComponent>(e);
        auto* t = world->GetComponent<TransformComponent>(e);
        if (!g || !t || !t->visible) continue;
        VolumeParamsCPU& p = params[count];
        put3(p.posHalfX, t->position); p.posHalfX[3] = g->halfExtents.x;
        put3(p.baseColorHalfZ, g->baseColor); p.baseColorHalfZ[3] = g->halfExtents.z;
        put3(p.tipColorHeight, g->tipColor); p.tipColorHeight[3] = g->shrubHeight;
        p.misc[0] = g->heightVariance; p.misc[1] = g->width;
        p.misc[2] = g->windSwayStrength; p.misc[3] = 1.0f;
        p.trunkColorIdxOff[3] = static_cast<f32>(m_Shrub.indexOffset);
        put3(p.wind, m_Wind); p.wind[3] = m_WindTime;
        draws[count] = { m_Shrub.indexCount, std::min(g->density, 60000u) };
        ++count;
    }
    for (Entity e : world->GetEntitiesWithComponent<TreeVolumeComponent>()) {
        if (count >= kMaxVolumes) break;
        auto* g = world->GetComponent<TreeVolumeComponent>(e);
        auto* t = world->GetComponent<TransformComponent>(e);
        if (!g || !t || !t->visible) continue;
        VolumeParamsCPU& p = params[count];
        put3(p.posHalfX, t->position); p.posHalfX[3] = g->halfExtents.x;
        put3(p.baseColorHalfZ, g->canopyBaseColor); p.baseColorHalfZ[3] = g->halfExtents.z;
        put3(p.tipColorHeight, g->canopyTipColor);
        // Overall extent, used only for the wind clamp; the shader scales trunk
        // and canopy from treeDims below. Summing the four authored dimensions
        // into one height and one width -- which is what this used to do -- gave
        // a canopy spanning 0.5x to 1.5x the WHOLE tree height, so web trees were
        // giant slabs while the editor drew them correctly from the same data.
        p.tipColorHeight[3] = g->trunkHeight + g->canopyOffset;
        p.misc[0] = 0.0f;
        p.misc[1] = g->trunkWidth;
        p.misc[2] = g->windSwayStrength * 0.25f;   // trees barely sway
        p.misc[3] = 2.0f;
        p.treeDims[0] = g->trunkWidth;
        p.treeDims[1] = g->trunkHeight;
        p.treeDims[2] = g->canopyRadius;
        p.treeDims[3] = g->canopyOffset;
        p.treeScale[0] = g->minHeightScale;
        p.treeScale[1] = g->maxHeightScale;
        p.treeScale[2] = 0.0f;
        p.treeScale[3] = 0.0f;
        put3(p.trunkColorIdxOff, g->trunkColor);
        p.trunkColorIdxOff[3] = static_cast<f32>(m_Tree.indexOffset);
        put3(p.wind, m_Wind); p.wind[3] = m_WindTime;
        draws[count] = { m_Tree.indexCount, std::min(g->density, 20000u) };
        ++count;
    }
    if (count == 0) return;

    auto* bufMgr = m_Renderer->GetBufferManager();
    bufMgr->UploadData(m_VolumeParams, params, count * sizeof(VolumeParamsCPU));

    // Same Y-flip the particle scene draw applies: the scene UBO negates
    // proj.m[5] (Vulkan Y-down -> WebGPU Y-up); any custom scene-pass draw must
    // match or its layer is vertically mirrored and swims with camera pitch.
    // Matches the WGSL ViewProj: two matrices then the scene's light. The
    // buffer is 128 bytes for the matrices plus 48 for the three vec4s.
    struct FrameUBO {
        Math::Matrix4 view, proj;
        f32 sunDir[4];
        f32 sunColor[4];
        f32 ambient[4];
    } vp{};
    vp.view = view;
    vp.proj = proj;
    vp.proj.m[5] = -vp.proj.m[5];
    vp.sunDir[0] = m_SunDir.x; vp.sunDir[1] = m_SunDir.y; vp.sunDir[2] = m_SunDir.z;
    vp.sunColor[0] = m_SunColor.x; vp.sunColor[1] = m_SunColor.y;
    vp.sunColor[2] = m_SunColor.z; vp.sunColor[3] = m_SunIntensity;
    vp.ambient[0] = m_Ambient.x; vp.ambient[1] = m_Ambient.y;
    vp.ambient[2] = m_Ambient.z; vp.ambient[3] = m_AmbientIntensity;
    bufMgr->UploadData(m_ViewProjUBO, &vp, sizeof(vp));

    auto* pipeMgrN = static_cast<WebGPUPipelineManager*>(m_Renderer->GetPipelineManager());
    auto* bgMgrN = static_cast<WebGPUBindGroupManager*>(m_Renderer->GetBindGroupManager());
    WGPURenderPipeline pipeline = pipeMgrN->GetNativePipeline(m_Pipeline);
    WGPUBindGroup bindGroup = bgMgrN->GetNativeGroup(m_BindGroup);
    if (!pipeline || !bindGroup) return;

    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, bindGroup, 0, nullptr);
    for (u32 v = 0; v < count; ++v) {
        // firstInstance carries the volume slot in the high bits; the shader
        // splits it back into (volume, blade).
        wgpuRenderPassEncoderDraw(pass, draws[v].indexCount, draws[v].density, 0, v << 16);
    }
}

} // namespace Renderer
} // namespace Enjin

#endif // ENJIN_PLATFORM_WEB
