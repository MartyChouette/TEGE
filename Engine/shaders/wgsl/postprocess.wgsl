// Enjin Engine — Full-screen post-processing (WebGPU / WGSL)
// ACES tonemapping + simplified FXAA + colorblind correction + brightness/contrast.

@group(0) @binding(0) var sceneTexture: texture_2d<f32>;
@group(0) @binding(1) var sceneSampler: sampler;

struct PostProcessParams {
    colorblindMode: u32,
    colorblindStrength: f32,
    brightness: f32,
    contrast: f32,
    previewEffect: u32,
    previewDivider: f32,
    _pad0: f32,
    _pad1: f32,
};
@group(0) @binding(2) var<uniform> params: PostProcessParams;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) uv: vec2<f32>,
};

// Full-screen triangle (no vertex buffer needed — generate from vertex ID)
@vertex
fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
    var out: VertexOutput;
    let x = f32(i32(vertexIndex & 1u) * 4 - 1);
    let y = f32(i32(vertexIndex >> 1u) * 4 - 1);
    out.position = vec4<f32>(x, y, 0.0, 1.0);
    out.uv = vec2<f32>((x + 1.0) * 0.5, (1.0 - y) * 0.5);
    return out;
}

// ACES tone mapping (simple fit)
fn aces_tonemap(color: vec3<f32>) -> vec3<f32> {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

fn luminance(c: vec3<f32>) -> f32 {
    return dot(c, vec3<f32>(0.299, 0.587, 0.114));
}

// Simplified FXAA — smooths high-contrast edges
// All textureSample calls must be in uniform control flow (WGSL rule),
// so we sample all 6 taps unconditionally and use select() instead of early return.
fn fxaa(uv: vec2<f32>, texelSize: vec2<f32>) -> vec3<f32> {
    let rgbM = textureSample(sceneTexture, sceneSampler, uv).rgb;
    let rgbN = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>(0.0, -texelSize.y)).rgb;
    let rgbS = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>(0.0,  texelSize.y)).rgb;
    let rgbE = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>( texelSize.x, 0.0)).rgb;
    let rgbW = textureSample(sceneTexture, sceneSampler, uv + vec2<f32>(-texelSize.x, 0.0)).rgb;

    let lumM = luminance(pow(aces_tonemap(rgbM), vec3<f32>(1.0 / 2.2)));
    let lumN = luminance(pow(aces_tonemap(rgbN), vec3<f32>(1.0 / 2.2)));
    let lumS = luminance(pow(aces_tonemap(rgbS), vec3<f32>(1.0 / 2.2)));
    let lumE = luminance(pow(aces_tonemap(rgbE), vec3<f32>(1.0 / 2.2)));
    let lumW = luminance(pow(aces_tonemap(rgbW), vec3<f32>(1.0 / 2.2)));

    let lumMin = min(lumM, min(min(lumN, lumS), min(lumE, lumW)));
    let lumMax = max(lumM, max(max(lumN, lumS), max(lumE, lumW)));
    let lumRange = lumMax - lumMin;

    // Determine edge direction and sample neighbor — all in uniform flow
    let edgeH = abs(lumN + lumS - 2.0 * lumM);
    let edgeV = abs(lumE + lumW - 2.0 * lumM);
    let isHorizontal = edgeH > edgeV;

    let gradNS = select(lumS - lumM, lumN - lumM, abs(lumN - lumM) > abs(lumS - lumM));
    let gradEW = select(lumW - lumM, lumE - lumM, abs(lumE - lumM) > abs(lumW - lumM));

    let hDir = select(vec2<f32>(0.0, texelSize.y), vec2<f32>(0.0, -texelSize.y), abs(lumN - lumM) > abs(lumS - lumM));
    let vDir = select(vec2<f32>(texelSize.x, 0.0), vec2<f32>(-texelSize.x, 0.0), abs(lumW - lumM) > abs(lumE - lumM));
    let blendDir = select(vDir, hDir, isHorizontal);

    let rgbNeighbor = textureSample(sceneTexture, sceneSampler, uv + blendDir).rgb;

    // Only blend if contrast is high enough; otherwise return center pixel
    let needsAA = lumRange >= max(0.0312, lumMax * 0.125);
    let blendFactor = select(0.0, clamp(lumRange / lumMax, 0.0, 0.75) * 0.5, needsAA);
    return mix(rgbM, rgbNeighbor, blendFactor);
}

