// Enjin Engine — PBR shader (WebGPU / WGSL)
// Cook-Torrance BRDF with directional, point, and spot light shadows.

// Bind group 0: Per-frame
struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};

struct LightingUBO {
    lightDir: array<vec4<f32>, 8>,       // 0-3: dir directions, 4-7: point positions
    lightColor: array<vec4<f32>, 8>,     // rgb + intensity
    lightParams: array<vec4<f32>, 8>,    // point: range, linear, quadratic, constant
    ambientColor: vec4<f32>,             // rgb + intensity
    fogColor: vec4<f32>,                 // rgb + density
    fogParams: vec4<f32>,                // start, end, heightFalloff, unused
    shadowParams: vec4<f32>,             // strength, softness, distance, unused
    lightCount: vec4<f32>,               // x = dirCount, y = pointCount, z = spotCount
    // Spot lights
    spotPos: array<vec4<f32>, 4>,        // position.xyz, range.w
    spotDir: array<vec4<f32>, 4>,        // direction.xyz
    spotColor: array<vec4<f32>, 4>,      // color.rgb, intensity.w
    spotParams: array<vec4<f32>, 4>,     // innerCutoff.x, outerCutoff.y, linear.z, quadratic.w
};

@group(0) @binding(0) var<uniform> viewProj: ViewProjection;
@group(0) @binding(1) var<uniform> lighting: LightingUBO;

// Bind group 1: Per-object
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

// Bone matrices SSBO (skeletal animation)
struct BoneSSBO {
    matrices: array<mat4x4<f32>>,
};
@group(1) @binding(1) var<storage, read> bones: BoneSSBO;

// Bind group 2: Textures
@group(2) @binding(0) var baseColorTex: texture_2d<f32>;
@group(2) @binding(1) var baseColorSmp: sampler;
@group(2) @binding(2) var normalTex: texture_2d<f32>;
@group(2) @binding(3) var normalSmp: sampler;
@group(2) @binding(4) var mrTex: texture_2d<f32>;  // metallic-roughness
@group(2) @binding(5) var mrSmp: sampler;

// Bind group 3: Shadow mapping (directional + spot + point)
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

    var skinnedPos = in.position;
    var skinnedNormal = in.normal;
    var skinnedTangent = in.tangent.xyz;

    // Skeletal skinning (FLAG_SKINNED = bit 3)
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

    let world_pos = object.model * vec4<f32>(skinnedPos, 1.0);
    out.clip_position = viewProj.proj * viewProj.view * world_pos;
    out.world_pos = world_pos.xyz;

    let normal_mat = mat3x3<f32>(
        object.model[0].xyz,
        object.model[1].xyz,
        object.model[2].xyz
    );
    out.world_normal = normalize(normal_mat * skinnedNormal);
    out.world_tangent = normalize(normal_mat * skinnedTangent);
    out.world_bitangent = cross(out.world_normal, out.world_tangent) * in.tangent.w;
    out.uv = in.uv;
    return out;
}

// GGX/Trowbridge-Reitz NDF
fn distributionGGX(N: vec3<f32>, H: vec3<f32>, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let NdotH = max(dot(N, H), 0.0);
    let NdotH2 = NdotH * NdotH;
    let denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * denom * denom);
}

// Schlick-GGX geometry function
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

