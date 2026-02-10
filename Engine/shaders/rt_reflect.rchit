#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 reflectPayload;

void main() {
    // Simple shading at hit point — use base color with simple diffuse
    vec3 hitColor = vec3(0.5, 0.5, 0.5);  // Default gray
    float NdotL = max(dot(vec3(0.0, 1.0, 0.0), normalize(-gl_WorldRayDirectionEXT)), 0.0);
    reflectPayload = vec4(hitColor * (0.3 + 0.7 * NdotL), 1.0);
}
