#pragma once

// The per-entity GPU layout for the WebGPU path, declared ONCE.
//
// This buffer used to be written out three times by hand: `WebObjectDataUBO` in
// RenderSystem.cpp, and `struct ObjectData` in both PBR_WGSL and OUTLINE_WGSL,
// the latter two being string literals a compiler cannot see into. Adding a
// field was a three-place edit, OUTLINE_WGSL was the one that got missed (it is
// a different shader that merely shares the buffer), and missing it produced a
// green build, a green shader compile, green tests, and every material on web
// reading at shifted offsets at once.
//
// Now there is one list. The C++ struct is generated from it, and so is the
// WGSL text, which the two shaders concatenate in place of their own copy. They
// cannot disagree, because there is no longer anything to disagree with.
//
// ADDING A FIELD: add one row here, keep the struct 16-byte aligned (the
// static_assert in RenderSystem.cpp will say so), and move that assert. Nothing
// else. tools/objectdata_parity.py checks that no hand-written `struct
// ObjectData` has reappeared in the shader header.
//
// Alignment is part of the row because C++ and WGSL agree on it only if it is
// stated: a vec3 has 16-byte alignment in WGSL, and a bare Math::Vector3 does
// not in C++.

#include "Enjin/Platform/Platform.h"
#include "Enjin/Math/Vector.h"

// Written by C++, checked by the shader. Any exactly-representable value
// nothing else would produce. See the canary note in RenderSystem.cpp.
#define ENJIN_WEB_OBJECT_LAYOUT_CANARY 1234.5f

// X(c++ declaration, field name, wgsl type, c++ initialiser)
#define ENJIN_WEB_OBJECTDATA_FIELDS(X)                                        \
    X(alignas(16) Math::Matrix4, model,              "mat4x4<f32>", {})       \
    X(alignas(16) Math::Vector3, baseColor,          "vec3<f32>",   {})       \
    X(f32,                       metallic,           "f32",         {})       \
    X(alignas(16) Math::Vector3, emissiveColor,      "vec3<f32>",   {})       \
    X(f32,                       roughness,          "f32",         {})       \
    X(f32,                       emissiveStrength,   "f32",         {})       \
    X(f32,                       opacity,            "f32",         {})       \
    X(f32,                       alphaCutoff,        "f32",         {})       \
    X(i32,                       flags,              "i32",         {})       \
    X(f32,                       parallaxScale,      "f32",         {})       \
    X(f32,                       uvScrollU,          "f32",         {})       \
    X(f32,                       uvScrollV,          "f32",         {})       \
    X(f32,                       scrollReflSpeedU,   "f32",         {})       \
    X(f32,                       scrollReflSpeedV,   "f32",         {})       \
    X(f32,                       scrollReflStrength, "f32",         {})       \
    X(f32,                       matcapBlend,        "f32",         {})       \
    X(f32,                       shoreWidth,         "f32",         {})       \
    X(f32,                       foamIntensity,      "f32",         {})       \
    X(f32,                       foamScale,          "f32",         {})       \
    X(f32,                       layoutCanary,       "f32",                   \
      ENJIN_WEB_OBJECT_LAYOUT_CANARY)

// One member of the C++ struct.
#define ENJIN_WEB_OBJECTDATA_MEMBER(decl, name, wgsl, init) decl name = init;

// One line of the WGSL struct. Expands to a string literal; the whole struct is
// a run of adjacent literals, concatenated by the compiler.
#define ENJIN_WEB_OBJECTDATA_WGSL_FIELD(decl, name, wgsl, init)               \
    "    " #name ": " wgsl ",\n"

// The WGSL declaration, for a shader source to concatenate in place of its own.
#define ENJIN_WEB_OBJECTDATA_WGSL                                             \
    "struct ObjectData {\n"                                                   \
    ENJIN_WEB_OBJECTDATA_FIELDS(ENJIN_WEB_OBJECTDATA_WGSL_FIELD)              \
    "};\n"
