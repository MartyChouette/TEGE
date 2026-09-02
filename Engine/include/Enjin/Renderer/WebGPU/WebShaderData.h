#pragma once

#include "Enjin/Platform/Platform.h"

#if ENJIN_PLATFORM_WEB

namespace Enjin::Renderer::WebShaderData {

// Embedded PBR shader (matches Engine/shaders/wgsl/pbr.wgsl)
static const char* PBR_WGSL = R"(
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
    spotPos: array<vec4<f32>, 4>,
    spotDir: array<vec4<f32>, 4>,
    spotColor: array<vec4<f32>, 4>,
    spotParams: array<vec4<f32>, 4>,
    windData: vec4<f32>,                 // xyz = wind dir * strength, w = wind clock
    skyTop: vec4<f32>,                   // xyz zenith, w = configured flag
    skyBottom: vec4<f32>,
    skyHorizon: vec4<f32>,               // w = horizon haze
    skySunDir: vec4<f32>,                // w = sun intensity
    skySunColor: vec4<f32>,              // w = sun size
    skyClouds: vec4<f32>,                // cov1, scale1, speed, cov2
    skyCloudColor: vec4<f32>,            // w = scale2
    snowParams: vec4<f32>,               // x = snow accumulation (0..1); yzw reserved
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
    uvScrollU: f32,
    uvScrollV: f32,
    scrollReflSpeedU: f32,
    scrollReflSpeedV: f32,
    scrollReflStrength: f32,
    matcapBlend: f32,
};
struct ObjectDataArray {
    data: array<ObjectData>,
};
@group(1) @binding(0) var<storage, read> objects: ObjectDataArray;

struct BoneSSBO {
    matrices: array<mat4x4<f32>>,
};
@group(1) @binding(1) var<storage, read> bones: BoneSSBO;

@group(2) @binding(0) var baseColorTex: texture_2d<f32>;
@group(2) @binding(1) var baseColorSmp: sampler;
@group(2) @binding(2) var normalTex: texture_2d<f32>;
@group(2) @binding(3) var normalSmp: sampler;
@group(2) @binding(4) var mrTex: texture_2d<f32>;
@group(2) @binding(5) var mrSmp: sampler;
// Hand-crafted reflection styles (no SSR here on purpose): matcap sampled by
// view-space normal, and the N64-chrome scrolling reflection. Defaults are
// flat 1x1 textures; ObjectData.matcapBlend / scrollReflStrength gate use.
@group(2) @binding(6) var matcapTex: texture_2d<f32>;
@group(2) @binding(7) var matcapSmp: sampler;
@group(2) @binding(8) var scrollReflTex: texture_2d<f32>;
@group(2) @binding(9) var scrollReflSmp: sampler;

struct ShadowViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    lightPos: vec3<f32>,
    _pad: f32,
};
@group(3) @binding(0) var<uniform> shadowVP: ShadowViewProjection;
@group(3) @binding(1) var shadowMap: texture_depth_2d;
@group(3) @binding(2) var shadowSampler: sampler_comparison;

// Spot light shadows (max 2)
struct SpotShadowVPs {
    viewProj: array<mat4x4<f32>, 4>,   // [0]=light0.view, [1]=light0.proj, [2]=light1.view, [3]=light1.proj
};
@group(3) @binding(3) var<uniform> spotShadowVPs: SpotShadowVPs;
@group(3) @binding(4) var spotShadowMap0: texture_depth_2d;
@group(3) @binding(5) var spotShadowMap1: texture_depth_2d;

