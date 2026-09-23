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
    renders    the renderer issued draw calls, not just a backdrop
    animates   two frames captured far apart are not the same frame

Pure stdlib on purpose -- no PIL, no numpy -- so it runs wherever the captures
land, including a CI box with nothing installed. Reads both capture formats: P6
PPM from the desktop player and PNG from the browser, through ONE reader, so a
desktop frame and a web frame are judged by identical code.
"""
import struct
import sys
import zlib


def read_png(path):
    """(width, height, RGB bytes) from an 8-bit PNG. Stdlib only, via zlib.

    The web capture path writes PNG (puppeteer screenshots the canvas) and the
    desktop path writes both PNG and PPM. Decoding PNG here rather than teaching
    the web tool to emit PPM keeps ONE reader, which matters because the point of
    the parity check is that a desktop frame and a web frame go through exactly
    the same claim code. Two readers is two places for a swizzle to differ, and a
    red/blue swap between the two would read as a renderer bug.

    Handles colour types 2 (RGB), 6 (RGBA) and 0 (greyscale) at 8 bits, which is
    everything either capture path produces. Interlaced PNGs are rejected rather
    than silently mis-decoded.
    """
    with open(path, 'rb') as f:
        data = f.read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('%s: not a PNG' % path)

    pos, idat, w, h, depth, ctype = 8, [], 0, 0, 0, 0
    while pos + 8 <= len(data):
        length, tag = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length                       # length + tag + body + crc
        if tag == b'IHDR':
            w, h, depth, ctype, _, _, interlace = struct.unpack('>IIBBBBB', body)
            if interlace:
                raise ValueError('%s: interlaced PNG not supported' % path)
            if depth != 8:
                raise ValueError('%s: %d-bit PNG not supported' % (path, depth))
        elif tag == b'IDAT':
            idat.append(body)
        elif tag == b'IEND':
            break
    if not idat or not w or not h:
        raise ValueError('%s: no image data' % path)

    channels = {0: 1, 2: 3, 6: 4}.get(ctype)
    if channels is None:
        raise ValueError('%s: unsupported PNG colour type %d' % (path, ctype))

    raw = zlib.decompress(b''.join(idat))
    stride = w * channels
    out = bytearray(w * h * 3)
    prev = bytearray(stride)
    at = 0
    for y in range(h):
        filt = raw[at]
        line = bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        # PNG scanline filters, per spec. Each is defined against the byte
        # `channels` back in this line and the byte above in the previous one.
        if filt == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 0xFF
        elif filt == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif filt == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif filt == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 0xFF
        elif filt != 0:
            raise ValueError('%s: bad PNG filter %d on row %d' % (path, filt, y))

        base = y * w * 3
        for x in range(w):
            s = x * channels
            if channels == 1:
                v = line[s]
                out[base + x * 3] = out[base + x * 3 + 1] = out[base + x * 3 + 2] = v
            else:
                out[base + x * 3] = line[s]
                out[base + x * 3 + 1] = line[s + 1]
                out[base + x * 3 + 2] = line[s + 2]
        prev = line
    return w, h, bytes(out)


def read_image(path):
    """Either capture format, chosen by extension."""
    return read_png(path) if path.lower().endswith('.png') else read_ppm(path)


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
    w, h, px = read_image(path)
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


def smooth(path, region, max_step=1.5, baseline=8):
    """Is a region that SHOULD shade smoothly free of hard edges?

    Every other claim in this file asks whether something happened. This one asks
    whether what happened has the right SHAPE, and that is the gap the reflection
    probe bug lived in for as long as it did: it drew hard-edged concentric
    squares across a flat floor, and `draws` and `animates` both passed it every
    time, because the frame was neither blank nor still.

    `region` is (x0, y0, x1, y1) as FRACTIONS of the frame, so a claim survives a
    resolution change. It is required and has no default on purpose -- a guessed
    region would pass for the wrong reason, which is worse than no claim.

    Each column in the band is averaged down its height first, which removes
    per-pixel noise without touching a vertical edge (the thing being looked
    for). The reported number is the largest change in that profile across
    `baseline` pixels. A smooth gradient -- a flat surface under a sky, or under
    an environment capture -- moves a fraction of a level over 8 pixels. A
    projection or tiling artefact moves several at once, and that step is the
    measurement.

    MEASURED against the bug it was written for, one capture either side of the
    fix. The reported scene: broken 2.30 of 255, fixed 0.11. The Reflections demo
    written to cover it: broken 6.91, fixed 0.55. The default ceiling of
    1.5 has the whole gap to sit in. Aim the region at the part of the surface
    that should be UNIFORM and leave out any legitimate shading ramp: pointed at
    a wider band that included the floor's own edge gradient, both frames failed
    at ~2.0 and the claim proved nothing.

    KNOWN LIMIT: this means something ONLY on a region meant to be smooth. Aimed
    at geometry edges, text or a detailed texture it fails correctly and
    uselessly. It is for flat receivers -- floors, water, walls -- and the region
    is the part of the claim that carries that judgement.
    """
    w, h, px = read_image(path)
    x0 = max(0, min(w - 1, int(region[0] * w)))
    x1 = max(x0 + 2 * baseline + 2, min(w, int(region[2] * w)))
    y0 = max(0, min(h - 1, int(region[1] * h)))
    y1 = max(y0 + 1, min(h, int(region[3] * h)))
    rows = list(range(y0, y1, 2))
    if not rows or x1 - x0 < 2 * baseline + 2:
        raise ValueError('%s: region %r is too small at %dx%d' % (path, region, w, h))
    prof = []
    for x in range(x0, x1):
        total = 0
        for y in rows:
            i = (y * w + x) * 3
            total += px[i] + px[i + 1] + px[i + 2]
        prof.append(total / (3.0 * len(rows)))
    worst, at = 0.0, x0
    for i in range(baseline, len(prof) - baseline):
        step = abs(prof[i + baseline] - prof[i - baseline])
        if step > worst:
            worst, at = step, x0 + i
    spread = max(prof) - min(prof)
    ok = worst <= max_step
    return ok, ('sharpest edge %.2f of 255 over %dpx at x=%d (ceiling %.2f), '
                'total spread %.2f across the region' % (worst, baseline, at, max_step, spread))


def animates(paths, tol=8, min_changed_pct=0.5):
    """Is anything moving across a series of captures?

    This is the check that catches the failure mode the harness exists for: a
    feature whose shader, data and pipeline all exist and which nothing ever
    switched on renders a perfectly valid frame that is byte-identical to the
    frame three hundred frames earlier.

    TAKES A LIST, and passes if ANY pair of captures differs. Two samples was the
    first design and it was not sound: captures are taken by frame ORDINAL while
    scenes move on a CLOCK, so two samples can land on the same phase of a
    periodic motion, or both land after a physics scene has settled. Measured on
    ShadowCheck, which reported 60.29% moved and then 0.00% on two identical runs
    of the same binary -- its frame 90 was byte-identical across both runs while
    its frame 400 differed between them by 60%. A check that flaky is worse than
    no check, because it teaches people to ignore red.

    `tol` is per channel, so dithering and one-bit noise do not count as motion.
    """
    if isinstance(paths, str):
        paths = [paths]
    if len(paths) < 2:
        return False, 'needs at least two captures, got %d' % len(paths)

    frames = []
    for p in paths:
        w, h, px = read_image(p)
        frames.append((w, h, px, p))

    size = (frames[0][0], frames[0][1])
    for w, h, _, p in frames:
        if (w, h) != size:
            return False, 'size mismatch: %s is %dx%d, expected %dx%d' % (p, w, h, size[0], size[1])

    best, best_pair = 0.0, ''
    step = 3 * STRIDE
    for i in range(len(frames)):
        for j in range(i + 1, len(frames)):
            a, b = frames[i][2], frames[j][2]
            changed = total = 0
            for k in range(0, min(len(a), len(b)) - 2, step):
                total += 1
                if (abs(a[k] - b[k]) > tol or abs(a[k + 1] - b[k + 1]) > tol
                        or abs(a[k + 2] - b[k + 2]) > tol):
                    changed += 1
            pct = 100.0 * changed / total if total else 0.0
            if pct > best:
                best, best_pair = pct, '%s vs %s' % (_tag(frames[i][3]), _tag(frames[j][3]))

    if not best_pair:
        best_pair = '%s vs %s' % (_tag(frames[0][3]), _tag(frames[-1][3]))
    return best >= min_changed_pct, '%.2f%% moved at best (%s), need %.2f%% across %d captures' % (
        best, best_pair, min_changed_pct, len(frames))


def renders(sidecar_path, min_draws=1):
    """Did the renderer issue any draw calls, or is the frame only a backdrop?

    This is the claim `draws` cannot make. A scene whose meshes all failed to
    draw in front of a procedural sky produces a frame with plenty of distinct
    colours and no dominant one, so `draws` passes it -- measured, not feared.

    The evidence is the DRAW CALL count. `entityRenderSlots` is reported next to
    it for context but is deliberately not tested: it is a high-water mark
    indexed by entity id, not a count of things that drew, so it stays non-zero
    for a scene that renders nothing. The web build's getEntityCount is the same
    number under a name that suggests otherwise.
    """
    import json
    with open(sidecar_path, encoding='utf-8') as f:
        data = json.load(f)
    draws_ = data.get('drawCalls', 0)
    ok = draws_ >= min_draws
    return ok, '%d draw calls (need %d), %d entity slots, world holds %d' % (
        draws_, min_draws, data.get('entityRenderSlots', 0), data.get('worldEntities', 0))


def hears(sidecar_path):
    """Is there an audio world at all, and is the listener standing in it?

    The claim `draws` and `renders` cannot make. RoomAcoustics is eleven rooms
    whose entire point is that they sound different; every pixel claim about it
    is a claim about a still photograph, and it will pass forever whatever the
    audio does.

    Three things are tested, and each one was broken for a long time with
    nothing able to see it:

    reverbBusReady   The Freeverb bus lived inside an OFF-by-default CMake
                     option, so every call that fed it returned at its first
                     line and every scene in the engine's history played dry.
                     "The reverb sounds wrong" and "there is no reverb" were
                     indistinguishable from outside.

    listener         It was frozen at the origin, so spatialisation was computed
                     against a point the player was never at. A non-origin
                     listener is weak evidence on its own -- a scene can legally
                     start there -- so it is reported either way and only FAILS
                     when the room says the listener should have moved.

    room             hasMeasuredRoom, or a non-zero decay time. A bus that
                     exists and is fed silence is the same as no bus.

    Twelve DSP suites were green through all of it, because none of them go
    through AudioEngine, which is the one place the feature was switched off.
    That is the gap this claim exists to close.
    """
    import json
    with open(sidecar_path, encoding='utf-8') as f:
        data = json.load(f)
    audio = data.get('audio')
    if audio is None:
        return False, ('no audio block in the sidecar -- this capture came from a '
                       'player built before the audio snapshot existed')

    bus = bool(audio.get('reverbBusReady'))
    decay = float(audio.get('reverbDecayTime', 0.0))
    measured = bool(audio.get('hasMeasuredRoom'))
    listener = audio.get('listener') or [0.0, 0.0, 0.0]
    at_origin = all(abs(float(c)) < 1e-6 for c in listener)

    detail = ('bus %s, decay %.2fs, measuredRoom %s, listener (%.2f, %.2f, %.2f)%s, '
              '%d clip(s) loaded, %d playing' % (
                  'ready' if bus else 'MISSING', decay, 'yes' if measured else 'no',
                  float(listener[0]), float(listener[1]), float(listener[2]),
                  ' AT ORIGIN' if at_origin else '',
                  int(audio.get('soundsLoaded', 0)),
                  int(audio.get('soundsPlaying', 0))))

    if not bus:
        return False, detail + ' -- the reverb bus is not in this build'
    if not measured and decay <= 0.0:
        return False, detail + ' -- the bus exists and nothing is feeding it'
    return True, detail


def _tag(path):
    """The frame marker out of a capture path, for readable reports."""
    import os
    name = os.path.basename(path)
    for part in name.split('.'):
        if part.startswith('f') and part[1:].isdigit():
            return part
    return name


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        print('usage: capture_claims.py draws FRAME.ppm')
        print('       capture_claims.py renders FRAME.json')
        print('       capture_claims.py hears FRAME.json')
        print('       capture_claims.py animates A.ppm B.ppm [min_changed_pct]')
        print('       capture_claims.py smooth FRAME.ppm x0,y0,x1,y1 [max_step]')
        return 2
    claim = sys.argv[1]
    try:
        if claim == 'draws':
            ok, detail = draws(sys.argv[2])
        elif claim == 'renders':
            ok, detail = renders(sys.argv[2])
        elif claim == 'hears':
            ok, detail = hears(sys.argv[2])
        elif claim == 'animates':
            ok, detail = animates(sys.argv[2:])
        elif claim == 'smooth':
            if len(sys.argv) < 4:
                print('smooth needs a region: x0,y0,x1,y1 as fractions of the frame')
                return 2
            reg = tuple(float(v) for v in sys.argv[3].split(','))
            if len(reg) != 4:
                print('smooth region must be four fractions: x0,y0,x1,y1')
                return 2
            step = float(sys.argv[4]) if len(sys.argv) > 4 else 1.5
            ok, detail = smooth(sys.argv[2], reg, step)
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
