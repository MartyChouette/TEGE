#version 450

// Post-Processing Fragment Shader
// Applies various effects: tone mapping, bloom, vignette, color grading, FXAA, retro effects,
// LUT color grading, CRT phosphor, VHS filter, palette lock

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
    uint colorblindMode;       // 0=off, 1=protanopia, 2=deuteranopia, 3=tritanopia, 4-6=anomalous, 7=achromatopsia
    float colorblindStrength;

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

    // Retro: Dithering
    uint ditherEnabled;
    uint ditherPattern;       // 0=Bayer2x2, 1=Bayer4x4, 2=Bayer8x8
    float ditherStrength;
    float _retroPad0;

    // Retro: Color quantization
    uint colorQuantEnabled;
    uint colorBitDepth;       // bits per channel
    float _retroPad1;
    float _retroPad2;

    // Retro: Resolution downscaling
    uint resDownscaleEnabled;
    uint internalWidth;
    uint internalHeight;
    uint usePointFiltering;

    // CRT scanlines
    uint crtEnabled;
    float scanlineIntensity;
    float scanlineWidth;
    float crtCurvature;

    // LUT color grading
    uint lutEnabled;
    float lutStrength;
    uint lutSize;
    float _lutPad0;

    // CRT Phosphor subpixel blending
    uint crtPhosphorEnabled;
    uint crtMaskType;          // 0=aperture grille, 1=shadow mask, 2=slot mask
    float crtMaskPitch;
    float crtBloomRadius;
    float crtBloomStrength;
    float _crtPad0;
    float _crtPad1;
    float _crtPad2;

    // VHS filter
    uint vhsEnabled;
    float vhsTrackingIntensity;
    float vhsTrackingSpeed;
    float vhsWobbleIntensity;
    float vhsWobbleSpeed;
    float vhsColorBleed;
    float vhsNoiseIntensity;
    float vhsBlueShift;
    uint vhsScreenTear;
    float vhsTearOffset;
    uint vhsInterlacing;
    float _vhsPad0;

    // Color palette lock
    uint paletteEnabled;
    uint paletteColors;
    float _palPad0;
    float _palPad1;

    // Depth of Field
    uint dofEnabled;
    float dofFocalDistance;
    float dofFocalRange;
    float dofNearBlurStrength;
    float dofFarBlurStrength;
    float dofBokehSize;
    uint dofApertureShape;
    uint dofDebugCoC;

    // Camera planes
    float cameraNearPlane;
    float cameraFarPlane;
    float _cameraPad0;
    float _cameraPad1;

    // Tilt-Shift
    uint tiltShiftEnabled;
    float tiltShiftFocusY;
    float tiltShiftBandWidth;
    float tiltShiftBlurAmount;

    // Cel shading outlines
    uint celOutlineEnabled;
    float celOutlineThickness;
    float celOutlineThreshold;
    float _celPad0;
    vec3 celOutlineColor;
    float _celPad1;

    // Full-screen stipple / dither
    uint stippleEnabled;
    uint stipplePatternMask;    // bitmask: bit0..bit7
    uint stippleColorMode;      // 0=mono, 1=duo-tone, 2=full color
    float stippleScale;
    float stippleDensity;
    float stippleStrength;
    float _stipplePad0;
    float _stipplePad1;
    vec3 stippleFgColor;
    float _stipplePad2;
    vec3 stippleBgColor;
    float _stipplePad3;

    // Screen-Space Effects infrastructure
    vec4 invViewProj0;    // Inverse view-projection matrix columns
    vec4 invViewProj1;
    vec4 invViewProj2;
    vec4 invViewProj3;
    vec3 lightDirWorld;   // Directional light direction (toward light)
    float _ssPad0;
    vec4 lightScreenPos;  // xy=NDC 0..1, z=depth, w=1 if on-screen

    // God Rays
    uint godRaysEnabled;
    float godRaysIntensity;
    float godRaysDecay;
    float godRaysDensity;
    uint godRaysSamples;
    float godRaysWeight;
    float _godRaysPad0;
    float _godRaysPad1;

    // SSAO
    uint ssaoEnabled;
    float ssaoRadius;
    float ssaoIntensity;
    float ssaoBias;
    uint ssaoSamples;
    float _ssaoPad0;
    float _ssaoPad1;
    float _ssaoPad2;

    // Contact Shadows
    uint contactShadowsEnabled;
    float contactShadowsLength;
    uint contactShadowsSteps;
    float contactShadowsIntensity;

    // Fake Caustics
    uint causticsEnabled;
    float causticsIntensity;
    float causticsScale;
    float causticsSpeed;
    float causticsWaterY;
    float _causticsPad0;
    float _causticsPad1;
    float _causticsPad2;

    // Fog Shafts
    uint fogShaftsEnabled;
    float fogShaftsIntensity;
    float fogShaftsDensity;
    float fogShaftsDecay;
    uint fogShaftsSamples;
    float fogShaftsMaxDistance;
    float _fogShaftsPad0;
    float _fogShaftsPad1;
} settings;

// LUT texture (binding 2)
layout(binding = 2) uniform sampler2D lutTexture;

// Depth texture (binding 3) for cel outline edge detection
layout(binding = 3) uniform sampler2D depthTexture;

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

// Colorblind correction using Daltonization (Brettel/Machado approach)
// Simulates how a colorblind person sees, computes error, redistributes to visible channels
#define CB_OFF 0
#define CB_PROTANOPIA 1
#define CB_DEUTERANOPIA 2
#define CB_TRITANOPIA 3
#define CB_PROTANOMALY 4
#define CB_DEUTERANOMALY 5
#define CB_TRITANOMALY 6
#define CB_ACHROMATOPSIA 7