// Point light shadows (max 1, cubemap)
struct PointShadowVPs {
    viewProj: array<mat4x4<f32>, 12>,  // 6 faces x (view + proj)
};
@group(3) @binding(6) var<uniform> pointShadowVPs: PointShadowVPs;
@group(3) @binding(7) var pointShadowCube: texture_depth_cube;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) tangent: vec4<f32>,
    @location(4) boneWeights: vec4<f32>,
    @location(5) boneIndices: vec4<u32>,
    @location(6) color: vec4<f32>,        // vertex color (SDF glyphs carry textColor here)
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_pos: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
    @location(3) world_tangent: vec3<f32>,
    @location(4) world_bitangent: vec3<f32>,
    @location(5) @interpolate(flat) instanceIdx: u32,
    @location(6) color: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput, @builtin(instance_index) instanceIdx: u32) -> VertexOutput {
    let object = objects.data[instanceIdx];
    var out: VertexOutput;

    var skinnedPos = in.position;
    var skinnedNormal = in.normal;
    var skinnedTangent = in.tangent.xyz;

    let isSkinned = (object.flags & 8) != 0;
    if (isSkinned) {
        let skinMatrix = in.boneWeights.x * bones.matrices[in.boneIndices.x]
                       + in.boneWeights.y * bones.matrices[in.boneIndices.y]
                       + in.boneWeights.z * bones.matrices[in.boneIndices.z]
                       + in.boneWeights.w * bones.matrices[in.boneIndices.w];
        skinnedPos = (skinMatrix * vec4<f32>(in.position, 1.0)).xyz;
        let skinNormalMat = mat3x3<f32>(skinMatrix[0].xyz, skinMatrix[1].xyz, skinMatrix[2].xyz);
        skinnedNormal = skinNormalMat * in.normal;
        skinnedTangent = skinNormalMat * in.tangent.xyz;
    }

    var world_pos = object.model * vec4<f32>(skinnedPos, 1.0);

    // Water surface waves (FLAG_WATER_SURFACE = bit 5): the same Gerstner-lite
    // displacement triangle.vert applies on desktop, so web water moves instead
    // of rendering as a static slab. Fixed wind heading for now (the web
    // lighting UBO carries no wind vector); ocean mode (bit 11) adds swell.
    if ((object.flags & 32) != 0) {
        // Wind-driven like desktop: heading + strength from the lighting UBO's
        // wind vector; calm scenes fall back to a gentle default so water never
        // freezes solid.
        var t = lighting.windData.w;
        if (t == 0.0) { t = viewProj.time; }
        var wdir = lighting.windData.xz;
        var wmag = length(wdir);
        if (wmag < 0.05) { wdir = vec2<f32>(0.62, 0.79); wmag = 1.0; }
        else { wdir = wdir / wmag; }
        let phase1 = dot(world_pos.xz, wdir * 0.3) + t * 1.2;
        let phase2 = dot(world_pos.xz, vec2<f32>(-wdir.y, wdir.x) * 0.5) + t * 2.0;
        var wave = (sin(phase1) * 0.15 + sin(phase2) * 0.08) * wmag;
        if ((object.flags & 2048) != 0) {
            wave = wave + sin(dot(world_pos.xz, wdir * 0.08) + t * 0.6) * 0.35 * wmag;
        }
        world_pos.y = world_pos.y + wave;
    }
    out.clip_position = viewProj.proj * viewProj.view * world_pos;
    out.world_pos = world_pos.xyz;
    let normal_mat = mat3x3<f32>(
        object.model[0].xyz, object.model[1].xyz, object.model[2].xyz
    );
    out.world_normal = normalize(normal_mat * skinnedNormal);
    out.world_tangent = normalize(normal_mat * skinnedTangent);
    out.world_bitangent = cross(out.world_normal, out.world_tangent) * in.tangent.w;
    // Material UV scroll (waterfalls, conveyors): SUBTRACT scroll*time so a
    // positive speed moves the pattern in +UV, matching triangle.frag. Wind
    // clock drives it like desktop; calm scenes fall back to frame time.
    var scrollT = lighting.windData.w;
    if (scrollT == 0.0) { scrollT = viewProj.time; }
    out.uv = in.uv - vec2<f32>(object.uvScrollU, object.uvScrollV) * scrollT;
    out.instanceIdx = instanceIdx;
    out.color = in.color;
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

// Directional shadow map lookup with 3x3 PCF (9 taps)
fn sampleShadow(worldPos: vec3<f32>) -> f32 {
    let lightClip = shadowVP.proj * shadowVP.view * vec4<f32>(worldPos, 1.0);
    let ndc = lightClip.xyz / lightClip.w;
    let shadowUV = vec2<f32>(ndc.x * 0.5 + 0.5, ndc.y * -0.5 + 0.5);
    let depth = ndc.z;
    let bias = 0.002;
    let texelSize = 1.0 / 2048.0;
    var shadow = 0.0;
    // 3x3 PCF kernel
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>(-texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>(       0.0, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>( texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>(-texelSize,        0.0), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV,                                     depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>( texelSize,        0.0), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>(-texelSize,  texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>(       0.0,  texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>( texelSize,  texelSize), depth - bias);
    shadow = shadow / 9.0;

    // Smooth fade at shadow map edges (avoids hard frustum boundary)
    let fadeEdge = 0.15;
    let fadeX = smoothstep(0.0, fadeEdge, shadowUV.x) * (1.0 - smoothstep(1.0 - fadeEdge, 1.0, shadowUV.x));
    let fadeY = smoothstep(0.0, fadeEdge, shadowUV.y) * (1.0 - smoothstep(1.0 - fadeEdge, 1.0, shadowUV.y));
    let fade = fadeX * fadeY;
    let fadeZ = smoothstep(0.0, 0.01, depth) * (1.0 - smoothstep(0.99, 1.0, depth));
    return mix(1.0, shadow, fade * fadeZ);
}

// Spot light shadow lookup (perspective projection, 3x3 PCF)
fn sampleSpotShadowMap(worldPos: vec3<f32>, spotView: mat4x4<f32>, spotProj: mat4x4<f32>, shadowTex: texture_depth_2d) -> f32 {
    let lightClip = spotProj * spotView * vec4<f32>(worldPos, 1.0);
    let ndc = lightClip.xyz / lightClip.w;
    let shadowUV = vec2<f32>(ndc.x * 0.5 + 0.5, ndc.y * -0.5 + 0.5);
    let depth = ndc.z;
    let bias = 0.003;
    let texelSize = 1.0 / 512.0;
    var shadow = 0.0;
    // 3x3 PCF kernel
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>(-texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>(       0.0, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>( texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>(-texelSize,        0.0), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV,                                     depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>( texelSize,        0.0), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>(-texelSize,  texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>(       0.0,  texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>( texelSize,  texelSize), depth - bias);
    shadow = shadow / 9.0;
    let inBounds = step(0.0, shadowUV.x) * step(shadowUV.x, 1.0) * step(0.0, shadowUV.y) * step(shadowUV.y, 1.0)
                 * step(0.0, depth) * step(depth, 1.0);
    return mix(1.0, shadow, inBounds);
}

// Point light shadow lookup (cubemap, comparison against linear depth)
fn samplePointShadow(worldPos: vec3<f32>, lightPos: vec3<f32>, range: f32) -> f32 {
    let fragToLight = worldPos - lightPos;
    let dist = length(fragToLight);
    let safeDist = max(dist, 0.1);
    let dir = normalize(fragToLight);
    // Match WebGPU perspective depth: far*(dist-near) / (dist*(far-near))
    let nearZ = 0.1;
    let refDepth = clamp(range * (safeDist - nearZ) / (safeDist * (range - nearZ)), 0.0, 1.0);
    let bias = 0.008;
    let shadow = textureSampleCompare(pointShadowCube, shadowSampler, dir, refDepth - bias);
    // Fade near range boundary, return 1.0 (no shadow) beyond range
    let rangeFade = 1.0 - smoothstep(range * 0.85, range, dist);
    return mix(1.0, shadow, rangeFade);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let object = objects.data[in.instanceIdx];

    let baseColorSample = textureSample(baseColorTex, baseColorSmp, in.uv);

    // SDF text coverage — the fwidth derivative and the smoothstep are computed
    // here in UNIFORM control flow (no branch), then APPLIED at the very end of
    // the shader. Doing the SDF early-return here instead would put the later
    // shadow textureSampleCompare() calls in non-uniform control flow, which
    // WGSL forbids (it invalidated the whole PBR pipeline). See the tail return.
    let sdfEdge = 180.0 / 255.0;
    // Clamp the AA half-width (matches triangle.frag): at small sizes fwidth
    // blows up and, unclamped, the band reaches the edge value so each glyph
    // quad's transparent interior picks up alpha and shows as a faint box.
    let sdfW = clamp(fwidth(baseColorSample.a), 1e-4, sdfEdge * 0.5);
    let sdfCov = smoothstep(sdfEdge - sdfW, sdfEdge + sdfW, baseColorSample.a);

    let albedo = baseColorSample.rgb * object.baseColor;
    let alpha = baseColorSample.a * object.opacity;

    if (object.alphaCutoff > 0.0 && alpha < object.alphaCutoff) {
        discard;
    }

    let mr = textureSample(mrTex, mrSmp, in.uv);
    let metallic = mr.b * object.metallic;
    let roughness = mr.g * object.roughness;

    // Normal mapping — always sample (WGSL uniform control flow), skip TBN if tangents are zero
    let tangentNormal = textureSample(normalTex, normalSmp, in.uv).rgb * 2.0 - 1.0;
    let tangentLen = dot(in.world_tangent, in.world_tangent);
    var N = normalize(in.world_normal);
    if (tangentLen > 0.001) {
        let T = normalize(in.world_tangent);
        let B = normalize(in.world_bitangent);
        let TBN = mat3x3<f32>(T, B, N);
        N = normalize(TBN * tangentNormal);
    }
    let V = normalize(viewProj.viewPos - in.world_pos);

    let F0_dielectric = vec3<f32>(0.04);
    let F0 = mix(F0_dielectric, albedo, metallic);
    var Lo = vec3<f32>(0.0);

    // Shadow parameters: x=strength, y=spotShadowCount, z=pointShadowCount
    let shadowStrength = lighting.shadowParams.x;
    let spotShadowCasterCount = i32(lighting.shadowParams.y);
    let pointShadowCasterCount = i32(lighting.shadowParams.z);

    // Pre-sample ALL shadow maps outside loops (WGSL uniform control flow requirement)
    let shadowFactor = sampleShadow(in.world_pos);
    // Only sample spot/point shadows if there are active shadow casters (otherwise textures contain garbage)
    var spotShadow0 = 1.0;
    var spotShadow1 = 1.0;
    var ptShadow0 = 1.0;
    if (spotShadowCasterCount > 0) {
        spotShadow0 = sampleSpotShadowMap(in.world_pos,
            spotShadowVPs.viewProj[0], spotShadowVPs.viewProj[1], spotShadowMap0);
    }
    if (spotShadowCasterCount > 1) {
        spotShadow1 = sampleSpotShadowMap(in.world_pos,
            spotShadowVPs.viewProj[2], spotShadowVPs.viewProj[3], spotShadowMap1);
    }
    if (pointShadowCasterCount > 0) {
        let pointLight0Pos = lighting.lightDir[4].xyz;
        let pointLight0Range = max(lighting.lightParams[4].x, 0.001);
        ptShadow0 = samplePointShadow(in.world_pos, pointLight0Pos, pointLight0Range);
    }

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
        // Sun shadow applies to ALL directional lights — they share one shadow map,
        // and any unshadowed directional re-lights shadowed areas (washes shadows out)
        let shadow = mix(1.0, shadowFactor, shadowStrength);
        Lo = Lo + (kD * albedo + specular) * radiance * NdotL * shadow;
    }

    let pointCount = i32(lighting.lightCount.y);
    for (var i = 0; i < pointCount; i = i + 1) {
        let idx = i + 4;
        let lightPos = lighting.lightDir[idx].xyz;
        let toLight = lightPos - in.world_pos;
        let dist = length(toLight);
        let range = lighting.lightParams[idx].x;
        if (dist > range) { continue; }

        let L = normalize(toLight);
        let H = normalize(V + L);

        let linAtt = lighting.lightParams[idx].y;
        let quadAtt = lighting.lightParams[idx].z;
        let constAtt = lighting.lightParams[idx].w;
        let distAtt = 1.0 / (constAtt + linAtt * dist + quadAtt * dist * dist);
        // Smooth range falloff (avoids hard circle edge)
        let rangeFade = 1.0 - smoothstep(range * 0.75, range, dist);
        let attenuation = distAtt * rangeFade;

        let radiance = lighting.lightColor[idx].rgb * lighting.lightColor[idx].w * attenuation;

        let NDF = distributionGGX(N, H, roughness);
        let G = geometrySmith(N, V, L, roughness);
        let F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        let numerator = NDF * G * F;
        let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        let specular = numerator / denominator;
        let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
        let NdotL = max(dot(N, L), 0.0);

        var ptShadow = 1.0;
        if (i == 0) { ptShadow = ptShadow0; }
        Lo = Lo + (kD * albedo + specular) * radiance * NdotL * ptShadow;
    }

    let spotCount = i32(lighting.lightCount.z);
    for (var i = 0; i < spotCount; i = i + 1) {
        let lightPos = lighting.spotPos[i].xyz;
        let range = lighting.spotPos[i].w;
        let toLight = lightPos - in.world_pos;
        let dist = length(toLight);
        if (dist > range) { continue; }

        let L = normalize(toLight);
        let H = normalize(V + L);
        let spotDirV = normalize(lighting.spotDir[i].xyz);

        let theta = dot(L, normalize(-spotDirV));
        let innerCos = lighting.spotParams[i].x;
        let outerCos = lighting.spotParams[i].y;
        let epsilon = innerCos - outerCos;
        let spotFactor = clamp((theta - outerCos) / max(epsilon, 0.0001), 0.0, 1.0);

        let linAtt = lighting.spotParams[i].z;
        let quadAtt = lighting.spotParams[i].w;
        let attenuation = spotFactor / (1.0 + linAtt * dist + quadAtt * dist * dist);

        let radiance = lighting.spotColor[i].rgb * lighting.spotColor[i].w * attenuation;

        let NDF = distributionGGX(N, H, roughness);
        let G = geometrySmith(N, V, L, roughness);
        let F = fresnelSchlick(max(dot(H, V), 0.0), F0);
        let numerator = NDF * G * F;
        let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        let specular = numerator / denominator;
        let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
        let NdotL = max(dot(N, L), 0.0);

        var spotShadow = 1.0;
        if (i == 0) { spotShadow = spotShadow0; }
        else if (i == 1) { spotShadow = spotShadow1; }
        Lo = Lo + (kD * albedo + specular) * radiance * NdotL * spotShadow;
    }

    let ambient = lighting.ambientColor.rgb * lighting.ambientColor.w * albedo;
    let emissive = object.emissiveColor * object.emissiveStrength;
    var color = ambient + Lo + emissive;

    // Matcap (hand-painted reflection): sampled by view-space normal, blended
    // by metallic like triangle.frag ~1850. Sampled unconditionally at LOD 0
    // (uniform-control-flow rule); matcapBlend=0 (default white 1x1) is a no-op.
    {
        let vFwd = normalize(viewProj.viewPos - in.world_pos);
        let vRight = normalize(cross(vec3<f32>(0.0, 1.0, 0.0), vFwd));
        let vUp = cross(vFwd, vRight);
        let mcUV = vec2<f32>(dot(N, vRight), -dot(N, vUp)) * 0.5 + 0.5;
        let mc = textureSampleLevel(matcapTex, matcapSmp, mcUV, 0.0).rgb;
        let mcBlend = mix(0.3, 1.0, metallic) * object.matcapBlend;
        color = mix(color, color * mc + mc * 0.2, mcBlend);
    }

    // Scrolling reflection (N64 chrome/water fake): reflection-direction UVs
    // scrolled by time, fresnel-masked, additive — mirrors triangle.frag ~1898.
    {
        let sV = normalize(viewProj.viewPos - in.world_pos);
        let sR = reflect(-sV, N);
        var sT = lighting.windData.w;
        if (sT == 0.0) { sT = viewProj.time; }
        let sUv = sR.xz * 0.5 + vec2<f32>(0.5, 0.5)
                + vec2<f32>(object.scrollReflSpeedU, object.scrollReflSpeedV) * sT;
        let sCol = textureSampleLevel(scrollReflTex, scrollReflSmp, sUv, 0.0).rgb;
        let sFres = pow(1.0 - max(dot(N, sV), 0.0), 3.0);
        color = color + sCol * object.scrollReflStrength * clamp(sFres + 0.25, 0.0, 1.0);
    }

    // Snow accumulation: whiten upward-facing surfaces by settled snow (mirrors
    // triangle.frag ~1832). snowParams.x builds up while snowing and melts slowly
    // when clear, so snow accumulates and then the scene returns to clear/spring.
    let snowAccum = lighting.snowParams.x;
    if (snowAccum > 0.0) {
        let snowCoverage = snowAccum * smoothstep(0.3, 0.8, N.y);
        color = mix(color, vec3<f32>(0.95, 0.97, 1.0), snowCoverage);
    }

    // Height-based distance fog (matches Vulkan)
    let fogDensity = lighting.fogParams.x;
    if (fogDensity > 0.0) {
        let fogStart = lighting.fogParams.y;
        let fogEnd = lighting.fogParams.z;
        let fogHeightFalloff = lighting.fogParams.w;
        let dist = length(viewProj.viewPos - in.world_pos);
        var fogFactor = clamp((dist - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
        fogFactor = fogFactor * fogDensity;
        let heightFog = exp(-max(in.world_pos.y, 0.0) * fogHeightFalloff);
        fogFactor = fogFactor * heightFog;
        color = mix(color, lighting.fogColor.rgb, fogFactor);
    }

    // SDF text (flags bit 6): applied HERE, after all shadow textureSampleCompare
    // calls, so the per-instance branch never makes them non-uniform. Output the
    // glyph's vertex color UNLIT with the thresholded coverage as alpha.
    if ((object.flags & 64) != 0) {
        if (sdfCov < 0.01) { discard; }
        return vec4<f32>(in.color.rgb * object.baseColor, sdfCov * in.color.a * object.opacity);
    }

    // Output linear HDR — post-process pass handles ACES tonemap + gamma
    return vec4<f32>(color, alpha);
}
)";

// Embedded shadow depth shader (matches Engine/shaders/wgsl/shadow.wgsl)
static const char* SHADOW_WGSL = R"(
struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};
@group(0) @binding(0) var<uniform> lightVP: ViewProjection;

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
    uvScrollU: f32,
    uvScrollV: f32,
    scrollReflSpeedU: f32,
    scrollReflSpeedV: f32,
    scrollReflStrength: f32,
    matcapBlend: f32,
};
@group(1) @binding(0) var<uniform> object: ObjectData;

struct VertexInput {
    @location(0) position: vec3<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let world_pos = object.model * vec4<f32>(in.position, 1.0);
    out.clip_position = lightVP.proj * lightVP.view * world_pos;
    return out;
}
)";

// Embedded post-process shader (ACES tonemap + FXAA)
static const char* POSTPROCESS_WGSL = R"(
@group(0) @binding(0) var sceneTexture: texture_2d<f32>;
@group(0) @binding(1) var sceneSampler: sampler;

struct PostProcessParams {
    colorblindMode: u32,
    colorblindStrength: f32,
    brightness: f32,
    contrast: f32,
    previewEffect: u32,
    previewDivider: f32,
    _pad0: f32,
    _pad1: f32,
};
@group(0) @binding(2) var<uniform> params: PostProcessParams;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    let x = f32(i32(vertexIndex & 1u) * 4 - 1);
    let y = f32(i32(vertexIndex >> 1u) * 4 - 1);
    out.position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    return out;
}

fn aces_tonemap(color: vec3<f32>) -> vec3<f32> {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

fn luminance(c: vec3<f32>) -> f32 {
    return dot(c, vec3<f32>(0.299, 0.587, 0.114));
}

// Exact sRGB OETF (linear -> sRGB display), the IEC 61966-2-1 piecewise curve.
// Replaces the old pow(1/2.2) approximation, which diverged from true sRGB most
// in the dark/indirect range — the exact spot GI and shadows live, and where
// Safari's color management made it read differently from Chrome/desktop. The
// desktop Vulkan swapchain is B8G8R8A8_SRGB (hardware does this exact curve);
// this makes the web output byte-identical to that intent, deterministic across
// every browser. Kept in-shader (not an sRGB swapchain view) so the ImGui/HUD
// overlay that shares the canvas isn't double-encoded.
fn linearToSrgb(c: vec3<f32>) -> vec3<f32> {
    let lo = c * 12.92;
    let hi = 1.055 * pow(max(c, vec3<f32>(0.0)), vec3<f32>(1.0 / 2.4)) - 0.055;
    return select(hi, lo, c <= vec3<f32>(0.0031308));
}

fn fxaa(uv: vec2<f32>, texelSize: vec2<f32>) -> vec3<f32> {
    let rgbM = textureSample(sceneTexture, sceneSampler, uv).rgb;
    let rgbN = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>(0.0, -texelSize.y)).rgb;
    let rgbS = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>(0.0,  texelSize.y)).rgb;
    let rgbE = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>( texelSize.x, 0.0)).rgb;
    let rgbW = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>(-texelSize.x, 0.0)).rgb;

    let lumM = luminance(pow(aces_tonemap(rgbM), vec3<f32>(1.0 / 2.2)));
    let lumN = luminance(pow(aces_tonemap(rgbN), vec3<f32>(1.0 / 2.2)));
    let lumS = luminance(pow(aces_tonemap(rgbS), vec3<f32>(1.0 / 2.2)));
    let lumE = luminance(pow(aces_tonemap(rgbE), vec3<f32>(1.0 / 2.2)));
    let lumW = luminance(pow(aces_tonemap(rgbW), vec3<f32>(1.0 / 2.2)));

    let lumMin = min(lumM, min(min(lumN, lumS), min(lumE, lumW)));
    let lumMax = max(lumM, max(max(lumN, lumS), max(lumE, lumW)));
    let lumRange = lumMax - lumMin;

    let edgeH = abs(lumN + lumS - 2.0 * lumM);
    let edgeV = abs(lumE + lumW - 2.0 * lumM);
    let isHorizontal = edgeH > edgeV;

    let hDir = select(vec2<f32>(0.0, texelSize.y), vec2<f32>(0.0, -texelSize.y), abs(lumN - lumM) > abs(lumS - lumM));
    let vDir = select(vec2<f32>(texelSize.x, 0.0), vec2<f32>(-texelSize.x, 0.0), abs(lumW - lumM) > abs(lumE - lumM));
    let blendDir = select(vDir, hDir, isHorizontal);

    let rgbNeighbor = textureSample(sceneTexture, sceneSampler, uv + blendDir).rgb;
    let needsAA = lumRange >= max(0.0312, lumMax * 0.125);
    let blendFactor = select(0.0, clamp(lumRange / lumMax, 0.0, 0.75) * 0.5, needsAA);
    return mix(rgbM, rgbNeighbor, blendFactor);
}

// Daltonization — colorblind correction (Brettel/Machado approach)
fn applyColorblindCorrection(color: vec3<f32>) -> vec3<f32> {
    let mode = params.colorblindMode;
    if (mode == 0u) { return color; }

    if (mode == 7u) {
        let gray = dot(color, vec3<f32>(0.299, 0.587, 0.114));
        return mix(color, vec3<f32>(gray), params.colorblindStrength);
    }

    var simR: vec3<f32>;
    var simG: vec3<f32>;
    var simB: vec3<f32>;

    var strength = params.colorblindStrength;
    var baseMode = mode;
    if (mode >= 4u && mode <= 6u) {
        strength *= 0.6;
        baseMode = mode - 3u;
    }

    if (baseMode == 1u) {
        simR = vec3<f32>(0.152286, 0.114503, -0.003882);
        simG = vec3<f32>(1.052583, 0.786281, -0.048116);
        simB = vec3<f32>(-0.204868, 0.099216, 1.051998);
    } else if (baseMode == 2u) {
        simR = vec3<f32>(0.367322, 0.280085, -0.011820);
        simG = vec3<f32>(0.860646, 0.672501, 0.042940);
        simB = vec3<f32>(-0.227968, 0.047413, 0.968881);
    } else {
        simR = vec3<f32>(1.255528, -0.078411, 0.004733);
        simG = vec3<f32>(-0.076749, 0.930809, 0.691367);
        simB = vec3<f32>(-0.178779, 0.147602, 0.303900);
    }

    let simulated = vec3<f32>(dot(color, simR), dot(color, simG), dot(color, simB));
    let error = color - simulated;
    var corrected = color;
    corrected.g = corrected.g + error.r * 0.7;
    corrected.b = corrected.b + error.r * 0.7;

    return mix(color, saturate(corrected), strength);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let texDim = vec2<f32>(textureDimensions(sceneTexture));
    let texelSize = vec2<f32>(1.0 / texDim.x, 1.0 / texDim.y);

    var color = fxaa(in.uv, texelSize);
    color = aces_tonemap(color);

    // Options preview split: left of the divider shows the frame WITHOUT the
    // previewed effect (5 = colorblind, 6 = brightness/contrast).
    let previewLeft = params.previewEffect != 0u && in.uv.x < params.previewDivider;
    if (!(previewLeft && params.previewEffect == 6u)) {
        color = (color - 0.5) * params.contrast + 0.5 + params.brightness;
    }
    if (!(previewLeft && params.previewEffect == 5u)) {
        color = applyColorblindCorrection(color);
    }
    color = linearToSrgb(color);
    if (params.previewEffect != 0u && abs(in.uv.x - params.previewDivider) < texelSize.x) {
        color = vec3<f32>(1.0, 1.0, 1.0);
    }
    return vec4<f32>(saturate(color), 1.0);
}
)";

// Bloom downsample shader — extracts bright pixels and progressively blurs
static const char* BLOOM_DOWN_WGSL = R"(
@group(0) @binding(0) var srcTexture: texture_2d<f32>;
@group(0) @binding(1) var srcSampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    let x = f32(i32(vertexIndex & 1u) * 4 - 1);
    let y = f32(i32(vertexIndex >> 1u) * 4 - 1);
    out.position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let texDim = vec2<f32>(textureDimensions(srcTexture));
    let texelSize = 1.0 / texDim;
    // Dual Kawase downsample: 5-tap filter (center + 4 diagonals at half-texel offset)
    var color = textureSample(srcTexture, srcSampler, in.uv) * 4.0;
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>(-texelSize.x, -texelSize.y));
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>( texelSize.x, -texelSize.y));
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>(-texelSize.x,  texelSize.y));
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>( texelSize.x,  texelSize.y));
    return color / 8.0;
}
)";

