#!/usr/bin/env python3
"""Which effects exist on which backend, and where they have drifted.

The desktop renderer is GLSL compiled to SPIR-V from Engine/shaders/*.
The web renderer is WGSL hand-written into one header,
Engine/include/Enjin/Renderer/WebGPU/WebShaderData.h.

Nothing connects them. There is no cross-compiler in the build, so every effect
that exists on both is WRITTEN TWICE BY HAND and nothing notices when the two
copies diverge. Two real cases found on 2026-09-04, neither deliberate:

  - triangle.vert's water displacement is three hardcoded sines scaled by the
    wind vector ("gentle Gerstner-lite"), while the desktop water3D path does
    real trochoidal Gerstner on the CPU from the component's settings. The
    shader ignores every wave field on the component.
  - the water fragment path derives a shore colour from baseColor * 1.5 instead
    of using the component's deepColor, which never reaches the shader at all.

This does not fix the split. It makes the split VISIBLE, which is the cheap
half, and it is the thing to run before hand-writing another WGSL shader.

    python tools/shader_parity.py            # the report
    python tools/shader_parity.py --strict   # exit 1 if a SHARED effect drifts
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GLSL_DIR = os.path.join(ROOT, "Engine", "shaders")
WGSL_FILE = os.path.join(ROOT, "Engine", "include", "Enjin", "Renderer",
                         "WebGPU", "WebShaderData.h")

# WGSL blobs are named for the effect, GLSL files for the stage. This maps the
# WGSL constant onto the GLSL sources it is supposed to mirror.
PAIRS = {
    "PBR":             ["triangle"],
    "SHADOW":          ["shadow"],
    "POSTPROCESS":     ["postprocess"],
    "PARTICLE":        ["particle"],
    "GRASS":           ["grass"],
    "TREE":            ["tree"],
    "SPRITE":          ["sprite"],
    "SKY":             ["skybox"],
    "OUTLINE":         ["outline"],
    "BLOOM_DOWN":      ["postprocess"],
    "BLOOM_UP":        ["postprocess"],
    "BLOOM_THRESHOLD": ["postprocess"],
    "BLOOM_COMPOSITE": ["postprocess"],
}


def glsl_shaders():
    out = {}
    if not os.path.isdir(GLSL_DIR):
        return out
    for f in os.listdir(GLSL_DIR):
        stem, ext = os.path.splitext(f)
        if ext in (".vert", ".frag", ".comp"):
            out.setdefault(stem, set()).add(ext[1:])
    return out


def wgsl_shaders():
    """Returns {NAME: body}. The blobs are raw string literals R"( ... )"."""
    if not os.path.isfile(WGSL_FILE):
        return {}
    text = open(WGSL_FILE, encoding="utf-8", errors="replace").read()
    out = {}
    for m in re.finditer(r'const char\*\s+([A-Za-z0-9_]+)_WGSL\s*=\s*R"\(', text):
        name = m.group(1)
        start = m.end()
        end = text.find(')"', start)
        out[name] = text[start:end if end > 0 else len(text)]
    return out


def stages(body):
    s = set()
    if "@vertex" in body:
        s.add("vert")
    if "@fragment" in body:
        s.add("frag")
    if "@compute" in body:
        s.add("comp")
    return s


def main():
    strict = "--strict" in sys.argv
    g, w = glsl_shaders(), wgsl_shaders()

    print("Shader backend parity\n")
    print("  GLSL sources (desktop / Vulkan) : %d effects, %d stage files"
          % (len(g), sum(len(v) for v in g.values())))
    print("  WGSL blobs   (web / WebGPU)     : %d effects, %d stages"
          % (len(w), sum(len(stages(b)) for b in w.values())))
    comp_desktop = sum(1 for v in g.values() if "comp" in v)
    comp_web = sum(1 for b in w.values() if "comp" in stages(b))
    print("  compute shaders                 : desktop %d, web %d" % (comp_desktop, comp_web))

    covered = set()
    drift = []
    print("\n  Effects present on BOTH backends")
    print("  " + "-" * 66)
    for name in sorted(PAIRS):
        if name not in w:
            continue
        ws = stages(w[name])
        for src in PAIRS[name]:
            covered.add(src)
            gs = g.get(src, set())
            miss = gs - ws - {"comp"}
            flag = ""
            if miss:
                flag = "  <-- GLSL has %s, WGSL does not" % ",".join(sorted(miss))
                drift.append((name, src, sorted(miss)))
            print("    %-18s <- %-16s glsl:%-12s wgsl:%s%s"
                  % (name + "_WGSL", src, ",".join(sorted(gs)) or "-",
                     ",".join(sorted(ws)) or "-", flag))

    only_desktop = sorted(set(g) - covered)
    print("\n  Desktop only: %d effects with NO web implementation" % len(only_desktop))
    print("  " + "-" * 66)
    for i in range(0, len(only_desktop), 4):
        print("    " + "  ".join("%-18s" % n for n in only_desktop[i:i + 4]))

    only_web = sorted(set(w) - set(PAIRS))
    if only_web:
        print("\n  Web only (no GLSL counterpart mapped): %s" % ", ".join(only_web))

    print("\n  Every effect above the 'Desktop only' line is maintained TWICE by")
    print("  hand. Nothing in the build compares them.")

    if drift:
        print("\n  DRIFT: %d shared effect(s) missing a stage on web" % len(drift))
        for name, src, miss in drift:
            print("    %s / %s missing %s" % (name, src, ",".join(miss)))
    if strict and drift:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
