import struct, os

BASE = "D:/GitHub/enjin"
SHADERS = BASE + "/Engine/shaders"
OUTPUT = BASE + "/Engine/include/Enjin/Renderer/Vulkan/ShaderData.h"
OUTPUT_CPP = BASE + "/Engine/src/Renderer/Vulkan/ShaderDataGenerated.cpp"
NL = chr(10)

def uchar(p):
    d = open(p, "rb").read(); s = len(d); w = s // 4; L = []
    for i in range(0, s, 16):
        c = d[i:i+16]; h = ", ".join("0x{:02x}".format(b) for b in c)
        L.append("    " + h)
    return ("," + NL).join(L), s, w

def u32fmt(p):
    d = open(p, "rb").read(); s = len(d); w = s // 4; V = []
    for i in range(0, s - 3, 4):
        v = struct.unpack("<I", d[i:i+4])[0]; V.append("0x{:08X}".format(v))
    L = []
    for i in range(0, len(V), 5):
        c = V[i:i+5]; L.append("    " + ", ".join(c))
    return ("," + NL).join(L), s, w

# The bytes go in ONE translation unit and the header only declares them.
# They used to be static arrays in the header, so 3.2 MB of hex literal was
# pasted into each of 28 files that include it, every build, and each of those
# files materialized its own copy of whatever it referenced. Same treatment
# EmbeddedComputeShaders.cpp already gives its own SPIR-V.
def decl_uchar(spv, name, sz, wd):
    r = []
    r.append("// {} ({} bytes, {} words)".format(spv, sz, wd))
    r.append("extern const unsigned char {}[];".format(name))
    r.append("extern const size_t {}Size;".format(name))
    return NL.join(r)

def decl_u32(spv, name, sz, wd):
    r = []
    r.append("// {} ({} bytes, {} words)".format(spv, sz, wd))
    r.append("extern const uint32_t {}[];".format(name))
    r.append("extern const size_t {}Size;".format(name))
    return NL.join(r)

def sblock_uchar(spv, name, content, sz, wd):
    r = []
    r.append("// {} ({} bytes, {} words)".format(spv, sz, wd))
    r.append("alignas(4) extern const unsigned char {}[] = {{".format(name))
    r.append(content)
    r.append("};")
    r.append("extern const size_t {}Size = sizeof({});".format(name, name))
    return NL.join(r)

def sblock_u32(spv, name, content, sz, wd):
    r = []
    r.append("// {} ({} bytes, {} words)".format(spv, sz, wd))
    r.append("extern const uint32_t {}[] = {{".format(name))
    r.append(content)
    r.append("};")
    r.append("extern const size_t {}Size = sizeof({});".format(name, name))
    return NL.join(r)

