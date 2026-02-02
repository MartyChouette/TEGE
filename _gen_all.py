import sys,struct,os
BASE="D:/GitHub/enjin"
SHADERS=BASE+"/Engine/shaders"
SPAD="C:/Users/jerma/AppData/Local/Temp/claude/D--GitHub-enjin/3f447f24-1046-42cc-82ea-39be6f597602/scratchpad"
NL=chr(10)
def uchar(p):
    d=open(p,"rb").read(); s=len(d); w=s//4; L=[]
    for i in range(0,s,16):
        c=d[i:i+16]; h=", ".join("0x{:02x}".format(b) for b in c)
        L.append("    "+h)
    return (","+NL).join(L),s,w
def u32fmt(p):
    d=open(p,"rb").read(); s=len(d); w=s//4; V=[]
    for i in range(0,s-3,4):
        v=struct.unpack("<I",d[i:i+4])[0]; V.append("0x{:08X}".format(v))
    L=[]
    for i in range(0,len(V),5):
        c=V[i:i+5]; L.append("    "+", ".join(c))
    return (","+NL).join(L),s,w
def read_particle():
    hdr=BASE+"/Engine/include/Enjin/Renderer/Vulkan/ShaderData.h"
    lines=open(hdr).readlines()
    pv_s=pv_e=pf_s=pf_e=0
    for i,l in enumerate(lines):
        if "ParticleVertexShaderData[]" in l: pv_s=i
        if "ParticleVertexShaderDataSize" in l: pv_e=i
        if "ParticleFragmentShaderData[]" in l: pf_s=i
        if "ParticleFragmentShaderDataSize" in l: pf_e=i
    pv="".join(lines[pv_s-1:pv_e+1]).rstrip()
    pf="".join(lines[pf_s-1:pf_e+1]).rstrip()
    return pv,pf
def sblock(spv,name,content,sz,wd):
    r=[]
    r.append("// {} ({} bytes, {} words)".format(spv,sz,wd))
    r.append("alignas(4) static const unsigned char {}[] = {{".format(name))
    r.append(content)
    r.append("};")
    r.append("static constexpr size_t {}Size = sizeof({});".format(name,name))
    return NL.join(r)
SH=[
("triangle.vert.spv","TriangleVertexShaderData"),
("triangle.frag.spv","TriangleFragmentShaderData"),
("grass.vert.spv","GrassVertexShaderData"),
("grass.frag.spv","GrassFragmentShaderData"),
("shrub.vert.spv","ShrubVertexShaderData"),
("shrub.frag.spv","ShrubFragmentShaderData"),
("tree.vert.spv","TreeVertexShaderData"),
("tree.frag.spv","TreeFragmentShaderData")]
pv_b,pf_b=read_particle()
BL=[]
for sp,nm in SH[:2]:
    c,s,w=uchar(SHADERS+"/"+sp)
    BL.append(sblock(sp,nm,c,s,w))
    print("Done: {} ({} bytes)".format(sp,s))
BL.append(pv_b)
BL.append(pf_b)
print("Preserved particle shaders")
for sp,nm in SH[2:]:
    c,s,w=uchar(SHADERS+"/"+sp)
    BL.append(sblock(sp,nm,c,s,w))
    print("Done: {} ({} bytes)".format(sp,s))
H=[]
H.append(chr(35)+"pragma once")
H.append("")
H.append("// Auto-generated SPIR-V shader data")
H.append("// Do not edit manually - regenerate from compiled .spv files")
H.append("")
H.append(chr(35)+"include "+chr(34)+"Enjin/Platform/Types.h"+chr(34))
H.append(chr(35)+"include <cstddef>")
H.append("")
H.append("namespace Enjin {")
H.append("namespace Renderer {")
H.append("namespace ShaderData {")
F=[]
F.append("")
F.append("} // namespace ShaderData")
F.append("} // namespace Renderer")
F.append("} // namespace Enjin")
F.append("")
hdr_str=NL.join(H)
body_str=(NL+NL).join(BL)
ftr_str=NL.join(F)
full=hdr_str+NL+NL+body_str+NL+ftr_str
out1=SPAD+"/ShaderData_new.h"
f=open(out1,"w")
f.write(full)
f.close()
print("Wrote: "+out1)
pp_c,pp_s,pp_w=u32fmt(SHADERS+"/postprocess.frag.spv")
f=open(SPAD+"/postprocess_frag_u32.txt","w")
f.write(pp_c)
f.close()
print("Wrote postprocess_frag_u32.txt ({} bytes {} words)".format(pp_s,pp_w))
fs_c,fs_s,fs_w=u32fmt(SHADERS+"/fullscreen.vert.spv")
f=open(SPAD+"/fullscreen_vert_u32.txt","w")
f.write(fs_c)
f.close()
print("Wrote fullscreen_vert_u32.txt ({} bytes {} words)".format(fs_s,fs_w))
print("ALL COMPLETE")
