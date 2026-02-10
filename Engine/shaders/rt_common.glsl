// rt_common.glsl — Shared structs, RNG, and hemisphere sampling for RT shaders

// PCG random number generator
uint pcg_hash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Generate uniform random float in [0, 1)
float rand_float(inout uint seed) {
    seed = pcg_hash(seed);
    return float(seed) / 4294967296.0;
}

// Generate 2D uniform random sample
vec2 rand_vec2(inout uint seed) {
    return vec2(rand_float(seed), rand_float(seed));
}

// Cosine-weighted hemisphere sampling (returns direction in tangent space)
vec3 cosine_hemisphere_sample(vec2 u) {
    float r = sqrt(u.x);
    float theta = 2.0 * 3.14159265359 * u.y;
    float x = r * cos(theta);
    float y = r * sin(theta);
    float z = sqrt(max(0.0, 1.0 - u.x));
    return vec3(x, y, z);
}

// Uniform hemisphere sampling
vec3 uniform_hemisphere_sample(vec2 u) {
    float z = u.x;
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = 2.0 * 3.14159265359 * u.y;
    return vec3(r * cos(phi), r * sin(phi), z);
}

// Build orthonormal basis from normal
void build_onb(vec3 N, out vec3 T, out vec3 B) {
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    T = normalize(cross(up, N));
    B = cross(N, T);
}

// Transform direction from tangent space to world space
vec3 tangent_to_world(vec3 dir, vec3 N, vec3 T, vec3 B) {
    return dir.x * T + dir.y * B + dir.z * N;
}

// Initialize RNG seed from pixel coords and frame number
uint init_rng(uvec2 pixel, uint frame) {
    return pcg_hash(pixel.x + pixel.y * 8192u + frame * 65536u);
}

// Material data for closest-hit shading
struct RTMaterial {
    vec3 baseColor;
    float metallic;
    vec3 emissive;
    float roughness;
};

// Light data for shadow rays
struct RTLight {
    vec3 position;
    float range;
    vec3 direction;
    float intensity;
    vec3 color;
    int type;  // 0=directional, 1=point, 2=spot
};