// Fresnel-Schlick
fn fresnelSchlick(cosTheta: f32, F0: vec3<f32>) -> vec3<f32> {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// Directional shadow map lookup with 4-tap PCF
fn sampleShadow(worldPos: vec3<f32>) -> f32 {
    let lightClip = shadowVP.proj * shadowVP.view * vec4<f32>(worldPos, 1.0);
    let ndc = lightClip.xyz / lightClip.w;
    let shadowUV = vec2<f32>(ndc.x * 0.5 + 0.5, ndc.y * -0.5 + 0.5);
    let depth = ndc.z;
    let bias = 0.002;
    let texelSize = 1.0 / 1024.0;
    var shadow = 0.0;
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>(-texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>( texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>(-texelSize,  texelSize), depth - bias);
    shadow += textureSampleCompare(shadowMap, shadowSampler, shadowUV + vec2<f32>( texelSize,  texelSize), depth - bias);
    shadow = shadow * 0.25;
    let inBounds = step(0.0, shadowUV.x) * step(shadowUV.x, 1.0) * step(0.0, shadowUV.y) * step(shadowUV.y, 1.0);
    return mix(1.0, shadow, inBounds);
}

// Spot light shadow lookup (perspective projection, 4-tap PCF)
fn sampleSpotShadowMap(worldPos: vec3<f32>, spotView: mat4x4<f32>, spotProj: mat4x4<f32>, shadowTex: texture_depth_2d) -> f32 {
    let lightClip = spotProj * spotView * vec4<f32>(worldPos, 1.0);
    let ndc = lightClip.xyz / lightClip.w;
    let shadowUV = vec2<f32>(ndc.x * 0.5 + 0.5, ndc.y * -0.5 + 0.5);
    let depth = ndc.z;
    let bias = 0.003;
    let texelSize = 1.0 / 512.0;
    var shadow = 0.0;
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>(-texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>( texelSize, -texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>(-texelSize,  texelSize), depth - bias);
    shadow += textureSampleCompare(shadowTex, shadowSampler, shadowUV + vec2<f32>( texelSize,  texelSize), depth - bias);
    shadow = shadow * 0.25;
    let inBounds = step(0.0, shadowUV.x) * step(shadowUV.x, 1.0) * step(0.0, shadowUV.y) * step(shadowUV.y, 1.0)
                 * step(0.0, depth) * step(depth, 1.0);
    return mix(1.0, shadow, inBounds);
}

// Point light shadow lookup (cubemap, comparison against linear depth)
fn samplePointShadow(worldPos: vec3<f32>, lightPos: vec3<f32>, range: f32) -> f32 {
    let fragToLight = worldPos - lightPos;
    let dist = length(fragToLight);
    let dir = fragToLight / dist;
    let refDepth = dist / range;  // normalize to [0,1] range
    let bias = 0.005;
    return textureSampleCompare(pointShadowCube, shadowSampler, dir, refDepth - bias);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Sample textures
    let baseColorSample = textureSample(baseColorTex, baseColorSmp, in.uv);
    let albedo = baseColorSample.rgb * object.baseColor;
    let alpha = baseColorSample.a * object.opacity;

    // Alpha cutoff (mask mode)
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

    // PBR lighting
    let F0_dielectric = vec3<f32>(0.04);
    let F0 = mix(F0_dielectric, albedo, metallic);

    var Lo = vec3<f32>(0.0);

    // Shadow lookup (applied to first directional light)
    let shadowFactor = sampleShadow(in.world_pos);
    let shadowStrength = lighting.shadowParams.x;

    // Directional lights (slots 0-3)
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

        // Apply shadow to first directional light
        var shadow = 1.0;
        if (i == 0) {
            shadow = mix(1.0, shadowFactor, shadowStrength);
        }
        Lo = Lo + (kD * albedo + specular) * radiance * NdotL * shadow;
    }

    // Point lights (slots 4-7)
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

        // Attenuation: 1 / (constant + linear*d + quadratic*d^2)
        let linAtt = lighting.lightParams[idx].y;
        let quadAtt = lighting.lightParams[idx].z;
        let constAtt = lighting.lightParams[idx].w;
        let attenuation = 1.0 / (constAtt + linAtt * dist + quadAtt * dist * dist);

        let radiance = lighting.lightColor[idx].rgb * lighting.lightColor[idx].w * attenuation;

        let NDF = distributionGGX(N, H, roughness);
        let G = geometrySmith(N, V, L, roughness);
        let F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        let numerator = NDF * G * F;
        let denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        let specular = numerator / denominator;

        let kD = (vec3<f32>(1.0) - F) * (1.0 - metallic);
        let NdotL = max(dot(N, L), 0.0);

        // Point shadow (first point light only)
        var ptShadow = 1.0;
        if (i == 0) {
            ptShadow = samplePointShadow(in.world_pos, lightPos, range);
        }
        Lo = Lo + (kD * albedo + specular) * radiance * NdotL * ptShadow;
    }

    // Spot lights
    let spotCount = i32(lighting.lightCount.z);
    for (var i = 0; i < spotCount; i = i + 1) {
        let lightPos = lighting.spotPos[i].xyz;
        let range = lighting.spotPos[i].w;
        let toLight = lightPos - in.world_pos;
        let dist = length(toLight);
        if (dist > range) { continue; }

        let L = normalize(toLight);
        let H = normalize(V + L);
        let spotDir = normalize(lighting.spotDir[i].xyz);

        // Cone falloff: smooth step between outer and inner cutoff
        let theta = dot(L, normalize(-spotDir));
        let innerCos = lighting.spotParams[i].x;
        let outerCos = lighting.spotParams[i].y;
        let epsilon = innerCos - outerCos;
        let spotFactor = clamp((theta - outerCos) / max(epsilon, 0.0001), 0.0, 1.0);

        // Distance attenuation
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

        // Spot shadow (first 2 spot lights)
        var spotShadow = 1.0;
        if (i == 0) {
            spotShadow = sampleSpotShadowMap(in.world_pos,
                spotShadowVPs.viewProj[0], spotShadowVPs.viewProj[1], spotShadowMap0);
        } else if (i == 1) {
            spotShadow = sampleSpotShadowMap(in.world_pos,
                spotShadowVPs.viewProj[2], spotShadowVPs.viewProj[3], spotShadowMap1);
        }
        Lo = Lo + (kD * albedo + specular) * radiance * NdotL * spotShadow;
    }

    // Ambient
    let ambient = lighting.ambientColor.rgb * lighting.ambientColor.w * albedo;

    // Emissive
    let emissive = object.emissiveColor * object.emissiveStrength;

    var color = ambient + Lo + emissive;

    // Reinhard tonemapping (approximates desktop post-process exposure)
    color = color / (color + vec3<f32>(1.0));
    color = pow(color, vec3<f32>(1.0 / 2.2));

    return vec4<f32>(color, alpha);
}
