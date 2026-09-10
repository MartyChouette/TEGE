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
// Bindless slots for this instance's own art. -1 = none. Carrying the texture
// per instance is what lets every sprite in the scene ride one draw call.
layout(location = 8) in int inTexIndex;        // base colour, -1 = untextured
layout(location = 9) in int inNormalIndex;     // normal map, -1 = none
layout(location = 10) in vec2 inPivot;         // 0.5,0.5 = centred on the entity

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
layout(location = 5) flat out int fragTexIndex;
layout(location = 6) flat out int fragNormalIndex;

void main() {
    // Size first, THEN rotate. The other order rotates a unit quad and scales
    // the result along the world axes, which only permutes the corners of a
    // non-square sprite: a 10 x 2 bar turned 90 degrees stayed 10 wide and 2
    // tall instead of standing up. Measured, not reasoned about.
    //
    // The pivot offset goes in before the rotation as well, so a sprite turns
    // about its own pivot rather than about its centre. Nothing honoured pivot
    // in this path at all until now -- it was authored, saved, and read only by
    // the per-entity fallback that no scene takes.
    vec2 local = (inQuadPos + (vec2(0.5) - inPivot)) * inSize;

    float cosR = cos(inRotation);
    float sinR = sin(inRotation);
    vec2 rotated = vec2(
        local.x * cosR - local.y * sinR,
        local.x * sinR + local.y * cosR
    );

    vec3 worldPos = inPosition + vec3(rotated, 0.0);

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);

    // Compute UV from quad UV + instance UV rect + flip flags
    vec2 uv = inQuadUV;

    // Flip before mapping to UV rect
    if ((inFlipFlags & 1u) != 0u) uv.x = 1.0 - uv.x;
    if ((inFlipFlags & 2u) != 0u) uv.y = 1.0 - uv.y;

    // Map [0,1] quad UV to the sprite's UV sub-rect
    fragUV = vec2(
        mix(inUVRect.x, inUVRect.z, uv.x),  // left -> right
        // uv.y is 0 at the quad's BOTTOM (world -Y) and 1 at its top, while a
        // texture's v = 0 is its TOP row. Mapping 0 -> uvRect.top therefore put
        // the top of the image along the bottom of the sprite: every textured
        // sprite rendered upside down, on both backends, while the same texture
        // on a mesh came out the right way up. Nothing in the tree caught it
        // because the only sprite example uses untextured, tinted quads.
        mix(inUVRect.w, inUVRect.y, uv.y)   // bottom -> top
    );

    fragTintAlpha = inTintAlpha;
    fragTexIndex = inTexIndex;
    fragNormalIndex = inNormalIndex;

    // World-space outputs for lighting
    fragWorldPos = worldPos;

    // Sprite plane normal: (0, 0, 1) rotated by the sprite's Z rotation
    fragNormal = vec3(-sinR * 0.0, 0.0, 1.0);  // Z-facing normal (sprites face camera)
    fragNormal = normalize(vec3(0.0, 0.0, 1.0));

    // Tangent: right direction of the rotated sprite (for normal mapping TBN)
    fragTangent = vec4(cosR, sinR, 0.0, 1.0);
}
