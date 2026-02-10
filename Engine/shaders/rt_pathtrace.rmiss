#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 pathPayload;

void main() {
    // Miss — signal no hit (w <= 0)
    pathPayload = vec4(0.0, 0.0, 0.0, -1.0);
}
