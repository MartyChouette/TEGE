// Enjin Engine — Full-screen post-processing (WebGPU / WGSL)
// ACES tonemapping + simplified FXAA anti-aliasing.

@group(0) @binding(0) var sceneTexture: texture_2d<f32>;
@group(0) @binding(1) var sceneSampler: sampler;

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

    if (lumRange < max(0.0312, lumMax * 0.125)) {
        return rgbM;
    }

    let edgeH = abs(lumN + lumS - 2.0 * lumM);
    let edgeV = abs(lumE + lumW - 2.0 * lumM);
    let isHorizontal = edgeH > edgeV;

    var blendDir = vec2<f32>(0.0);
    if (isHorizontal) {
        let gradS = lumS - lumM;
        let gradN = lumN - lumM;
        if (abs(gradN) > abs(gradS)) { blendDir = vec2<f32>(0.0, -texelSize.y); }
        else { blendDir = vec2<f32>(0.0, texelSize.y); }
    } else {
        let gradE = lumE - lumM;
        let gradW = lumW - lumM;
        if (abs(gradW) > abs(gradE)) { blendDir = vec2<f32>(-texelSize.x, 0.0); }
        else { blendDir = vec2<f32>(texelSize.x, 0.0); }
    }

    let rgbNeighbor = textureSample(sceneTexture, sceneSampler, uv + blendDir).rgb;
    let blendFactor = clamp(lumRange / lumMax, 0.0, 0.75) * 0.5;
    return mix(rgbM, rgbNeighbor, blendFactor);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let texDim = vec2<f32>(textureDimensions(sceneTexture));
    let texelSize = vec2<f32>(1.0 / texDim.x, 1.0 / texDim.y);

    var color = fxaa(in.uv, texelSize);
    color = aces_tonemap(color);
    color = pow(color, vec3<f32>(1.0 / 2.2));
    return vec4<f32>(color, 1.0);
}
