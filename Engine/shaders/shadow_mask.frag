#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Shadow Map Alpha-Cutout Fragment Shader
// Depth-only pass with a discard: masked materials sample their base color
// alpha from the bindless array so cutout shapes (foliage cards, hair) cast
// shaped shadows instead of full rectangles. No color outputs.

layout(location = 0) in vec2 fragUV;

// Bindless texture array (set 1) — texIndex chosen per draw by RenderEntityShadow
layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor;
    float metallic;
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    uint flags;
    float parallaxScale;
    float texIndex;   // base color bindless index, -1 = keep every fragment
    float _pad1;
    float _pad2;
} material;

void main() {
    if (material.texIndex < 0.0) {
        return;
    }
    float alpha = texture(sampler2D(bindlessTextures[nonuniformEXT(int(material.texIndex + 0.5))],
                                    bindlessSamplers[0]), fragUV).a * material.opacity;
    if (alpha < material.alphaCutoff) {
        discard;
    }
}
