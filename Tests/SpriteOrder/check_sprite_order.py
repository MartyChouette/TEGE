#!/usr/bin/env python3
"""Check sprite draw order, texture orientation, and per-sprite texturing.

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

Third, two more sprites sit above and below the marker carrying DIFFERENT solid
textures, green over blue. They are what says each sprite drew with its own
image. Sprites used to be grouped into one draw per texture with a shared
descriptor rebound between the groups, and those rebinds happen while the
command buffer is being recorded while the draws do not run until it is
submitted -- so every group sampled whatever the LAST one bound, and a scene
turned into copies of its final group the moment it used a second texture. One
texture is one group and looks perfect, which is why nothing caught it.

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

    # The two solid-texture patches. The camera is orthographic with a half
    # height of 12 world units, so world y maps to row h/2 * (1 - y/12): the
    # sprite centred at y = +7 lands at 0.208h and the one at y = -7 at 0.792h,
    # each 4 units tall, which is a third of the half-view either side.
    green = patch(int(h * 0.208))
    blue = patch(int(h * 0.792))
    print(f"upper patch: rgb{green}   lower patch: rgb{blue}")

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
    if is_blue(top) and is_amber(bottom):
        fail("the sprite's texture is upside down: v = 0 landed on the bottom "
             "of the image (check the mix() in sprite.vert / SPRITE_WGSL)")
    if not (is_amber(top) and is_blue(bottom)):
        fail(f"neither orientation: upper rgb{top} lower rgb{bottom} - did the "
             "texture load at all?")
    print("texture orientation OK: v = 0 is the top of the image")

    # --- 3. one texture per sprite ------------------------------------------
    def is_green(c):  return c[1] > c[0] and c[1] > c[2]

    if not is_green(green):
        fail(f"the green sprite did not draw its own texture: rgb{green}. "
             "Every sprite carries a bindless texture index in its instance "
             "data; if they share one bound descriptor instead, they all sample "
             "whichever texture was bound last.")
    if not is_blue(blue):
        fail(f"the blue sprite did not draw its own texture: rgb{blue}. "
             "See the note above - two sprites with two images must not become "
             "two copies of one image.")
    print("per-sprite texturing OK: two sprites, two different images")


if __name__ == "__main__":
    main()
