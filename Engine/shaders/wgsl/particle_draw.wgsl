// GPU particle billboard draw (WebGPU port of gpu_particle.vert + .frag).
//
// Draws maxParticles instances. Reads the particle storage buffer by
// instance_index (WebGPU vertex shaders can read storage buffers), builds a
// camera-facing quad from the view matrix, and collapses dead slots to a
// clipped degenerate triangle. The fragment shader is a soft radial sprite
// tinted by the per-particle color with a life fade. No texture (matches the
// Vulkan path); textured particles are a later addition.

struct Particle {
    position : vec3<f32>,
    lifetime : f32,
    velocity : vec3<f32>,
    age : f32,
    color : vec4<f32>,
    size : f32,
    rotation : f32,
    gravityScale : f32,
    drag : f32,
};

struct ViewProj {
    view : mat4x4<f32>,
    proj : mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> ubo : ViewProj;
@group(0) @binding(1) var<storage, read> particles : array<Particle>;

struct VSOut {
    @builtin(position) pos : vec4<f32>,
    @location(0) uv : vec2<f32>,
    @location(1) color : vec4<f32>,
    @location(2) lifeT : f32,   // 0 at spawn -> 1 at death
};

@vertex
fn vs_main(@builtin(vertex_index) vid : u32,
           @builtin(instance_index) iid : u32) -> VSOut {
    var out : VSOut;
    let p = particles[iid];
    let lifetime = p.lifetime;

    // Dead slot (also catches NaN: !(x > 0) is true for NaN and for <= 0).
    if (!(lifetime > 0.0)) {
        out.pos = vec4<f32>(0.0, 0.0, 2.0, 1.0);   // clipped
        out.uv = vec2<f32>(0.0);
        out.color = vec4<f32>(0.0);
        out.lifeT = 1.0;
        return out;
    }

    // Two-triangle quad in [-0.5, 0.5] (function-local so the dynamic index is legal).
    var corners = array<vec2<f32>, 6>(
        vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, -0.5), vec2<f32>(0.5, 0.5),
        vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, 0.5), vec2<f32>(-0.5, 0.5)
    );
    var uvs = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0), vec2<f32>(0.0, 1.0)
    );

    var corner = corners[vid];
    let c = cos(p.rotation);
    let s = sin(p.rotation);
    corner = vec2<f32>(corner.x * c - corner.y * s, corner.x * s + corner.y * c);

    // Billboard using the camera right/up axes pulled from the view matrix.
    let camRight = vec3<f32>(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    let camUp    = vec3<f32>(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);
    let worldPos = p.position + (camRight * corner.x + camUp * corner.y) * p.size;

    out.pos = ubo.proj * ubo.view * vec4<f32>(worldPos, 1.0);
    out.uv = uvs[vid];
    out.color = p.color;
    let total = p.age + max(lifetime, 0.0001);
    out.lifeT = clamp(p.age / total, 0.0, 1.0);
    return out;
}

@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    // Soft circular falloff from the sprite center.
    let dist = length(in.uv - vec2<f32>(0.5));
    let falloff = smoothstep(0.5, 0.15, dist);

    // Quick ease-in, long ease-out so bursts pop then dissolve.
    let fadeIn = smoothstep(0.0, 0.08, in.lifeT);
    let fadeOut = 1.0 - smoothstep(0.55, 1.0, in.lifeT);
    let life = fadeIn * fadeOut;

    let alpha = in.color.a * falloff * life;
    if (alpha < 0.01) {
        discard;
    }
    return vec4<f32>(in.color.rgb, alpha);
}