vec3 applyColorblindCorrection(vec3 color) {
    if (settings.colorblindMode == CB_OFF) return color;

    // Achromatopsia: convert to luminance
    if (settings.colorblindMode == CB_ACHROMATOPSIA) {
        float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
        return mix(color, vec3(lum), settings.colorblindStrength);
    }

    // Simulation matrices (how colorblind person perceives color)
    mat3 simMatrix;
    float strength = settings.colorblindStrength;

    // Anomalous modes use same matrices at reduced strength
    uint baseMode = settings.colorblindMode;
    if (baseMode >= CB_PROTANOMALY && baseMode <= CB_TRITANOMALY) {
        baseMode = baseMode - 3; // Map 4->1, 5->2, 6->3
        strength *= 0.6;
    }

    if (baseMode == CB_PROTANOPIA) {
        // Protanopia: reduced red sensitivity
        simMatrix = mat3(
            0.152286, 0.114503, -0.003882,
            1.052583, 0.786281, -0.048116,
           -0.204868, 0.099216,  1.051998
        );
    } else if (baseMode == CB_DEUTERANOPIA) {
        // Deuteranopia: reduced green sensitivity
        simMatrix = mat3(
            0.367322, 0.280085, -0.011820,
            0.860646, 0.672501,  0.042940,
           -0.227968, 0.047413,  0.968881
        );
    } else {
        // Tritanopia: reduced blue sensitivity
        simMatrix = mat3(
            1.255528, -0.078411, 0.004733,
           -0.076749,  0.930809, 0.691367,
           -0.178779,  0.147602, 0.303900
        );
    }

    // Simulate colorblind vision
    vec3 simColor = simMatrix * color;
    simColor = clamp(simColor, 0.0, 1.0);

    // Compute error (what the colorblind person is missing)
    vec3 error = color - simColor;

    // Redistribute error to visible channels
    // Shift red/green errors into blue, blue errors into red/green
    vec3 correction;
    correction.r = error.r * 0.0 + error.g * 0.7 + error.b * 0.7;
    correction.g = error.r * 0.7 + error.g * 0.0 + error.b * 0.7;
    correction.b = error.r * 0.7 + error.g * 0.7 + error.b * 0.0;

    // Apply correction
    vec3 result = color + correction * strength;
    return clamp(result, 0.0, 1.0);
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

// ============================================================
// Retro post-processing effects
// ============================================================

// Resolution downscaling: snap UV to lower-resolution pixel grid
vec2 applyResolutionDownscale(vec2 uv) {
    if (settings.resDownscaleEnabled == 0) return uv;

    float iw = float(settings.internalWidth);
    float ih = float(settings.internalHeight);

    // Snap to internal resolution grid
    uv = floor(uv * vec2(iw, ih)) / vec2(iw, ih);
    // Center within the texel
    uv += 0.5 / vec2(iw, ih);

    return uv;
}

// Ordered dithering using Bayer matrices
vec3 applyDithering(vec3 color, vec2 screenPos) {
    if (settings.ditherEnabled == 0) return color;

    float threshold = 0.0;
    ivec2 pos = ivec2(screenPos);

    if (settings.ditherPattern == 0) {
        // Bayer 2x2
        const float bayer2[4] = float[4](0.0, 2.0, 3.0, 1.0);
        int idx = (pos.x % 2) + (pos.y % 2) * 2;
        threshold = bayer2[idx] / 4.0 - 0.5;
    } else if (settings.ditherPattern == 1) {
        // Bayer 4x4
        const float bayer4[16] = float[16](
             0.0,  8.0,  2.0, 10.0,
            12.0,  4.0, 14.0,  6.0,
             3.0, 11.0,  1.0,  9.0,
            15.0,  7.0, 13.0,  5.0
        );
        int idx = (pos.x % 4) + (pos.y % 4) * 4;
        threshold = bayer4[idx] / 16.0 - 0.5;
    } else {
        // Bayer 8x8
        const float bayer8[64] = float[64](
             0.0, 32.0,  8.0, 40.0,  2.0, 34.0, 10.0, 42.0,
            48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
            12.0, 44.0,  4.0, 36.0, 14.0, 46.0,  6.0, 38.0,
            60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
             3.0, 35.0, 11.0, 43.0,  1.0, 33.0,  9.0, 41.0,
            51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
            15.0, 47.0,  7.0, 39.0, 13.0, 45.0,  5.0, 37.0,
            63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
        );
        int idx = (pos.x % 8) + (pos.y % 8) * 8;
        threshold = bayer8[idx] / 64.0 - 0.5;
    }

    // Apply dither as an offset before quantization
    color += threshold * settings.ditherStrength / max(float(settings.colorBitDepth), 5.0);

    return color;
}

// Color quantization: reduce to N-bit color
vec3 applyColorQuantization(vec3 color) {
    if (settings.colorQuantEnabled == 0) return color;

    float levels = pow(2.0, float(settings.colorBitDepth)) - 1.0;
    color = floor(color * levels + 0.5) / levels;

    return clamp(color, 0.0, 1.0);
}

// CRT scanlines + optional barrel distortion
vec3 applyCRT(vec3 color, vec2 uv) {
    if (settings.crtEnabled == 0) return color;

    // Scanlines
    float screenY = uv.y * float(settings.screenHeight);
    float scanline = sin(screenY * 3.14159 / settings.scanlineWidth);
    scanline = scanline * scanline; // square for sharper lines
    color *= 1.0 - scanline * settings.scanlineIntensity;

    // Barrel distortion (CRT curvature)
    if (settings.crtCurvature > 0.0) {
        vec2 centered = uv * 2.0 - 1.0;
        float r2 = dot(centered, centered);
        float distortion = 1.0 + r2 * settings.crtCurvature;
        vec2 distorted = centered * distortion;
        // Darken edges that fall outside the screen
        if (abs(distorted.x) > 1.0 || abs(distorted.y) > 1.0) {
            color *= 0.0;
        }
    }

    return color;
}

// ============================================================
// LUT Color Grading
// ============================================================
vec3 applyLUT(vec3 color) {
    if (settings.lutEnabled == 0) return color;

    // 2D strip LUT: image is lutSize^2 wide, lutSize tall
    float size = float(settings.lutSize);

    // Clamp input color to valid range
    color = clamp(color, 0.0, 1.0);

    // Blue channel selects the slice
    float blueSlice = color.b * (size - 1.0);
    float slice0 = floor(blueSlice);
    float slice1 = min(slice0 + 1.0, size - 1.0);
    float blendFactor = blueSlice - slice0;

    // UV within each slice
    vec2 uv0 = vec2((slice0 + color.r * (size - 1.0) / size + 0.5 / size) / size, (color.g * (size - 1.0) + 0.5) / size);
    vec2 uv1 = vec2((slice1 + color.r * (size - 1.0) / size + 0.5 / size) / size, (color.g * (size - 1.0) + 0.5) / size);

    vec3 lut0 = texture(lutTexture, uv0).rgb;
    vec3 lut1 = texture(lutTexture, uv1).rgb;
    vec3 lutColor = mix(lut0, lut1, blendFactor);

    return mix(color, lutColor, settings.lutStrength);
}

// ============================================================
// CRT Phosphor Subpixel Blending
// ============================================================
vec3 applyCRTPhosphor(vec3 color, vec2 uv) {
    if (settings.crtPhosphorEnabled == 0) return color;

    vec2 screenPos = uv * vec2(settings.screenWidth, settings.screenHeight);
    float pitch = max(settings.crtMaskPitch, 0.5);

    // RGB subpixel mask pattern
    vec3 mask = vec3(1.0);
    int px = int(floor(screenPos.x / pitch));
    int py = int(floor(screenPos.y / pitch));

    if (settings.crtMaskType == 0) {
        // Aperture grille: vertical RGB stripes
        int stripe = int(mod(float(px), 3.0));
        if (stripe == 0) mask = vec3(1.0, 0.3, 0.3);
        else if (stripe == 1) mask = vec3(0.3, 1.0, 0.3);
        else mask = vec3(0.3, 0.3, 1.0);
    } else if (settings.crtMaskType == 1) {
        // Shadow mask: triangular RGB dot pattern
        int col = int(mod(float(px), 3.0));
        bool oddRow = (int(mod(float(py), 2.0)) == 1);
        int offset = oddRow ? 1 : 0;
        int stripe = int(mod(float(col + offset), 3.0));
        if (stripe == 0) mask = vec3(1.0, 0.25, 0.25);
        else if (stripe == 1) mask = vec3(0.25, 1.0, 0.25);
        else mask = vec3(0.25, 0.25, 1.0);
    } else {
        // Slot mask: rectangular RGB dot groups
        int col = int(mod(float(px), 3.0));
        bool evenSlot = (int(mod(float(py), 4.0)) < 2);
        if (!evenSlot) col = int(mod(float(col + 1), 3.0));
        if (col == 0) mask = vec3(1.0, 0.2, 0.2);
        else if (col == 1) mask = vec3(0.2, 1.0, 0.2);
        else mask = vec3(0.2, 0.2, 1.0);
    }

    // Phosphor bloom: sample neighbors weighted by gaussian
    vec2 texelSize = 1.0 / vec2(settings.screenWidth, settings.screenHeight);
    float radius = settings.crtBloomRadius;
    vec3 bloom = vec3(0.0);
    float totalWeight = 0.0;

    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            vec2 offset = vec2(float(dx), float(dy)) * texelSize * radius;
            float dist = length(vec2(float(dx), float(dy)));
            float weight = exp(-dist * dist * 0.5);
            bloom += texture(sceneTexture, uv + offset).rgb * weight;
            totalWeight += weight;
        }
    }
    bloom /= totalWeight;

    // Combine: mask the direct color, blend bloom for phosphor glow
    vec3 masked = color * mask;
    return mix(masked, bloom, settings.crtBloomStrength);
}

