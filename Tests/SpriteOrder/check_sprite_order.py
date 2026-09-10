#!/usr/bin/env python3
"""Check sprite draw order AND texture orientation from one capture.

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

The same capture also checks which way up the sprite's texture is. The marker
carries a 16x16 image that is amber on its top half and blue on its bottom half,
so the two samples either side of centre say directly whether v = 0 landed on the
top of the image or the bottom. It used to land on the bottom: every textured
sprite rendered upside down on both backends, while the same texture on a mesh
came out the right way up, and the only sprite example in the tree used
untextured quads so nothing caught it.

Sampled well inside large flat regions, so lavapipe's rasterisation differences
from a real GPU cannot move the result.

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

    def patch(cy):
        """Average a 7x7 patch on the vertical centre line at row cy."""
        rr = gg = bb = 0
        n = 0
        for dy in range(-3, 4):
            for dx in range(-3, 4):
                i = ((cy + dy) * w + (w // 2 + dx)) * 3
                rr += pixels[i]; gg += pixels[i + 1]; bb += pixels[i + 2]
                n += 1
        return rr // n, gg // n, bb // n

    # Either side of centre, never AT it: the marker's texture changes colour
    # exactly on its middle row, so a patch straddling that seam averages the two
    # halves into mud and says nothing about either question. A 10-unit sprite in
    # a 24-unit view is a bit under half the frame, so a sixth either way stays
    # well inside it.
    top = patch(h // 2 - h // 6)
    bottom = patch(h // 2 + h // 6)
    print(f"upper half: rgb{top}   lower half: rgb{bottom}")

    # The three things a sample can be. Compared by which channel leads rather
    # than against exact values, because tonemapping shifts the absolute numbers.
    def is_ground(c):  return c[0] > c[1] * 2 and c[0] > c[2] * 2   # red, dark elsewhere
    def is_amber(c):   return c[0] > c[2] and c[1] > c[2]           # red and green over blue
    def is_blue(c):    return c[2] > c[0] and c[2] > c[1]

    # --- 1. draw order -------------------------------------------------------
    if is_ground(top) and is_ground(bottom):
        fail("the ground painted over the marker: sprites drew in entity order, "
             "not in sortingLayer/orderInLayer order")
    print("sprite order OK: the marker sorted on top, as authored")

    # --- 2. texture orientation ---------------------------------------------
    if is_amber(top) and is_blue(bottom):
        print("texture orientation OK: v = 0 is the top of the image")
        return
    if is_blue(top) and is_amber(bottom):
        fail("the sprite's texture is upside down: v = 0 landed on the bottom "
             "of the image (check the mix() in sprite.vert / SPRITE_WGSL)")
    fail(f"neither orientation: upper rgb{top} lower rgb{bottom} - did the "
         "texture load at all?")


if __name__ == "__main__":
    main()
