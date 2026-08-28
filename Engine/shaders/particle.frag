#version 450

// Simple alpha particle fragment shader

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;
layout(location = 2) in vec3 fragColor;   // per-particle tint (emitter colour over life)

layout(location = 0) out vec4 outColor;

// Custom art-asset texture (bound at binding 3, same slot sprites use). Sampled only
// when flags bit 0 is set — the renderer binds the emitter's texture and sets the bit.
layout(binding = 3) uniform sampler2D baseColorTexture;

// Push constants for particle color and type
layout(push_constant) uniform PushConstants {
    mat4 model;       // unused, but layout must match pipeline
    vec3 baseColor;
    float metallic;   // reused: 0 = rain (full fill), 1 = snow (radial falloff)
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
    float parallaxScale;
    float _pad0;
    float _pad1;
    float _pad2;
} material;

void main() {
    float alpha = fragAlpha * material.opacity;
    vec3 col = fragColor;   // per-particle tint

    if ((material.flags & 1) != 0) {
        // Custom art asset: the emitter's texture supplies the shape (alpha) and image
        // colour, tinted by the per-particle colour.
        vec4 tex = texture(baseColorTexture, fragUV);
        col *= tex.rgb;
        alpha *= tex.a;
    } else if (material.metallic > 0.5) {
        // Built-in soft dot (no texture): radial falloff from centre.
        float dist = length(fragUV - vec2(0.5));
        alpha *= smoothstep(0.5, 0.2, dist);
    }

    if (alpha < 0.01) {
        discard;
    }

    outColor = vec4(col, alpha);
}
