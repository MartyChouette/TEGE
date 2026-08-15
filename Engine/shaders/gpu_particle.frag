#version 450

// GPU-compute particle fragment shader: soft radial sprite tinted by the
// per-particle color the simulation interpolates (start -> end incl alpha).
// Writes only location 0; the main pass's velocity attachment has its write
// mask disabled for extra attachments by the pipeline config.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Soft circular falloff from the sprite center
    float dist = length(fragUV - vec2(0.5));
    float falloff = smoothstep(0.5, 0.15, dist);

    float alpha = fragColor.a * falloff;
    if (alpha < 0.01) {
        discard;
    }
    outColor = vec4(fragColor.rgb, alpha);
}
