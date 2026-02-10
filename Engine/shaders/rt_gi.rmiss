#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 giPayload;

void main() {
    // Miss — sample sky radiance for GI
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    vec3 skyRadiance = mix(vec3(0.4, 0.45, 0.5), vec3(0.6, 0.8, 1.0), t) * 0.5;
    giPayload = vec4(skyRadiance, 0.0);
}
