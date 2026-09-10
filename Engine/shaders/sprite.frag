#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Simple 2D sprite fragment shader — texture + tint, no lighting

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragTintAlpha;
layout(location = 2) flat in int fragTexIndex;

layout(location = 0) out vec4 outColor;

// Bindless texture array (set 1) — a sprite's art is addressed by an index it
// carries in its own instance data, so a scene using twenty different images
// still draws in one call. Binding a shared descriptor between per-texture
// draws cannot work: the rebinds happen while the command buffer is RECORDED
// and the draws do not run until it is submitted, so every draw sampled
// whatever the last one bound.
layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];
#define SPRITE_TEX(idx) sampler2D(bindlessTextures[nonuniformEXT(idx)], bindlessSamplers[0])

// Push constants — minimal, just need to match pipeline layout
layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float metallic;
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
    // An untextured sprite is a flat tinted quad, so white is the identity here.
    vec4 texColor = vec4(1.0);
    if (fragTexIndex >= 0) texColor = texture(SPRITE_TEX(fragTexIndex), fragUV);

    vec3 color = texColor.rgb * fragTintAlpha.rgb;
    float alpha = texColor.a * fragTintAlpha.a;

    // Alpha discard
    if (alpha < 0.01) {
        discard;
    }

    outColor = vec4(color, alpha);
}
