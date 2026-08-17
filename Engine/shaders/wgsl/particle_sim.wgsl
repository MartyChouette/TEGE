// GPU particle simulation (WebGPU port of particle_simulate.comp).
//
// One invocation per particle: integrate physics and decrement lifetime. The
// draw pass draws all maxParticles instances and collapses dead slots in the
// vertex shader, so this port omits the alive-index append buffer the Vulkan
// path keeps for indirect draw (nothing consumes it yet).
//
// Struct layout matches Effects::GPUParticle (64 bytes) and the GLSL comp:
// vec3+f32 pairs pack (vec3 is align 16 / size 12, the f32 sits at offset 12).

struct Particle {
    position : vec3<f32>,
    lifetime : f32,        // remaining seconds; <= 0 = dead
    velocity : vec3<f32>,
    age : f32,             // seconds since spawn
    color : vec4<f32>,
    size : f32,
    rotation : f32,
    gravityScale : f32,    // 0 = float, <0 = rise (smoke)
    drag : f32,
};

struct EmitterParams {
    emitterPosition : vec3<f32>,
    deltaTime : f32,
    emitterDirection : vec3<f32>,
    emitterSpread : f32,
    gravity : vec3<f32>,
    damping : f32,
    windForce : vec3<f32>,
    maxParticles : u32,
    startColor : vec4<f32>,
    endColor : vec4<f32>,
    startSize : f32,
    endSize : f32,
    maxLifetime : f32,
    spawnRate : f32,
    turbulenceStrength : f32,
    turbulenceFrequency : f32,
    frameNumber : u32,
    pad0 : f32,
};

@group(0) @binding(0) var<storage, read_write> particles : array<Particle>;
@group(0) @binding(1) var<uniform> params : EmitterParams;

fn hashU(n0 : u32) -> f32 {
    var n = n0;
    n = (n << 13u) ^ n;
    n = n * (n * n * 15731u + 789221u) + 1376312589u;
    return f32(n & 0x7FFFFFFFu) / f32(0x7FFFFFFF);
}

fn turbulence(pos : vec3<f32>, freq : f32, strength : f32, seed : u32) -> vec3<f32> {
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
    if (p.lifetime <= 0.0) {
        particles[idx] = p;   // died this frame
        return;
    }

    // Per-particle gravity scale makes presets differ (sparks fall, smoke rises).
    var acceleration = params.gravity * p.gravityScale + params.windForce;
    if (params.turbulenceStrength > 0.0) {
        acceleration = acceleration + turbulence(p.position, params.turbulenceFrequency,
                                                 params.turbulenceStrength, idx + params.frameNumber);
    }

    p.velocity = p.velocity + acceleration * params.deltaTime;
    p.velocity = p.velocity * (1.0 - (params.damping + p.drag) * params.deltaTime);
    p.position = p.position + p.velocity * params.deltaTime;

    // Color/size stay baked at spawn; the draw fragment shader does the life fade.
    particles[idx] = p;
}
