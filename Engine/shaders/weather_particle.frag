#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Weather particle fragment shader: procedural rain/snow plus optional custom
// sprite from the bindless array. Split from particle.frag because that shader
// is shared by the CPU particle and fluid renderers, whose pipelines don't
// carry the bindless set-1 layout.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;

layout(location = 0) out vec4 outColor;

// Bindless texture array (set 1) — used when texIndex >= 0 (custom rain/snow sprite)
layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];

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
    float texIndex;   // custom sprite bindless index, -1 = procedural
    float _pad1;
    float _pad2;
} material;

void main() {
    float alpha = fragAlpha * material.opacity;

    if (material.texIndex >= 0.0) {
        // Custom sprite: the image supplies shape and tint. Quad UV has V=0 at
        // the bottom; flip so the sprite reads upright.
        vec4 tex = texture(sampler2D(bindlessTextures[nonuniformEXT(int(material.texIndex + 0.5))],
                                     bindlessSamplers[0]), vec2(fragUV.x, 1.0 - fragUV.y));
        alpha *= tex.a;
        if (alpha < 0.01) {
            discard;
        }
        outColor = vec4(tex.rgb, alpha);
        return;
    }

    // Snow: radial falloff from center
    if (material.metallic > 0.5) {
        float dist = length(fragUV - vec2(0.5));
        alpha *= smoothstep(0.5, 0.2, dist);
    }

    if (alpha < 0.01) {
        discard;
    }

    outColor = vec4(material.baseColor, alpha);
}