# All shaders in order matching the existing ShaderData.h layout
# (spv_file, array_name, format: "uchar" or "u32")
SHADERS_LIST = [
    ("triangle.vert.spv",      "TriangleVertexShaderData",       "uchar"),
    ("triangle.frag.spv",      "TriangleFragmentShaderData",     "uchar"),
    ("postprocess.frag.spv",   "PostProcessFragmentShaderData",  "uchar"),
    ("particle.vert.spv",      "ParticleVertexShaderData",       "uchar"),
    ("particle.frag.spv",      "ParticleFragmentShaderData",     "uchar"),
    ("weather_particle.frag.spv", "WeatherParticleFragmentShaderData", "uchar"),
    ("grass.vert.spv",         "GrassVertexShaderData",          "uchar"),
    ("grass.frag.spv",         "GrassFragmentShaderData",        "uchar"),
    ("shrub.vert.spv",         "ShrubVertexShaderData",          "uchar"),
    ("shrub.frag.spv",         "ShrubFragmentShaderData",        "uchar"),
    ("tree.vert.spv",          "TreeVertexShaderData",           "uchar"),
    ("tree.frag.spv",          "TreeFragmentShaderData",         "uchar"),
    ("skybox.vert.spv",        "SkyboxVertexShaderData",         "u32"),
    ("skybox.frag.spv",        "SkyboxFragmentShaderData",       "u32"),
    ("sprite.vert.spv",        "SpriteVertexShaderData",         "uchar"),
    ("sprite.frag.spv",        "SpriteFragmentShaderData",       "uchar"),
    ("sprite_lit.vert.spv",    "SpriteLitVertexShaderData",      "uchar"),
    ("sprite_lit.frag.spv",    "SpriteLitFragmentShaderData",    "uchar"),
    ("outline.vert.spv",       "OutlineVertexShaderData",        "uchar"),
    ("outline.frag.spv",       "OutlineFragmentShaderData",      "uchar"),
    ("wireframe.frag.spv",     "WireframeFragmentShaderData",    "uchar"),
    ("shadow.vert.spv",        "ShadowVertexShaderData",         "uchar"),
    ("shadow_mask.vert.spv",   "ShadowMaskVertexShaderData",     "uchar"),
    ("shadow_mask.frag.spv",   "ShadowMaskFragmentShaderData",   "uchar"),
    ("fullscreen.vert.spv",    "FullscreenVertexShaderData",     "uchar"),
    ("sky2d.frag.spv",         "Sky2DFragmentShaderData",        "uchar"),
    ("water2d.frag.spv",       "Water2DFragmentShaderData",      "uchar"),
    ("fluid.vert.spv",         "FluidVertexShaderData",          "uchar"),
    ("fluid.frag.spv",         "FluidFragmentShaderData",        "uchar"),
    ("oit_composite.frag.spv", "OitCompositeFragmentShaderData", "uchar"),
    ("taa_resolve.comp.spv",   "TAAResolveComputeShaderData",    "uchar"),
    ("rt_hybrid_apply.frag.spv", "RTHybridApplyFragmentShaderData", "uchar"),
    ("gpu_particle.vert.spv",  "GpuParticleVertexShaderData",    "uchar"),
    ("gpu_particle.frag.spv",  "GpuParticleFragmentShaderData",  "uchar"),
    ("splat.vert.spv",         "SplatVertexShaderData",          "uchar"),
    ("splat.frag.spv",         "SplatFragmentShaderData",        "uchar"),
]

BL = []
DL = []
for spv, name, fmt in SHADERS_LIST:
    path = SHADERS + "/" + spv
    if fmt == "u32":
        c, s, w = u32fmt(path)
        BL.append(sblock_u32(spv, name, c, s, w))
        DL.append(decl_u32(spv, name, s, w))
    else:
        c, s, w = uchar(path)
        BL.append(sblock_uchar(spv, name, c, s, w))
        DL.append(decl_uchar(spv, name, s, w))
    print("Done: {} ({} bytes)".format(spv, s))

H = []
H.append(chr(35) + "pragma once")
H.append("")
H.append("// Auto-generated SPIR-V shader data")
H.append("// Do not edit manually - regenerate with: python _gen_all.py")
H.append("")
H.append(chr(35) + "include " + chr(34) + "Enjin/Platform/Types.h" + chr(34))
H.append(chr(35) + "include <cstddef>")
H.append(chr(35) + "include <cstdint>")
H.append("")
H.append("namespace Enjin {")
H.append("namespace Renderer {")
H.append("namespace ShaderData {")
F = []
F.append("")
F.append("} // namespace ShaderData")
F.append("} // namespace Renderer")
F.append("} // namespace Enjin")
F.append("")
hdr_str = NL.join(H)
ftr_str = NL.join(F)

# Header: declarations only.
decl_str = (NL + NL).join(DL)
f = open(OUTPUT, "w")
f.write(hdr_str + NL + NL + decl_str + NL + ftr_str)
f.close()
print("Wrote: " + OUTPUT)

# Source: the bytes, once.
C = []
C.append("// Auto-generated SPIR-V shader data")
C.append("// Do not edit manually - regenerate with: python _gen_all.py")
C.append("")
C.append(chr(35) + "include " + chr(34) + "Enjin/Renderer/Vulkan/ShaderData.h" + chr(34))
C.append("")
C.append("namespace Enjin {")
C.append("namespace Renderer {")
C.append("namespace ShaderData {")
body_str = (NL + NL).join(BL)
f = open(OUTPUT_CPP, "w")
f.write(NL.join(C) + NL + NL + body_str + NL + ftr_str)
f.close()
print("Wrote: " + OUTPUT_CPP)
print("ALL COMPLETE")
