#version 450

// Player hybrid RT apply pass. The player renders the scene straight to the
// swapchain with no offscreen target, so the editor's post-process overlay
// (which samples the scene color) cannot be reused. Instead this runs as two
// fullscreen draws inside the open swapchain pass and blends onto the scene via
// fixed-function ROP blending, never reading the framebuffer in-shader:
//   mode 0 (multiply blend): output shadow*AO   -> scene *= shadow*AO
//   mode 1 (additive blend): output reflect+GI   -> scene += reflect+GI
// Screen-space aligned with the scene (same camera), sampled at fragUV.

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D rtShadow;   // r: 0 = shadowed, 1 = lit
layout(binding = 1) uniform sampler2D rtAO;       // r: 0 = occluded, 1 = open
layout(binding = 2) uniform sampler2D rtReflect;  // rgb: reflection radiance
layout(binding = 3) uniform sampler2D rtGI;       // rgb: indirect GI radiance

layout(push_constant) uniform PushConstants {
    uint mode;            // 0 = multiply (shadow*AO), 1 = additive (reflect+GI)
    float shadowStrength;
    float aoStrength;
    float reflectStrength;
    float giStrength;
} pc;

void main() {
    if (pc.mode == 0u) {
        float s = mix(1.0, texture(rtShadow, fragUV).r, pc.shadowStrength);
        float a = mix(1.0, texture(rtAO, fragUV).r, pc.aoStrength);
        outColor = vec4(vec3(s * a), 1.0);
    } else {
        vec3 c = texture(rtReflect, fragUV).rgb * pc.reflectStrength
               + texture(rtGI, fragUV).rgb * pc.giStrength;
        outColor = vec4(c, 1.0);
    }
}
