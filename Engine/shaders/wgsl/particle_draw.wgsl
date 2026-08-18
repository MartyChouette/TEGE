// GPU particle billboard draw (WebGPU). REFERENCE COPY of the embedded WGSL
// in WebGPUParticleSystem.cpp -- keep the two in sync when editing.

struct Particle {
    position : vec3<f32>, lifetime : f32,
    velocity : vec3<f32>, age : f32,
    color : vec4<f32>,
    size : f32, rotation : f32, gravityScale : f32, drag : f32,
    spriteParams : vec4<f32>,   // x=sprite card, y=softness, z=texIndex(unused on web), w=pad
    collision : vec4<f32>,      // x=bounciness, y=slide keep, z=extra radius, w=collide flag
};
struct ViewProj { view : mat4x4<f32>, proj : mat4x4<f32>, };
@group(0) @binding(0) var<uniform> ubo : ViewProj;
@group(0) @binding(1) var<storage, read> particles : array<Particle>;
struct VSOut {
    @builtin(position) pos : vec4<f32>,
    @location(0) uv : vec2<f32>,
    @location(1) color : vec4<f32>,
    @location(2) lifeT : f32,
    @location(3) sprite : vec2<f32>,   // x=card, y=softness
};
@vertex
fn vs_main(@builtin(vertex_index) vid : u32, @builtin(instance_index) iid : u32) -> VSOut {
    var out : VSOut;
    let p = particles[iid];
    let lifetime = p.lifetime;
    if (!(lifetime > 0.0)) {
        out.pos = vec4<f32>(0.0, 0.0, 2.0, 1.0);
        out.uv = vec2<f32>(0.0); out.color = vec4<f32>(0.0); out.lifeT = 1.0;
        out.sprite = vec2<f32>(0.0, 1.0);
        return out;
    }
    var corners = array<vec2<f32>, 6>(
        vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, -0.5), vec2<f32>(0.5, 0.5),
        vec2<f32>(-0.5, -0.5), vec2<f32>(0.5, 0.5), vec2<f32>(-0.5, 0.5));
    var uvs = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
        vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0), vec2<f32>(0.0, 1.0));
    var corner = corners[vid];
    let c = cos(p.rotation); let s = sin(p.rotation);
    corner = vec2<f32>(corner.x * c - corner.y * s, corner.x * s + corner.y * c);
    var worldPos : vec3<f32>;
    if (p.collision.w > 1.5) {   // stain: lie on the surface (velocity = normal)
        let n = normalize(p.velocity);
        let helper = select(vec3<f32>(1.0, 0.0, 0.0), vec3<f32>(0.0, 1.0, 0.0), abs(n.y) < 0.99);
        let t = normalize(cross(n, helper));
        let b = cross(n, t);
        worldPos = p.position + (t * corner.x + b * corner.y) * p.size;
    } else {
        let camRight = vec3<f32>(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
        let camUp    = vec3<f32>(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);
        worldPos = p.position + (camRight * corner.x + camUp * corner.y) * p.size;
    }
    out.pos = ubo.proj * ubo.view * vec4<f32>(worldPos, 1.0);
    out.uv = uvs[vid];
    out.color = p.color;
    let total = p.age + max(lifetime, 0.0001);
    out.lifeT = clamp(p.age / total, 0.0, 1.0);
    out.sprite = vec2<f32>(p.spriteParams.x, p.spriteParams.y);
    return out;
}
fn spriteMask(card : i32, uv : vec2<f32>, softness : f32) -> f32 {
    let c = uv - vec2<f32>(0.5);
    var w = mix(0.02, 0.35, clamp(softness, 0.0, 1.0));
    var d : f32;
    if (card == 2) {                       // square
        d = max(abs(c.x), abs(c.y));
    } else if (card == 3) {                // streak
        d = length(vec2<f32>(c.x * 3.0, c.y));
    } else if (card == 4) {                // 4-point star
        let ang = atan2(c.y, c.x);
        d = length(c) * (1.0 + 1.2 * pow(abs(sin(2.0 * ang)), 2.0));
    } else {                               // circles (0 soft / 1 hard; 5 texture falls back to soft)
        d = length(c);
        if (card == 1) { w = mix(0.02, 0.10, clamp(softness, 0.0, 1.0)); }
    }
    return 1.0 - smoothstep(0.5 - w, 0.5, d);
}

@fragment
fn fs_main(in : VSOut) -> @location(0) vec4<f32> {
    let fadeIn = smoothstep(0.0, 0.08, in.lifeT);
    let fadeOut = 1.0 - smoothstep(0.55, 1.0, in.lifeT);
    let life = fadeIn * fadeOut;
    let mask = spriteMask(i32(in.sprite.x + 0.5), in.uv, in.sprite.y);
    let alpha = in.color.a * mask * life;
    if (alpha < 0.01) { discard; }
    return vec4<f32>(in.color.rgb, alpha);
}
