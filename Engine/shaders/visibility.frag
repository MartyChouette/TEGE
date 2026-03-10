#version 450

// Visibility buffer fragment shader — writes triangle ID + instance/object ID.
// Output: R32G32_UINT where:
//   .r = triangle ID (gl_PrimitiveID)
//   .g = object/instance ID (from vertex shader)

layout(location = 0) flat in uint inObjectID;

layout(location = 0) out uvec2 outVisibility;

void main() {
    // Pack triangle ID and object ID
    outVisibility.r = uint(gl_PrimitiveID);
    outVisibility.g = inObjectID;
}
