// Basic PBR shader for WebGPU (WGSL)
// Handles: position, normal, basic directional lighting, base color

// Override constants — WebGPU equivalent of Vulkan specialization constants.
override SPEC_HAS_BASE_COLOR_TEX: u32 = 1u;
override SPEC_HAS_NORMAL_TEX: u32 = 1u;
override SPEC_HAS_METALLIC_TEX: u32 = 1u;
override SPEC_HAS_EMISSIVE_TEX: u32 = 1u;
override SPEC_HAS_HEIGHT_TEX: u32 = 1u;
override SPEC_DOUBLE_SIDED: u32 = 1u;
override SPEC_FLAT_SHADING: u32 = 0u;
override SPEC_ALPHA_MODE: u32 = 0u;

struct ViewUniforms {
    view: mat4x4<f32>,
    proj: mat4x4<f32>,
    cameraPos: vec3<f32>,
    _pad: f32,
    lightDir: vec3<f32>,
    lightIntensity: f32,
    lightColor: vec3<f32>,
    ambientIntensity: f32,
    ambientColor: vec3<f32>,
    _pad2: f32,
};

struct ObjectUniforms {
    model: mat4x4<f32>,
    baseColor: vec3<f32>,
    metallic: f32,
    emissive: vec3<f32>,
    roughness: f32,
    opacity: f32,
    _pad: vec3<f32>,
};

@group(0) @binding(0) var<uniform> view: ViewUniforms;
@group(1) @binding(0) var<uniform> object: ObjectUniforms;

struct VertexInput {
    @location(0) position: vec3<f32>,
    @location(1) normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};

struct VertexOutput {
    @builtin(position) clip_position: vec4<f32>,
    @location(0) world_pos: vec3<f32>,
    @location(1) world_normal: vec3<f32>,
    @location(2) uv: vec2<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let world_pos = object.model * vec4<f32>(in.position, 1.0);
    out.clip_position = view.proj * view.view * world_pos;
    out.world_pos = world_pos.xyz;
    out.world_normal = normalize((object.model * vec4<f32>(in.normal, 0.0)).xyz);
    out.uv = in.uv;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let N = normalize(in.world_normal);
    let L = normalize(-view.lightDir);
    let V = normalize(view.cameraPos - in.world_pos);
    let H = normalize(L + V);

    // Diffuse (Lambert)
    let ndotl = max(dot(N, L), 0.0);
    let diffuse = object.baseColor * ndotl * view.lightColor * view.lightIntensity;

    // Specular (Blinn-Phong approx)
    let ndoth = max(dot(N, H), 0.0);
    let shininess = mix(8.0, 256.0, 1.0 - object.roughness);
    let spec = pow(ndoth, shininess) * (1.0 - object.roughness) * 0.5;
    let specular = view.lightColor * spec * view.lightIntensity;

    // Ambient
    let ambient = view.ambientColor * view.ambientIntensity * object.baseColor;

    // Emissive
    let emissive = object.emissive;

    let color = ambient + diffuse + specular + emissive;

    // Simple tone mapping
    let mapped = color / (color + vec3<f32>(1.0));

    return vec4<f32>(mapped, object.opacity);
}
