#!/usr/bin/env python3
"""Decide what a capture PROVES, without comparing it to a reference image.

Golden images answer "did this change", which is the wrong question most of the
time: every deliberate visual change breaks every golden, so the goldens get
re-recorded in bulk and stop being read. Worse, a golden cannot be recorded for
a feature that never worked, and "never worked" is the case that keeps happening
here -- water waves, the procedural sky, shore foam, the fluid sim, orthographic
cameras and the camera clear colour were all fully written and simply never
switched on. A build, a test run and a shader compile were green through every
one.

So these are CLAIMS instead: statements about a frame that are true of a working
scene and false of a broken one, and that survive somebody legitimately changing
how the scene looks.

    draws      the frame is not one flat colour
    animates   two frames captured far apart are not the same frame

Pure stdlib on purpose -- no PIL, no numpy -- so it runs wherever the captures
land, including a CI box with nothing installed. P6 PPM is the input because
that is what both capture paths already write beside their PNG.
"""
import sys


def read_ppm(path):
    """(width, height, bytes) from a binary P6 PPM. Raises on anything else."""
    with open(path, 'rb') as f:
        data = f.read()
    tokens, i = [], 0
    while len(tokens) < 4 and i < len(data):
        c = data[i:i + 1]
        if c.isspace():
            i += 1
        elif c == b'#':
            while i < len(data) and data[i:i + 1] != b'\n':
                i += 1
        else:
            j = i
            while j < len(data) and not data[j:j + 1].isspace():
                j += 1
            tokens.append(data[i:j])
            i = j
    i += 1  # the single whitespace after maxval
    if len(tokens) < 4 or tokens[0] != b'P6':
        raise ValueError('%s: not a P6 PPM' % path)
    w, h, maxval = int(tokens[1]), int(tokens[2]), int(tokens[3])
    if maxval != 255:
        raise ValueError('%s: unsupported maxval %d' % (path, maxval))
    px = data[i:i + w * h * 3]
    if len(px) < w * h * 3:
        raise ValueError('%s: truncated pixel data' % path)
    return w, h, px


# Every claim samples on a stride rather than reading every pixel. A 1600x900
# frame is 4.3 MB and the harness reads a hundred of them; sampling one pixel in
# 37 is ~39,000 samples per frame, which is far more than any of these decisions
# needs and keeps a full run in seconds instead of minutes. 37 is prime so the
# stride cannot lock onto a repeating pattern in the image and sample one column.
STRIDE = 37


def _samples(px, stride=STRIDE):
    for i in range(0, len(px) - 2, 3 * stride):
        yield px[i], px[i + 1], px[i + 2]


def draws(path, quantise=8):
    """Did anything get drawn, or is this the clear colour and nothing else?

    Returns (ok, detail). Colours are quantised before counting so that a smooth
    gradient sky does not read as thousands of 'distinct' colours and pass a
    scene that drew no geometry at all.

    KNOWN LIMIT, measured: a full-screen gradient with no geometry in front of it
    still passes -- 32 distinct colours, no dominant one. So this claim proves
    the frame is not BLANK; it does not prove anything was rendered into it. A
    scene whose meshes all failed to draw against a procedural sky reads as a
    pass here, and wants an entity count or a region probe on top.
    """
    w, h, px = read_ppm(path)
    counts = {}
    total = 0
    for r, g, b in _samples(px):
        key = (r // quantise, g // quantise, b // quantise)
        counts[key] = counts.get(key, 0) + 1
        total += 1
    if not total:
        return False, 'no pixels'
    modal = max(counts.values())
    modal_pct = 100.0 * modal / total
    distinct = len(counts)
    # Both conditions, because either alone has a false pass. A scene that is
    # 97% sky still draws; a two-tone gradient fill has few distinct colours but
    # no dominant one.
    ok = modal_pct < 98.0 and distinct >= 16
    return ok, '%dx%d, %d distinct colours, %.1f%% is the single most common' % (
        w, h, distinct, modal_pct)


def animates(path_a, path_b, tol=8, min_changed_pct=0.5):
    """Is anything moving between two captures taken frames apart?

    This is the check that catches the failure mode this harness exists for: a
    feature whose shader, data and pipeline all exist and which nothing ever
    switched on renders a perfectly valid frame that is byte-identical to the
    frame three hundred frames earlier.

    `tol` is per channel, so dithering and one-bit noise do not count as motion.
    """
    wa, ha, a = read_ppm(path_a)
    wb, hb, b = read_ppm(path_b)
    if (wa, ha) != (wb, hb):
        return False, 'size mismatch %dx%d vs %dx%d' % (wa, ha, wb, hb)
    changed = total = 0
    step = 3 * STRIDE
    for i in range(0, min(len(a), len(b)) - 2, step):
        total += 1
        if (abs(a[i] - b[i]) > tol or abs(a[i + 1] - b[i + 1]) > tol
                or abs(a[i + 2] - b[i + 2]) > tol):
            changed += 1
    if not total:
        return False, 'no pixels'
    pct = 100.0 * changed / total
    return pct >= min_changed_pct, '%.2f%% of sampled pixels moved (need %.2f%%)' % (
        pct, min_changed_pct)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        print('usage: capture_claims.py draws FRAME.ppm')
        print('       capture_claims.py animates A.ppm B.ppm [min_changed_pct]')
        return 2
    claim = sys.argv[1]
    try:
        if claim == 'draws':
            ok, detail = draws(sys.argv[2])
        elif claim == 'animates':
            pct = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5
            ok, detail = animates(sys.argv[2], sys.argv[3], min_changed_pct=pct)
        else:
            print('unknown claim %r' % claim)
            return 2
    except (OSError, ValueError) as e:
        print('ERROR: %s' % e)
        return 2
    print('%s  %s: %s' % ('PASS' if ok else 'FAIL', claim, detail))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
