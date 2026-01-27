#version 450

// Lit Mesh Fragment Shader with Blinn-Phong lighting

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

// Lighting UBO
layout(binding = 1) uniform LightingUBO {
    vec3 ambientColor;
    float ambientIntensity;
    vec3 cameraPos;
    float _pad0;
    vec3 lightDir;      // Directional light direction (towards light)
    float lightIntensity;
    vec3 lightColor;
    float _pad1;
} lighting;

// Material properties (hardcoded for now)
const vec3 materialColor = vec3(0.8, 0.8, 0.8);
const float shininess = 32.0;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(lighting.cameraPos - fragWorldPos);

    // Ambient
    vec3 ambient = lighting.ambientColor * lighting.ambientIntensity;

    // Directional light
    vec3 lightDirection = normalize(lighting.lightDir);

    // Diffuse (Lambert)
    float diff = max(dot(normal, lightDirection), 0.0);
    vec3 diffuse = diff * lighting.lightColor * lighting.lightIntensity;

    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDirection + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specular = spec * lighting.lightColor * lighting.lightIntensity * 0.5;

    // Combine
    vec3 result = (ambient + diffuse + specular) * materialColor;

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    outColor = vec4(result, 1.0);
}