// ============================================================
// VHS Filter
// ============================================================
vec3 applyVHS(vec2 uv) {
    if (settings.vhsEnabled == 0) return texture(sceneTexture, uv).rgb;

    vec2 origUV = uv;
    float t = settings.time;

    // Screen tear: offset top/bottom half horizontally
    if (settings.vhsScreenTear != 0) {
        float tearLine = fract(t * 0.13 + 0.5);
        if (uv.y < tearLine) {
            uv.x += settings.vhsTearOffset * sin(t * 3.7) * 0.02;
        }
    }

    // Tracking lines: scrolling horizontal band that displaces UV.x
    float trackBand = sin(uv.y * 20.0 - t * settings.vhsTrackingSpeed * 5.0);
    trackBand = smoothstep(0.8, 1.0, trackBand);
    uv.x += trackBand * settings.vhsTrackingIntensity * 0.02;

    // Wobble: sine-wave horizontal distortion
    uv.x += sin(uv.y * 50.0 + t * settings.vhsWobbleSpeed * 10.0) * settings.vhsWobbleIntensity;

    // Sample with color bleed: offset R and B channels horizontally
    float bleed = settings.vhsColorBleed;
    float r = texture(sceneTexture, vec2(uv.x + bleed, uv.y)).r;
    float g = texture(sceneTexture, uv).g;
    float b = texture(sceneTexture, vec2(uv.x - bleed, uv.y)).b;
    vec3 color = vec3(r, g, b);

    // Per-line noise (heavier at screen edges)
    float lineNoise = fract(sin(dot(vec2(origUV.y * 1000.0, t), vec2(12.9898, 78.233))) * 43758.5453);
    lineNoise = (lineNoise * 2.0 - 1.0) * settings.vhsNoiseIntensity;
    float edgeFactor = 1.0 + abs(origUV.x - 0.5) * 2.0;  // Stronger at edges
    color += vec3(lineNoise * edgeFactor);

    // Blue shift
    color *= vec3(1.0 - settings.vhsBlueShift, 1.0, 1.0 + settings.vhsBlueShift);

    // Interlacing: darken odd or even lines alternating each frame
    if (settings.vhsInterlacing != 0) {
        int frame = int(t * 60.0) % 2;
        int line = int(origUV.y * float(settings.screenHeight));
        if ((line + frame) % 2 == 0) {
            color *= 0.85;
        }
    }

    return color;
}

