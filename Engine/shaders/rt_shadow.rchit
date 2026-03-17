#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT float shadowPayload;

void main() {
    // Fetch material to check opacity — semi-transparent surfaces cast softer shadows
    MaterialGPU m = materials[gl_InstanceCustomIndexEXT];

    // Alpha mask mode (bits 8-9 of flags): 0=opaque, 1=mask, 2=blend
    int alphaMode = (m.flags >> 8) & 0x3;

    if (alphaMode == 1) {
        // Alpha mask: below cutoff is fully transparent (no shadow)
        shadowPayload = (m.opacity < m.alphaCutoff) ? 1.0 : 0.0;
    } else if (alphaMode == 2) {
        // Alpha blend: partial shadow based on opacity
        shadowPayload = 1.0 - m.opacity;
    } else {
        // Opaque: fully occluded
        shadowPayload = 0.0;
    }
}
