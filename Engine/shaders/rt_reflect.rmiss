#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 reflectPayload;

void main() {
    // Miss — store sky color in RGB, sentinel w = -2.0 so the rgen shader
    // can distinguish "BVH miss" from "closest-hit".  The rgen shader will
    // attempt SDF fallback when w == -2.0 and SDF is enabled; otherwise it
    // uses the sky color stored here.
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    vec3 skyColor = mix(vec3(0.5, 0.6, 0.7), vec3(0.2, 0.4, 0.8), t);
    reflectPayload = vec4(skyColor, -2.0);
}
