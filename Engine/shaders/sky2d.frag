#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// 2D scene sky: a full-screen authored backdrop drawn behind sprites in Scene2D.
// Screen-space (not ray-based like the 3D skybox, which needs a perspective
// camera) so it reads correctly under the ortho 2D camera: vertical gradient
// (top -> horizon -> bottom) plus two wind-drifted cloud layers and a horizon
// haze band. Clouds are domain-warped FBM by default; a custom cloud texture
// can drive the shape instead. Driven by SkyboxConfig via push constants.

layout(location = 0) in vec2 fragUV;   // 0..2 fullscreen triangle; y=0 top, y grows down
layout(location = 0) out vec4 outColor;

// Bindless set 1 (custom cloud texture; only sampled when cloudTexIndex >= 0)
layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];

layout(push_constant) uniform SkyPush {
    vec4 topColor;      // rgb = zenith, w = horizonHaze strength
    vec4 horizonColor;  // rgb = horizon band, w = cloudSpeed
    vec4 bottomColor;   // rgb = ground-arc, w = cloudCoverage (layer 1)
    vec4 cloudColor;    // rgb = cloud tint, w = cloud2Coverage (layer 2)
    vec4 scaleWind;     // x = cloudScale, y = cloud2Scale, zw = wind dir (xy)
    vec4 timeMisc;      // x = time, y = cloudSoftness, z = cloudTexIndex, w reserved
} sky;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 5; ++i) {   // one more octave than before for finer detail
        v += amp * noise(p);
        p = p * 2.0 + 17.0;
        amp *= 0.5;
    }
    return v;
}
// Domain-warped FBM: fluffier, less grid-aligned than plain FBM (Quilez trick).
float clouds(vec2 p) {
    vec2 q = vec2(fbm(p), fbm(p + vec2(5.2, 1.3)));
    vec2 r = vec2(fbm(p + 4.0 * q + vec2(1.7, 9.2)),
                  fbm(p + 4.0 * q + vec2(8.3, 2.8)));
    return fbm(p + 4.0 * r);
}

// Cloud mask for coverage c at a drifting UV, edge width set by softness.
float cloudMask(vec2 uv, float c, float softness, float texIndex) {
    float density;
    if (texIndex >= 0.0) {
        // Custom cloud texture: luminance drives the shape (tiled + drifting).
        vec3 t = texture(sampler2D(bindlessTextures[nonuniformEXT(int(texIndex + 0.5))],
                                   bindlessSamplers[0]), fract(uv)).rgb;
        density = dot(t, vec3(0.299, 0.587, 0.114));
    } else {
        density = clouds(uv);
    }
    float edge = mix(0.02, 0.4, clamp(softness, 0.0, 1.0));
    return smoothstep(1.0 - c, 1.0 - c + edge, density);
}

void main() {
    float t = clamp(fragUV.y * 0.5, 0.0, 1.0);   // fullscreen tri UV spans 0..2
    vec3 col;
    const float horizonAt = 0.62;
    if (t < horizonAt) {
        col = mix(sky.topColor.rgb, sky.horizonColor.rgb, t / horizonAt);
    } else {
        col = mix(sky.horizonColor.rgb, sky.bottomColor.rgb, (t - horizonAt) / (1.0 - horizonAt));
    }

    float haze = sky.topColor.w;
    if (haze > 0.0) {
        float band = exp(-abs(t - horizonAt) * 14.0);
        col = mix(col, sky.horizonColor.rgb * 1.15, band * haze);
    }

    vec2 wind = sky.scaleWind.zw;
    float time = sky.timeMisc.x;
    float softness = sky.timeMisc.y;
    float texIdx = sky.timeMisc.z;
    float above = smoothstep(horizonAt + 0.02, horizonAt - 0.35, t);
    if (above > 0.0) {
        vec2 uvBase = vec2(fragUV.x * 0.5, t);
        float cov1 = sky.bottomColor.w;
        if (cov1 > 0.0) {
            vec2 uv = uvBase * (2.5 * sky.scaleWind.x) + wind * time * 0.02 * sky.horizonColor.w;
            float m = cloudMask(uv, cov1, softness, texIdx);
            // Soft shading: denser cloud cores read slightly darker at the base
            col = mix(col, sky.cloudColor.rgb, m * above);
        }
        float cov2 = sky.cloudColor.w;
        if (cov2 > 0.0) {
            vec2 uv = uvBase * (5.0 * sky.scaleWind.y) + wind * time * 0.05 * sky.horizonColor.w + 11.3;
            float m = cloudMask(uv, cov2, softness, texIdx);
            col = mix(col, sky.cloudColor.rgb, m * above * 0.8);
        }
    }

    outColor = vec4(col, 1.0);
}
