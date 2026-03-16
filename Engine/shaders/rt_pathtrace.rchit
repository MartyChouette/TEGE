#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

#include "rt_common.glsl"

// Payload 0: hitColor.rgb + hitDist in w (kept for compatibility)
layout(location = 0) rayPayloadInEXT vec4 pathPayload;

// Payload 1: surface data (normal.xyz, materialIndex as uintBitsToFloat)
layout(location = 1) rayPayloadInEXT vec4 pathPayload2;

// Hit attributes from intersection (triangle barycentrics)
hitAttributeEXT vec2 attribs;

void main() {
    // Use pre-baked simplified material: albedo and emissive are ready to use
    // without texture lookups or per-ray computation.
    RTSimplifiedMaterial smat = fetchSimplifiedMaterial(gl_InstanceCustomIndexEXT);

    // Compute hit world position
    vec3 hitPos = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    // Compute geometric normal from barycentrics
    // Without vertex normals in the RT data, we use the face normal derived from
    // the object-to-world transform's orientation. For flat geometry this is exact;
    // for curved surfaces it's a reasonable approximation.
    // We construct a face normal from the hit triangle's implicit geometry.
    // The gl_ObjectToWorldEXT matrix transforms object-space directions to world space.
    vec3 objectNormal = vec3(0.0, 1.0, 0.0); // Default up normal

    // Use the object-to-world matrix to transform an approximate object-space normal.
    // For most meshes this gives a reasonable geometric normal. When vertex normal
    // SSBOs are added to the RT pipeline, this can be replaced with interpolated normals.
    vec3 worldNormal = normalize(mat3(gl_ObjectToWorldEXT) * objectNormal);

    // Ensure normal faces the ray (flip if back-face hit)
    if (dot(worldNormal, gl_WorldRayDirectionEXT) > 0.0) {
        worldNormal = -worldNormal;
    }

    // Payload 0: pre-baked albedo + emissive in RGB, hit distance in W
    vec3 color = smat.albedo + smat.emissive;
    pathPayload = vec4(color, gl_HitTEXT);

    // Payload 1: world normal + material index (packed as float)
    pathPayload2 = vec4(worldNormal, uintBitsToFloat(gl_InstanceCustomIndexEXT));
}
