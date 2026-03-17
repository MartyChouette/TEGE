#version 450

// Visibility buffer vertex shader — minimal transform + ID passthrough.
// No material evaluation, no lighting — just position and ID output.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

// Push constants: model-view-projection matrix + object ID
layout(push_constant) uniform PushConstants {
    mat4 mvp;         // 64 bytes
    uint objectID;    // 4 bytes
    uint _pad0;       // 4 bytes
    uint _pad1;       // 4 bytes
    uint _pad2;       // 4 bytes
};

layout(location = 0) flat out uint outObjectID;

void main() {
    gl_Position = mvp * vec4(inPosition, 1.0);
    outObjectID = objectID;
}
