#version 450

// Gaussian falloff in sigma space; the quad spans +-3 sigma.
layout(location = 0) in vec2 sigmaPos;
layout(location = 1) in vec4 fragColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outVelocity;   // MRT rule: main pass has 2 attachments

void main() {
    float power = -0.5 * dot(sigmaPos, sigmaPos);
    float alpha = fragColor.a * exp(power);
    if (alpha < 1.0 / 255.0) discard;
    outColor = vec4(fragColor.rgb, alpha);
    outVelocity = vec4(0.0);   // write mask is 0 on the velocity attachment anyway
}
