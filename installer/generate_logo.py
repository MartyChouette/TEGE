"""Generate the TEGE logo, app icons, and social preview.

The mark is fashioned after the engine's intro screen: a serif wordmark between
two ruled lines with arrow caps, a soft centre glow, concentric orbits, corner
brackets, and a scattered field of dots and shapes.

Everything is drawn as vector. The wordmark is converted to outlines from the
same Playfair Display the splash uses, taken from the engine's own embedded
copy, so the SVGs carry no font dependency and the art cannot drift from the
intro screen.

    python installer/generate_logo.py

Writes tege_logo.svg, tege_circle{,_small,_tiny}.svg, TEGE_ICON*.png,
enjin.ico and social_preview.png, all beside this script.

Requires: fonttools, cairosvg, pillow.
"""
import io
import math
import os
import re

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
EMBEDDED_FONT = os.path.join(REPO, 'Engine', 'include', 'Enjin', 'GUI', 'EmbeddedPlayfair.h')

# Sampled from the intro screen.
BG   = '#40404F'
INK  = '#E5EEE3'
LINE = '#A9AEB6'
GLOW = '#7A857F'

ICON_SIZES = [16, 24, 32, 48, 64, 128, 256]
SMALL_UNTIL = 32          # below this the scatter field is noise, so drop it
TINY_UNTIL = 16           # at this size four serif letters cannot read at all


def load_font():
    """Playfair Display, read out of the engine's embedded byte array."""
    src = io.open(EMBEDDED_FONT, encoding='utf-8').read()
    i = src.index('PlayfairDisplayTTF[]')
    body = src[src.index('{', i) + 1:src.index('};', i)]
    data = bytes(int(x) for x in re.findall(r'\d+', body))
    return TTFont(io.BytesIO(data))


def wordmark(font, text, size_px, cx, cy, tracking):
    """Outlines for `text`, horizontally centred on cx with baseline at cy."""
    upem = font['head'].unitsPerEm
    glyphset = font.getGlyphSet()
    cmap = font.getBestCmap()
    hmtx = font['hmtx']
    scale = size_px / upem
    track = tracking * size_px

    names = [cmap[ord(c)] for c in text]
    total = sum(hmtx[n][0] for n in names) * scale + track * (len(names) - 1)

    x = cx - total / 2.0
    parts = []
    for n in names:
        pen = SVGPathPen(glyphset)
        glyphset[n].draw(pen)
        d = pen.getCommands()
        if d:
            parts.append('<g transform="translate(%.3f %.3f) scale(%.6f %.6f)">'
                         '<path d="%s"/></g>' % (x, cy, scale, -scale, d))
        x += hmtx[n][0] * scale + track
    return '\n    '.join(parts), total


def ring(cx, cy, r, op):
    return ('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="%s" '
            'stroke-width="1" opacity="%.3f"/>' % (cx, cy, r, LINE, op))


def dot(cx, cy, r, op):
    return ('<circle cx="%.1f" cy="%.1f" r="%.1f" fill="%s" opacity="%.3f"/>'
            '<circle cx="%.1f" cy="%.1f" r="%.1f" fill="%s" opacity="%.3f"/>'
            % (cx, cy, r * 2.6, INK, op * 0.16, cx, cy, r, INK, op))


def diamond(cx, cy, r, op):
    return ('<path d="M %.1f %.1f L %.1f %.1f L %.1f %.1f L %.1f %.1f Z" fill="%s" opacity="%.3f"/>'
            % (cx, cy - r, cx + r, cy, cx, cy + r, cx - r, cy, INK, op))


def tri(cx, cy, r, op, up=True):
    d = -1 if up else 1
    return ('<path d="M %.1f %.1f L %.1f %.1f L %.1f %.1f Z" fill="%s" opacity="%.3f"/>'
            % (cx, cy + d * r, cx + r * 0.86, cy - d * r * 0.5,
               cx - r * 0.86, cy - d * r * 0.5, INK, op))