// ============================================================
// Color Palette Lock
// ============================================================
vec3 applyPaletteLock(vec3 color) {
    if (settings.paletteEnabled == 0) return color;

    float n = float(settings.paletteColors);
    color = floor(color * (n - 1.0) + 0.5) / (n - 1.0);
    return clamp(color, 0.0, 1.0);
}

// ============================================================
// Full-Screen Stipple / Dither
// ============================================================

// Stipple pattern threshold generators (return 0..1)
float stippleBayer4x4(ivec2 pos) {
    const float bayer4[16] = float[16](
         0.0,  8.0,  2.0, 10.0,
        12.0,  4.0, 14.0,  6.0,
         3.0, 11.0,  1.0,  9.0,
        15.0,  7.0, 13.0,  5.0
    );
    int idx = (pos.x % 4) + (pos.y % 4) * 4;
    return bayer4[idx] / 16.0;
}

float stippleBayer8x8(ivec2 pos) {
    const float bayer8[64] = float[64](
         0.0, 32.0,  8.0, 40.0,  2.0, 34.0, 10.0, 42.0,
        48.0, 16.0, 56.0, 24.0, 50.0, 18.0, 58.0, 26.0,
        12.0, 44.0,  4.0, 36.0, 14.0, 46.0,  6.0, 38.0,
        60.0, 28.0, 52.0, 20.0, 62.0, 30.0, 54.0, 22.0,
         3.0, 35.0, 11.0, 43.0,  1.0, 33.0,  9.0, 41.0,
        51.0, 19.0, 59.0, 27.0, 49.0, 17.0, 57.0, 25.0,
        15.0, 47.0,  7.0, 39.0, 13.0, 45.0,  5.0, 37.0,
        63.0, 31.0, 55.0, 23.0, 61.0, 29.0, 53.0, 21.0
    );
    int idx = (pos.x % 8) + (pos.y % 8) * 8;
    return bayer8[idx] / 64.0;
}

float stippleBlueNoise(ivec2 pos) {
    // Interleaved gradient noise (Jimenez 2014)
    vec2 p = vec2(pos);
    return fract(52.9829189 * fract(0.06711056 * p.x + 0.00583715 * p.y));
}

float stippleHalftone(ivec2 pos) {
    vec2 center = mod(vec2(pos), 8.0) - 3.5;
    return length(center) / 5.0;
}

float stippleCrosshatch(ivec2 pos) {
    float d1 = mod(float(pos.x + pos.y), 8.0) / 8.0;
    float d2 = mod(float(pos.x - pos.y + 800), 8.0) / 8.0;
    return min(d1, d2);
}

float stippleOverlook(ivec2 pos) {
    // Hex geometric pattern
    vec2 p = vec2(pos);
    float hexY = mod(p.y, 6.0);
    float offset = (hexY < 3.0) ? 0.0 : 3.0;
    float hexX = mod(p.x + offset, 6.0);
    vec2 center = vec2(3.0, 3.0);
    float dist = length(vec2(hexX, mod(hexY, 3.0) * 2.0) - center);
    return clamp(dist / 4.0, 0.0, 1.0);
}

float stippleOrdered2x2(ivec2 pos) {
    const float bayer2[4] = float[4](0.0, 2.0, 3.0, 1.0);
    int idx = (pos.x % 2) + (pos.y % 2) * 2;
    return bayer2[idx] / 4.0;
}

float stippleFloydSteinberg(ivec2 pos, float luma) {
    // Pseudo error-diffusion: compare luminance against a local average approximation
    // using neighboring pixel noise to simulate diffused error
    float noise = fract(sin(dot(vec2(pos), vec2(12.9898, 78.233))) * 43758.5453);
    return mix(luma, noise, 0.5);
}