// Daltonization — colorblind correction (Brettel/Machado approach)
// Simulates what a colorblind person sees, then redistributes lost
// information into channels they CAN perceive.
fn applyColorblindCorrection(color: vec3<f32>) -> vec3<f32> {
    let mode = params.colorblindMode;
    if (mode == 0u) { return color; }

    // Achromatopsia (complete color blindness) — convert to grayscale.
    // Achromatomaly (mode 8, weak color vision) — same at half strength.
    if (mode == 7u || mode == 8u) {
        let gray = dot(color, vec3<f32>(0.299, 0.587, 0.114));
        var achromaStrength = params.colorblindStrength;
        if (mode == 8u) { achromaStrength = achromaStrength * 0.5; }
        return mix(color, vec3<f32>(gray), achromaStrength);
    }

    // Simulation matrices for full deficiency (Brettel/Machado)
    var simR: vec3<f32>;
    var simG: vec3<f32>;
    var simB: vec3<f32>;

    // Determine effective strength: anomalous trichromacy uses 0.6x
    var strength = params.colorblindStrength;
    var baseMode = mode;
    if (mode >= 4u && mode <= 6u) {
        strength *= 0.6;
        baseMode = mode - 3u; // Map anomalous to corresponding full deficiency
    }

    // Protanopia / Protanomaly (red-blind)
    if (baseMode == 1u) {
        simR = vec3<f32>(0.152286, 0.114503, -0.003882);
        simG = vec3<f32>(1.052583, 0.786281, -0.048116);
        simB = vec3<f32>(-0.204868, 0.099216, 1.051998);
    }
    // Deuteranopia / Deuteranomaly (green-blind)
    else if (baseMode == 2u) {
        simR = vec3<f32>(0.367322, 0.280085, -0.011820);
        simG = vec3<f32>(0.860646, 0.672501, 0.042940);
        simB = vec3<f32>(-0.227968, 0.047413, 0.968881);
    }
    // Tritanopia / Tritanomaly (blue-blind)
    else {
        simR = vec3<f32>(1.255528, -0.078411, 0.004733);
        simG = vec3<f32>(-0.076749, 0.930809, 0.691367);
        simB = vec3<f32>(-0.178779, 0.147602, 0.303900);
    }

    // Simulate colorblind perception
    let simulated = vec3<f32>(
        dot(color, simR),
        dot(color, simG),
        dot(color, simB)
    );

    // Error: what information is lost
    let error = color - simulated;

    // Redistribute error into visible channels (green and blue get red error, etc.)
    var corrected = color;
    corrected.g = corrected.g + error.r * 0.7 + error.g * 0.0;
    corrected.b = corrected.b + error.r * 0.7 + error.b * 0.0;

    return mix(color, saturate(corrected), strength);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let texDim = vec2<f32>(textureDimensions(sceneTexture));
    let texelSize = vec2<f32>(1.0 / texDim.x, 1.0 / texDim.y);

    var color = fxaa(in.uv, texelSize);
    color = aces_tonemap(color);

    // Options preview split: left of the divider shows the frame WITHOUT the
    // previewed effect (5 = colorblind, 6 = brightness/contrast).
    let previewLeft = params.previewEffect != 0u && in.uv.x < params.previewDivider;

    // Apply brightness (additive) and contrast (multiplicative)
    if (!(previewLeft && params.previewEffect == 6u)) {
        color = (color - 0.5) * params.contrast + 0.5 + params.brightness;
    }

    // Colorblind correction (Daltonization)
    if (!(previewLeft && params.previewEffect == 5u)) {
        color = applyColorblindCorrection(color);
    }

    color = pow(color, vec3<f32>(1.0 / 2.2));

    // Divider line so the comparison reads as intentional
    if (params.previewEffect != 0u && abs(in.uv.x - params.previewDivider) < texelSize.x) {
        color = vec3<f32>(1.0, 1.0, 1.0);
    }
    return vec4<f32>(saturate(color), 1.0);
}
