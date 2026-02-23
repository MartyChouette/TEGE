#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 causticPayload;

void main() {
    // Hit — check if this is a transmissive surface
    // For now, contribute a basic caustic pattern based on hit geometry
    // The concentration of refracted rays creates brightness variation
    float causticIntensity = 0.3;
    vec3 causticColor = vec3(1.0, 1.0, 1.0);
    causticPayload = vec4(causticColor, causticIntensity);
}