vec3 applyStipple(vec3 color, vec2 screenPos) {
    if (settings.stippleEnabled == 0) return color;

    float scale = max(settings.stippleScale, 0.5);
    ivec2 pos = ivec2(floor(screenPos / scale));

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));

    // Accumulate thresholds from all enabled patterns (bitmask)
    uint mask = settings.stipplePatternMask;
    float threshold = 0.0;
    float count = 0.0;

    if ((mask & 0x01u) != 0u) { threshold += stippleBayer4x4(pos);              count += 1.0; }
    if ((mask & 0x02u) != 0u) { threshold += stippleBayer8x8(pos);              count += 1.0; }
    if ((mask & 0x04u) != 0u) { threshold += stippleBlueNoise(pos);             count += 1.0; }
    if ((mask & 0x08u) != 0u) { threshold += stippleHalftone(pos);              count += 1.0; }
    if ((mask & 0x10u) != 0u) { threshold += stippleCrosshatch(pos);            count += 1.0; }
    if ((mask & 0x20u) != 0u) { threshold += stippleOverlook(pos);              count += 1.0; }
    if ((mask & 0x40u) != 0u) { threshold += stippleOrdered2x2(pos);            count += 1.0; }
    if ((mask & 0x80u) != 0u) { threshold += stippleFloydSteinberg(pos, luma);  count += 1.0; }

    if (count < 1.0) threshold = 0.5;
    else threshold /= count;

    // Apply density bias: shift threshold so more/fewer dots appear
    float biasedLuma = luma + (settings.stippleDensity - 0.5);

    // Determine stippled output
    vec3 stippled;
    if (settings.stippleColorMode == 0) {
        // Monochrome: fg/bg based on luma vs threshold
        stippled = (biasedLuma > threshold) ? settings.stippleFgColor : settings.stippleBgColor;
    } else if (settings.stippleColorMode == 1) {
        // Duo-tone: two configurable colors mapped to dark/light
        stippled = (biasedLuma > threshold) ? settings.stippleFgColor : settings.stippleBgColor;
    } else {
        // Full color: apply pattern to luminance, preserve hue
        float factor = (biasedLuma > threshold) ? 1.0 : 0.0;
        stippled = color * factor;
    }

    // Blend with original based on strength
    return mix(color, stippled, settings.stippleStrength);
}

// Linearize Vulkan [0,1] reverse-Z depth to view-space distance
float linearizeDepth(float d, float near, float far) {
    return near * far / (far - d * (far - near));
}

// ============================================================
// Screen-Space Effects — Shared Utilities
// ============================================================

// Interleaved gradient noise (Jimenez 2014) — low-discrepancy, no texture needed
float interleavedGradientNoise(vec2 screenPos) {
    return fract(52.9829189 * fract(0.06711056 * screenPos.x + 0.00583715 * screenPos.y));
}

// Reconstruct world-space position from UV + depth using inverse view-projection matrix
vec3 reconstructWorldPos(vec2 uv, float depth) {
    // NDC: x,y in [-1,1], z = depth (Vulkan 0..1 reversed Z)
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    // Manually multiply by inverse view-projection matrix (stored as 4 column vectors)
    vec4 world;
    world.x = dot(clip, vec4(settings.invViewProj0.x, settings.invViewProj1.x, settings.invViewProj2.x, settings.invViewProj3.x));
    world.y = dot(clip, vec4(settings.invViewProj0.y, settings.invViewProj1.y, settings.invViewProj2.y, settings.invViewProj3.y));
    world.z = dot(clip, vec4(settings.invViewProj0.z, settings.invViewProj1.z, settings.invViewProj2.z, settings.invViewProj3.z));
    world.w = dot(clip, vec4(settings.invViewProj0.w, settings.invViewProj1.w, settings.invViewProj2.w, settings.invViewProj3.w));
    return world.xyz / world.w;
}

// Reconstruct normal from depth buffer using cross product of partial derivatives
vec3 reconstructNormal(vec2 uv) {
    vec2 texelSize = 1.0 / vec2(settings.screenWidth, settings.screenHeight);
    float dC = texture(depthTexture, uv).r;
    float dR = texture(depthTexture, uv + vec2(texelSize.x, 0.0)).r;
    float dU = texture(depthTexture, uv + vec2(0.0, texelSize.y)).r;

    vec3 posC = reconstructWorldPos(uv, dC);
    vec3 posR = reconstructWorldPos(uv + vec2(texelSize.x, 0.0), dR);
    vec3 posU = reconstructWorldPos(uv + vec2(0.0, texelSize.y), dU);

    return normalize(cross(posR - posC, posU - posC));
}

// ============================================================
// God Rays — Screen-space radial blur from projected sun position
// (GPU Gems 3, Crysis 2007)
// ============================================================
vec3 applyGodRays(vec3 color, vec2 uv) {
    if (settings.godRaysEnabled == 0) return color;
    if (settings.lightScreenPos.w < 0.5) return color; // Light not on screen

    vec2 lightPos = settings.lightScreenPos.xy;
    vec2 deltaUV = (uv - lightPos) * settings.godRaysDensity / float(settings.godRaysSamples);

    vec2 sampleUV = uv;
    float illumination = 0.0;
    float decay = 1.0;

    for (uint i = 0u; i < settings.godRaysSamples; i++) {
        sampleUV -= deltaUV;
        vec2 clampedUV = clamp(sampleUV, 0.001, 0.999);
        float sampleLuma = dot(texture(sceneTexture, clampedUV).rgb, vec3(0.2126, 0.7152, 0.0722));
        // Only accumulate bright areas (sky/emissive) — threshold at 0.5
        sampleLuma = max(sampleLuma - 0.5, 0.0);
        illumination += sampleLuma * decay * settings.godRaysWeight;
        decay *= settings.godRaysDecay;
    }

    return color + vec3(illumination * settings.godRaysIntensity);
}

