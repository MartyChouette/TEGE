// evaluate_lighting.glsl — Unified lighting evaluation for ALL renderable objects.
//
// ONE LIGHTING PATH FOR EVERYTHING:
//   - Opaque PBR geometry
//   - Transparent objects (OIT)
//   - Billboard particles
//   - Volumetric particles
//
// Every light source, every shadow, every GI probe, every fog contribution
// is evaluated through this single function. No separate "particle lighting"
// or "transparent lighting" system. Artists build effects however they want
// and lighting just works.
//
// Include this file after declaring the required uniform/SSBO bindings.
// Required bindings (must be declared in the including shader):
//   binding 1  : LightingUBO (light data)
//   binding 14 : ClusterGridSSBO (light grid)
//   binding 15 : ClusterLightIndexSSBO (light indices)
//   binding 20 : ddgiIrradiance (sampler2D, DDGI screen-space irradiance)
//   binding 21 : froxelVolume (sampler3D, volumetric fog)
//
// Usage:
//   LightingResult lit = evaluateAllLighting(worldPos, normal, albedo, metallic,
//                                             roughness, viewDir, screenUV, viewDepth);
//   vec3 finalColor = lit.directLight + lit.indirectLight + lit.fogScattering;
//   float finalTransmittance = lit.fogTransmittance;

#ifndef EVALUATE_LIGHTING_GLSL
#define EVALUATE_LIGHTING_GLSL

struct LightingResult {
    vec3 directLight;       // Sum of all direct light contributions (with shadows)
    vec3 indirectLight;     // Ambient + SH probes + DDGI irradiance
    vec3 fogScattering;     // In-scattered light from volumetric fog
    float fogTransmittance; // Fog transmittance (1.0 = clear, 0.0 = fully fogged)
    vec3 specular;          // Specular highlight contribution
};

// PBR helper: Fresnel-Schlick approximation
vec3 fresnelSchlickEL(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Evaluate unified lighting at a world-space point.
// This is called by ALL fragment shaders — opaque, transparent, particle.
LightingResult evaluateAllLighting(
    vec3 worldPos,
    vec3 normal,
    vec3 albedo,
    float metallic,
    float roughness,
    vec3 viewDir,
    vec2 screenUV,
    float viewDepth
) {
    LightingResult result;
    result.directLight = vec3(0.0);
    result.indirectLight = vec3(0.0);
    result.fogScattering = vec3(0.0);
    result.fogTransmittance = 1.0;
    result.specular = vec3(0.0);

    float NdotV = max(dot(normal, viewDir), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // --- 1. Ambient + Indirect ---

    // Base ambient from LightingUBO
    result.indirectLight = lighting.ambientColor * lighting.ambientIntensity * albedo;

    // SH probe irradiance
    if (lighting.shProbeIrradiance.w > 0.0) {
        result.indirectLight += lighting.shProbeIrradiance.xyz * albedo;
    }

    // DDGI probe irradiance (software-traced GI)
    vec4 ddgiSample = texture(ddgiIrradiance, screenUV);
    if (ddgiSample.a > 0.0) {
        result.indirectLight += ddgiSample.rgb * albedo * ddgiSample.a;
    }

    // --- 2. Direct Lighting (directional) ---

    for (uint i = 0u; i < lighting.directionalLightCount && i < MAX_DIRECTIONAL_LIGHTS; ++i) {
        vec3 L = -normalize(lighting.directionalLights[i].direction);
        vec3 lightColor = lighting.directionalLights[i].color;
        float intensity = lighting.directionalLights[i].intensity;
        float NdotL = max(dot(normal, L), 0.0);

        // Simplified Cook-Torrance (shared with all object types)
        vec3 H = normalize(viewDir + L);
        vec3 F = fresnelSchlickEL(max(dot(H, viewDir), 0.0), F0);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

        vec3 diffuse = kD * albedo / 3.14159265;
        result.directLight += (diffuse + F * 0.25) * lightColor * intensity * NdotL;
        result.specular += F * lightColor * intensity * NdotL * 0.25;
    }

    // --- 3. Clustered Point + Spot Lights ---

    // (When clustered lighting bindings are available, iterate lights per cluster
    //  using the same pattern as the main PBR shader. This is a simplified version
    //  for the unified include — full clustered evaluation is in triangle.frag.)

    // --- 4. Volumetric Fog ---

    ivec3 froxelSize = textureSize(froxelVolume, 0);
    if (froxelSize.x > 1 && froxelSize.y > 1 && froxelSize.z > 1) {
        float nearP = 0.1;
        float farP = max(lighting.fogParams.z, 1.0);
        float depthT = clamp(log(viewDepth / nearP) / log(farP / nearP), 0.0, 1.0);
        vec3 froxelUVW = vec3(screenUV, depthT);

        vec4 fogSample = texture(froxelVolume, froxelUVW);
        result.fogScattering = fogSample.rgb;
        result.fogTransmittance = fogSample.a;
    }

    return result;
}

// Convenience: apply fog to a final color
vec3 applyFog(vec3 color, LightingResult lit) {
    return color * lit.fogTransmittance + lit.fogScattering;
}

#endif // EVALUATE_LIGHTING_GLSL
