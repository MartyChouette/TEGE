#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 giPayload;

void main() {
    // Simple diffuse bounce — approximate albedo with gray
    vec3 hitAlbedo = vec3(0.5);
    // Approximate cosine-weighted irradiance from sky
    float NdotUp = max(gl_HitTEXT * 0.01, 0.3);
    giPayload = vec4(hitAlbedo * NdotUp, 1.0);
}