// Bloom brightness extraction — first downsample pass with threshold
static const char* BLOOM_THRESHOLD_WGSL = R"(
@group(0) @binding(0) var srcTexture: texture_2d<f32>;
@group(0) @binding(1) var srcSampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    let x = f32(i32(vertexIndex & 1u) * 4 - 1);
    let y = f32(i32(vertexIndex >> 1u) * 4 - 1);
    out.position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let color = textureSample(srcTexture, srcSampler, in.uv);
    let brightness = dot(color.rgb, vec3<f32>(0.2126, 0.7152, 0.0722));
    // Soft threshold with knee. Scene is linear HDR pre-ACES: only over-white
    // pixels should bloom, else bright albedo under 2 suns blooms everywhere.
    let threshold = 1.0;
    let knee = 0.5;
    let soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    let contrib = soft * soft / (4.0 * knee + 0.0001);
    let factor = max(brightness - threshold, contrib) / max(brightness, 0.0001);
    return vec4<f32>(color.rgb * factor, 1.0);
}
)";

// Bloom upsample shader — Dual Kawase upsample with additive blend
static const char* BLOOM_UP_WGSL = R"(
@group(0) @binding(0) var srcTexture: texture_2d<f32>;
@group(0) @binding(1) var srcSampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    let x = f32(i32(vertexIndex & 1u) * 4 - 1);
    let y = f32(i32(vertexIndex >> 1u) * 4 - 1);
    out.position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let texDim = vec2<f32>(textureDimensions(srcTexture));
    let t = 1.0 / texDim;
    // Dual Kawase upsample: 8-tap filter (4 cardinal + 4 diagonal at half texel)
    var color = vec4<f32>(0.0);
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>(-t.x,      0.0)) * 2.0;
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>( t.x,      0.0)) * 2.0;
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>(    0.0, -t.y)) * 2.0;
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>(    0.0,  t.y)) * 2.0;
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>(-t.x, -t.y));
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>( t.x, -t.y));
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>(-t.x,  t.y));
    color += textureSample(srcTexture, srcSampler, in.uv + vec2<f32>( t.x,  t.y));
    return color / 12.0;
}
)";

