#version 460
#extension GL_EXT_ray_tracing : require

// Primary ray miss — signal no hit (w <= 0)
layout(location = 0) rayPayloadInEXT vec4 pathPayload;

void main() {
    pathPayload = vec4(0.0, 0.0, 0.0, -1.0);
}
