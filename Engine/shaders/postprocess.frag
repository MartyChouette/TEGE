#version 450

// Post-Processing Fragment Shader
// Applies various effects: tone mapping, bloom, vignette, color grading, FXAA

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// Scene HDR texture
layout(binding = 0) uniform sampler2D sceneTexture;

// Post-processing settings
layout(binding = 1) uniform PostProcessSettings {
    // Tone mapping
    uint toneMappingMode;
    float exposure;
    float gamma;
    float whitePoint;

    // Bloom
    uint bloomEnabled;
    float bloomThreshold;
    float bloomIntensity;
    float bloomRadius;

    // Vignette
    uint vignetteEnabled;
    float vignetteIntensity;
    float vignetteSmoothness;
    float _pad0;

    // Chromatic aberration
    uint chromaticAberrationEnabled;
    float chromaticAberrationIntensity;
    float _pad1;
    float _pad2;

    // Color grading
    vec3 colorFilter;
    float saturation;
    float contrast;
    float brightness;
    float _pad3;
    float _pad4;

    // Film grain
    uint filmGrainEnabled;
    float filmGrainIntensity;
    float time;
    float _pad5;

    // FXAA
    uint fxaaEnabled;
    float fxaaSpanMax;
    float fxaaReduceMin;
    float fxaaReduceMul;

    // Screen resolution
    uint screenWidth;
    uint screenHeight;
    float _pad6;
    float _pad7;
} settings;

// Tone mapping mode constants
#define TONEMAP_NONE 0
#define TONEMAP_REINHARD 1
#define TONEMAP_REINHARD_EXT 2
#define TONEMAP_ACES 3
#define TONEMAP_UNCHARTED2 4
#define TONEMAP_AGX 5

// Reinhard tone mapping
vec3 tonemapReinhard(vec3 color) {
    return color / (color + vec3(1.0));
}

// Reinhard with white point
vec3 tonemapReinhardExtended(vec3 color, float whitePoint) {
    float Lp = max(max(color.r, color.g), color.b);
    float L = (Lp * (1.0 + Lp / (whitePoint * whitePoint))) / (1.0 + Lp);
    return color * (L / Lp);
}

// ACES filmic tone mapping approximation
vec3 tonemapACES(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Uncharted 2 tone mapping helper
vec3 uncharted2ToneMapHelper(vec3 x) {
    const float A = 0.15;
    const float B = 0.50;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.02;
    const float F = 0.30;
    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

// Uncharted 2 tone mapping
vec3 tonemapUncharted2(vec3 color) {
    const float W = 11.2;
    vec3 curr = uncharted2ToneMapHelper(color);
    vec3 whiteScale = 1.0 / uncharted2ToneMapHelper(vec3(W));
    return curr * whiteScale;
}

// AgX tone mapping (more accurate highlight handling)
vec3 tonemapAgX(vec3 color) {
    const mat3 agxMatrix = mat3(
        0.842479062253094, 0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772, 0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104
    );
    const mat3 agxMatrixInv = mat3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116
    );

    color = agxMatrix * color;
    color = clamp(log2(color), -10.0, 6.5);
    color = (color + 10.0) / 16.5;

    // AgX look
    vec3 val = clamp(color, 0.0, 1.0);
    val = val * val * (3.0 - 2.0 * val);

    color = agxMatrixInv * val;
    return clamp(color, 0.0, 1.0);
}

// Apply tone mapping based on mode
vec3 applyToneMapping(vec3 color) {
    color *= settings.exposure;

    switch (settings.toneMappingMode) {
        case TONEMAP_REINHARD:
            color = tonemapReinhard(color);
            break;
        case TONEMAP_REINHARD_EXT:
            color = tonemapReinhardExtended(color, settings.whitePoint);
            break;
        case TONEMAP_ACES:
            color = tonemapACES(color);
            break;
        case TONEMAP_UNCHARTED2:
            color = tonemapUncharted2(color);
            break;
        case TONEMAP_AGX:
            color = tonemapAgX(color);
            break;
        default:
            // No tone mapping, just clamp
            color = clamp(color, 0.0, 1.0);
            break;
    }

    return color;
}

// Vignette effect
vec3 applyVignette(vec3 color, vec2 uv) {
    if (settings.vignetteEnabled == 0) return color;

    vec2 center = uv - 0.5;
    float dist = length(center);
    float vignette = 1.0 - smoothstep(0.5 - settings.vignetteSmoothness, 0.5, dist * settings.vignetteIntensity);
    return color * vignette;
}

// Chromatic aberration
vec3 applyChromaticAberration(vec2 uv) {
    if (settings.chromaticAberrationEnabled == 0) {
        return texture(sceneTexture, uv).rgb;
    }

    vec2 center = uv - 0.5;
    float dist = length(center);
    float offset = settings.chromaticAberrationIntensity * dist;

    float r = texture(sceneTexture, uv + center * offset).r;
    float g = texture(sceneTexture, uv).g;
    float b = texture(sceneTexture, uv - center * offset).b;

    return vec3(r, g, b);
}

// Color grading (saturation, contrast, brightness)
vec3 applyColorGrading(vec3 color) {
    // Brightness
    color += settings.brightness;

    // Contrast
    color = (color - 0.5) * settings.contrast + 0.5;

    // Saturation
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, settings.saturation);

    // Color filter
    color *= settings.colorFilter;

    return clamp(color, 0.0, 1.0);
}

