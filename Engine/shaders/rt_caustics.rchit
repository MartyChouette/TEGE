#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 causticPayload;

void main() {
    RTMaterial mat = fetchMaterial(gl_InstanceCustomIndexEXT);

    // Only transmissive surfaces produce caustics
    if (mat.transmission < 0.01) {
        // Opaque surface — no caustic contribution
        causticPayload = vec4(0.0);
        return;
    }

    // Caustic intensity scales with transmission and IOR deviation from 1.0
    // Higher IOR = more refraction = stronger caustic concentration
    float iorFactor = abs(mat.ior - 1.0);
    float causticIntensity = mat.transmission * clamp(iorFactor * 2.0, 0.0, 1.0);

    // Tint caustics by the material's base color (e.g., colored glass)
    vec3 causticColor = mat.baseColor;

    causticPayload = vec4(causticColor, causticIntensity);
}