// Bloom composite — additively blend bloom result onto scene
static const char* BLOOM_COMPOSITE_WGSL = R"(
@group(0) @binding(0) var sceneTexture: texture_2d<f32>;
@group(0) @binding(1) var sceneSampler: sampler;
@group(0) @binding(2) var bloomTexture: texture_2d<f32>;
@group(0) @binding(3) var bloomSampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    let x = f32(i32(vertexIndex & 1u) * 4 - 1);
    let y = f32(i32(vertexIndex >> 1u) * 4 - 1);
    out.position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let scene = textureSample(sceneTexture, sceneSampler, in.uv).rgb;
    let bloom = textureSample(bloomTexture, bloomSampler, in.uv).rgb;
    let bloomIntensity = 0.15;
    return vec4<f32>(scene + bloom * bloomIntensity, 1.0);
}
)";

// Particle billboard shader — instanced quads facing camera
static const char* PARTICLE_WGSL = R"(
struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};
@group(0) @binding(0) var<uniform> viewProj: ViewProjection;

struct VertexInput {
    @location(0) position: vec2<f32>,  // Quad vertex (-0.5 to 0.5)
    @location(1) uv: vec2<f32>,
};
struct InstanceInput {
    @location(2) worldPos: vec3<f32>,
    @location(3) size: f32,
    @location(4) alpha: f32,
    @location(5) colorR: f32,
    @location(6) colorG: f32,
    @location(7) colorB: f32,
};
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec3<f32>,
    @location(2) alpha: f32,
};

