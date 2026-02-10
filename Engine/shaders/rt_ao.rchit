#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT float aoPayload;

void main() {
    // Ray hit — occluded (AO = 0.0)
    aoPayload = 0.0;
}
