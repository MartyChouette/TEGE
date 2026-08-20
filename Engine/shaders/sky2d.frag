#version 450

// 2D scene sky: a full-screen authored backdrop drawn behind sprites in Scene2D.
// Screen-space (not ray-based like the 3D skybox, which needs a perspective
// camera) so it reads correctly under the ortho 2D camera: vertical gradient
// (top -> horizon -> bottom) plus two wind-drifted FBM cloud layers and a
// horizon haze band. Driven by the scene's SkyboxConfig via push constants.

layout(location = 0) in vec2 fragUV;   // 0..2 fullscreen triangle; y=0 top, y grows down
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform SkyPush {
    vec4 topColor;      // rgb = zenith, w = horizonHaze strength
    vec4 horizonColor;  // rgb = horizon band, w = cloudSpeed
    vec4 bottomColor;   // rgb = ground-arc, w = cloudCoverage (layer 1)
    vec4 cloudColor;    // rgb = cloud tint, w = cloud2Coverage (layer 2)
    vec4 scaleWind;     // x = cloudScale, y = cloud2Scale, zw = wind dir (xy)
    vec4 timeMisc;      // x = time seconds, yzw reserved
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
    for (int i = 0; i < 4; ++i) {
        v += amp * noise(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return v;
}

void main() {
    // Vertical gradient over the screen: top -> horizon (~62% down) -> bottom
    float t = clamp(fragUV.y * 0.5, 0.0, 1.0);   // fullscreen tri UV spans 0..2
    vec3 col;
    const float horizonAt = 0.62;
    if (t < horizonAt) {
        col = mix(sky.topColor.rgb, sky.horizonColor.rgb, t / horizonAt);
    } else {
        col = mix(sky.horizonColor.rgb, sky.bottomColor.rgb, (t - horizonAt) / (1.0 - horizonAt));
    }

    // Horizon haze: bright band hugging the horizon line
    float haze = sky.topColor.w;
    if (haze > 0.0) {
        float band = exp(-abs(t - horizonAt) * 14.0);
        col = mix(col, sky.horizonColor.rgb * 1.15, band * haze);
    }

    // Cloud layers drift along the wind heading; fade out below the horizon
    vec2 wind = sky.scaleWind.zw;
    float time = sky.timeMisc.x;
    float above = smoothstep(horizonAt + 0.02, horizonAt - 0.35, t);
    if (above > 0.0) {
        vec2 uvBase = vec2(fragUV.x * 0.5, t);
        float cov1 = sky.bottomColor.w;
        if (cov1 > 0.0) {
            vec2 uv = uvBase * (3.0 * sky.scaleWind.x) + wind * time * 0.02 * sky.horizonColor.w;
            float c = smoothstep(1.0 - cov1, 1.0, fbm(uv));
            col = mix(col, sky.cloudColor.rgb, c * above);
        }
        float cov2 = sky.cloudColor.w;
        if (cov2 > 0.0) {
            vec2 uv = uvBase * (6.0 * sky.scaleWind.y) + wind * time * 0.05 * sky.horizonColor.w;
            float c = smoothstep(1.0 - cov2, 1.0, fbm(uv + 11.3));
            col = mix(col, sky.cloudColor.rgb, c * above * 0.8);
        }
    }

    outColor = vec4(col, 1.0);
}
