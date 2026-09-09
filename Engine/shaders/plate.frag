#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// Pre-rendered background plate.
//
// Draws a finished image of a room and, from a second image, writes the depth
// that room was rendered at. Everything live is then depth-tested against it,
// so a character walks BEHIND a pillar that only exists as pixels. This is the
// Resident Evil / Final Fantasy VII trick, and the reason it still matters is
// that the plate's quality is bounded by offline render time rather than by the
// frame budget.
//
// Runs first in the scene pass, writes both colour and depth, and does not
// depth-test itself: it IS the background, so there is nothing behind it.

layout(location = 0) in vec2 fragUV;   // fullscreen triangle: 0..2, only 0..1 visible
layout(location = 0) out vec4 outColor;

// Bindless set 1. Sampler slot 1 is Point and slot 2 is Bilinear.
layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];

layout(push_constant) uniform PlatePush {
    vec4 tex;       // x = colour texture index, y = depth texture index (-1 = none)
    vec4 mapping;   // x = a, y = b, z = 1 if depth is a + b/dist, w = world-unit bias
    vec4 range;     // x = plate near distance, y = plate far distance
} plate;

void main() {
    vec2 uv = fragUV;

    // Bilinear on the colour plate: it is a photograph of a room and may be a
    // different resolution than the window.
    if (plate.tex.x >= 0.0) {
        vec4 c = texture(sampler2D(bindlessTextures[nonuniformEXT(int(plate.tex.x + 0.5))],
                                   bindlessSamplers[2]), uv);
        outColor = vec4(c.rgb, 1.0);
    } else {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
    }

    if (plate.tex.y < 0.0) {
        // No depth plate: a flat backdrop. Everything live draws in front of it,
        // which is right for a painted sky and useless for a room.
        gl_FragDepth = 1.0;
        return;
    }

    // POINT sampling, and the texture is UNORM. Both are load-bearing: these
    // three bytes are one 24-bit NUMBER, not a colour. Bilinear would average
    // the high bytes of two unrelated distances into a wall that is somewhere
    // between them, and an sRGB decode would rescale the number itself.
    vec3 p = texture(sampler2D(bindlessTextures[nonuniformEXT(int(plate.tex.y + 0.5))],
                               bindlessSamplers[1]), uv).rgb;

    // Back to whole bytes before recombining. The sampler hands back a float
    // that is a byte over 255, and reassembling 24 bits from three of those
    // without rounding lets one ulp in the top byte move a wall by metres.
    vec3 bytes = floor(p * 255.0 + 0.5);
    float n = dot(bytes, vec3(65536.0, 256.0, 1.0)) / 16777215.0;

    float dist = mix(plate.range.x, plate.range.y, n) + plate.mapping.w;
    float depth = (plate.mapping.z > 0.5)
        ? (plate.mapping.x + plate.mapping.y / max(dist, 1e-4))
        : (plate.mapping.x + plate.mapping.y * dist);

    // Clamped rather than clipped. This engine's projection is the OpenGL form,
    // so anything nearer than the harmonic mean of near and far maps below zero;
    // letting that through would drop those pixels of the plate entirely and
    // punch holes in the room.
    gl_FragDepth = clamp(depth, 0.0, 1.0);
}