// ============================================================
// SSAO — Screen-Space Ambient Occlusion
// Hemisphere sampling with reconstructed normals (Crysis 2007)
// ============================================================
vec3 applySSAO(vec3 color, vec2 uv) {
    if (settings.ssaoEnabled == 0) return color;

    float depth = texture(depthTexture, uv).r;
    if (depth < 0.0001) return color; // Sky

    vec3 fragPos = reconstructWorldPos(uv, depth);
    vec3 normal = reconstructNormal(uv);

    vec2 texelSize = 1.0 / vec2(settings.screenWidth, settings.screenHeight);
    vec2 screenPos = uv * vec2(settings.screenWidth, settings.screenHeight);

    float occlusion = 0.0;
    uint sampleCount = min(settings.ssaoSamples, 32u);

    for (uint i = 0u; i < sampleCount; i++) {
        // Generate pseudo-random sample direction using IGN + golden angle spiral
        float fi = float(i);
        float noise = interleavedGradientNoise(screenPos + vec2(fi * 7.0, fi * 13.0));
        float angle = fi * 2.399963 + noise * 6.2831853; // Golden angle spiral + noise
        float radius = (fi + noise) / float(sampleCount);
        radius = mix(0.1, 1.0, radius * radius); // Quadratic distribution

        // Create sample offset in tangent space
        vec3 sampleDir = vec3(cos(angle) * radius, sin(angle) * radius, sqrt(max(1.0 - radius * radius, 0.0)));

        // Orient to hemisphere around normal using Gram-Schmidt
        vec3 tangent = normalize(cross(abs(normal.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0), normal));
        vec3 bitangent = cross(normal, tangent);
        vec3 sampleOffset = tangent * sampleDir.x + bitangent * sampleDir.y + normal * sampleDir.z;

        vec3 samplePos = fragPos + sampleOffset * settings.ssaoRadius;

        // Project sample back to screen
        vec4 projSample;
        projSample.x = dot(vec4(samplePos, 1.0), vec4(settings.invViewProj0.x, settings.invViewProj0.y, settings.invViewProj0.z, settings.invViewProj0.w));
        // We need VIEW-PROJ, not INV-VIEW-PROJ to project. Use depth comparison instead:
        // Approximate: offset UV by projected radius and compare depths
        vec2 offsetUV = uv + sampleOffset.xy * settings.ssaoRadius * texelSize * 100.0;
        offsetUV = clamp(offsetUV, 0.001, 0.999);
        float sampleDepth = texture(depthTexture, offsetUV).r;
        float sampleLinear = linearizeDepth(sampleDepth, settings.cameraNearPlane, settings.cameraFarPlane);
        float fragLinear = linearizeDepth(depth, settings.cameraNearPlane, settings.cameraFarPlane);

        // Range check + depth comparison
        float rangeCheck = smoothstep(0.0, 1.0, settings.ssaoRadius / max(abs(fragLinear - sampleLinear), 0.001));
        occlusion += (sampleLinear <= fragLinear - settings.ssaoBias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(sampleCount)) * settings.ssaoIntensity;
    occlusion = clamp(occlusion, 0.0, 1.0);

    return color * occlusion;
}

// ============================================================
// Contact Shadows — Screen-space ray march toward light in depth buffer
// (Uncharted 4, DOOM 2016)
// ============================================================
vec3 applyContactShadows(vec3 color, vec2 uv) {
    if (settings.contactShadowsEnabled == 0) return color;

    float depth = texture(depthTexture, uv).r;
    if (depth < 0.0001) return color; // Sky

    // March from fragment toward light in screen space
    // Project light direction to screen-space step
    vec2 lightDir2D = normalize(settings.lightScreenPos.xy - uv);
    vec2 stepUV = lightDir2D * settings.contactShadowsLength / float(settings.contactShadowsSteps);

    float fragLinear = linearizeDepth(depth, settings.cameraNearPlane, settings.cameraFarPlane);
    vec2 sampleUV = uv;
    float shadow = 0.0;

    for (uint i = 1u; i <= settings.contactShadowsSteps; i++) {
        sampleUV += stepUV;
        if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) break;

        float sampleDepth = texture(depthTexture, sampleUV).r;
        float sampleLinear = linearizeDepth(sampleDepth, settings.cameraNearPlane, settings.cameraFarPlane);

        // Interpolated expected depth along ray (linear interpolation in view space)
        float t = float(i) / float(settings.contactShadowsSteps);
        float expectedDepth = fragLinear - t * settings.contactShadowsLength * fragLinear;

        // Hit test: sample is closer than expected (occluder found)
        float thickness = abs(sampleLinear - expectedDepth);
        if (sampleLinear < expectedDepth && thickness < settings.contactShadowsLength * fragLinear * 0.5) {
            shadow = 1.0 - t; // Fade with distance
            break;
        }
    }

    shadow *= settings.contactShadowsIntensity;
    return color * (1.0 - shadow);
}

// ============================================================
// Fake Caustics — Procedural animated Voronoi pattern below water plane
// (Tomb Raider 2013 style)
// ============================================================
vec3 applyCaustics(vec3 color, vec2 uv) {
    if (settings.causticsEnabled == 0) return color;

    float depth = texture(depthTexture, uv).r;
    if (depth < 0.0001) return color; // Sky

    vec3 worldPos = reconstructWorldPos(uv, depth);

    // Only apply below water plane
    if (worldPos.y > settings.causticsWaterY) return color;

    // Animated Voronoi pattern on XZ plane
    float t = settings.time * settings.causticsSpeed;
    vec2 p = worldPos.xz * settings.causticsScale;

    // Two-layer Voronoi for more organic look
    float minDist1 = 1.0;
    float minDist2 = 1.0;
    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec2 cell = vec2(float(x), float(y));
            vec2 cellID = floor(p) + cell;
            // Pseudo-random cell center
            vec2 cellCenter = cellID + vec2(
                fract(sin(dot(cellID, vec2(127.1, 311.7))) * 43758.5453),
                fract(sin(dot(cellID, vec2(269.5, 183.3))) * 43758.5453)
            );
            // Animate cell centers
            cellCenter += 0.3 * vec2(sin(t + cellCenter.x * 6.28), cos(t + cellCenter.y * 6.28));
            float d = length(fract(p) - cell - fract(cellCenter));
            if (d < minDist1) {
                minDist2 = minDist1;
                minDist1 = d;
            } else if (d < minDist2) {
                minDist2 = d;
            }
        }
    }

    // Caustic brightness from cell edge proximity
    float caustic = smoothstep(0.0, 0.3, minDist2 - minDist1);
    caustic *= caustic; // Sharpen

    // Fade with depth below water
    float waterDepthFade = clamp((settings.causticsWaterY - worldPos.y) * 0.1, 0.0, 1.0);

    return color + vec3(caustic * settings.causticsIntensity * waterDepthFade);
}

