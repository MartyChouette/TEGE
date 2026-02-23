#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 causticPayload;

void main() {
    // Miss — no caustic contribution (ray didn't hit refractive surface)
    causticPayload = vec4(0.0);
}
