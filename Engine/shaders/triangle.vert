#version 450

// Lit Mesh Vertex Shader with Shadow Support

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Push constant for per-object model matrix
layout(push_constant) uniform PushConstants {
    mat4 model;
} pushConstants;

// Uniform buffer for view/projection (shared across all objects)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Access lightSpaceMatrix from lighting UBO
// Must match C++ LightingUBO structure exactly
layout(binding = 1) uniform LightingUBO {
    vec3 ambientColor;
    float ambientIntensity;
    vec3 cameraPos;
    float _pad0;
    uint directionalLightCount;
    uint pointLightCount;
    uint spotLightCount;
    uint _pad1;
    mat4 lightSpaceMatrix;
    float shadowBias;
    int shadowEnabled;
    vec2 _shadowPad;
    // Note: light arrays follow but we only need lightSpaceMatrix in vertex shader
} lighting;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out vec4 fragPosLightSpace;

void main() {
    // Transform vertex position to world space
    vec4 worldPos = pushConstants.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    // Transform to clip space
    gl_Position = ubo.proj * ubo.view * worldPos;

    // Transform normal to world space (using normal matrix)
    mat3 normalMatrix = transpose(inverse(mat3(pushConstants.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    // Pass through UV
    fragUV = inUV;

    // Calculate position in light space for shadow mapping
    fragPosLightSpace = lighting.lightSpaceMatrix * worldPos;
}
