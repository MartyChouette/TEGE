#!/usr/bin/env python3
"""Which RenderSystem methods exist on which backend, and where they have drifted.

`Engine/src/ECS/Systems/RenderSystem.cpp` holds BOTH renderer backends in one
file under a single `#if ENJIN_RENDERER_WEBGPU` / `#else` / `#endif`. Only one
half compiles in any given build, so a feature added to one and not the other is
invisible: the compiler never sees the half you did not touch, and neither do
the tests, because the tests only ever run one backend.

That is not theoretical. Every one of these was found by hand, after shipping:

  - tilemaps generated no mesh at all on web, because the call site existed only
    in the Vulkan `Update` body. Not a port -- three lines nobody added.
  - snow reached the PBR shader and not the vegetation shaders.
  - geometry outlines existed on Vulkan and not on web, while SceneRenderSettings
    wrote the settings into both.
  - `TickHighlightTime` was called in the web `Update` and read only by the
    Vulkan outline pass, so hover highlights animated on neither.
  - `m_WebGrassPipeline` / `m_WebTreePipeline` were built and logged every boot
    and never drawn.

This does not fix the split. It makes the split a BUILD FAILURE instead of a bug
report, which is step 1 of `_docs_internal/RENDERSYSTEM_SPLIT.md` and the cheap
half of the problem.

    python tools/rendersystem_parity.py            # the report
    python tools/rendersystem_parity.py --strict   # exit 1 on NEW divergence

`--strict` fails on NEW divergence only. Two mechanisms, and the difference
between them matters:

  ALLOW-LIST -- a method that is deliberately one-sided forever, with the reason
  written next to it. Ray tracing is not coming to a browser.

  BASELINE (`tools/rendersystem_parity_baseline.txt`) -- a snapshot of the
  one-sided methods that already existed when this check was written. It is
  DEBT, not permission. The file is meant to shrink, and `--update-baseline`
  rewrites it so that removing an entry is a deliberate act with a diff.

Failing the build on all 189 existing one-sided methods would mean turning the
check off on day one, which is how a gate becomes decoration. Failing on the
190th is the whole point.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Engine", "src", "ECS", "Systems", "RenderSystem.cpp")
BASELINE = os.path.join(ROOT, "tools", "rendersystem_parity_baseline.txt")

# ---------------------------------------------------------------------------
# Deliberately one-sided methods.
#
# The rule for adding an entry: it is here because the OTHER backend cannot have
# it or has no use for it, not because nobody has got round to it. "Not yet
# ported" is drift and belongs in the backlog, not in this dict.
# ---------------------------------------------------------------------------
ALLOWED_WEB_ONLY = {
    "WebUpdateLightCookies":   "web builds its cookie atlas per frame; Vulkan bakes one at load",
    "WebUpdateSceneLightmap":  "same shape as cookies -- no persistent bindless slot on web",
    "WebUpdateScenePalette":   "palette texture is re-uploaded per frame on web",
}

ALLOWED_VULKAN_ONLY = {
    # Vulkan-only hardware and API surface. No browser exposes any of this.
    "RecordRTFrame":             "no ray tracing in any browser",
    "GetRTShadows":              "ray tracing",
    "GetRTReflections":          "ray tracing",
    "GetRTAO":                   "ray tracing",
    "GetRTGI":                   "ray tracing",
    "GetRTCompositor":           "ray tracing",
    "SetRTCameraOverride":       "ray tracing",
    "RenderShadowPassForCamera": "shadow atlas + secondary command buffers, no WebGPU equivalent",
    "RenderSplitscreen":         "multi-viewport recording into one command buffer",
    "RenderToTarget":            "editor offscreen path; the editor is desktop-only",
    "RenderScriptTargets":       "FR-4 render targets ride the Vulkan offscreen slot",
    "RenderGridLines":           "editor viewport grid; editor is desktop-only",
    "RenderWeatherParticles":    "editor viewport overlay",
}


def read_regions():
    """Return (lines, web_start, web_end, vk_start, vk_end).

    Walks preprocessor depth rather than grepping, because the file has nested
    guards inside both halves and a naive search finds the wrong ones. That is
    not hypothetical -- the first version of this script reported the #if at
    line 22724 because it kept overwriting its match.
    """
    lines = open(SRC, encoding="utf-8").read().split("\n")
    depth = 0
    start = els = end = None
    start_depth = None
    for i, line in enumerate(lines, 1):
        s = line.strip()
        if re.match(r"#\s*if", s):
            depth += 1
            # Must be `#if ENJIN_RENDERER_WEBGPU`, NOT `#if !ENJIN_RENDERER_WEBGPU`.
            # The file opens with a negated guard around an include, and matching
            # on substring picked that one -- so this reported the web region as
            # starting at line 10 and counted a shared method hoisted above the
            # real #if as web-only. Caught by the check contradicting a change
            # that was correct, which is the check doing its job on itself.
            if start is None and depth == 1 and re.match(r"#\s*if\s+ENJIN_RENDERER_WEBGPU\b", s):
                start, start_depth = i, depth
        elif re.match(r"#\s*el(se|if)", s):
            if start is not None and els is None and depth == start_depth:
                els = i
        elif re.match(r"#\s*endif", s):
            if start is not None and els is not None and end is None and depth == start_depth:
                end = i
            depth -= 1
    if not (start and els and end):
        sys.exit("could not locate the top-level ENJIN_RENDERER_WEBGPU block")
    return lines, start, els, end


METHOD = re.compile(r"^[A-Za-z_][\w:<>,&*\s]*\bRenderSystem::(\w+)\s*\(")


def methods_in(lines, a, b):
    """Method name -> line count of its definition, for definitions in [a, b)."""
    found = {}
    current = None
    for i in range(a, b - 1):
        m = METHOD.match(lines[i])
        if m:
            current = m.group(1)
            found.setdefault(current, 0)
        elif current is not None:
            found[current] += 1
        if lines[i].startswith("}"):
            current = None
    return found


def load_baseline():
    """Names recorded as already one-sided. Debt, not permission."""
    if not os.path.exists(BASELINE):
        return set()
    out = set()
    for line in open(BASELINE, encoding="utf-8"):
        line = line.split("#", 1)[0].strip()
        if line:
            out.add(line)
    return out


def write_baseline(web_only, vk_only):
    with open(BASELINE, "w", encoding="utf-8") as f:
        f.write("# RenderSystem backend parity baseline\n")
        f.write("#\n")
        f.write("# Methods that exist on ONE backend only and were already like that when\n")
        f.write("# the parity check was written. This is DEBT, not permission: the list is\n")
        f.write("# meant to shrink. Every name here is a place where a change to one\n")
        f.write("# backend cannot be seen by the other.\n")
        f.write("#\n")
        f.write("# Regenerate deliberately with:\n")
        f.write("#     python tools/rendersystem_parity.py --update-baseline\n")
        f.write("#\n")
        f.write("# A name LEAVING this file is progress. A name being ADDED to it should\n")
        f.write("# have been a code review conversation instead.\n")
        f.write("\n# --- web only ---\n")
        for m in sorted(web_only):
            f.write(m + "\n")
        f.write("\n# --- vulkan only ---\n")
        for m in sorted(vk_only):
            f.write(m + "\n")


def main():
    strict = "--strict" in sys.argv
    update = "--update-baseline" in sys.argv
    lines, start, els, end = read_regions()

    web = methods_in(lines, start, els)
    vk = methods_in(lines, els, end)

    both = sorted(set(web) & set(vk))
    web_only = sorted(set(web) - set(vk))
    vk_only = sorted(set(vk) - set(web))

    print("RenderSystem.cpp: %d lines" % len(lines))
    print("  web    region: lines %5d-%5d  (%5d lines, %3d methods)"
          % (start, els, els - start, len(web)))
    print("  vulkan region: lines %5d-%5d  (%5d lines, %3d methods)"
          % (els, end, end - els, len(vk)))
    print()
    print("  in both backends : %d" % len(both))
    print("  web only         : %d" % len(web_only))
    print("  vulkan only      : %d" % len(vk_only))
    print()

    # Where a shared method is wildly different in size, one of the two is
    # probably missing something. Not an error -- the backends genuinely differ
    # in how much code a pass takes -- but it is where to look first.
    print("Shared methods, biggest size gap first (web vs vulkan lines):")
    gaps = sorted(both, key=lambda m: -abs(web[m] - vk[m]))
    for m in gaps[:12]:
        print("  %-34s web %5d   vulkan %5d   gap %5d"
              % (m, web[m], vk[m], abs(web[m] - vk[m])))
    print()

    unexplained_web = [m for m in web_only if m not in ALLOWED_WEB_ONLY]
    unexplained_vk = [m for m in vk_only if m not in ALLOWED_VULKAN_ONLY]

    if unexplained_web:
        print("WEB ONLY, not explained (%d):" % len(unexplained_web))
        for m in unexplained_web:
            print("  %s" % m)
        print()
    if unexplained_vk:
        print("VULKAN ONLY, not explained (%d):" % len(unexplained_vk))
        for m in unexplained_vk:
            print("  %s" % m)
        print()

    if update:
        write_baseline(unexplained_web, unexplained_vk)
        print("baseline rewritten: %d web-only, %d vulkan-only"
              % (len(unexplained_web), len(unexplained_vk)))
        return 0

    baseline = load_baseline()
    new_web = [m for m in unexplained_web if m not in baseline]
    new_vk = [m for m in unexplained_vk if m not in baseline]
    fixed = sorted(baseline - set(unexplained_web) - set(unexplained_vk))

    print("Against the baseline: %d new, %d retired." % (len(new_web) + len(new_vk), len(fixed)))
    if fixed:
        print("  no longer one-sided: %s" % ", ".join(fixed[:8]))
    print()

    if strict:
        if new_web or new_vk:
            print("FAIL: %d method(s) landed on one backend only." % (len(new_web) + len(new_vk)))
            for m in new_web:
                print("  web only   : %s" % m)
            for m in new_vk:
                print("  vulkan only: %s" % m)
            print()
            print("This is the drift that produced the tilemap bug, the snow-on-vegetation")
            print("bug and the hover-highlight bug -- a feature on one backend that the")
            print("other cannot see and the compiler never checks.")
            print()
            print("Three honest ways out:")
            print("  1. Implement it on the other backend.")
            print("  2. Add it to ALLOW_LIST in this file WITH the reason it cannot exist")
            print("     there (no browser has ray tracing; the editor is desktop-only).")
            print("  3. If it genuinely has to ship one-sided for now, say so in the")
            print("     backlog and run --update-baseline as a deliberate, reviewable diff.")
            return 1
        print("OK: no new one-sided methods.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
