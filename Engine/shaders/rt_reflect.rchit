#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 reflectPayload;

void main() {
    // Use pre-baked simplified material to reduce hit shader divergence:
    // F0 and kDiffuse are already computed on the CPU, roughness is pre-clamped,
    // and emissive is pre-multiplied by emissiveStrength.
    RTSimplifiedMaterial smat = fetchSimplifiedMaterial(gl_InstanceCustomIndexEXT);

    // Approximate shading at reflection hit point
    // Use material base color modulated by a simple NdotL term
    float NdotL = max(dot(vec3(0.0, 1.0, 0.0), normalize(-gl_WorldRayDirectionEXT)), 0.0);

    // Use pre-baked F0 for reflection tint (encodes metallic/dielectric distinction)
    // For metals, F0 ~ baseColor; for dielectrics, F0 ~ 0.04
    vec3 reflectTint = mix(vec3(1.0), smat.f0, 1.0 - smat.kDiffuse);

    // Rougher surfaces produce dimmer, more diffuse reflections
    float roughnessFade = 1.0 - smat.effectiveRoughness * 0.5;

    vec3 hitColor = reflectTint * (0.3 + 0.7 * NdotL) * roughnessFade + smat.emissive;
    reflectPayload = vec4(hitColor, 1.0);
}
