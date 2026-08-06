#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference_uvec2 : require

#define RT_USE_INSTANCE_GEOM
#include "rt_common.glsl"

// Single incoming payload (VUID-04700: one IncomingRayPayloadKHR per entry point).
// colorDist: hitColor.rgb + hitDist in w (w <= 0 = miss)
// normalMat: surface normal.xyz + materialIndex as uintBitsToFloat in w
struct PathPayload {
    vec4 colorDist;
    vec4 normalMat;
};
layout(location = 0) rayPayloadInEXT PathPayload pathPayload;

// Hit attributes from intersection (triangle barycentrics)
hitAttributeEXT vec2 attribs;

void main() {
    // Use pre-baked simplified material: albedo and emissive are ready to use
    // without texture lookups or per-ray computation.
    RTSimplifiedMaterial smat = fetchSimplifiedMaterial(gl_InstanceCustomIndexEXT);

    // Compute hit world position
    vec3 hitPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    // Real interpolated vertex normal from the hit triangle (binding 10 instance
    // geometry table + buffer device addresses), face-forward flipped
    vec3 worldNormal = rtHitWorldNormal(attribs);

    // Pre-baked albedo + emissive in RGB, hit distance in W
    vec3 color = smat.albedo + smat.emissive;
    pathPayload.colorDist = vec4(color, gl_HitTEXT);

    // World normal + material index (packed as float)
    pathPayload.normalMat = vec4(worldNormal, uintBitsToFloat(gl_InstanceCustomIndexEXT));
}
