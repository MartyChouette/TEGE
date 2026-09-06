#!/usr/bin/env python3
"""Catch a system that ticks in one runtime and not another.

There are three runtimes and each hand-maintains its own list of system ticks:
the editor (PlayMode + EditorLayer), the desktop player (Player/src/main.cpp)
and the web player (Player/src/web_main.cpp). Adding a system to one and
forgetting another produces no warning, no error, and a feature that silently
does nothing in the runtime you were not testing.

That is not hypothetical. On 2026-09-05 a review found FlowerSystem and
AudioReactiveSystem ticked on desktop and mentioned nowhere in web_main.cpp,
so the shipped flower template's pluckable petals were dead in a browser and
every audio-reactive component with them. Looking properly turned up four more:
AudioGraphRuntime, AudioIndicators (an accessibility feature), FluidSimulation
and FluidTerrainCoupling.

This is tools/backend_parity.py applied to the runtimes, and it works the same
way: it does not try to make the three lists equal, because some divergence is
correct - a browser has no UDP socket and no MIDI device. It pins the CURRENT
divergence in a baseline and fails when that divergence GROWS.

    python tools/runtime_parity.py             # report
    python tools/runtime_parity.py --strict    # exit 1 on new divergence
    python tools/runtime_parity.py --record    # accept the current state

When --strict fails, the fix is usually to tick the system in the runtime that
is missing it. When the gap is deliberate, --record it, and the diff on the
baseline file is where you say why.

Caveat worth knowing: this reads `m_Thing.Update(` call sites out of the source
text. It cannot see a system ticked through a helper, a base class or a lambda,
so a system it reports as missing may be reached some other way - check before
believing it. It is a drift alarm, not a proof of correctness. The real fix is
one declarative schedule all three runtimes execute; this makes the drift
visible until that exists.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(ROOT, "tools", "runtime_parity_baseline.txt")

# runtime name -> source files that own its per-frame tick list
RUNTIMES = {
    "editor": [
        os.path.join(ROOT, "Engine", "src", "Editor", "PlayMode.cpp"),
    ],
    "desktop": [
        os.path.join(ROOT, "Player", "src", "main.cpp"),
    ],
    "web": [
        os.path.join(ROOT, "Player", "src", "web_main.cpp"),
    ],
}

# The editor splits its ticking between PlayMode (play-mode systems) and
# EditorLayer (systems that also run while editing). Both count.
EDITOR_EXTRA_GLOB = os.path.join(ROOT, "Engine", "src", "Editor")

# Matches both `m_Thing.Update(` and `m_Thing->Update(`: several systems are
# held by unique_ptr or raw pointer, and missing the arrow form made the tool
# report CurlNoiseSystem and Water3D as editor gaps when the editor ticks both.
TICK = re.compile(
    r"m_([A-Za-z0-9_]+)\s*(?:\.|->)\s*(?:Update|UpdateRealtime|UpdatePresentation|FixedUpdate|LateUpdate)\s*\("
)


def ticked_in(paths):
    found = set()
    for p in paths:
        if not os.path.isfile(p):
            continue
        text = open(p, encoding="utf-8", errors="replace").read()
        found |= set(TICK.findall(text))
    return found


def editor_paths():
    paths = list(RUNTIMES["editor"])
    if os.path.isdir(EDITOR_EXTRA_GLOB):
        for name in sorted(os.listdir(EDITOR_EXTRA_GLOB)):
            if name.startswith("EditorLayer") and name.endswith(".cpp"):
                paths.append(os.path.join(EDITOR_EXTRA_GLOB, name))
    return paths


def collect():
    sets = {}
    for runtime in RUNTIMES:
        paths = editor_paths() if runtime == "editor" else RUNTIMES[runtime]
        sets[runtime] = ticked_in(paths)
    return sets


def divergence(sets):
    """Every (system, runtime) pair where the system ticks somewhere else but
    not here. Keyed 'runtime:System' so a baseline entry reads as a claim about
    one runtime."""
    everywhere = set().union(*sets.values())
    out = set()
    for system in everywhere:
        for runtime, ticked in sets.items():
            if system not in ticked:
                out.add("%s:%s" % (runtime, system))
    return out


def main():
    sets = collect()
    current = divergence(sets)

    if "--record" in sys.argv:
        with open(BASELINE, "w", encoding="utf-8") as f:
            f.write("# Systems deliberately NOT ticked in a given runtime.\n")
            f.write("# Regenerate with: python tools/runtime_parity.py --record\n")
            f.write("# A diff here is where you explain why the gap is intended.\n")
            f.write("#\n")
            f.write("# Format: <runtime>:<SystemMember>  meaning that runtime does\n")
            f.write("# not tick it while at least one other runtime does.\n")
            for entry in sorted(current):
                f.write(entry + "\n")
        print("recorded %d runtime gaps" % len(current))
        return 0

    known = set()
    if os.path.isfile(BASELINE):
        for line in open(BASELINE, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#"):
                known.add(line)

    print("Runtime system-tick parity\n")
    for runtime in sorted(sets):
        print("  %-8s : %3d systems ticked" % (runtime, len(sets[runtime])))
    print("  %-8s : %3d systems ticked in every runtime"
          % ("shared", len(set.intersection(*sets.values()))))
    print("  %-8s : %3d (%d baselined)" % ("gaps", len(current), len(known)))

    new = sorted(current - known)
    gone = sorted(known - current)

    if new:
        print("\n  NEW gaps (ticked in another runtime, not in this one)")
        print("  " + "-" * 66)
        for entry in new:
            runtime, system = entry.split(":", 1)
            others = sorted(r for r, s in sets.items() if system in s)
            print("    %-28s missing in %-8s (ticks in: %s)"
                  % (system, runtime, ", ".join(others)))

    if gone:
        print("\n  Baselined gaps that are now closed (run --record)")
        for entry in gone:
            print("    " + entry)

    if not new and not gone:
        print("\n  No new divergence.")

    if "--strict" in sys.argv and new:
        print("\nFAIL: %d system/runtime gap(s) are not baselined." % len(new))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
