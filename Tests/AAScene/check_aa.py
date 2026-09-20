#!/usr/bin/env python3
"""Prove each antialiasing mode actually does something, and that the spatial
ones hold still.

Renders the same scene once per AA mode and compares the captures. Until
2026-09-20 nothing verified this at all: the roadmap item asked for "a capture
that proves TAA resolves" and there was no way to produce one, because the
golden scene never enabled the feature (see BOTH OPT-INS below).

WHAT IS ASSERTED, and why each one is shaped the way it is:

  1. Every mode differs from aaMode 0. An antialiasing mode that produces a
     byte-identical image is inert, whatever its code looks like.

  2. FXAA and SMAA are IDENTICAL between two consecutive frames of a static
     scene. They are spatial filters: same input, same output. A spatial filter
     that changes frame to frame is being fed temporal jitter it cannot resolve,
     which is strictly worse than being off.

  3. TAA differs from aaMode 0, and settles. It is temporal, so it is allowed to
     change between frames while the history fills -- but only a little. Pure
     jitter with no accumulation measured 20.7% of bytes at max channel delta 73;
     a working resolve measured 3.7% at max delta 5. The ceiling below separates
     those two regimes with room to spare rather than pinning today's number.

Deliberately NOT a pixel diff against reference images, for the same reason
check_golden.py is not: CI renders on lavapipe, whose rasterization differs from
a real GPU. Every assertion here is RELATIONAL -- one capture against another
from the same run on the same rasterizer -- so it holds anywhere.

BOTH OPT-INS MATTER, and each one silently disables this whole test:
  - `renderSettings.useProjectDefaults` must be false, or the scene's aaMode is
    discarded in favour of the project's.
  - the camera's `enablePostProcessing` must be true. It defaults to FALSE, and
    with it off the post-process pass does not run, so every mode captures
    identical and the test "passes" while measuring nothing.
Both are set in scenes/Main.enjin. If this test ever goes quiet, check them first.

Usage: check_aa.py <EnjinEditor.exe> [--keep]
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
FRAMES_A = 61
FRAMES_B = 62

# A temporal filter may still be settling; a stuck-in-jitter one is far worse
# than this. See the header for the two measured regimes.
TAA_SETTLE_MAX_DELTA = 20


def fail(msg):
    print(f"AA CHECK FAILED: {msg}")
    sys.exit(1)


def read_ppm(path):
    """Return (width, height, pixel bytes) from a P6 file, stdlib only."""
    if not os.path.exists(path):
        fail(f"capture not found: {path} (did the editor render and exit cleanly?)")
    with open(path, "rb") as f:
        data = f.read()
    fields, i = [], 0
    while len(fields) < 4:
        while i < len(data) and data[i:i + 1].isspace():
            i += 1
        j = i
        while j < len(data) and not data[j:j + 1].isspace():
            j += 1
        fields.append(data[i:j])
        i = j
    if fields[0] != b"P6":
        fail(f"not a P6 ppm: {path}")
    return int(fields[1]), int(fields[2]), data[i + 1:]


def compare(a, b):
    """(bytes differing, max channel delta) between two captures."""
    wa, ha, pa = read_ppm(a)
    wb, hb, pb = read_ppm(b)
    if (wa, ha) != (wb, hb):
        fail(f"captures differ in size: {wa}x{ha} vs {wb}x{hb}")
    diff = sum(1 for x, y in zip(pa, pb) if x != y)
    peak = max((abs(x - y) for x, y in zip(pa, pb)), default=0)
    return diff, peak, len(pa)


def build_variant(root, label, aa_mode):
    """A copy of this project whose scene asks for one AA mode."""
    dst = os.path.join(root, label)
    os.makedirs(os.path.join(dst, "scenes"), exist_ok=True)
    shutil.copy(os.path.join(HERE, "AAScene.enjinproject"),
                os.path.join(dst, "AAScene.enjinproject"))
    if os.path.isdir(os.path.join(HERE, "scripts")):
        shutil.copytree(os.path.join(HERE, "scripts"),
                        os.path.join(dst, "scripts"), dirs_exist_ok=True)
    with open(os.path.join(HERE, "scenes", "Main.enjin"), encoding="utf-8") as f:
        scene = json.load(f)
    scene["renderSettings"]["aaMode"] = aa_mode
    with open(os.path.join(dst, "scenes", "Main.enjin"), "w", encoding="utf-8") as f:
        json.dump(scene, f, indent=2)
    return os.path.join(dst, "AAScene.enjinproject")


def capture(editor, project, out_base, frames):
    cmd = [editor, project, "--play", "--golden", out_base,
           "--golden-frames", str(frames)]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
    if r.returncode != 0:
        tail = (r.stdout or "")[-1500:]
        fail(f"editor exited {r.returncode} for {os.path.basename(project)}\n{tail}")
    return out_base + ".ppm"


def main():
    if len(sys.argv) < 2:
        fail("usage: check_aa.py <EnjinEditor.exe> [--keep]")
    editor = sys.argv[1]
    keep = "--keep" in sys.argv
    if not os.path.exists(editor):
        fail(f"editor not found: {editor}")

    root = tempfile.mkdtemp(prefix="enjin_aa_")
    modes = {"none": 0, "fxaa": 1, "taa": 2, "smaa": 3}
    caps = {}
    try:
        for label, mode in modes.items():
            project = build_variant(root, label, mode)
            caps[label] = (
                capture(editor, project, os.path.join(root, f"{label}_a"), FRAMES_A),
                capture(editor, project, os.path.join(root, f"{label}_b"), FRAMES_B),
            )

        failures = []

        # 1. Every mode has to change the image.
        for label in ("fxaa", "taa", "smaa"):
            diff, peak, total = compare(caps["none"][0], caps[label][0])
            pct = 100.0 * diff / total
            print(f"{label:>5} vs none : {diff:>8} bytes ({pct:5.2f}%) max delta {peak}")
            if diff == 0:
                failures.append(f"{label} is byte-identical to no antialiasing -- it is inert")

        # 2. Spatial filters must not move.
        for label in ("none", "fxaa", "smaa"):
            diff, peak, total = compare(*caps[label])
            print(f"{label:>5} frame {FRAMES_A} vs {FRAMES_B}: {diff} bytes, max delta {peak}")
            if diff != 0:
                failures.append(
                    f"{label} changes between two frames of a STATIC scene "
                    f"({diff} bytes, max delta {peak}) -- it is being jittered "
                    f"with nothing resolving it")

        # 3. TAA is allowed to settle, but not to thrash.
        diff, peak, total = compare(*caps["taa"])
        print(f"  taa frame {FRAMES_A} vs {FRAMES_B}: {diff} bytes "
              f"({100.0 * diff / total:5.2f}%), max delta {peak}")
        if peak > TAA_SETTLE_MAX_DELTA:
            failures.append(
                f"TAA is not settling: max channel delta {peak} between consecutive "
                f"frames of a static scene (ceiling {TAA_SETTLE_MAX_DELTA}). That is "
                f"the signature of jitter without accumulation.")

        if failures:
            for f in failures:
                print(f"  - {f}")
            fail(f"{len(failures)} antialiasing check(s) failed")
        print("AA CHECK PASSED: all modes active, spatial modes stable, TAA settling")
    finally:
        if keep:
            print(f"captures kept in {root}")
        else:
            shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    main()
