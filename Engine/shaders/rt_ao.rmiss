#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float aoPayload;

void main() {
    // Ray missed — unoccluded (full AO = 1.0)
    aoPayload = 1.0;
}