@vertex
fn vs_main(vert: VertexInput, inst: InstanceInput) -> VertexOutput {
    // Extract camera right/up from view matrix (transposed rotation)
    let right = vec3<f32>(viewProj.view[0][0], viewProj.view[1][0], viewProj.view[2][0]);
    let up = vec3<f32>(viewProj.view[0][1], viewProj.view[1][1], viewProj.view[2][1]);
    let worldPosition = inst.worldPos + right * vert.position.x * inst.size + up * vert.position.y * inst.size;

    var out: VertexOutput;
    out.position = viewProj.proj * viewProj.view * vec4<f32>(worldPosition, 1.0);
    out.uv = vert.uv;
    out.color = vec3<f32>(inst.colorR, inst.colorG, inst.colorB);
    // Soft circular falloff
    out.alpha = inst.alpha;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Soft circular particle (distance from center)
    let d = length(in.uv - vec2<f32>(0.5));
    let circle = 1.0 - smoothstep(0.3, 0.5, d);
    return vec4<f32>(in.color, in.alpha * circle);
}
)";

// Grass blade shader — instanced procedural placement
static const char* GRASS_WGSL = R"(
struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};
@group(0) @binding(0) var<uniform> viewProj: ViewProjection;

struct VolumeParams {
    volumePos: vec3<f32>,
    bladeHeight: f32,
    halfExtents: vec3<f32>,
    bladeWidth: f32,
    baseColor: vec3<f32>,
    windStrength: f32,
    tipColor: vec3<f32>,
    _pad: f32,
};
@group(1) @binding(0) var<uniform> volume: VolumeParams;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec3<f32>,
    @location(1) normal: vec3<f32>,
};

