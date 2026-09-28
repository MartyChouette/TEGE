#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Textured CPU particle fragment shader: the emitter's art asset from the
// bindless array, tinted by the per-particle colour.
//
// Split from particle.frag, which the fluid renderer shares and whose pipeline
// carries no bindless set. particle.frag took its texture from binding 3, one
// descriptor for the whole pass, so every textured emitter in a scene drew the
// first one's texture (H1). The CPU particle renderer now draws one run of
// instances per texture, each with its own bindless index.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in float fragAlpha;
layout(location = 2) in vec3 fragColor;   // per-particle tint (emitter colour over life)

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];

// Same block as the other particle shaders; surfaceParam1 carries the texture
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
    float texIndex;   // bindless index of this run's texture
    float _pad1;
    float _pad2;
} material;

void main() {
    float alpha = fragAlpha * material.opacity;
    vec4 tex = texture(sampler2D(bindlessTextures[nonuniformEXT(int(material.texIndex + 0.5))],
                                 bindlessSamplers[0]), fragUV);
    alpha *= tex.a;
    if (alpha < 0.01) {
        discard;
    }
    outColor = vec4(fragColor * tex.rgb, alpha);
}
