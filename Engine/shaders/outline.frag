#version 450

// Inverted-Hull Geometry Outline Fragment Shader
// Outputs solid outline color. MRT: color + zero velocity.

layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outVelocity;  // MRT velocity attachment (RG16F)

void main() {
    outColor = fragColor;
    outVelocity = vec2(0.0);  // No motion for outline geometry
}
