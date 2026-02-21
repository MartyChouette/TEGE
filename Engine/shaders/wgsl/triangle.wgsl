// Enjin Engine — Basic triangle/mesh shader (WebGPU / WGSL)
// Equivalent to Engine/shaders/triangle.vert + triangle.frag

// Bind group 0: Per-frame uniforms
struct ViewProjection {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    viewPos: vec3<f32>,
    time: f32,
};
@group(0) @binding(0) var<uniform> viewProj: ViewProjection;

// Bind group 1: Per-object uniforms (push constant equivalent)
struct ObjectData {
    model: mat4x4<f32>,
    baseColor: vec3<f32>,
    metallic: f32,
    emissiveColor: vec3<f32>,
    roughness: f32,
    emissiveStrength: f32,
    opacity: f32,
    alphaCutoff: f32,
    flags: i32,
    parallaxScale: f32,
};
@group(1) @binding(0) var<uniform> object: ObjectData;

// Bind group 2: Textures
@group(2) @binding(0) var baseColorTexture: texture_2d<f32>;
@group(2) @binding(1) var baseColorSampler: sampler;

// Vertex input/output
struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_normal: vec3<f32>,
    @location(1) uv: vec2<f32>,
    @location(2) world_pos: vec3<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;

    let world_pos = object.model * vec4<f32>(in.position, 1.0);
    out.clip_position = viewProj.proj * viewProj.view * world_pos;
    out.world_pos = world_pos.xyz;

    // Transform normal (simplified — no inverse transpose for uniform scale)
    let normal_mat = mat3x3<f32>(
        object.model[0].xyz,
        object.model[1].xyz,
        object.model[2].xyz
    );
    out.world_normal = normalize(normal_mat * in.normal);
    out.uv = in.uv;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Sample base color texture
    var color = textureSample(baseColorTexture, baseColorSampler, in.uv);
    color = vec4<f32>(color.rgb * object.baseColor, color.a * object.opacity);

    // Simple directional light (sun direction hardcoded for now)
    let lightDir = normalize(vec3<f32>(0.5, 1.0, 0.3));
    let ndotl = max(dot(in.world_normal, lightDir), 0.0);
    let ambient = vec3<f32>(0.1, 0.1, 0.15);
    let lit = color.rgb * (ambient + ndotl * 0.9);

    // Emissive
    let emissive = object.emissiveColor * object.emissiveStrength;

    // Alpha cutoff
    if (color.a < object.alphaCutoff) {
        discard;
    }

    return vec4<f32>(lit + emissive, color.a);
}
