#version 450

// Lit Mesh Fragment Shader with Blinn-Phong lighting and material support

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

// Material UBO
layout(binding = 2) uniform MaterialUBO {
    vec3 baseColor;
    float metallic;
    vec3 emissiveColor;
    float roughness;
    float emissiveStrength;
    float opacity;
    float alphaCutoff;
    int flags;
} material;

void main() {
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(lighting.cameraPos - fragWorldPos);

    // Material properties
    vec3 albedo = material.baseColor;
    float metallic = material.metallic;
    float roughness = material.roughness;

    // Convert roughness to shininess for Blinn-Phong
    float shininess = max(2.0, (2.0 / (roughness * roughness + 0.0001)) - 2.0);
    shininess = clamp(shininess, 2.0, 256.0);

    // Ambient
    vec3 ambient = lighting.ambientColor * lighting.ambientIntensity;

    // Directional light
    vec3 lightDirection = normalize(lighting.lightDir);

    // Diffuse (Lambert)
    float diff = max(dot(normal, lightDirection), 0.0);
    vec3 diffuse = diff * lighting.lightColor * lighting.lightIntensity;

    // Specular (Blinn-Phong) - metallic surfaces have colored specular
    vec3 halfwayDir = normalize(lightDirection + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
    vec3 specularColor = mix(vec3(0.04), albedo, metallic);
    vec3 specular = spec * specularColor * lighting.lightColor * lighting.lightIntensity;

    // Combine lighting with albedo
    vec3 result = ambient * albedo + diffuse * albedo * (1.0 - metallic) + specular;

    // Add emission
    result += material.emissiveColor * material.emissiveStrength;

    // Gamma correction
    result = pow(result, vec3(1.0 / 2.2));

    // Alpha handling
    float alpha = material.opacity;
    int alphaMode = (material.flags >> 8) & 0x3;
    if (alphaMode == 1) { // Mask mode
        if (alpha < material.alphaCutoff) {
            discard;
        }
        alpha = 1.0;
    }

    outColor = vec4(result, alpha);
}
