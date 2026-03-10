#version 450

// Virtual Texturing feedback fragment shader.
// Renders at 1/8 resolution. Writes page ID (texture ID + mip + page coordinates)
// to an R32G32_UINT render target for CPU readback.

layout(location = 0) in vec2 inUV;
layout(location = 1) flat in uint inTextureID;

layout(location = 0) out uvec2 outFeedback;

// Virtual texture parameters
layout(push_constant) uniform PushConstants {
    uint virtualWidth;   // Virtual texture width in pixels
    uint virtualHeight;  // Virtual texture height in pixels
    uint pageSize;       // Page size in pixels (128)
    float bias;          // Mip bias (0 = auto, >0 = force lower mip)
};

void main() {
    // Compute mip level from UV derivatives
    vec2 dx = dFdx(inUV * vec2(virtualWidth, virtualHeight));
    vec2 dy = dFdy(inUV * vec2(virtualWidth, virtualHeight));
    float maxDeriv = max(dot(dx, dx), dot(dy, dy));
    float mipLevel = max(0.0, 0.5 * log2(maxDeriv) + bias);
    uint mip = uint(mipLevel);

    // Compute page coordinates at this mip level
    float mipScale = 1.0 / float(1 << mip);
    vec2 virtualUV = inUV * vec2(virtualWidth, virtualHeight) * mipScale;
    uint pageX = uint(virtualUV.x) / pageSize;
    uint pageY = uint(virtualUV.y) / pageSize;

    // Pack output: .r = textureID, .g = (mip << 24) | (pageY << 12) | pageX
    outFeedback.r = inTextureID;
    outFeedback.g = (mip << 24) | ((pageY & 0xFFF) << 12) | (pageX & 0xFFF);
}
