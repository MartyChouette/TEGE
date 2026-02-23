#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 transPayload;

void main() {
    // Miss — refracted ray exits into sky/background
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    vec3 skyColor = mix(vec3(0.5, 0.6, 0.7), vec3(0.2, 0.4, 0.8), t);
    transPayload = vec4(skyColor, 0.0);
}
