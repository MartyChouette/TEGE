#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 transPayload;

void main() {
    RTMaterial mat = fetchMaterial(gl_InstanceCustomIndexEXT);

    // Shading at refracted hit point using real material properties
    float NdotL = max(dot(vec3(0.0, 1.0, 0.0), normalize(-gl_WorldRayDirectionEXT)), 0.0);

    // Base shading from material color
    vec3 hitColor = mat.baseColor * (0.3 + 0.7 * NdotL) + mat.emissive;

    // Thickness-based absorption: thicker surfaces absorb more light
    if (mat.thickness > 0.0) {
        float absorption = exp(-gl_HitTEXT / max(mat.thickness, 0.001));
        hitColor *= absorption;
    }

    // SSS tint: subsurface scattering colors the transmitted light
    if (mat.sssIntensity > 0.0) {
        vec3 sssTint = mix(vec3(1.0), mat.sssColor, mat.sssIntensity);
        hitColor *= sssTint;
    }

    transPayload = vec4(hitColor, 1.0);
}
