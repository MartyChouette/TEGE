#version 450

// Lit Mesh Vertex Shader
// Supports position, normal, and UV attributes with lighting

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;

void main() {
    // Transform vertex position to world space
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    // Transform to clip space
    gl_Position = ubo.proj * ubo.view * worldPos;

    // Transform normal to world space (using normal matrix)
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    // Pass through UV
    fragUV = inUV;
}
