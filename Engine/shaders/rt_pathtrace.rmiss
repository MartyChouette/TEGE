#version 460
#extension GL_EXT_ray_tracing : require

// Primary ray miss — signal no hit (colorDist.w <= 0)
struct PathPayload {
    vec4 colorDist;
    vec4 normalMat;
};
layout(location = 0) rayPayloadInEXT PathPayload pathPayload;

void main() {
    pathPayload.colorDist = vec4(0.0, 0.0, 0.0, -1.0);
}
