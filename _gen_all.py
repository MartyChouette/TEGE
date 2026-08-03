import struct, os

BASE = "D:/GitHub/enjin"
SHADERS = BASE + "/Engine/shaders"
OUTPUT = BASE + "/Engine/include/Enjin/Renderer/Vulkan/ShaderData.h"
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

def sblock_uchar(spv, name, content, sz, wd):
    r = []
    r.append("// {} ({} bytes, {} words)".format(spv, sz, wd))
    r.append("alignas(4) static const unsigned char {}[] = {{".format(name))
    r.append(content)
    r.append("};")
    r.append("static constexpr size_t {}Size = sizeof({});".format(name, name))
    return NL.join(r)

def sblock_u32(spv, name, content, sz, wd):
    r = []
    r.append("// {} ({} bytes, {} words)".format(spv, sz, wd))
    r.append("static const uint32_t {}[] = {{".format(name))
    r.append(content)
    r.append("};")
    r.append("static constexpr size_t {}Size = sizeof({});".format(name, name))
    return NL.join(r)

# All shaders in order matching the existing ShaderData.h layout
# (spv_file, array_name, format: "uchar" or "u32")
SHADERS_LIST = [
    ("triangle.vert.spv",      "TriangleVertexShaderData",       "uchar"),
    ("triangle.frag.spv",      "TriangleFragmentShaderData",     "uchar"),
    ("postprocess.frag.spv",   "PostProcessFragmentShaderData",  "uchar"),
    ("particle.vert.spv",      "ParticleVertexShaderData",       "uchar"),
    ("particle.frag.spv",      "ParticleFragmentShaderData",     "uchar"),
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
    ("fullscreen.vert.spv",    "FullscreenVertexShaderData",     "uchar"),
    ("fluid.vert.spv",         "FluidVertexShaderData",          "uchar"),
    ("fluid.frag.spv",         "FluidFragmentShaderData",        "uchar"),
    ("oit_composite.frag.spv", "OitCompositeFragmentShaderData", "uchar"),
    ("taa_resolve.comp.spv",   "TAAResolveComputeShaderData",    "uchar"),
]

BL = []
for spv, name, fmt in SHADERS_LIST:
    path = SHADERS + "/" + spv
    if fmt == "u32":
        c, s, w = u32fmt(path)
        BL.append(sblock_u32(spv, name, c, s, w))
    else:
        c, s, w = uchar(path)
        BL.append(sblock_uchar(spv, name, c, s, w))
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
body_str = (NL + NL).join(BL)
ftr_str = NL.join(F)
full = hdr_str + NL + NL + body_str + NL + ftr_str
f = open(OUTPUT, "w")
f.write(full)
f.close()
print("Wrote: " + OUTPUT)
print("ALL COMPLETE")
