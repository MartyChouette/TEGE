#version 450
#extension GL_EXT_nonuniform_qualifier : enable

// GPU-compute particle fragment shader: sprite-card mask (procedural shapes or a
// bindless texture) tinted by the per-particle color the simulation interpolates.
// Writes only location 0; the main pass's velocity attachment has its write
// mask disabled for extra attachments by the pipeline config.

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in float fragLifeT;
layout(location = 3) in vec4 fragSprite;   // x = sprite card, y = softness, z = texIndex
layout(location = 4) in float fragStain;   // 2 = surface stain

layout(location = 0) out vec4 outColor;

// Bindless texture array (set 1) — used when sprite card == 5 (Textured).
layout(set = 1, binding = 0) uniform texture2D bindlessTextures[];
layout(set = 1, binding = 2) uniform sampler bindlessSamplers[8];

// Procedural sprite masks. softness: 0 = hard edge, 1 = fully soft falloff.
// 0=SoftGlow 1=HardCircle 2=Square 3=Streak 4=Star
float SpriteMask(int card, vec2 uv, float softness) {
    vec2 c = uv - vec2(0.5);
    float w = mix(0.02, 0.35, clamp(softness, 0.0, 1.0));  // edge width
    float d;
    if (card == 2) {                       // Square
        d = max(abs(c.x), abs(c.y));
    } else if (card == 3) {                // Streak (narrow x, long y; rotation orients it)
        d = length(vec2(c.x * 3.0, c.y));
    } else if (card == 4) {                // 4-point star (angular pinch)
        float ang = atan(c.y, c.x);
        d = length(c) * (1.0 + 1.2 * pow(abs(sin(2.0 * ang)), 2.0));
    } else {                               // circles (0 soft glow / 1 hard)
        d = length(c);
        if (card == 1) w = mix(0.02, 0.10, clamp(softness, 0.0, 1.0));
    }
    return 1.0 - smoothstep(0.5 - w, 0.5, d);
}

void main() {
    // Life fade: quick ease-in, long ease-out so bursts pop then dissolve.
    float fadeIn = smoothstep(0.0, 0.08, fragLifeT);
    float fadeOut = 1.0 - smoothstep(0.55, 1.0, fragLifeT);
    float life = fadeIn * fadeOut;
    if (fragStain > 1.5) {
        // Stains appear instantly and only fade near the end of their life.
        life = 1.0 - smoothstep(0.7, 1.0, fragLifeT);
    }

    int card = int(fragSprite.x + 0.5);
    vec3 rgb = fragColor.rgb;
    float alpha;
    if (card == 5 && fragSprite.z >= 0.0) {
        // Textured sprite: the image supplies shape and tint, modulated by the
        // per-particle color (incl. the emitter's Opacity in fragColor.a).
        vec4 tex = texture(sampler2D(bindlessTextures[nonuniformEXT(int(fragSprite.z + 0.5))],
                                     bindlessSamplers[0]), fragUV);
        rgb = tex.rgb * fragColor.rgb;
        alpha = tex.a * fragColor.a * life;
    } else {
        alpha = fragColor.a * SpriteMask(card, fragUV, fragSprite.y) * life;
    }

    if (alpha < 0.01) {
        discard;
    }
    outColor = vec4(rgb, alpha);
}
