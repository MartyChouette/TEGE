#!/usr/bin/env python3
"""Check that sprites drew in sortingLayer/orderInLayer order, not entity order.

The scene beside this script is built so the two disagree. A teal marker is
created FIRST (entity id 2) but sorts LAST (orderInLayer 10); a large red ground
is created SECOND (id 3) but sorts FIRST (orderInLayer 0).

  drawn in entity iteration order -> ground paints over marker -> centre is RED
  drawn in sorted order           -> marker paints last        -> centre is TEAL

The bug this guards against: SpriteBatchRenderer rebuilt its sprite list from
scratch every call, in entity iteration order, but skipped the sort whenever the
sort keys matched the previous call. The sort therefore ran on the first call and
was skipped forever after, so from frame two onward sortingLayer and orderInLayer
did nothing. Authoring a draw order looked like it was being ignored, because it
was. A capture taken at any frame past the first shows it.

Sampled dead centre of a large solid quad, so lavapipe's rasterisation
differences from a real GPU cannot move the result.

Usage: check_sprite_order.py <capture-base-path>   (reads <base>.ppm)
"""
import sys
import os


def fail(msg):
    print(f"SPRITE ORDER CHECK FAILED: {msg}")
    sys.exit(1)


def main():
    if len(sys.argv) < 2:
        fail("usage: check_sprite_order.py <capture-base-path>")
    base = sys.argv[1]
    path = base if base.endswith(".ppm") else base + ".ppm"
    if not os.path.exists(path):
        fail(f"capture not found: {path} (did the editor render + exit cleanly?)")

    with open(path, "rb") as f:
        data = f.read()

    parts = data.split(b"\n", 3)
    if len(parts) < 4 or parts[0].strip() != b"P6":
        fail(f"not a P6 ppm: {path}")
    try:
        w, h = map(int, parts[1].split())
        int(parts[2])           # maxval, unused
    except ValueError:
        fail("unparseable ppm header")
    pixels = parts[3]
    if w < 64 or h < 64:
        fail(f"suspicious dimensions {w}x{h}")
    if len(pixels) < w * h * 3:
        fail(f"truncated pixel data: {len(pixels)} < {w * h * 3}")

    # Average a small patch at the centre rather than one pixel, so a stray
    # dithered or filtered texel cannot decide the outcome.
    r = g = b = 0
    n = 0
    for dy in range(-3, 4):
        for dx in range(-3, 4):
            x, y = w // 2 + dx, h // 2 + dy
            i = (y * w + x) * 3
            r += pixels[i]
            g += pixels[i + 1]
            b += pixels[i + 2]
            n += 1
    r, g, b = r // n, g // n, b // n
    print(f"centre pixel: rgb({r},{g},{b})")

    # Teal marker (0.10, 1.00, 0.80) vs red ground (0.85, 0.15, 0.10). Compared
    # by which channel dominates, not against exact values, because tonemapping
    # and colour management shift the absolute numbers.
    if g > r and b > r:
        print("sprite order OK: the marker sorted on top, as authored")
        return
    if r > g and r > b:
        fail("the ground painted over the marker: sprites drew in entity order, "
             "not in sortingLayer/orderInLayer order")
    fail(f"centre is neither marker nor ground: rgb({r},{g},{b}) - did the scene render?")


if __name__ == "__main__":
    main()