def hexagon(cx, cy, r, op):
    pts = ' '.join('%.1f,%.1f' % (cx + r * math.cos(math.radians(60 * i - 30)),
                                  cy + r * math.sin(math.radians(60 * i - 30)))
                   for i in range(6))
    return ('<polygon points="%s" fill="none" stroke="%s" stroke-width="1" opacity="%.3f"/>'
            % (pts, LINE, op))


def rule(cx, cy, half, op):
    x0, x1 = cx - half, cx + half
    a = 5.0
    return ('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" stroke="%s" stroke-width="1.6" opacity="%.3f"/>'
            '<path d="M %.1f %.1f L %.1f %.1f L %.1f %.1f Z" fill="%s" opacity="%.3f"/>'
            '<path d="M %.1f %.1f L %.1f %.1f L %.1f %.1f Z" fill="%s" opacity="%.3f"/>'
            % (x0, cy, x1, cy, LINE, op,
               x0, cy, x0 + a, cy - a * 0.62, x0 + a, cy + a * 0.62, LINE, op,
               x1, cy, x1 - a, cy - a * 0.62, x1 - a, cy + a * 0.62, LINE, op))


def corners(w, h, inset, arm, op):
    out = []
    for sx, sy, ax, ay in ((0, 0, 1, 1), (1, 0, -1, 1), (0, 1, 1, -1), (1, 1, -1, -1)):
        x = inset + sx * (w - 2 * inset)
        y = inset + sy * (h - 2 * inset)
        out.append('<path d="M %.1f %.1f L %.1f %.1f M %.1f %.1f L %.1f %.1f" '
                   'stroke="%s" stroke-width="1.6" fill="none" opacity="%.3f"/>'
                   % (x, y + ay * arm, x, y, x, y, x + ax * arm, y, LINE, op))
    return '\n    '.join(out)


def field(cx, cy, seed, count, rmax, rmin):
    """Scattered orbiting shapes. Seeded, so the art is identical every run."""
    out = []
    s = seed

    def rnd():
        nonlocal s
        s = (s * 1103515245 + 12345) & 0x7FFFFFFF
        return s / 0x7FFFFFFF

    for _ in range(count):
        ang = rnd() * math.tau
        rad = rmin + (rmax - rmin) * math.sqrt(rnd())
        x, y = cx + math.cos(ang) * rad, cy + math.sin(ang) * rad * 0.92
        k = rnd()
        if k < 0.10:
            out.append(hexagon(x, y, 12 + rnd() * 16, 0.10 + rnd() * 0.06))
        elif k < 0.20:
            out.append(tri(x, y, 3.4 + rnd() * 1.8, 0.20 + rnd() * 0.14, up=rnd() > 0.5))
        elif k < 0.28:
            out.append(diamond(x, y, 4.0 + rnd() * 2.4, 0.24 + rnd() * 0.16))
        else:
            out.append(dot(x, y, 1.6 + rnd() * 4.2, 0.22 + rnd() * 0.45))
    return '\n    '.join(out)


