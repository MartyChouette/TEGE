# Regenerates Engine/include/Enjin/Renderer/RayTracing/RTShaderData.h from
# compiled .spv files in Engine/shaders/. Compile the shaders first:
#   cd Engine/shaders
#   glslc --target-env=vulkan1.2 -I. <shader> -o <shader>.spv
# Then: python _gen_rt.py
import struct

# Repo root, derived rather than hardcoded -- see _repo.py.
from _repo import ROOT as BASE
SHADERS = BASE + "/Engine/shaders"
OUTPUT = BASE + "/Engine/include/Enjin/Renderer/RayTracing/RTShaderData.h"
NL = chr(10)

# (spv_file, array_name, description) — order matches the header layout.
# A tuple of (None, None, raw_text) emits raw_text verbatim (for .inl includes).
BLOCKS = [
    ("rt_shadow.rgen.spv",       "RT_SHADOW_RGEN_SPV",       "Shadow ray generation"),
    ("rt_shadow.rmiss.spv",      "RT_SHADOW_RMISS_SPV",      "Shadow ray miss"),
    ("rt_shadow.rchit.spv",      "RT_SHADOW_RCHIT_SPV",      "Shadow ray closest hit"),
    ("rt_reflect.rgen.spv",      "RT_REFLECT_RGEN_SPV",      "Reflection ray generation"),
    ("rt_reflect.rmiss.spv",     "RT_REFLECT_RMISS_SPV",     "Reflection ray miss"),
    ("rt_reflect.rchit.spv",     "RT_REFLECT_RCHIT_SPV",     "Reflection ray closest hit"),
    ("rt_ao.rgen.spv",           "RT_AO_RGEN_SPV",           "AO ray generation"),
    ("rt_ao.rmiss.spv",          "RT_AO_RMISS_SPV",          "AO ray miss"),
    ("rt_ao.rchit.spv",          "RT_AO_RCHIT_SPV",          "AO ray closest hit"),
    ("rt_gi.rgen.spv",           "RT_GI_RGEN_SPV",           "GI ray generation"),
    ("rt_gi.rmiss.spv",          "RT_GI_RMISS_SPV",          "GI ray miss"),
    ("rt_gi.rchit.spv",          "RT_GI_RCHIT_SPV",          "GI ray closest hit"),
    ("rt_pathtrace.rgen.spv",    "RT_PATHTRACE_RGEN_SPV",    "Path tracer ray generation"),
    ("rt_pathtrace.rmiss.spv",   "RT_PATHTRACE_RMISS_SPV",   "Path tracer ray miss"),
    ("rt_pathtrace.rchit.spv",   "RT_PATHTRACE_RCHIT_SPV",   "Path tracer ray closest hit"),
    ("rt_gbuffer.rgen.spv",      "RT_GBUFFER_RGEN_SPV",      "Hybrid RT G-buffer ray generation (primary rays -> depth+normal)"),
    ("svgf_temporal.comp.spv",   "RT_SVGF_TEMPORAL_COMP_SPV", "SVGF temporal accumulation compute"),
    ("svgf_variance.comp.spv",   "RT_SVGF_VARIANCE_COMP_SPV", "SVGF variance estimation compute"),
    ("svgf_atrous.comp.spv",     "RT_SVGF_ATROUS_COMP_SPV",   "SVGF a-trous wavelet compute"),
    ("rt_composite.comp.spv",    "RT_COMPOSITE_COMP_SPV",     "RT compositor compute"),
    ("rt_translucency.rgen.spv",  "RT_TRANSLUCENCY_RGEN_SPV",  "Translucency ray generation"),
    ("rt_translucency.rmiss.spv", "RT_TRANSLUCENCY_RMISS_SPV", "Translucency ray miss"),
    ("rt_translucency.rchit.spv", "RT_TRANSLUCENCY_RCHIT_SPV", "Translucency ray closest hit"),
    ("rt_caustics.rgen.spv",     "RT_CAUSTICS_RGEN_SPV",     "Caustics ray generation"),
    ("rt_caustics.rmiss.spv",    "RT_CAUSTICS_RMISS_SPV",    "Caustics ray miss"),
    ("rt_caustics.rchit.spv",    "RT_CAUSTICS_RCHIT_SPV",    "Caustics ray closest hit"),
    (None, None,
     "// ReSTIR compute shaders - see Engine/shaders/restir_initial.comp, restir_temporal.comp, restir_spatial.comp" + NL +
     '#include "ReSTIRShaderData.inl"' + NL + NL +
     "// Radiance Cache compute shaders - see Engine/shaders/radiance_cache_update.comp and radiance_cache_read.comp" + NL +
     '#include "RadianceCacheShaderData.inl"' + NL + NL +
     "// Surfel Radiance Cache compute shaders - see Engine/shaders/surfel_placement.comp, surfel_update.comp, surfel_lookup.comp" + NL +
     '#include "SurfelRadianceCacheShaderData.inl"' + NL + NL +
     "// Adaptive Ray Budget compute shaders - see Engine/shaders/adaptive_ray_variance.comp and adaptive_ray_budget.comp" + NL +
     '#include "AdaptiveRayBudgetShaderData.inl"'),
    ("rt_temporal_reuse.comp.spv", "RT_TEMPORAL_REUSE_COMP_SPV", "RT temporal reuse compute"),
]