fn hash(n: f32) -> f32 {
    return fract(sin(n) * 43758.5453);
}

@vertex
fn vs_main(vert: VertexInput, @builtin(instance_index) instanceIdx: u32) -> VertexOutput {
    let seed = f32(instanceIdx);
    // Pseudo-random position within volume
    let rx = hash(seed * 1.0) * 2.0 - 1.0;
    let rz = hash(seed * 2.0) * 2.0 - 1.0;
    let posX = volume.volumePos.x + rx * volume.halfExtents.x;
    let posZ = volume.volumePos.z + rz * volume.halfExtents.z;
    let posY = volume.volumePos.y;

    // Random rotation around Y
    let angle = hash(seed * 3.0) * 6.2832;
    let ca = cos(angle);
    let sa = sin(angle);

    // Scale blade and apply rotation
    var localPos = vert.position;
    localPos.x = localPos.x * volume.bladeWidth;
    localPos.y = localPos.y * volume.bladeHeight * (0.7 + hash(seed * 4.0) * 0.6);
    let rotatedX = localPos.x * ca - localPos.z * sa;
    let rotatedZ = localPos.x * sa + localPos.z * ca;

    let worldPos = vec3<f32>(posX + rotatedX, posY + localPos.y, posZ + rotatedZ);

    var out: VertexOutput;
    out.position = viewProj.proj * viewProj.view * vec4<f32>(worldPos, 1.0);
    out.color = mix(volume.baseColor, volume.tipColor, vert.uv.y);
    out.normal = vec3<f32>(0.0, 1.0, 0.0);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Simple directional lighting
    let lightDir = normalize(vec3<f32>(0.5, -0.8, 0.3));
    let NdotL = max(dot(in.normal, -lightDir), 0.0) * 0.6 + 0.4;
    return vec4<f32>(in.color * NdotL, 1.0);
}
)";

// Tree shader — instanced trunk+canopy crossing quads
static const char* TREE_WGSL = R"(
struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};
@group(0) @binding(0) var<uniform> viewProj: ViewProjection;

struct VolumeParams {
    volumePos: vec3<f32>,
    trunkHeight: f32,
    halfExtents: vec3<f32>,
    trunkWidth: f32,
    trunkColor: vec3<f32>,
    canopyRadius: f32,
    canopyColor: vec3<f32>,
    canopyOffset: f32,
};
@group(1) @binding(0) var<uniform> volume: VolumeParams;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec3<f32>,
};

fn hash(n: f32) -> f32 {
    return fract(sin(n) * 43758.5453);
}