def build(font, w, h, word_px, tracking, rings, dots, seed, inset, circle=False, solo=None):
    cx, cy = w / 2.0, h / 2.0
    text = solo or 'TEGE'
    path, adv = wordmark(font, text, word_px, cx, cy + word_px * 0.355, tracking)
    half = adv / 2.0 + word_px * 0.42
    gap = word_px * 0.72

    o = ['<rect width="%d" height="%d" fill="%s"/>' % (w, h, BG),
         '<circle cx="%.1f" cy="%.1f" r="%.1f" fill="url(#glow)"/>' % (cx, cy, word_px * 1.95)]
    o += [ring(cx, cy, r, op) for r, op in rings]
    if dots:
        o.append(field(cx, cy, seed, dots, min(w, h) * 0.62, word_px * 1.5))
    o.append(corners(w, h, inset, min(w, h) * 0.055, 0.30))
    if not solo:
        o.append(rule(cx, cy - gap, half, 0.55))
        o.append(rule(cx, cy + gap, half, 0.55))
        o.append(diamond(cx - half - word_px * 0.30, cy, word_px * 0.115, 0.55))
        o.append(diamond(cx + half + word_px * 0.30, cy, word_px * 0.115, 0.55))
    o.append('<g fill="%s">\n    %s\n  </g>' % (INK, path))

    body = '\n  '.join(o)
    clip = ''
    rim = ''
    if circle:
        # Round mark: everything is clipped to a centred disc, so the art sits
        # inside the circle and the corners come out transparent.
        r = min(w, h) / 2.0 - 1.0
        clip = ('<clipPath id="disc"><circle cx="%.1f" cy="%.1f" r="%.1f"/></clipPath>'
                % (cx, cy, r))
        body = '<g clip-path="url(#disc)">\n  %s\n  </g>' % body
        rim = ('\n  <circle cx="%.1f" cy="%.1f" r="%.1f" fill="none" stroke="%s" '
               'stroke-width="5" opacity="0.85"/>' % (cx, cy, r, LINE))

    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" width="%d" height="%d">\n'
            '  <defs>%s<radialGradient id="glow">'
            '<stop offset="0%%" stop-color="%s" stop-opacity="0.55"/>'
            '<stop offset="100%%" stop-color="%s" stop-opacity="0"/>'
            '</radialGradient></defs>\n  %s%s\n</svg>\n'
            % (w, h, w, h, clip, GLOW, GLOW, body, rim))


def main():
    font = load_font()

    svgs = {
        # Wide lockup for the README banner and the GitHub social preview.
        'tege_logo.svg': build(font, 1280, 640, 132, 0.10,
                               [(150, .20), (215, .15), (290, .11), (375, .08), (470, .05)],
                               54, 20260905, 42),
        # Round mark: a circular crop of the same centre. The corners come out
        # transparent, so it drops into a launcher or an avatar slot as-is.
        'tege_circle.svg': build(font, 512, 512, 94, 0.09,
                                 [(126, .22), (176, .15), (232, .09)],
                                 22, 64064079, 26, circle=True),
        # Below ~32px the scatter field and outer rings turn to noise, so the
        # small mark drops them and gives the wordmark the room instead.
        'tege_circle_small.svg': build(font, 512, 512, 152, 0.04,
                                       [(198, .20)], 0, 1, 30, circle=True),
        # 16px gets a single letter. Four serif letters in sixteen pixels is not
        # winnable, and a smear of grey is worse than one clear glyph.
        'tege_circle_tiny.svg': build(font, 512, 512, 300, 0.0,
                                      [], 0, 1, 30, circle=True, solo='T'),
    }
    for name, data in svgs.items():
        io.open(os.path.join(HERE, name), 'w', encoding='utf-8', newline='\n').write(data)

    import cairosvg
    from PIL import Image

    def render(name, px):
        buf = io.BytesIO()
        cairosvg.svg2png(url=os.path.join(HERE, name), write_to=buf,
                         output_width=px, output_height=px)
        buf.seek(0)
        return Image.open(buf).convert('RGBA')

    icons = {}
    for s in ICON_SIZES:
        src = ('tege_circle_tiny.svg' if s <= TINY_UNTIL else
               'tege_circle_small.svg' if s <= SMALL_UNTIL else 'tege_circle.svg')
        icons[s] = render(src, s)
        icons[s].save(os.path.join(HERE, 'TEGE_ICON_%d.png' % s))
    render('tege_circle.svg', 512).save(os.path.join(HERE, 'TEGE_ICON.png'))
    icons[256].save(os.path.join(HERE, 'enjin.ico'), format='ICO',
                    sizes=[(s, s) for s in ICON_SIZES])

    buf = io.BytesIO()
    cairosvg.svg2png(url=os.path.join(HERE, 'tege_logo.svg'), write_to=buf,
                     output_width=1280, output_height=640)
    buf.seek(0)
    Image.open(buf).convert('RGB').save(os.path.join(HERE, 'social_preview.png'))

    print('wrote %d SVGs, %d icons, enjin.ico, social_preview.png'
          % (len(svgs), len(ICON_SIZES) + 1))


if __name__ == '__main__':
    main()
