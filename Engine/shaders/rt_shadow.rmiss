#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float shadowPayload;

void main() {
    // Ray missed all geometry — fully lit (no shadow)
    shadowPayload = 1.0;
}
