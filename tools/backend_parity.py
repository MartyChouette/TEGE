#!/usr/bin/env python3
"""Catch a RenderSystem method that lands on one backend and not the other.

RenderSystem.cpp is one file with two implementations in it, split by
`#if ENJIN_RENDERER_WEBGPU`. A backend only compiles its own half, so adding a
method to one and forgetting the other produces no warning, no error, and a
feature that silently does nothing on the backend you were not testing. Every
bug found on 2026-09-05 had that shape:

  - snow reached the PBR shader and not the vegetation shaders
  - outlines existed on Vulkan while SceneRenderSettings wrote the setting to both
  - TickHighlightTime was ticked in the web Update and read by the Vulkan pass,
    so hover highlights animated on neither

This is the shader_parity.py idea applied to the C++: it does not try to make
the two halves equal - 165 methods are legitimately Vulkan-only - it pins the
CURRENT divergence in a baseline and fails when that divergence GROWS.

    python tools/backend_parity.py             # report
    python tools/backend_parity.py --strict    # exit 1 on new divergence
    python tools/backend_parity.py --record    # accept the current state

When --strict fails, the fix is usually to implement the method on the other
backend. When the divergence is deliberate (a Vulkan-only feature like ray
tracing), --record it, and the diff on the baseline file is where you say why.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "Engine", "src", "ECS", "Systems", "RenderSystem.cpp")
BASELINE = os.path.join(ROOT, "tools", "backend_parity_baseline.txt")

WEB_GUARD = "#if ENJIN_RENDERER_WEBGPU"
SPLIT = "#else // !ENJIN_RENDERER_WEBGPU"
END = "#endif // !ENJIN_RENDERER_WEBGPU"


def regions():
    lines = open(SRC, encoding="utf-8", errors="replace").read().split("\n")
    try:
        start = next(i for i, l in enumerate(lines) if l.startswith(WEB_GUARD))
        mid = next(i for i, l in enumerate(lines) if l.startswith(SPLIT))
        end = max(i for i, l in enumerate(lines) if l.startswith(END))
    except (StopIteration, ValueError):
        print("could not find the backend split markers in RenderSystem.cpp")
        sys.exit(2)
    return lines[start:mid], lines[mid:end]


def methods(lines):
    return set(re.findall(r"RenderSystem::(\w+)\(", "\n".join(lines)))


def main():
    web_lines, vk_lines = regions()
    web, vk = methods(web_lines), methods(vk_lines)
    web_only = web - vk
    vk_only = vk - web
    current = {"web:" + m for m in web_only} | {"vulkan:" + m for m in vk_only}

    if "--record" in sys.argv:
        with open(BASELINE, "w", encoding="utf-8") as f:
            f.write("# Methods deliberately implemented on ONE backend only.\n")
            f.write("# Regenerate with: python tools/backend_parity.py --record\n")
            f.write("# A diff here is where you explain why a divergence is intended.\n")
            for entry in sorted(current):
                f.write(entry + "\n")
        print("recorded %d one-sided methods" % len(current))
        return 0

    known = set()
    if os.path.isfile(BASELINE):
        for line in open(BASELINE, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#"):
                known.add(line)

    print("RenderSystem backend parity\n")
    print("  web region    : %5d lines, %3d methods" % (len(web_lines), len(web)))
    print("  vulkan region : %5d lines, %3d methods" % (len(vk_lines), len(vk)))
    print("  both backends : %3d methods" % len(web & vk))
    print("  one backend   : %3d methods (%d baselined)" % (len(current), len(known)))

    new = sorted(current - known)
    gone = sorted(known - current)

    if new:
        print("\n  NEW one-sided methods (implemented on one backend, not the other)")
        print("  " + "-" * 66)
        for entry in new:
            side, name = entry.split(":", 1)
            other = "vulkan" if side == "web" else "web"
            print("    %-40s %s only, missing on %s" % (name, side, other))

    if gone:
        print("\n  Baselined methods that are no longer one-sided (run --record)")
        for entry in gone:
            print("    " + entry)

    if not new and not gone:
        print("\n  No new divergence.")

    if "--strict" in sys.argv and new:
        print("\nFAIL: %d method(s) exist on one backend only and are not baselined." % len(new))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