// ============================================================
// Fog Shafts — Noisy volumetric-look fog ray march
// (The Last of Us, Skyrim style)
// ============================================================
vec3 applyFogShafts(vec3 color, vec2 uv) {
    if (settings.fogShaftsEnabled == 0) return color;

    float depth = texture(depthTexture, uv).r;
    float fragLinear = linearizeDepth(depth, settings.cameraNearPlane, settings.cameraFarPlane);
    float maxDist = min(fragLinear, settings.fogShaftsMaxDistance);

    vec2 screenPos = uv * vec2(settings.screenWidth, settings.screenHeight);
    float noise0 = interleavedGradientNoise(screenPos);

    vec2 lightDir2D = normalize(settings.lightScreenPos.xy - uv);
    float fog = 0.0;
    float decay = 1.0;

    uint sampleCount = min(settings.fogShaftsSamples, 32u);

    for (uint i = 0u; i < sampleCount; i++) {
        float t = (float(i) + noise0) / float(sampleCount);
        vec2 sampleUV = uv - lightDir2D * t * 0.2; // March toward light
        sampleUV = clamp(sampleUV, 0.001, 0.999);

        float sampleDepth = texture(depthTexture, sampleUV).r;
        float sampleLinear = linearizeDepth(sampleDepth, settings.cameraNearPlane, settings.cameraFarPlane);

        // Accumulate fog if sample is behind geometry (in open space)
        float sampleDist = t * maxDist;
        if (sampleLinear > sampleDist) {
            fog += settings.fogShaftsDensity * decay;
        }
        decay *= settings.fogShaftsDecay;
    }

    // Modulate by light direction alignment (stronger when looking toward light)
    float lightAlignment = max(dot(normalize(settings.lightScreenPos.xy - vec2(0.5)), uv - vec2(0.5)), 0.0);
    lightAlignment = pow(lightAlignment + 0.3, 2.0); // Bias to prevent total darkness

    fog *= settings.fogShaftsIntensity * lightAlignment;
    return color + vec3(clamp(fog, 0.0, 1.0));
}

// Cel shading outline: Sobel edge detection on depth buffer
vec3 applyCelOutline(vec3 color, vec2 uv) {
    if (settings.celOutlineEnabled == 0) return color;

    vec2 texelSize = vec2(1.0 / float(settings.screenWidth), 1.0 / float(settings.screenHeight));
    float thickness = settings.celOutlineThickness;

    // Sample 3x3 depth neighborhood
    float d00 = texture(depthTexture, uv + vec2(-thickness, -thickness) * texelSize).r;
    float d10 = texture(depthTexture, uv + vec2( 0.0,      -thickness) * texelSize).r;
    float d20 = texture(depthTexture, uv + vec2( thickness, -thickness) * texelSize).r;
    float d01 = texture(depthTexture, uv + vec2(-thickness,  0.0)      * texelSize).r;
    float d21 = texture(depthTexture, uv + vec2( thickness,  0.0)      * texelSize).r;
    float d02 = texture(depthTexture, uv + vec2(-thickness,  thickness) * texelSize).r;
    float d12 = texture(depthTexture, uv + vec2( 0.0,       thickness) * texelSize).r;
    float d22 = texture(depthTexture, uv + vec2( thickness,  thickness) * texelSize).r;

    // Linearize depth for better edge detection (Vulkan reversed Z: 1=near, 0=far)
    // Simple linearization: use 1/d to amplify differences at distance

    // Sobel operators
    float sobelX = (d00 + 2.0 * d01 + d02) - (d20 + 2.0 * d21 + d22);
    float sobelY = (d00 + 2.0 * d10 + d20) - (d02 + 2.0 * d12 + d22);
    float edgeMagnitude = sqrt(sobelX * sobelX + sobelY * sobelY);

    float edge = smoothstep(settings.celOutlineThreshold * 0.5, settings.celOutlineThreshold, edgeMagnitude);
    return mix(color, settings.celOutlineColor, edge);
}

