#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 pathPayload;

void main() {
    RTMaterial mat = fetchMaterial(gl_InstanceCustomIndexEXT);

    // Return hit albedo (with emissive baked into rgb) and hit distance in w
    vec3 color = mat.baseColor + mat.emissive;
    pathPayload = vec4(color, gl_HitTEXT);
}
