#!/usr/bin/env python3
"""Tests for the capture claims, built on synthetic images rather than captures.

The claims decide whether a rendering bug gets reported, so they are exactly the
thing that must not be quietly loosened. Every one of these was run against a
real capture by hand when it was written; this file is what stops the next edit
from moving a threshold until everything passes.

Synthetic images on purpose. A test that needs a 4 MB capture checked into the
repo is a test that gets skipped, and a claim's behaviour at its BOUNDARY -- one
flat colour, two colours, a frame identical to itself -- is easier to state in
six lines of bytes than to find in a screenshot.

    python tools/test_capture_claims.py
"""
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import capture_claims as cc  # noqa: E402

FAILED = []


def check(name, got, want):
    if got == want:
        print('  pass  %s' % name)
    else:
        print('  FAIL  %s: got %r, wanted %r' % (name, got, want))
        FAILED.append(name)


def ppm(path, w, h, pixel):
    """Write a P6 PPM whose pixel at (x, y) comes from pixel(x, y)."""
    body = bytearray()
    for y in range(h):
        for x in range(w):
            body.extend(pixel(x, y))
    with open(path, 'wb') as f:
        f.write(b'P6\n%d %d\n255\n' % (w, h))
        f.write(bytes(body))
    return path


def png_from_ppm(ppm_path, png_path):
    """Re-encode a PPM as a PNG, so the two readers can be compared on one image."""
    import struct
    import zlib
    w, h, px = cc.read_ppm(ppm_path)
    raw = bytearray()
    for y in range(h):
        raw.append(0)                       # filter type 0 (None) for every row
        raw.extend(px[y * w * 3:(y + 1) * w * 3])

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data
                + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))

    with open(png_path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n')
        f.write(chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b'IDAT', zlib.compress(bytes(raw))))
        f.write(chunk(b'IEND', b''))
    return png_path


def main():
    tmp = tempfile.mkdtemp(prefix='enjin_claims_')
    W, H = 160, 120

    # ---- draws ----
    # The failure this claim exists for: a frame that is the clear colour and
    # nothing else, which is what a scene whose render path died looks like.
    flat = ppm(os.path.join(tmp, 'flat.ppm'), W, H, lambda x, y: (26, 26, 38))
    check('draws rejects one flat colour', cc.draws(flat)[0], False)

    # Two colours is still not a scene. The distinct-colour floor is what keeps
    # a two-tone fill from passing.
    two = ppm(os.path.join(tmp, 'two.ppm'), W, H,
              lambda x, y: (26, 26, 38) if x < W // 2 else (200, 40, 40))
    check('draws rejects two flat colours', cc.draws(two)[0], False)

    # Noise stands in for a rendered scene: many colours, none dominant.
    def noisy(x, y):
        v = (x * 7 + y * 13) % 256
        return (v, (v * 3) % 256, (v * 5) % 256)
    scene = ppm(os.path.join(tmp, 'scene.ppm'), W, H, noisy)
    check('draws accepts a varied frame', cc.draws(scene)[0], True)

    # The MEASURED limit, pinned so nobody discovers it again by surprise: a
    # gradient with no geometry passes. `draws` proves a frame is not blank and
    # does not prove anything rendered into it, which is what `renders` is for.
    grad = ppm(os.path.join(tmp, 'grad.ppm'), W, H,
               lambda x, y: (int(255 * y / H), int(255 * y / H), 255 - int(255 * y / H)))
    check('draws passes a bare gradient (known limit)', cc.draws(grad)[0], True)

    # ---- animates ----
    # The failure this claim exists for: a feature that renders a perfectly
    # valid frame and never moves.
    check('animates rejects a frame against itself',
          cc.animates([scene, scene])[0], False)

    shifted = ppm(os.path.join(tmp, 'shifted.ppm'), W, H,
                  lambda x, y: noisy(x + 3, y))
    check('animates accepts a changed frame', cc.animates([scene, shifted])[0], True)

    # Passes if ANY pair differs, which is the whole reason it takes a list:
    # two samples can both land after a scene has settled.
    check('animates accepts one changed frame among still ones',
          cc.animates([scene, scene, shifted, scene])[0], True)
    check('animates rejects a series that never moves',
          cc.animates([scene, scene, scene, scene])[0], False)

    # Below the tolerance is not motion. Guards against dither and one-bit noise
    # reading as animation.
    nudged = ppm(os.path.join(tmp, 'nudged.ppm'), W, H,
                 lambda x, y: tuple(min(255, c + 2) for c in noisy(x, y)))
    check('animates ignores sub-tolerance noise', cc.animates([scene, nudged])[0], False)

    check('animates refuses a single capture', cc.animates([scene])[0], False)

    # ---- renders ----
    import json
    zero = os.path.join(tmp, 'zero.json')
    with open(zero, 'w') as f:
        json.dump({'frame': 1, 'drawCalls': 0, 'entityRenderSlots': 9, 'worldEntities': 8}, f)
    check('renders rejects zero draw calls', cc.renders(zero)[0], False)

    # entityRenderSlots is deliberately NOT tested by the claim: it is a
    # high-water mark indexed by entity id and stays non-zero for a scene that
    # renders nothing. If somebody starts testing it, this fails.
    some = os.path.join(tmp, 'some.json')
    with open(some, 'w') as f:
        json.dump({'frame': 1, 'drawCalls': 3, 'entityRenderSlots': 0, 'worldEntities': 0}, f)
    check('renders accepts draws with no entity slots', cc.renders(some)[0], True)

    # ---- one reader, two formats ----
    # The parity check compares a desktop PPM against a web PNG, which only means
    # anything if both decode identically. A channel swap between the two would
    # read as a renderer bug.
    as_png = png_from_ppm(scene, os.path.join(tmp, 'scene.png'))
    check('PNG and PPM of one image decode identically',
          cc.read_png(as_png)[2] == cc.read_ppm(scene)[2], True)
    check('draws agrees across formats', cc.draws(as_png)[1], cc.draws(scene)[1])

    print()
    if FAILED:
        print('%d failing: %s' % (len(FAILED), ', '.join(FAILED)))
        return 1
    print('all capture-claim tests passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
