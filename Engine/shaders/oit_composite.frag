#version 450

// OIT Composite Fragment Shader
// Reads accumulation (RGBA16F) and revealage (R8) textures from the
// weighted blended OIT transparent pass and composites the result.
//
// Formula (McGuire & Bavoil 2013):
//   avgColor = accumulation.rgb / max(accumulation.a, 1e-5)
//   outColor = vec4(avgColor, 1.0 - revealage)
//
// Blended over the opaque scene using standard alpha blending.

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D accumTexture;
layout(set = 0, binding = 1) uniform sampler2D revealTexture;

void main() {
    vec4 accum = texture(accumTexture, fragUV);
    float reveal = texture(revealTexture, fragUV).r;

    // Fully transparent — no transparent geometry here
    if (reveal >= 1.0)
        discard;

    // Recover average color from weighted sum
    vec3 avgColor = accum.rgb / max(accum.a, 1e-5);

    outColor = vec4(avgColor, 1.0 - reveal);
}
