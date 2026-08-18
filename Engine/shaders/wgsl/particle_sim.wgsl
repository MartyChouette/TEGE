// GPU particle simulation (WebGPU). REFERENCE COPY of the embedded WGSL
// in WebGPUParticleSystem.cpp -- keep the two in sync when editing.

struct Particle {
    position : vec3<f32>, lifetime : f32,
    velocity : vec3<f32>, age : f32,
    color : vec4<f32>,
    size : f32, rotation : f32, gravityScale : f32, drag : f32,
    spriteParams : vec4<f32>,   // x=sprite card, y=softness, z=texIndex(unused on web), w=pad
    collision : vec4<f32>,      // x=bounciness, y=slide keep, z=extra radius, w=collide flag
};
struct EmitterParams {
    emitterPosition : vec3<f32>, deltaTime : f32,
    emitterDirection : vec3<f32>, emitterSpread : f32,
    gravity : vec3<f32>, damping : f32,
    windForce : vec3<f32>, maxParticles : u32,
    startColor : vec4<f32>, endColor : vec4<f32>,
    startSize : f32, endSize : f32, maxLifetime : f32, spawnRate : f32,
    turbulenceStrength : f32, turbulenceFrequency : f32, frameNumber : u32, colliderCount : u32,
};
struct ColliderShape {
    posKind : vec4<f32>,   // xyz = center, w = kind (0 box, 1 sphere, 2 capsule)
    rot : vec4<f32>,       // rotation quaternion
    dims : vec4<f32>,      // box: half extents | sphere: x=r | capsule: x=r, y=halfHeight
};
@group(0) @binding(0) var<storage, read_write> particles : array<Particle>;
@group(0) @binding(1) var<uniform> params : EmitterParams;
@group(0) @binding(2) var<storage, read> shapes : array<ColliderShape>;
fn quatRot(q : vec4<f32>, v : vec3<f32>) -> vec3<f32> {
    return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
}
fn quatRotInv(q : vec4<f32>, v : vec3<f32>) -> vec3<f32> {
    return quatRot(vec4<f32>(-q.xyz, q.w), v);
}
fn hashU(n0 : u32) -> f32 {
    var n = n0;
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return f32(n & 0x7FFFFFFFu) / f32(0x7FFFFFFF);
}
fn turb(pos : vec3<f32>, freq : f32, strength : f32, seed : u32) -> vec3<f32> {
    let p = pos * freq;
    let nx = hashU(seed + u32(p.x * 73.0 + p.y * 157.0 + p.z * 113.0));
    let ny = hashU(seed + u32(p.x * 97.0 + p.y * 131.0 + p.z * 89.0));
    let nz = hashU(seed + u32(p.x * 61.0 + p.y * 173.0 + p.z * 149.0));
    return (vec3<f32>(nx, ny, nz) * 2.0 - 1.0) * strength;
}
@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid : vec3<u32>) {
    let idx = gid.x;
    if (idx >= params.maxParticles) { return; }
    var p = particles[idx];
    if (p.lifetime <= 0.0) { return; }
    p.lifetime = p.lifetime - params.deltaTime;
    p.age = p.age + params.deltaTime;
    if (p.lifetime <= 0.0) { particles[idx] = p; return; }
    if (p.collision.w > 1.5) {   // stain: ages in place, no physics
        particles[idx] = p;
        return;
    }
    var acc = params.gravity * p.gravityScale + params.windForce;
    if (params.turbulenceStrength > 0.0) {
        acc = acc + turb(p.position, params.turbulenceFrequency, params.turbulenceStrength, idx + params.frameNumber);
    }
    p.velocity = p.velocity + acc * params.deltaTime;
    p.velocity = p.velocity * (1.0 - (params.damping + p.drag) * params.deltaTime);

    // Substep the motion when colliders exist so fast particles can't tunnel
    // through thin shapes in one frame step.
    let substeps = select(1u, 4u, params.colliderCount > 0u && p.collision.w > 0.5);
    let stepDt = params.deltaTime / f32(substeps);
    for (var ss = 0u; ss < substeps; ss = ss + 1u) {
    p.position = p.position + p.velocity * stepDt;

    // Collision: push out of world shapes and bounce (restitution 0.35, 70% slide).
    if (p.collision.w > 0.5) {
    for (var s = 0u; s < params.colliderCount && s < 32u; s = s + 1u) {
        let sc = shapes[s].posKind.xyz;
        let kind = i32(shapes[s].posKind.w + 0.5);
        var n = vec3<f32>(0.0);
        var pen = 0.0;
        let skin = 0.02 + p.collision.z;
        if (kind == 1) {
            let d = p.position - sc;
            let dist = length(d);
            let r = shapes[s].dims.x + skin;
            if (dist < r) {
                if (dist > 1e-6) { n = d / dist; } else { n = vec3<f32>(0.0, 1.0, 0.0); }
                pen = r - dist;
            }
        } else if (kind == 2) {
            let axis = quatRot(shapes[s].rot, vec3<f32>(0.0, 1.0, 0.0));
            let d = p.position - sc;
            let t = clamp(dot(d, axis), -shapes[s].dims.y, shapes[s].dims.y);
            let closest = sc + axis * t;
            let rd = p.position - closest;
            let dist = length(rd);
            let r = shapes[s].dims.x + skin;
            if (dist < r) {
                if (dist > 1e-6) { n = rd / dist; } else { n = vec3<f32>(0.0, 1.0, 0.0); }
                pen = r - dist;
            }
        } else {
            let lp = quatRotInv(shapes[s].rot, p.position - sc);
            let h = shapes[s].dims.xyz + vec3<f32>(skin);
            let a = abs(lp);
            if (a.x < h.x && a.y < h.y && a.z < h.z) {
                let depth = h - a;
                var ln = vec3<f32>(0.0);
                if (depth.x <= depth.y && depth.x <= depth.z) {
                    ln = vec3<f32>(sign(lp.x), 0.0, 0.0); pen = depth.x;
                } else if (depth.y <= depth.x && depth.y <= depth.z) {
                    ln = vec3<f32>(0.0, sign(lp.y), 0.0); pen = depth.y;
                } else {
                    ln = vec3<f32>(0.0, 0.0, sign(lp.z)); pen = depth.z;
                }
                n = quatRot(shapes[s].rot, ln);
            }
        }
        if (pen > 0.0) {
            p.position = p.position + n * pen;
            let vn = dot(p.velocity, n);
            if (vn < 0.0) {
                let vTan = p.velocity - vn * n;
                p.velocity = vTan * p.collision.y - n * (vn * p.collision.x);
            }
        }
    }
    }   // collide flag
    }   // substeps
    particles[idx] = p;
}
