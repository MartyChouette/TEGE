#version 450

// Billboard particle vertex shader
// Quad mesh (4 verts) + per-instance data (position, size, alpha, stretch)

// Per-vertex (shared quad mesh)
layout(location = 0) in vec2 inQuadPos;   // [-0.5, 0.5] quad corners
layout(location = 1) in vec2 inQuadUV;    // [0, 1] UV coordinates

// Per-instance data
layout(location = 2) in vec3 inWorldPos;   // Particle world position
layout(location = 3) in vec2 inSizeAlpha;  // x = size, y = alpha
layout(location = 4) in vec2 inStretchDir; // xy = screen-space stretch direction (for rain elongation)
layout(location = 5) in float inStretch;   // Stretch factor (1.0 = no stretch)

// View/Projection UBO
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out float fragAlpha;

void main() {
    float size = inSizeAlpha.x;
    float alpha = inSizeAlpha.y;

    // Extract camera right and up vectors from view matrix
    vec3 camRight = vec3(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    vec3 camUp    = vec3(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);

    // Apply stretch: elongate the quad along the stretch direction in billboard space
    vec2 localPos = inQuadPos;
    localPos.y *= max(inStretch, 1.0); // Stretch vertically for rain

    // Billboard: expand quad in world space using camera-aligned axes
    vec3 worldOffset = camRight * localPos.x * size + camUp * localPos.y * size;
    vec3 worldPos = inWorldPos + worldOffset;

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);

    fragUV = inQuadUV;
    fragAlpha = alpha;
}
