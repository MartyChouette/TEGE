#version 450

// GPU-compute particle billboard vertex shader.
//
// The particle SSBO written by particle_simulate.comp is bound directly as an
// instance-rate vertex buffer (stride = 64B GPUParticle). No CPU copy, no
// readback: we draw maxParticles instances and emit degenerate quads for dead
// slots (age >= lifetime). Simulate() already emits the COMPUTE -> VERTEX_INPUT
// barrier for exactly this consumption.

// Per-instance GPUParticle fields (must match Effects::GPUParticle / the comp)
layout(location = 0) in vec4 inPosLife;   // xyz = world position, w = lifetime
layout(location = 1) in vec4 inVelAge;    // xyz = velocity,       w = age
layout(location = 2) in vec4 inColor;     // premixed start->end color (incl alpha)
layout(location = 3) in vec4 inSizeRot;   // x = size, y = rotation (radians)

// Shared set-0 view/projection UBO (same binding the main pass uses)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

// Two-triangle quad in [-0.5, 0.5]
const vec2 kCorners[6] = vec2[6](
    vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.5, 0.5),
    vec2(-0.5, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5)
);
const vec2 kUVs[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
);

void main() {
    float lifetime = inPosLife.w;
    float age = inVelAge.w;

    // Dead slot: collapse the quad (clipped, zero area, zero fill cost)
    if (lifetime <= 0.0 || age >= lifetime) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        fragUV = vec2(0.0);
        fragColor = vec4(0.0);
        return;
    }

    vec2 corner = kCorners[gl_VertexIndex];

    // In-plane rotation
    float c = cos(inSizeRot.y);
    float s = sin(inSizeRot.y);
    corner = vec2(corner.x * c - corner.y * s, corner.x * s + corner.y * c);

    // Billboard using camera axes from the view matrix
    vec3 camRight = vec3(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
    vec3 camUp    = vec3(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);
    vec3 worldPos = inPosLife.xyz + (camRight * corner.x + camUp * corner.y) * inSizeRot.x;

    gl_Position = ubo.proj * ubo.view * vec4(worldPos, 1.0);
    fragUV = kUVs[gl_VertexIndex];
    fragColor = inColor;
}
