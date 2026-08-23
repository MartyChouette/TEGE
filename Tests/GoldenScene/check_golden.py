#!/usr/bin/env python3
"""Sanity-check a --golden capture (CI render smoke test).

Reads the .ppm the editor writes next to the .png (P6, stdlib-parseable) and
fails unless the image looks like a real render:
  - file exists with sane dimensions
  - not flat (a single color = swapchain cleared but nothing drew)
  - not all-black / all-white (dead lighting or a blown pipeline)

This is deliberately NOT a pixel-diff against reference images: CI renders on
lavapipe (software Vulkan) whose rasterization differs from real GPUs, so exact
comparisons would flake. Structural "did a real scene render" checks are stable.

Usage: check_golden.py <capture-base-path>   (reads <base>.ppm)
"""
import sys
import os


def fail(msg):
    print(f"GOLDEN CHECK FAILED: {msg}")
    sys.exit(1)


def main():
    if len(sys.argv) < 2:
        fail("usage: check_golden.py <capture-base-path>")
    base = sys.argv[1]
    path = base if base.endswith(".ppm") else base + ".ppm"
    if not os.path.exists(path):
        fail(f"capture not found: {path} (did the editor render + exit cleanly?)")

    with open(path, "rb") as f:
        data = f.read()

    # P6 header: magic, width, height, maxval, then raw RGB
    parts = data.split(b"\n", 3)
    if len(parts) < 4 or parts[0].strip() != b"P6":
        fail(f"not a P6 ppm: {path}")
    try:
        w, h = map(int, parts[1].split())
        maxval = int(parts[2])
    except ValueError:
        fail("unparseable ppm header")
    pixels = parts[3]
    if w < 64 or h < 64:
        fail(f"suspicious dimensions {w}x{h}")
    need = w * h * 3
    if len(pixels) < need:
        fail(f"truncated pixel data: {len(pixels)} < {need}")
    pixels = pixels[:need]

    lo, hi, total = 255, 0, 0
    # sample every 16th byte - plenty for structural checks, fast in plain python
    step = 16
    n = 0
    for i in range(0, need, step):
        v = pixels[i]
        if v < lo:
            lo = v
        if v > hi:
            hi = v
        total += v
        n += 1
    mean = total / max(1, n)

    print(f"golden: {w}x{h} maxval={maxval}  min={lo} max={hi} mean={mean:.1f}")
    if hi - lo < 24:
        fail(f"image is flat (min={lo}, max={hi}) - nothing rendered over the clear color")
    if mean < 4:
        fail(f"image is essentially black (mean={mean:.1f})")
    if mean > 251:
        fail(f"image is essentially white (mean={mean:.1f})")
    print("golden: OK - scene rendered with real content")


if __name__ == "__main__":
    main()
