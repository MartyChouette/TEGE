#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 pathPayload;

void main() {
    // Return hit distance in w, approximate albedo in rgb
    vec3 albedo = vec3(0.6, 0.6, 0.6);  // Default gray — will be enhanced with material SSBO
    pathPayload = vec4(albedo, gl_HitTEXT);
}
