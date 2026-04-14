#version 450

// Visibility buffer pass — fragment shader
// Writes triangle ID (gl_PrimitiveID) and instance ID to an R32G32_UINT target.
// No material evaluation — shading happens in the resolve compute pass.

layout(location = 0) flat in uint inInstanceID;

layout(location = 0) out uvec2 outVisibility; // x = triangleID, y = instanceID

void main() {
    outVisibility = uvec2(uint(gl_PrimitiveID), inInstanceID);
}
