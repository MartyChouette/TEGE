#version 450

// Lit 2D sprite vertex shader — instanced billboards with world-space outputs for lighting
// Same instanced input as sprite.vert, additionally outputs world position, normal, and tangent

// Per-vertex (shared quad mesh: [-0.5, 0.5] positions + [0, 1] UVs)
layout(location = 0) in vec2 inQuadPos;
layout(location = 1) in vec2 inQuadUV;

// Per-instance data
layout(location = 2) in vec3 inPosition;      // World position
layout(location = 3) in vec2 inSize;           // Width, Height in world units
layout(location = 4) in float inRotation;      // Z-axis rotation in radians
layout(location = 5) in vec4 inUVRect;         // left, top, right, bottom
layout(location = 6) in vec4 inTintAlpha;      // rgb = tint, a = alpha
layout(location = 7) in uint inFlipFlags;      // bit 0 = flipX, bit 1 = flipY

// View/Projection UBO (same binding as main pipeline)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragTintAlpha;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragNormal;
layout(location = 4) out vec4 fragTangent;

void main() {
    // Apply Z-rotation to quad position
    float cosR = cos(inRotation);
    float sinR = sin(inRotation);
    vec2 rotated = vec2(
        inQuadPos.x * cosR - inQuadPos.y * sinR,
        inQuadPos.x * sinR + inQuadPos.y * cosR
    );

    // Scale by sprite size
    vec3 worldPos = inPosition + vec3(rotated * inSize, 0.0);

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);

    // Compute UV from quad UV + instance UV rect + flip flags
    vec2 uv = inQuadUV;

    // Flip before mapping to UV rect
    if ((inFlipFlags & 1u) != 0u) uv.x = 1.0 - uv.x;
    if ((inFlipFlags & 2u) != 0u) uv.y = 1.0 - uv.y;

    // Map [0,1] quad UV to the sprite's UV sub-rect
    fragUV = vec2(
        mix(inUVRect.x, inUVRect.z, uv.x),  // left -> right
        mix(inUVRect.y, inUVRect.w, uv.y)   // top -> bottom
    );

    fragTintAlpha = inTintAlpha;

    // World-space outputs for lighting
    fragWorldPos = worldPos;

    // Sprite plane normal: (0, 0, 1) rotated by the sprite's Z rotation
    fragNormal = vec3(-sinR * 0.0, 0.0, 1.0);  // Z-facing normal (sprites face camera)
    fragNormal = normalize(vec3(0.0, 0.0, 1.0));

    // Tangent: right direction of the rotated sprite (for normal mapping TBN)
    fragTangent = vec4(cosR, sinR, 0.0, 1.0);
}
