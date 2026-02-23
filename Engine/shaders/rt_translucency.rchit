#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 transPayload;

void main() {
    // Simple shading at refracted hit point
    vec3 hitColor = vec3(0.6, 0.6, 0.6);
    float NdotL = max(dot(vec3(0.0, 1.0, 0.0), normalize(-gl_WorldRayDirectionEXT)), 0.0);
    transPayload = vec4(hitColor * (0.3 + 0.7 * NdotL), 1.0);
}
