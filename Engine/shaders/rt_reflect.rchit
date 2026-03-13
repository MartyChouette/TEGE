#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 reflectPayload;

void main() {
    RTMaterial mat = fetchMaterial(gl_InstanceCustomIndexEXT);

    // Approximate shading at reflection hit point
    // Use material base color modulated by a simple NdotL term
    float NdotL = max(dot(vec3(0.0, 1.0, 0.0), normalize(-gl_WorldRayDirectionEXT)), 0.0);

    // Metallic surfaces reflect their base color; dielectrics reflect white
    vec3 reflectTint = mix(vec3(1.0), mat.baseColor, mat.metallic);

    // Rougher surfaces produce dimmer, more diffuse reflections
    float roughnessFade = 1.0 - mat.roughness * 0.5;

    vec3 hitColor = reflectTint * (0.3 + 0.7 * NdotL) * roughnessFade + mat.emissive;
    reflectPayload = vec4(hitColor, 1.0);
}
