#version 450

// Gaussian splat billboard expansion. Standard 3DGS rasterization math:
// build the 3D covariance from the splat's scale + rotation, project it to a
// 2D screen-space covariance through the view rotation and the perspective
// Jacobian, eigen-decompose the 2x2, and stretch a quad over +-3 sigma along
// the eigenvectors. The fragment shader evaluates the gaussian falloff in
// sigma space. Splats draw back-to-front (CPU-sorted, throttled) with alpha
// blending, depth-tested against the scene but not writing depth.

// Per-instance SplatInstance fields (64B, must match Assets::SplatInstance)
layout(location = 0) in vec4 inPosOpacity;   // xyz world position, w opacity
layout(location = 1) in vec4 inColor;        // rgb base color
layout(location = 2) in vec4 inScale;        // xyz gaussian scale (world units)
layout(location = 3) in vec4 inRot;          // quaternion x,y,z,w

// Shared set-0 view/projection UBO (same binding the main pass uses)
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// model = the entity's world transform; params = viewport W, H, opacityScale,
// splatScale
layout(push_constant) uniform SplatPush {
    mat4 model;
    vec4 params;
} pc;

layout(location = 0) out vec2 sigmaPos;   // quad coord in sigma units
layout(location = 1) out vec4 fragColor;  // rgb + final opacity

const vec2 kCorners[6] = vec2[6](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
);

void main() {
    float opacity = inPosOpacity.w * pc.params.z;
    vec4 world = pc.model * vec4(inPosOpacity.xyz, 1.0);
    vec4 viewPos = ubo.view * world;

    // Behind or too close to the camera: emit a degenerate quad
    if (viewPos.z > -0.05 || opacity < 0.004) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        sigmaPos = vec2(0.0);
        fragColor = vec4(0.0);
        return;
    }

    // 3D covariance: M = R * S (model rotation folded in through its 3x3)
    vec4 q = inRot;
    mat3 R = mat3(
        1.0 - 2.0*(q.y*q.y + q.z*q.z), 2.0*(q.x*q.y + q.w*q.z),       2.0*(q.x*q.z - q.w*q.y),
        2.0*(q.x*q.y - q.w*q.z),       1.0 - 2.0*(q.x*q.x + q.z*q.z), 2.0*(q.y*q.z + q.w*q.x),
        2.0*(q.x*q.z + q.w*q.y),       2.0*(q.y*q.z - q.w*q.x),       1.0 - 2.0*(q.x*q.x + q.y*q.y)
    );
    vec3 s = inScale.xyz * pc.params.w;
    mat3 modelRot = mat3(pc.model);   // includes model scale; fine for rigid+uniform
    mat3 M = modelRot * R * mat3(s.x, 0, 0, 0, s.y, 0, 0, 0, s.z);
    mat3 sigma = M * transpose(M);

    // Project: T = J * W, cov2d = T * sigma * T^T (EWA splatting)
    float W = pc.params.x;
    float H = pc.params.y;
    float fx = ubo.proj[0][0] * 0.5 * W;
    float fy = ubo.proj[1][1] * 0.5 * H;
    float tz = viewPos.z;
    mat3 Wv = mat3(ubo.view);
    mat3 J = mat3(
        fx / tz, 0.0, 0.0,
        0.0, fy / tz, 0.0,
        -fx * viewPos.x / (tz * tz), -fy * viewPos.y / (tz * tz), 0.0
    );
    mat3 T = J * Wv;
    mat3 cov3 = T * sigma * transpose(T);
    // Low-pass: every splat covers at least ~a pixel (antialias floor)
    float a = cov3[0][0] + 0.3;
    float b = cov3[1][0];
    float d = cov3[1][1] + 0.3;

    // Eigen-decompose the 2x2 covariance
    float mid = 0.5 * (a + d);
    float det = a * d - b * b;
    float disc = sqrt(max(mid * mid - det, 0.01));
    float l1 = mid + disc;
    float l2 = max(mid - disc, 0.01);
    vec2 e1 = normalize(abs(b) > 1e-6 ? vec2(b, l1 - a) : vec2(1.0, 0.0));
    vec2 e2 = vec2(-e1.y, e1.x);
    float r1 = 3.0 * sqrt(l1);   // pixels, 3 sigma
    float r2 = 3.0 * sqrt(l2);

    // Oversized splats (numeric blowups) get clamped instead of eating the screen
    r1 = min(r1, 0.5 * W);
    r2 = min(r2, 0.5 * W);

    vec4 clipCenter = ubo.proj * viewPos;
    vec2 ndcCenter = clipCenter.xy / clipCenter.w;
    vec2 pxCenter = (ndcCenter * 0.5 + 0.5) * vec2(W, H);

    vec2 corner = kCorners[gl_VertexIndex];
    vec2 px = pxCenter + corner.x * r1 * e1 + corner.y * r2 * e2;
    vec2 ndc = (px / vec2(W, H)) * 2.0 - 1.0;

    gl_Position = vec4(ndc * clipCenter.w, clipCenter.z, clipCenter.w);
    sigmaPos = corner * 3.0;
    fragColor = vec4(inColor.rgb, opacity);
}
