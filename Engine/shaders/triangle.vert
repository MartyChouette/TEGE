#version 450

// Lit Mesh Vertex Shader with Shadow Support and Retro Effects

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

// Push constant for per-object model matrix + material flags
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
layout(location = 4) out vec4 fragVertColor;
layout(location = 5) out float fragClipW;

// Retro flag bits (must match C++ Material.h)
#define FLAG_FLAT_SHADING     (1 << 20)
#define FLAG_AFFINE_TEXTURING (1 << 21)
#define FLAG_VERTEX_SNAPPING  (1 << 22)
#define FLAG_STIPPLE_TRANS    (1 << 23)

void main() {
    // Transform vertex position to world space
    vec4 worldPos = pushConstants.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;

    // Transform to clip space
    vec4 clipPos = ubo.proj * ubo.view * worldPos;

    // Vertex snapping (PS1-style): snap clip-space XY to a low-res grid
    if ((pushConstants.flags & FLAG_VERTEX_SNAPPING) != 0) {
        // Extract snap resolution from bits 24-31 (u8, 0 = disabled)
        int snapRes = (pushConstants.flags >> 24) & 0xFF;
        if (snapRes > 0) {
            float grid = float(snapRes);
            // Snap in clip space (after perspective divide)
            vec2 snapped = clipPos.xy / clipPos.w;           // NDC
            snapped = floor(snapped * grid + 0.5) / grid;    // snap to grid
            clipPos.xy = snapped * clipPos.w;                 // back to clip
        }
    }

    gl_Position = clipPos;

    // Transform normal to world space (using normal matrix)
    mat3 normalMatrix = transpose(inverse(mat3(pushConstants.model)));
    fragNormal = normalize(normalMatrix * inNormal);

    // Pass through UV - for affine texturing, multiply by w to undo perspective correction
    if ((pushConstants.flags & FLAG_AFFINE_TEXTURING) != 0) {
        fragUV = inUV * clipPos.w;
        fragClipW = clipPos.w;
    } else {
        fragUV = inUV;
        fragClipW = 1.0;  // no-op divisor in fragment shader
    }

    // Pass through vertex color
    fragVertColor = inColor;

    // Calculate position in light space for shadow mapping
    fragPosLightSpace = lighting.lightSpaceMatrix * worldPos;
}