# .inl files included by RTShaderData.h — each regenerated from compiled .spv files.
# (output_path, [(spv_file, array_name, description), ...])
INL_FILES = [
    (BASE + "/Engine/include/Enjin/Renderer/RayTracing/ReSTIRShaderData.inl", [
        ("restir_initial.comp.spv",  "RESTIR_INITIAL_COMP_SPV",  "ReSTIR initial candidate generation compute"),
        ("restir_temporal.comp.spv", "RESTIR_TEMPORAL_COMP_SPV", "ReSTIR temporal reuse compute"),
        ("restir_spatial.comp.spv",  "RESTIR_SPATIAL_COMP_SPV",  "ReSTIR spatial reuse compute"),
    ]),
    (BASE + "/Engine/include/Enjin/Renderer/RayTracing/RadianceCacheShaderData.inl", [
        ("radiance_cache_update.comp.spv", "RADIANCE_CACHE_UPDATE_COMP_SPV", "Radiance cache tile update compute"),
        ("radiance_cache_read.comp.spv",   "RADIANCE_CACHE_READ_COMP_SPV",   "Radiance cache read/interpolate compute"),
    ]),
    (BASE + "/Engine/include/Enjin/Renderer/RayTracing/SurfelRadianceCacheShaderData.inl", [
        ("surfel_placement.comp.spv", "SURFEL_PLACEMENT_COMP_SPV", "Surfel placement compute"),
        ("surfel_update.comp.spv",    "SURFEL_UPDATE_COMP_SPV",    "Surfel update compute"),
        ("surfel_lookup.comp.spv",    "SURFEL_LOOKUP_COMP_SPV",    "Surfel lookup compute"),
    ]),
    (BASE + "/Engine/include/Enjin/Renderer/RayTracing/AdaptiveRayBudgetShaderData.inl", [
        ("adaptive_ray_variance.comp.spv", "ADAPTIVE_RAY_VARIANCE_COMP_SPV", "Adaptive ray budget variance estimation compute"),
        ("adaptive_ray_budget.comp.spv",   "ADAPTIVE_RAY_BUDGET_COMP_SPV",   "Adaptive ray budget allocation compute"),
    ]),
]

def u32block(path):
    d = open(path, "rb").read()
    size = len(d)
    words = size // 4
    V = ["0x{:08x}".format(struct.unpack("<I", d[i:i+4])[0]) for i in range(0, size - 3, 4)]
    lines = ["    " + ", ".join(V[i:i+8]) for i in range(0, len(V), 8)]
    return ("," + NL).join(lines), size, words

out = []
out.append("#pragma once")
out.append("")
out.append("// Embedded SPIR-V bytecode for ray tracing and compute shaders.")
out.append("// Auto-generated from compiled GLSL shaders in Engine/shaders/.")
out.append("// Compiled with: glslc --target-env=vulkan1.2")
out.append("//")
out.append("// To regenerate: compile each shader, then run: python _gen_rt.py")
out.append("// See docs/BUILD.md for compilation instructions.")
out.append("")
out.append("#include <cstdint>")
out.append("")
out.append("namespace Enjin {")
out.append("namespace Renderer {")
out.append("")

for spv, name, desc in BLOCKS:
    if spv is None:
        out.append(desc)
        out.append("")
        continue
    content, size, words = u32block(SHADERS + "/" + spv)
    out.append("// {} ({} bytes, {} words)".format(desc, size, words))
    out.append("static const uint32_t {}[] = {{".format(name))
    out.append(content)
    out.append("};")
    out.append("")
    print("Embedded: {} ({} bytes)".format(spv, size))

out.append("} // namespace Renderer")
out.append("} // namespace Enjin")
out.append("")

with open(OUTPUT, "w", newline=NL) as f:
    f.write(NL.join(out))
print("Wrote: " + OUTPUT)

for inl_path, blocks in INL_FILES:
    inl = []
    inl.append("// Auto-generated by _gen_rt.py from compiled SPIR-V in Engine/shaders/ - do not edit by hand.")
    inl.append("// Included by RTShaderData.h. Compiled with: glslc --target-env=vulkan1.2 -I.")
    inl.append("")
    for spv, name, desc in blocks:
        content, size, words = u32block(SHADERS + "/" + spv)
        inl.append("// {} ({} bytes, {} words)".format(desc, size, words))
        inl.append("static const uint32_t {}[] = {{".format(name))
        inl.append(content)
        inl.append("};")
        inl.append("")
        print("Embedded: {} ({} bytes)".format(spv, size))
    with open(inl_path, "w", newline=NL) as f:
        f.write(NL.join(inl))
    print("Wrote: " + inl_path)