// Film grain
vec3 applyFilmGrain(vec3 color, vec2 uv) {
    if (settings.filmGrainEnabled == 0) return color;

    // Simple noise function
    float noise = fract(sin(dot(uv + fract(settings.time), vec2(12.9898, 78.233))) * 43758.5453);
    noise = noise * 2.0 - 1.0;
    noise *= settings.filmGrainIntensity;

    return color + vec3(noise);
}

// FXAA (Fast Approximate Anti-Aliasing)
vec3 applyFXAA(vec2 uv) {
    if (settings.fxaaEnabled == 0) {
        return texture(sceneTexture, uv).rgb;
    }

    vec2 texelSize = 1.0 / vec2(settings.screenWidth, settings.screenHeight);

    // Sample neighbors
    vec3 rgbNW = texture(sceneTexture, uv + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 rgbNE = texture(sceneTexture, uv + vec2( 1.0, -1.0) * texelSize).rgb;
    vec3 rgbSW = texture(sceneTexture, uv + vec2(-1.0,  1.0) * texelSize).rgb;
    vec3 rgbSE = texture(sceneTexture, uv + vec2( 1.0,  1.0) * texelSize).rgb;
    vec3 rgbM  = texture(sceneTexture, uv).rgb;

    // Convert to luminance
    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM, luma);

    // Compute edge direction
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * settings.fxaaReduceMul), settings.fxaaReduceMin);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(settings.fxaaSpanMax), max(vec2(-settings.fxaaSpanMax), dir * rcpDirMin)) * texelSize;

    // Sample along edge
    vec3 rgbA = 0.5 * (texture(sceneTexture, uv + dir * (1.0/3.0 - 0.5)).rgb +
                       texture(sceneTexture, uv + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (texture(sceneTexture, uv + dir * -0.5).rgb +
                                      texture(sceneTexture, uv + dir *  0.5).rgb);

    float lumaB = dot(rgbB, luma);

    if (lumaB < lumaMin || lumaB > lumaMax) {
        return rgbA;
    }
    return rgbB;
}

void main() {
    vec2 uv = fragUV;

    // Sample scene with optional FXAA or chromatic aberration
    vec3 color;
    if (settings.fxaaEnabled != 0 && settings.chromaticAberrationEnabled == 0) {
        color = applyFXAA(uv);
    } else {
        color = applyChromaticAberration(uv);
    }

    // Apply tone mapping
    if (settings.toneMappingMode != TONEMAP_NONE) {
        color = applyToneMapping(color);
    }

    // Apply color grading
    color = applyColorGrading(color);

    // Apply vignette
    color = applyVignette(color, uv);

    // Apply film grain
    color = applyFilmGrain(color, uv);

    // Gamma correction
    color = pow(color, vec3(1.0 / settings.gamma));

    outColor = vec4(color, 1.0);
}
