#version 450

// Shadow Map Depth-Only Vertex Shader
// Renders geometry from light's perspective for shadow map generation

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Light space transform UBO
layout(binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;  // Light's view-projection matrix
    mat4 model;             // Model matrix
} shadow;

void main() {
    gl_Position = shadow.lightSpaceMatrix * shadow.model * vec4(inPosition, 1.0);
}
