#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 giPayload;

void main() {
    RTMaterial mat = fetchMaterial(gl_InstanceCustomIndexEXT);

    // GI bounce: use material albedo for diffuse energy transfer
    vec3 hitAlbedo = mat.baseColor;

    // Approximate cosine-weighted irradiance contribution
    float NdotUp = max(gl_HitTEXT * 0.01, 0.3);

    // Add emissive contribution — emissive surfaces act as indirect light sources
    vec3 color = hitAlbedo * NdotUp + mat.emissive;
    giPayload = vec4(color, 1.0);
}