@vertex
fn vs_main(vert: VertexInput, @builtin(instance_index) instanceIdx: u32) -> VertexOutput {
    let seed = f32(instanceIdx);
    let rx = hash(seed * 1.0) * 2.0 - 1.0;
    let rz = hash(seed * 2.0) * 2.0 - 1.0;
    let treeOrigin = vec3<f32>(
        volume.volumePos.x + rx * volume.halfExtents.x,
        volume.volumePos.y,
        volume.volumePos.z + rz * volume.halfExtents.z);

    // Per-instance size + rotation, matching desktop tree.vert so web trees
    // don't all face the same way and vary in height.
    let sizeVar = 0.7 + hash(seed * 3.0) * 0.6;
    let rotAngle = hash(seed * 7.0 + 5.0) * 6.28318;
    let cosR = cos(rotAngle);
    let sinR = sin(rotAngle);

    let isCanopy = vert.uv.y > 0.5;

    // Remap the baked template mesh into the volume's AUTHORED dimensions.
    // The old shader scaled the whole tree (canopy included) by trunkWidth,
    // collapsing the crown to a sliver and ignoring trunkHeight/canopyRadius.
    //   trunk mesh: xz in [-0.5,0.5], y in [0,1]
    //   canopy mesh: xz in [-1.5,1.5], y in [1.2,4.2]
    var localPos: vec3<f32>;
    if (isCanopy) {
        // Canopy mesh is now a UNIT sphere (xyz in [-1,1]). Scale by canopyRadius and
        // lift so the sphere sits centered at canopyOffset + r above the trunk.
        let r = volume.canopyRadius * sizeVar;
        localPos = vec3<f32>(
            vert.position.x * r,
            volume.canopyOffset * sizeVar + r + vert.position.y * r,
            vert.position.z * r);
    } else {
        let w = volume.trunkWidth * 2.0 * sizeVar;
        localPos = vec3<f32>(
            vert.position.x * w,
            vert.position.y * volume.trunkHeight * sizeVar,
            vert.position.z * w);
    }

    // Rotate around Y
    let rotated = vec3<f32>(
        localPos.x * cosR - localPos.z * sinR,
        localPos.y,
        localPos.x * sinR + localPos.z * cosR);

    // Gentle wind sway. Web trees carry no per-zone wind field, so use a fixed
    // breeze with a per-instance phase: the canopy tips lever from the neck,
    // the trunk bends quadratically with height (same shape as desktop).
    let windDir = vec3<f32>(0.80, 0.0, 0.60);
    let windPhase = dot(treeOrigin.xz, vec2<f32>(0.08, 0.15)) + viewProj.time;
    let windAngle = sin(windPhase) * 0.12;
    var windDisp = vec3<f32>(0.0, 0.0, 0.0);
    if (isCanopy) {
        let span = max(volume.canopyRadius * 2.0 * sizeVar, 0.01);
        let lever = clamp((localPos.y - volume.canopyOffset * sizeVar) / span, 0.0, 1.0);
        windDisp = windDir * (windAngle * 0.5 + windAngle * lever);
    } else {
        let t = localPos.y / max(volume.trunkHeight * sizeVar, 0.01);
        windDisp = windDir * (t * t * windAngle * 0.5);
    }

    let worldPos = treeOrigin + rotated + windDisp;

    var out: VertexOutput;
    out.position = viewProj.proj * viewProj.view * vec4<f32>(worldPos, 1.0);
    // UV.y > 0.5 = canopy, else trunk; a little vertical shading gives the
    // crude crossed billboards some depth.
    let base = select(volume.trunkColor, volume.canopyColor, isCanopy);
    let shade = select(0.8, 0.7 + (vert.uv.y - 0.6) * 0.75, isCanopy);
    out.color = base * shade;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return vec4<f32>(in.color, 1.0);
}
)";

// Sprite shader — unlit textured billboard with tint
static const char* SPRITE_WGSL = R"(
struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};
@group(0) @binding(0) var<uniform> viewProj: ViewProjection;
@group(1) @binding(0) var spriteTex: texture_2d<f32>;
@group(1) @binding(1) var spriteSmp: sampler;

struct VertexInput {
    @location(0) position: vec2<f32>,
    @location(1) uv: vec2<f32>,
};
struct InstanceInput {
    @location(2) worldPos: vec3<f32>,
    @location(3) sizeX: f32,
    @location(4) sizeY: f32,
    @location(5) rotation: f32,
    @location(6) tintR: f32,
    @location(7) tintG: f32,
    @location(8) tintB: f32,
    @location(9) tintA: f32,
    @location(10) uvLeft: f32,
    @location(11) uvTop: f32,
    @location(12) uvRight: f32,
    @location(13) uvBottom: f32,
};
struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) tint: vec4<f32>,
};

@vertex
fn vs_main(vert: VertexInput, inst: InstanceInput) -> VertexOutput {
    let ca = cos(inst.rotation);
    let sa = sin(inst.rotation);
    let rx = vert.position.x * ca - vert.position.y * sa;
    let ry = vert.position.x * sa + vert.position.y * ca;

    // Billboard in world space (camera-facing)
    let right = vec3<f32>(viewProj.view[0][0], viewProj.view[1][0], viewProj.view[2][0]);
    let up = vec3<f32>(viewProj.view[0][1], viewProj.view[1][1], viewProj.view[2][1]);
    let worldPos = inst.worldPos + right * rx * inst.sizeX + up * ry * inst.sizeY;

    var out: VertexOutput;
    out.position = viewProj.proj * viewProj.view * vec4<f32>(worldPos, 1.0);
    // Remap UV to atlas rect
    out.uv = vec2<f32>(
        mix(inst.uvLeft, inst.uvRight, vert.uv.x),
        mix(inst.uvTop, inst.uvBottom, vert.uv.y)
    );
    out.tint = vec4<f32>(inst.tintR, inst.tintG, inst.tintB, inst.tintA);
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let texColor = textureSample(spriteTex, spriteSmp, in.uv);
    return texColor * in.tint;
}
)";

// Embedded procedural sky shader (gradient sky, no cubemap needed)
static const char* SKY_WGSL = R"(
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
    spotPos: array<vec4<f32>, 4>,
    spotDir: array<vec4<f32>, 4>,
    spotColor: array<vec4<f32>, 4>,
    spotParams: array<vec4<f32>, 4>,
    windData: vec4<f32>,                 // xyz = wind dir * strength, w = wind clock
    skyTop: vec4<f32>,                   // xyz zenith, w = configured flag
    skyBottom: vec4<f32>,
    skyHorizon: vec4<f32>,               // w = horizon haze
    skySunDir: vec4<f32>,                // w = sun intensity
    skySunColor: vec4<f32>,              // w = sun size
    skyClouds: vec4<f32>,                // cov1, scale1, speed, cov2
    skyCloudColor: vec4<f32>,            // w = scale2
    snowParams: vec4<f32>,               // x = snow accumulation (0..1); yzw reserved
};
@group(0) @binding(0) var<uniform> viewProj: ViewProjection;
@group(0) @binding(1) var<uniform> lighting: LightingUBO;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) clipPos: vec4<f32>,
};