// Depth of Field — 16-tap Poisson disc blur weighted by Circle of Confusion
vec3 applyDoF(vec3 color, vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    float linearDepth = linearizeDepth(depth, settings.cameraNearPlane, settings.cameraFarPlane);

    // Circle of Confusion: signed distance from focal plane
    float coc = (linearDepth - settings.dofFocalDistance) / max(settings.dofFocalRange, 0.001);
    float cocSign = sign(coc);
    float cocMag = clamp(abs(coc), 0.0, 1.0);

    // Apply near/far blur strength
    float blurStrength = (cocSign < 0.0) ? settings.dofNearBlurStrength : settings.dofFarBlurStrength;
    cocMag *= blurStrength;

    // Debug CoC visualization
    if (settings.dofDebugCoC != 0u) {
        if (cocSign < 0.0)
            return mix(color, vec3(0.0, 0.0, 1.0), cocMag); // Near = blue
        else if (cocMag < 0.01)
            return mix(color, vec3(0.0, 1.0, 0.0), 0.5);     // In-focus = green
        else
            return mix(color, vec3(1.0, 0.0, 0.0), cocMag); // Far = red
    }

    if (cocMag < 0.01) return color; // In focus, skip blur

    // 16-tap Poisson disc kernel
    const vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216), vec2( 0.94558609, -0.76890725),
        vec2(-0.09418410, -0.92938870), vec2( 0.34495938,  0.29387760),
        vec2(-0.91588581,  0.45771432), vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543,  0.27676845), vec2( 0.97484398,  0.75648379),
        vec2( 0.44323325, -0.97511554), vec2( 0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023), vec2( 0.79197514,  0.19090188),
        vec2(-0.24188840,  0.99706507), vec2(-0.81409955,  0.91437590),
        vec2( 0.19984126,  0.78641367), vec2( 0.14383161, -0.14100790)
    );

    vec2 texelSize = 1.0 / vec2(settings.screenWidth, settings.screenHeight);
    float radius = settings.dofBokehSize * cocMag;

    vec3 result = vec3(0.0);
    float totalWeight = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 sampleUV = uv + poissonDisk[i] * texelSize * radius;
        float sampleDepth = texture(depthTexture, sampleUV).r;
        float sampleLinear = linearizeDepth(sampleDepth, settings.cameraNearPlane, settings.cameraFarPlane);
        float sampleCoC = abs(sampleLinear - settings.dofFocalDistance) / max(settings.dofFocalRange, 0.001);
        sampleCoC = clamp(sampleCoC, 0.0, 1.0);
        // Weight: don't let sharp samples bleed into blurry areas
        float w = (sampleCoC >= cocMag * 0.5) ? 1.0 : sampleCoC / max(cocMag * 0.5, 0.001);
        result += texture(sceneTexture, sampleUV).rgb * w;
        totalWeight += w;
    }
    return result / max(totalWeight, 0.001);
}

// Tilt-Shift — screen-space Y-driven blur with smoothstep band
vec3 applyTiltShift(vec3 color, vec2 uv) {
    float dist = abs(uv.y - settings.tiltShiftFocusY);
    float halfBand = settings.tiltShiftBandWidth * 0.5;
    float blur = smoothstep(halfBand * 0.5, halfBand, dist);
    if (blur < 0.01) return color;

    vec2 texelSize = 1.0 / vec2(settings.screenWidth, settings.screenHeight);
    float radius = settings.tiltShiftBlurAmount * blur;

    // 9-tap horizontal+vertical box blur
    vec3 result = vec3(0.0);
    float totalWeight = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize * radius;
            float w = 1.0 - 0.15 * (abs(float(x)) + abs(float(y)));
            result += texture(sceneTexture, uv + offset).rgb * w;
            totalWeight += w;
        }
    }
    return result / totalWeight;
}

void main() {
    vec2 uv = fragUV;

    // Resolution downscale: snap UV at the very start so all sampling uses the low-res grid
    uv = applyResolutionDownscale(uv);

    // CRT barrel distortion: warp UV before sampling if curvature is enabled
    if (settings.crtEnabled != 0 && settings.crtCurvature > 0.0) {
        vec2 centered = uv * 2.0 - 1.0;
        float r2 = dot(centered, centered);
        centered *= 1.0 + r2 * settings.crtCurvature;
        uv = centered * 0.5 + 0.5;
    }

    // VHS: applied early because it warps UVs for the raw image sampling
    vec3 color;
    if (settings.vhsEnabled != 0) {
        color = applyVHS(uv);
    } else if (settings.fxaaEnabled != 0 && settings.chromaticAberrationEnabled == 0) {
        color = applyFXAA(uv);
    } else {
        color = applyChromaticAberration(uv);
    }

    // Screen-space effects (HDR, before DoF/tone mapping)
    color = applySSAO(color, uv);
    color = applyContactShadows(color, uv);
    color = applyCaustics(color, uv);
    color = applyGodRays(color, uv);
    color = applyFogShafts(color, uv);

    // Apply Depth of Field (before tone mapping for HDR-correct blur)
    if (settings.dofEnabled != 0u) {
        color = applyDoF(color, uv);
    }

    // Apply Tilt-Shift miniature effect
    if (settings.tiltShiftEnabled != 0u) {
        color = applyTiltShift(color, uv);
    }

    // Apply tone mapping
    if (settings.toneMappingMode != TONEMAP_NONE) {
        color = applyToneMapping(color);
    }

    // Apply LUT color grading (after tone mapping, before color grading)
    color = applyLUT(color);

    // Apply color grading
    color = applyColorGrading(color);

    // Apply colorblind correction (after color grading, before vignette)
    color = applyColorblindCorrection(color);

    // Apply cel outline (Sobel edge detection on depth)
    color = applyCelOutline(color, uv);

    // Apply vignette
    color = applyVignette(color, uv);

    // Apply film grain
    color = applyFilmGrain(color, uv);

    // Retro: dithering (apply before quantization for best results)
    vec2 screenPos = fragUV * vec2(settings.screenWidth, settings.screenHeight);
    color = applyDithering(color, screenPos);

    // Retro: color quantization
    color = applyColorQuantization(color);

    // Color palette lock (after quantization)
    color = applyPaletteLock(color);

    // Full-screen stipple / dither (after palette lock, before gamma)
    color = applyStipple(color, screenPos);

    // Gamma correction
    color = pow(color, vec3(1.0 / settings.gamma));

    // CRT phosphor subpixel blending (before scanlines for best effect)
    color = applyCRTPhosphor(color, fragUV);

    // CRT scanlines (applied after gamma, as the last effect)
    color = applyCRT(color, fragUV);

    outColor = vec4(color, 1.0);
}
