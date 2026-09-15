#pragma once

// The per-frame lighting buffer for the WebGPU path, declared ONCE.
//
// This had the same shape the object layout did: `WebLightingUBO` in
// RenderSystem.cpp plus a `struct LightingUBO` inside PBR_WGSL and another
// inside SKY_WGSL, the shader copies being string literals nothing can check.
// Twenty-four fields across three hand-maintained declarations, and the C++
// comment claiming a total of 992 bytes when the fields add up to 1008 -- which
// is what a hand-maintained size does over time.
//
// Every field is a vec4 (WebLightVec4 on the C++ side), so a row is a name and a
// count. The struct and both shader declarations are generated from this list.
//
// ADDING A FIELD: append one row -- append, do not insert, or every offset after
// it moves and the cookie rows above were appended for exactly that reason. Then
// move the static_assert in RenderSystem.cpp. Nothing else.
//
// Guarded by tools/gpu_layout_parity.py --strict, in CI.

#include "Enjin/Platform/Platform.h"

// X1(name) for a single vec4, XN(name, count) for an array of them.
#define ENJIN_WEB_LIGHTING_FIELDS(X1, XN) \
    XN(lightDir, 8)               /* 128  (0-3: dir directions, 4-7: point positions) */ \
    XN(lightColor, 8)             /* 128  (matching color.rgb + intensity.w) */ \
    XN(lightParams, 8)            /* 128  (point: range, linear, quadratic, constant) */ \
    X1(ambientColor)              /* 16 */ \
    X1(fogColor)                  /* 16 */ \
    X1(fogParams)                 /* 16 */ \
    X1(shadowParams)              /* 16 */ \
    X1(lightCount)                /* 16   (x=dir, y=point, z=spot) */ \
    /* Spot lights (separate arrays since they need both position and direction) */ \
    XN(spotPos, 4)                /* 64   position.xyz, range.w */ \
    XN(spotDir, 4)                /* 64   direction.xyz */ \
    XN(spotColor, 4)              /* 64   color.rgb, intensity.w */ \
    XN(spotParams, 4)             /* 64   innerCutoff.x, outerCutoff.y */ \
    X1(windData)                  /* 16   xyz = wind dir * strength, w = wind clock */ \
    /* Sky palette + atmosphere (mirrors the sky block in pbr.wgsl/SKY_WGSL) */ \
    X1(skyTop)                    /* xyz zenith color, w = configured flag */ \
    X1(skyBottom)                 /* xyz ground-arc color */ \
    X1(skyHorizon)                /* xyz horizon color, w = horizon haze */ \
    X1(skySunDir)                 /* xyz sun direction, w = sun intensity */ \
    X1(skySunColor)               /* xyz sun color, w = sun size */ \
    X1(skyClouds)                 /* x cov1, y scale1, z speed, w cov2 */ \
    X1(skyCloudColor)             /* xyz cloud color, w = scale2 */ \
    X1(snowParams)                /* x = snow accumulation (0..1); yzw reserved */ \
    /* Light cookies. APPENDED rather than squeezed into the spot arrays: every */ \
    /* field above keeps its offset, so nothing that writes this UBO had to be */ \
    /* touched or re-checked. */ \
    XN(spotCookie, 4)             /* 64   x = atlas cell (-1 = none), y = scale, z = intensity */ \
    XN(spotCookieRight, 4)        /* 64   xyz = the light's local +X */ \
    /* Baked lightmap strength in x. The atlases are textures on the frame */ \
    /* group; only the dial lives here. */ \
    X1(lightmapParams)            /* 16 */

// One member of the C++ struct.
#define ENJIN_WEB_LIGHTING_MEMBER1(name)        WebLightVec4 name;
#define ENJIN_WEB_LIGHTING_MEMBERN(name, count) WebLightVec4 name[count];

// One line of the WGSL struct, as a string literal. The whole struct is a run of
// adjacent literals that the compiler concatenates.
#define ENJIN_WEB_LIGHTING_WGSL1(name)        "    " #name ": vec4<f32>,\n"
#define ENJIN_WEB_LIGHTING_WGSLN(name, count) "    " #name ": array<vec4<f32>, " #count ">,\n"

// The WGSL declaration, for a shader to splice in place of its own copy.
#define ENJIN_WEB_LIGHTING_WGSL                                               \
    "struct LightingUBO {\n"                                                  \
    ENJIN_WEB_LIGHTING_FIELDS(ENJIN_WEB_LIGHTING_WGSL1, ENJIN_WEB_LIGHTING_WGSLN) \
    "};\n"
