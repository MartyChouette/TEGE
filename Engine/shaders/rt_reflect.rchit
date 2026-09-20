#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
// Needed for the real hit normal: rtHitWorldNormal reaches the vertex and index
// buffers through device addresses, which is what these two extensions are for.
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#define RT_USE_INSTANCE_GEOM
#include "rt_common.glsl"

layout(location = 0) rayPayloadInEXT vec4 reflectPayload;
hitAttributeEXT vec2 attribs;

void main() {
    // Use pre-baked simplified material to reduce hit shader divergence:
    // F0 and kDiffuse are already computed on the CPU, roughness is pre-clamped,
    // and emissive is pre-multiplied by emissiveStrength.
    RTSimplifiedMaterial smat = fetchSimplifiedMaterial(gl_InstanceCustomIndexEXT);

    // The REAL interpolated surface normal at the hit, the same way
    // rt_pathtrace.rchit gets it (binding 10 instance geometry table + buffer
    // device addresses, face-forward flipped).
    //
    // This used to be the literal constant vec3(0, 1, 0). Every reflected
    // surface shaded as though it faced the sky, so a reflection off a wall and
    // one off the floor came back identically lit and nothing in the image
    // responded to geometry.
    vec3 worldNormal = rtHitWorldNormal(attribs);

    // How square-on the reflection ray strikes the surface. Named for what it
    // IS: this pipeline has no light binding, so there is no light direction to
    // shade against here, and the previous code computed this same view term
    // while calling it NdotL. A grazing hit returns less, which is the right
    // direction for a reflection falloff even though it is not lighting.
    float viewFacing = max(dot(worldNormal, normalize(-gl_WorldRayDirectionEXT)), 0.0);

    // Use pre-baked F0 for reflection tint (encodes metallic/dielectric distinction)
    // For metals, F0 ~ baseColor; for dielectrics, F0 ~ 0.04
    vec3 reflectTint = mix(vec3(1.0), smat.f0, 1.0 - smat.kDiffuse);

    // Rougher surfaces produce dimmer, more diffuse reflections
    float roughnessFade = 1.0 - smat.effectiveRoughness * 0.5;

    vec3 hitColor = reflectTint * (0.3 + 0.7 * viewFacing) * roughnessFade + smat.emissive;
    reflectPayload = vec4(hitColor, 1.0);
}
