#version 450

// 2D scene water: a full-screen overlay drawn AFTER the sprites in Scene2D, so
// everything below the world-space waterline reads as submerged. Screen-space
// like the 2D sky (it runs under the ortho 2D camera), but it reconstructs the
// world position of each pixel from the camera so the waterline sits at a fixed
// world Y and the whole body scrolls correctly with the camera. Alpha-blended:
// a wavy foam-capped surface line, a tint that deepens with distance below the
// line, and a gentle caustic shimmer. Driven by Water2DConfig via push constants.

layout(location = 0) in vec2 fragUV;   // 0..1 across the visible screen; y=0 top, y grows down
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform WaterPush {
    vec4 surface;   // rgb = surface tint, w = opacity (max tint strength at depth)
    vec4 deep;      // rgb = deep tint,    w = depthFalloff (world units to reach deep)
    vec4 foam;      // rgb = foam color,   w = foamWidth (world-unit band at the line)
    vec4 waveParm;  // x = waterLineY, y = waveAmplitude, z = waveLength, w = waveSpeed
    vec4 camParm;   // x = camY, y = orthoHalfHeight, z = time, w = causticStrength
    vec4 spanParm;  // x = camX, y = worldWidth (visible), zw reserved
} w;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 345.45));
    p += dot(p, p + 34.345);
    return fract(p.x * p.y);
}
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 4; ++i) {
        v += amp * noise(p);
        p = p * 2.0 + 11.0;
        amp *= 0.5;
    }
    return v;
}

void main() {
    float camY = w.camParm.x;
    float halfH = max(w.camParm.y, 0.0001);
    float time  = w.camParm.z;
    float caustic = w.camParm.w;

    // Reconstruct this pixel's world position under the ortho 2D camera.
    // Screen top (fragUV.y = 0) maps to camY + halfH; bottom maps to camY - halfH.
    float worldY = camY + halfH - fragUV.y * (2.0 * halfH);
    float worldX = w.spanParm.x + (fragUV.x - 0.5) * w.spanParm.y;

    // Surface line: two sine bands plus a little noise for a natural, non-repeating
    // ripple. waveLength is in world units, so the surface looks consistent as the
    // camera scrolls or zooms.
    float waveLen = max(w.waveParm.z, 0.001);
    float amp = w.waveParm.y;
    float spd = w.waveParm.w;
    float k = 6.28318530718 / waveLen;
    float surf = w.waveParm.x
               + amp * sin(worldX * k + time * spd)
               + amp * 0.45 * sin(worldX * k * 2.3 - time * spd * 1.7)
               + amp * 0.25 * (noise(vec2(worldX * 0.15, time * 0.4)) - 0.5) * 2.0;

    float depthBelow = surf - worldY;   // >0 = underwater, <0 = above the line
    if (depthBelow <= -w.foam.w) {
        // Well above the surface: fully transparent, leave the scene untouched.
        outColor = vec4(0.0);
        return;
    }

    // Depth tint: surface color near the line, deep color far below.
    float falloff = max(w.deep.w, 0.001);
    float t = clamp(depthBelow / falloff, 0.0, 1.0);
    vec3 tint = mix(w.surface.rgb, w.deep.rgb, t);

    // Caustic shimmer: brighter near the surface, fades with depth.
    float causticFade = (1.0 - t) * caustic;
    if (causticFade > 0.0) {
        float c = fbm(vec2(worldX * 0.5 + time * 0.6, worldY * 0.5 - time * 0.3));
        c = pow(clamp(c, 0.0, 1.0), 2.0);
        tint += vec3(c * causticFade * 0.6);
    }

    // Opacity deepens with depth so the surface is see-through and the depths are
    // solid. Underwater pixels get at least a light tint so the line reads.
    float alpha = mix(0.35, 1.0, t) * w.surface.w;

    // Foam band hugging the wavy surface line, on both sides of it.
    float foamBand = w.foam.w;
    float foamT = 1.0 - clamp(abs(depthBelow) / max(foamBand, 0.001), 0.0, 1.0);
    if (foamT > 0.0) {
        float sparkle = 0.6 + 0.4 * noise(vec2(worldX * 1.3, time * 2.0));
        float f = pow(foamT, 1.5) * sparkle;
        tint = mix(tint, w.foam.rgb, f);
        alpha = max(alpha, f);
    }

    outColor = vec4(tint, clamp(alpha, 0.0, 1.0));
}
