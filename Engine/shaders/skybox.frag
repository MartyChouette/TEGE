#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Sky compositor: samples the cubemap (gradient bake, authored cubemap, or
// solid color) then layers live atmosphere on top - horizon haze, a sun disc
// with glow, and two FBM cloud layers drifting with the wind. All parameters
// default to zero, which reproduces the plain cubemap sample exactly.

layout(binding = 0) uniform SkyUBO {
    mat4 viewProj;        // consumed by skybox.vert (must stay first)
    vec4 sunDirTime;      // xyz sun direction, w wind clock
    vec4 sunColorSize;    // xyz sun color, w disc size
    vec4 cloudParams;     // x coverage1, y scale1, z speed, w coverage2
    vec4 cloudColorHaze;  // xyz cloud color, w horizon haze
    vec4 misc;            // x sun intensity, y scale2, z/w wind drift dir
    vec4 cloudExtra;      // x cloud softness, y custom cloud tex index (-1 none), zw reserved
} sky;
layout(binding = 1) uniform samplerCube skybox;

// Bindless set 1 (custom cloud texture; only sampled when cloudExtra.y >= 0).
// Mirrors sky2d.frag: luminance of the user image drives the cloud shape.
layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];

layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash21(i), b = hash21(i + vec2(1, 0));
    float c = hash21(i + vec2(0, 1)), d = hash21(i + vec2(1, 1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) { v += vnoise(p) * a; p = p * 2.03 + 17.0; a *= 0.5; }
    return v;
}
// Domain-warped FBM: fluffier, less grid-aligned clouds (matches the 2D sky).
float clouds(vec2 p) {
    vec2 q = vec2(fbm(p), fbm(p + vec2(5.2, 1.3)));
    vec2 r = vec2(fbm(p + 4.0 * q + vec2(1.7, 9.2)), fbm(p + 4.0 * q + vec2(8.3, 2.8)));
    return fbm(p + 4.0 * r);
}
// Cloud density at a drifted plane coord: the custom texture's luminance when
// one is set (tiled via fract, same as the 2D sky), procedural FBM otherwise.
float cloudDensity(vec2 p) {
    float texIdx = sky.cloudExtra.y;
    if (texIdx >= 0.0) {
        vec3 t = texture(sampler2D(bindlessTextures[nonuniformEXT(int(texIdx + 0.5))],
                                   bindlessSamplers[0]), fract(p)).rgb;
        return dot(t, vec3(0.299, 0.587, 0.114));
    }
    return clouds(p);
}

void main() {
    vec3 dir = normalize(fragTexCoord);
    vec3 col = texture(skybox, fragTexCoord).rgb;
    float time = sky.sunDirTime.w;

    // Horizon haze: a soft bright band hugging the horizon line
    float haze = sky.cloudColorHaze.w;
    if (haze > 0.001) {
        float band = pow(1.0 - clamp(abs(dir.y), 0.0, 1.0), 6.0);
        col = mix(col, mix(col, vec3(0.82, 0.86, 0.92), 0.6), band * haze);
    }

    // Sun disc + glow
    float sunI = sky.misc.x;
    if (sunI > 0.001) {
        vec3 sunDir = normalize(sky.sunDirTime.xyz);
        float d = max(dot(dir, sunDir), 0.0);
        float size = max(sky.sunColorSize.w, 0.001);
        float disc = smoothstep(1.0 - size, 1.0 - size * 0.35, d);
        float glow = pow(d, 48.0) * 0.35;
        col += sky.sunColorSize.xyz * (disc + glow) * sunI;
    }

    // Cloud layers: project the upper dome onto a plane; drift with the wind
    float upness = smoothstep(0.02, 0.18, dir.y);
    if (upness > 0.0) {
        vec2 windDrift = vec2(sky.misc.z, sky.misc.w);
        if (dot(windDrift, windDrift) < 1e-5) windDrift = vec2(1.0, 0.35);
        windDrift = normalize(windDrift);
        vec2 plane = dir.xz / (dir.y + 0.18);
        float speed = sky.cloudParams.z * 0.01;

        float edge = mix(0.02, 0.4, clamp(sky.cloudExtra.x, 0.0, 1.0));
        float cov1 = sky.cloudParams.x;
        if (cov1 > 0.001) {
            vec2 p = plane * (2.0 * sky.cloudParams.y) + windDrift * (time * speed);
            float n = cloudDensity(p);
            float m = smoothstep(1.0 - cov1, 1.0 - cov1 + edge, n) * upness;
            float lit = 0.75 + 0.25 * clamp(normalize(sky.sunDirTime.xyz).y, 0.0, 1.0);
            col = mix(col, sky.cloudColorHaze.xyz * lit, m * 0.85);
        }
        float cov2 = sky.cloudParams.w;
        if (cov2 > 0.001) {
            vec2 p2 = plane * (2.0 * sky.misc.y) + windDrift * (time * speed * 1.7) + vec2(37.7, 11.3);
            float n2 = cloudDensity(p2);
            float m2 = smoothstep(1.0 - cov2, 1.0 - cov2 + edge, n2) * upness;
            col = mix(col, sky.cloudColorHaze.xyz * 0.92, m2 * 0.55);
        }
    }

    outColor = vec4(col, 1.0);
}
