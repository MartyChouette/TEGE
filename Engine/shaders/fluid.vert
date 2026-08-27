#version 450

// Fluid cell billboard vertex shader
// Reuses the same layout as particle.vert for pipeline compatibility

// Per-vertex (shared quad mesh)
layout(location = 0) in vec2 inQuadPos;   // [-0.5, 0.5] quad corners
layout(location = 1) in vec2 inQuadUV;    // [0, 1] UV coordinates

// Per-instance data
layout(location = 2) in vec3 inWorldPos;   // Cell world position
layout(location = 3) in vec2 inSizeAlpha;  // x = size, y = alpha
layout(location = 4) in vec2 inStretchDir; // fluid: per-cell colour RG
layout(location = 5) in float inStretch;   // fluid: per-cell colour B

// View/Projection UBO
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float fragAlpha;
layout(location = 2) out vec3 fragColor;   // per-cell fluid colour (from Fluid Volume's Color)

void main() {
    float size = inSizeAlpha.x;

    // Extract camera right and up vectors from view matrix
    vec3 camRight = vec3(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    vec3 camUp    = vec3(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);

    // Billboard: expand quad in world space using camera-aligned axes
    vec3 worldOffset = camRight * inQuadPos.x * size + camUp * inQuadPos.y * size;
    vec3 worldPos = inWorldPos + worldOffset;

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);

    fragUV = inQuadUV;
    fragAlpha = inSizeAlpha.y;
    fragColor = vec3(inStretchDir.x, inStretchDir.y, inStretch);
}