fn skyHash(p0: vec2<f32>) -> f32 {
    var p = fract(p0 * vec2<f32>(123.34, 456.21));
    p = p + dot(p, p + 45.32);
    return fract(p.x * p.y);
}
fn skyNoise(p: vec2<f32>) -> f32 {
    let i = floor(p);
    var f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    let a = skyHash(i);
    let b = skyHash(i + vec2<f32>(1.0, 0.0));
    let c = skyHash(i + vec2<f32>(0.0, 1.0));
    let d = skyHash(i + vec2<f32>(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
fn skyFbm(p0: vec2<f32>) -> f32 {
    var p = p0;
    var v = 0.0;
    var a = 0.5;
    for (var i = 0; i < 4; i = i + 1) {
        v = v + skyNoise(p) * a;
        p = p * 2.03;
        a = a * 0.5;
    }
    return v;
}

@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    let x = f32(i32(vertexIndex & 1u) * 4 - 1);
    let y = f32(i32(vertexIndex >> 1u) * 4 - 1);
    out.position = vec4<f32>(x, y, 1.0, 1.0);
    out.clipPos = out.position;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Reconstruct view direction from clip position via inverse view-projection
    let invProj = viewProj.proj;
    let invView = viewProj.view;
    // Approximate: unproject clip position and normalize
    let ndc = in.clipPos.xy / in.clipPos.w;
    // Build view ray from NDC (simplified inverse projection)
    let viewDir = normalize(vec3<f32>(ndc.x / invProj[0][0], ndc.y / invProj[1][1], -1.0));
    // Transform to world space with the TRANSPOSED view rotation (inverse of the
    // rotation-only view matrix). WGSL mat[c][r] is column-major: row-i·v needs
    // invView[j][i] — the previous indexing applied the UNtransposed matrix and
    // rolled/mirrored the sky against camera yaw+pitch (diagonal horizons).
    let worldDir = normalize(vec3<f32>(
        invView[0][0] * viewDir.x + invView[0][1] * viewDir.y + invView[0][2] * viewDir.z,
        invView[1][0] * viewDir.x + invView[1][1] * viewDir.y + invView[1][2] * viewDir.z,
        invView[2][0] * viewDir.x + invView[2][1] * viewDir.y + invView[2][2] * viewDir.z
    ));

    // Palette: authored scene sky when configured, classic defaults otherwise
    var horizon = vec3<f32>(0.6, 0.7, 0.85);
    var zenith = vec3<f32>(0.25, 0.4, 0.75);
    var ground = vec3<f32>(0.4, 0.45, 0.4);
    let configured = lighting.skyTop.w > 0.5;
    if (configured) {
        zenith = lighting.skyTop.xyz;
        ground = lighting.skyBottom.xyz;
        horizon = lighting.skyHorizon.xyz;
    }
    // Same ramp the desktop cubemap bake uses (Skybox::CreateProcedural):
    // a tight 10% horizon band, then linear blends to zenith/ground. The old
    // pow-curve + steep below-horizon mix painted a wide pale band of
    // bottomColor across the horizon that desktop never shows.
    let t = clamp(worldDir.y * 0.5 + 0.5, 0.0, 1.0);
    var sky = horizon;
    if (t > 0.55) {
        sky = mix(horizon, zenith, (t - 0.55) / 0.45);
    } else if (t < 0.45) {
        sky = mix(horizon, ground, (0.45 - t) / 0.45);
    }

    if (configured) {
        // Horizon haze
        let haze = lighting.skyHorizon.w;
        if (haze > 0.001) {
            let band = pow(1.0 - clamp(abs(worldDir.y), 0.0, 1.0), 6.0);
            sky = mix(sky, mix(sky, vec3<f32>(0.82, 0.86, 0.92), 0.6), band * haze);
        }
        // Sun disc + glow
        let sunI = lighting.skySunDir.w;
        if (sunI > 0.001) {
            let sunDir = normalize(lighting.skySunDir.xyz);
            let d = max(dot(worldDir, sunDir), 0.0);
            let size = max(lighting.skySunColor.w, 0.001);
            let disc = smoothstep(1.0 - size, 1.0 - size * 0.35, d);
            let glow = pow(d, 48.0) * 0.35;
            sky = sky + lighting.skySunColor.xyz * (disc + glow) * sunI;
        }
        // Cloud layers drifting with the wind
        let upness = smoothstep(0.02, 0.18, worldDir.y);
        if (upness > 0.0) {
            var drift = lighting.windData.xz;
            if (dot(drift, drift) < 1e-5) { drift = vec2<f32>(1.0, 0.35); }
            drift = normalize(drift);
            let plane = worldDir.xz / (worldDir.y + 0.18);
            let time = lighting.windData.w;
            let speed = lighting.skyClouds.z * 0.01;
            let cov1 = lighting.skyClouds.x;
            if (cov1 > 0.001) {
                let p = plane * (2.0 * lighting.skyClouds.y) + drift * (time * speed);
                let n = skyFbm(p);
                let m = smoothstep(1.0 - cov1, 1.0 - cov1 + 0.28, n) * upness;
                let lit2 = 0.75 + 0.25 * clamp(normalize(lighting.skySunDir.xyz).y, 0.0, 1.0);
                sky = mix(sky, lighting.skyCloudColor.xyz * lit2, m * 0.85);
            }
            let cov2 = lighting.skyClouds.w;
            if (cov2 > 0.001) {
                let p2 = plane * (2.0 * lighting.skyCloudColor.w) + drift * (time * speed * 1.7) + vec2<f32>(37.7, 11.3);
                let n2 = skyFbm(p2);
                let m2 = smoothstep(1.0 - cov2, 1.0 - cov2 + 0.22, n2) * upness;
                sky = mix(sky, lighting.skyCloudColor.xyz * 0.92, m2 * 0.55);
            }
        }
    }
    return vec4<f32>(sky, 1.0);
}
)";

} // namespace Enjin::Renderer::WebShaderData

#endif // ENJIN_PLATFORM_WEB
