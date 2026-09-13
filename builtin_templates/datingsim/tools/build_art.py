#!/usr/bin/env python3
"""Portrait art for the VN template, as SVG layers mounted with displayGraphic.

WHY SVG AND NOT QUADS. A displayGraphic entity tessellates its SVG to real
triangles: crisp at any scale, unlit, ALPHA BLENDED, paint order preserved. That
last pair is what a face needs - a brow has to sit over a forehead with nothing
square around it, and a coloured quad cannot do that.

THE ONE IDEA THAT MAKES THIS WORK: every layer is a FULL-FACE document on the
same 200x260 viewBox, with only its own piece drawn and the rest transparent. So
every layer mounts at the identical transform and alignment is baked into the
art instead of into a table of offsets in the scene builder. Swapping a brow can
never nudge it out of place, and an artist redrawing one piece only has to keep
the viewBox.

v1 tessellator limits, per TEGE_NOTES: SOLID FILLS ONLY (a gradient collapses to
its first stop), NO even-odd holes (a donut fills in), strokes are miterless. So
everything here is built from filled paths and ellipses, and anything that wants
to read as an outline is drawn as a filled shape instead.

    python tools/build_art.py     writes assets/art/vn/*.svg

The layer names match PortraitRig's convention exactly, because the scene mounts
them by that name.
"""
import os

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)                      # the template folder
OUT = os.path.join(ROOT, "assets", "art", "vn")
os.makedirs(OUT, exist_ok=True)

W, H = 200, 260

# ---------------------------------------------------------------------------
# THE FACE, in viewBox units. Every piece below is positioned against these, so
# moving an eye is one number here and not a hunt through thirty files.
EYE_Y   = 116
EYE_LX  = 72
EYE_RX  = 128
EYE_RXR = 17     # sclera radii
EYE_RYR = 12
BROW_Y  = 88
MOUTH_Y = 168
BLUSH_Y = 146

INK     = "#1f1c26"      # brows, lashes, mouth lines
# One iris colour, because these files are shared by both busts. Picking it
# from the mirror sign gave every character one blue eye and one purple one.
IRIS    = "#4a5f74"
SCLERA  = "#f4f0ea"
TEETH   = "#efe7dc"
BLUSH   = "#dd8079"
MOUTHIN = "#8c4a52"


def svg(name, body):
    doc = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" '
           'width="%d" height="%d">\n%s\n</svg>\n' % (W, H, W, H, body))
    with open(os.path.join(OUT, name + ".svg"), "w", encoding="utf-8") as f:
        f.write(doc)
    return name


def ell(cx, cy, rx, ry, fill, rot=None):
    t = ' transform="rotate(%g %g %g)"' % (rot, cx, cy) if rot else ""
    return '  <ellipse cx="%g" cy="%g" rx="%g" ry="%g" fill="%s"%s/>' % (
        cx, cy, rx, ry, fill, t)


def path(d, fill):
    return '  <path d="%s" fill="%s"/>' % (d, fill)


def mirror(fn):
    """Draw a piece on both sides of the face. Faces are symmetric and authoring
    each side by hand is how they end up subtly crooked."""
    return fn(EYE_LX, +1) + "\n" + fn(EYE_RX, -1)


# ---------------------------------------------------------------------------
# BASE HEADS. The only pieces that differ between the two characters, because
# everything else is ink and reads the same on either face.
def head(name, skin, hair, hair2, collar, shirt, style):
    parts = []
    # shoulders first, so the head overlaps them
    parts.append(path("M 18 260 C 24 216 62 198 100 198 C 138 198 176 216 182 260 Z", shirt))
    parts.append(path("M 74 200 C 84 214 116 214 126 200 C 118 226 82 226 74 200 Z", collar))
    parts.append(ell(100, 118, 66, 80, skin))          # face
    parts.append(ell(34, 124, 9, 15, skin))            # ears
    parts.append(ell(166, 124, 9, 15, skin))
    if style == "swept":
        # A side-swept fringe with a tail: reads as one silhouette at bust size.
        parts.append(path("M 34 104 C 30 48 66 26 102 26 C 142 26 172 50 168 106 "
                          "C 160 74 150 62 128 58 C 104 76 62 72 48 92 Z", hair))
        parts.append(path("M 126 56 C 152 62 162 80 168 106 C 174 78 168 46 148 32 Z", hair))
    else:
        # A blunt bob, squarer, so the two silhouettes never read as the same
        # person in a different colour.
        parts.append(path("M 32 112 C 28 46 68 24 100 24 C 134 24 174 46 170 112 "
                          "C 166 86 160 70 146 62 C 120 54 80 54 54 62 C 40 70 36 86 32 112 Z", hair))
        parts.append(path("M 28 108 C 22 150 26 176 34 190 C 40 160 38 130 40 110 Z", hair2))
        parts.append(path("M 174 108 C 180 150 176 176 168 190 C 162 160 164 130 162 110 Z", hair2))
    return svg(name, "\n".join(parts))


head("base_head_ren",  "#caa183", "#3c2b28", "#54403a", "#8a6a55", "#6a5347", "swept")
head("base_head_mira", "#d3ab93", "#2b3347", "#3e4a63", "#5d6b84", "#465368", "bob")

# THE DIMMER, as the character's own silhouette rather than a rectangle laid
# over them. A quad overlay leaves a visible box around the bust, and on an
# emissive material a blend washes the face grey instead of darkening it.
# displayGraphic is unlit and alpha blended, so the same shape in one near-black
# at partial opacity darkens exactly the person and nothing around them.
#
# TWO SHAPES ONLY, barely overlapping. Reusing the head() parts would stack five
# translucent fills and the overlaps would read as blotches, which is worse than
# not dimming at all.
SHADE, SHADE_A = "#0b0a11", "0.60"
for who, hood in (("ren",  "M 30 116 C 26 44 66 22 100 22 C 136 22 174 46 170 116 "
                           "C 170 168 140 202 100 202 C 60 202 30 168 30 116 Z"),
                  ("mira", "M 28 118 C 24 42 68 20 100 20 C 134 20 176 44 172 118 "
                           "C 176 170 140 204 100 204 C 60 204 24 170 28 118 Z")):
    svg("dim_" + who,
        '  <path d="M 18 260 C 24 214 62 196 100 196 C 138 196 176 214 182 260 Z" '
        'fill="%s" fill-opacity="%s"/>\n' % (SHADE, SHADE_A) +
        '  <path d="%s" fill="%s" fill-opacity="%s"/>' % (hood, SHADE, SHADE_A))

# ---------------------------------------------------------------------------
# BROWS. Filled wedges rather than strokes: a stroke here would be miterless and
# read as a rounded worm.
BROWS = {
    "neutral":   ("M -26 -2 C -12 -9 12 -10 26 -8 L 26 -1 C 12 -3 -12 -2 -26 5 Z", 0),
    "raised":    ("M -26 -2 C -12 -16 12 -16 26 -3 L 26 5 C 12 -8 -12 -8 -26 6 Z", 0),
    "furrowed":  ("M -26 -8 C -12 -6 12 2 26 8 L 24 15 C 10 8 -12 0 -26 0 Z", 0),
    "worried":   ("M -26 4 C -12 -6 12 -10 26 -4 L 26 4 C 12 -2 -12 2 -26 12 Z", 0),
    "skeptical": ("M -26 -4 C -12 -12 12 -12 26 -5 L 26 3 C 12 -4 -12 -4 -26 4 Z", 0),
}
for nm, (d, _) in BROWS.items():
    def brow(cx, s, d=d, nm=nm):
        # The skeptical brow is the asymmetric one: only the left goes up.
        dy = -7 if (nm == "skeptical" and s > 0) else 0
        return ('  <g transform="translate(%g %g) scale(%d 1)">\n    <path d="%s" fill="%s"/>\n  </g>'
                % (cx, BROW_Y + dy, s, d, INK))
    svg("brow_" + nm, mirror(brow))

# ---------------------------------------------------------------------------
# EYES. Sclera, iris, then a lash wedge across the top, which is what actually
# sells an eye at bust size.
def eye_open(iris_dy=0, iris_dx=0, lid=0.0, iris=True):
    def one(cx, s):
        p = [ell(cx, EYE_Y, EYE_RXR, EYE_RYR, SCLERA)]
        if iris:
            p.append(ell(cx + iris_dx * s, EYE_Y + iris_dy, 8.5, 9.5, IRIS))
            p.append(ell(cx + iris_dx * s, EYE_Y + iris_dy, 4, 4.5, INK))
            p.append(ell(cx + iris_dx * s - 3, EYE_Y + iris_dy - 4, 2.4, 2.4, "#ffffff"))
        if lid > 0:
            p.append('  <rect x="%g" y="%g" width="%g" height="%g" fill="%s"/>'
                     % (cx - EYE_RXR, EYE_Y - EYE_RYR, EYE_RXR * 2, EYE_RYR * 2 * lid, INK))
        # lash line
        p.append(path("M %g %g C %g %g %g %g %g %g L %g %g C %g %g %g %g %g %g Z"
                      % (cx - EYE_RXR, EYE_Y - 3,
                         cx - 10, EYE_Y - EYE_RYR - 3, cx + 10, EYE_Y - EYE_RYR - 3,
                         cx + EYE_RXR, EYE_Y - 3,
                         cx + EYE_RXR, EYE_Y - 7,
                         cx + 10, EYE_Y - EYE_RYR - 8, cx - 10, EYE_Y - EYE_RYR - 8,
                         cx - EYE_RXR, EYE_Y - 7), INK))
        return "\n".join(p)
    return mirror(one)


svg("eye_open",  eye_open())
svg("eye_wide",  eye_open(iris_dy=1))
svg("eye_left",  eye_open(iris_dx=-6))
svg("eye_right", eye_open(iris_dx=6))
svg("eye_down",  eye_open(iris_dy=5))
svg("eye_half",  eye_open(lid=0.45))
svg("eye_teary", eye_open(iris_dy=2) + "\n" +
    mirror(lambda cx, s: ell(cx + 12 * s, EYE_Y + 10, 5, 7, "#a8d4ec")))

# Closed eyes are a filled arc, not an ellipse: a line that curves the right way
# is the whole difference between asleep and content.
svg("eye_closed", mirror(lambda cx, s: path(
    "M %g %g C %g %g %g %g %g %g L %g %g C %g %g %g %g %g %g Z"
    % (cx - 16, EYE_Y - 1, cx - 8, EYE_Y + 7, cx + 8, EYE_Y + 7, cx + 16, EYE_Y - 1,
       cx + 16, EYE_Y - 6, cx + 8, EYE_Y + 1, cx - 8, EYE_Y + 1, cx - 16, EYE_Y - 6), INK)))

svg("eye_happy", mirror(lambda cx, s: path(
    "M %g %g C %g %g %g %g %g %g L %g %g C %g %g %g %g %g %g Z"
    % (cx - 16, EYE_Y + 4, cx - 8, EYE_Y - 8, cx + 8, EYE_Y - 8, cx + 16, EYE_Y + 4,
       cx + 16, EYE_Y + 9, cx + 8, EYE_Y - 2, cx - 8, EYE_Y - 2, cx - 16, EYE_Y + 9), INK)))

svg("eye_squeezed", mirror(lambda cx, s: (
    path("M %g %g L %g %g L %g %g L %g %g Z"
         % (cx - 15, EYE_Y - 9, cx + 2, EYE_Y + 1, cx - 1, EYE_Y + 5, cx - 17, EYE_Y - 5), INK)
    + "\n" +
    path("M %g %g L %g %g L %g %g L %g %g Z"
         % (cx + 15, EYE_Y - 9, cx - 2, EYE_Y + 1, cx + 1, EYE_Y + 5, cx + 17, EYE_Y - 5), INK))))

# ---------------------------------------------------------------------------
# MOUTHS.
M = {
    "neutral":    path("M 84 %d C 92 %d 108 %d 116 %d L 116 %d C 108 %d 92 %d 84 %d Z"
                       % (MOUTH_Y, MOUTH_Y + 3, MOUTH_Y + 3, MOUTH_Y,
                          MOUTH_Y + 4, MOUTH_Y + 7, MOUTH_Y + 7, MOUTH_Y + 4), INK),
    "smile":      path("M 80 %d C 90 %d 110 %d 120 %d L 120 %d C 110 %d 90 %d 80 %d Z"
                       % (MOUTH_Y - 2, MOUTH_Y + 9, MOUTH_Y + 9, MOUTH_Y - 2,
                          MOUTH_Y + 2, MOUTH_Y + 14, MOUTH_Y + 14, MOUTH_Y + 2), INK),
    "open-smile": (path("M 78 %d C 90 %d 110 %d 122 %d C 112 %d 88 %d 78 %d Z"
                        % (MOUTH_Y - 3, MOUTH_Y + 18, MOUTH_Y + 18, MOUTH_Y - 3,
                           MOUTH_Y - 7, MOUTH_Y - 7, MOUTH_Y - 3), INK) + "\n" +
                   path("M 84 %d C 94 %d 106 %d 116 %d C 108 %d 92 %d 84 %d Z"
                        % (MOUTH_Y - 2, MOUTH_Y + 2, MOUTH_Y + 2, MOUTH_Y - 2,
                           MOUTH_Y - 4, MOUTH_Y - 4, MOUTH_Y - 2), TEETH)),
    "big-open":   (ell(100, MOUTH_Y + 6, 22, 17, INK) + "\n" +
                   ell(100, MOUTH_Y + 12, 13, 9, MOUTHIN)),
    "frown":      path("M 82 %d C 92 %d 108 %d 118 %d L 118 %d C 108 %d 92 %d 82 %d Z"
                       % (MOUTH_Y + 8, MOUTH_Y - 3, MOUTH_Y - 3, MOUTH_Y + 8,
                          MOUTH_Y + 4, MOUTH_Y - 7, MOUTH_Y - 7, MOUTH_Y + 4), INK),
    "small-o":    (ell(100, MOUTH_Y + 3, 10, 12, INK) + "\n" +
                   ell(100, MOUTH_Y + 4, 6, 8, MOUTHIN)),
    "gritted":    (path("M 78 %d L 122 %d L 122 %d L 78 %d Z"
                        % (MOUTH_Y - 4, MOUTH_Y - 4, MOUTH_Y + 10, MOUTH_Y + 10), INK) + "\n" +
                   path("M 81 %d L 119 %d L 119 %d L 81 %d Z"
                        % (MOUTH_Y - 1, MOUTH_Y - 1, MOUTH_Y + 7, MOUTH_Y + 7), TEETH) + "\n" +
                   path("M 81 %d L 119 %d L 119 %d L 81 %d Z"
                        % (MOUTH_Y + 2, MOUTH_Y + 2, MOUTH_Y + 4, MOUTH_Y + 4), INK)),
    "pout":       (ell(100, MOUTH_Y + 4, 13, 9, INK) + "\n" +
                   ell(100, MOUTH_Y + 2, 9, 4, MOUTHIN)),
    "wavy":       path("M 78 %d C 86 %d 94 %d 100 %d C 106 %d 114 %d 122 %d "
                       "L 122 %d C 114 %d 106 %d 100 %d C 94 %d 86 %d 78 %d Z"
                       % (MOUTH_Y + 2, MOUTH_Y - 6, MOUTH_Y + 10, MOUTH_Y + 2,
                          MOUTH_Y - 6, MOUTH_Y + 10, MOUTH_Y + 2,
                          MOUTH_Y + 6, MOUTH_Y + 14, MOUTH_Y - 2, MOUTH_Y + 6,
                          MOUTH_Y + 14, MOUTH_Y - 2, MOUTH_Y + 6), INK),
    # The lip-sync halfway mouth. Three phases is enough when the words are text.
    "mid":        (ell(100, MOUTH_Y + 4, 15, 9, INK) + "\n" +
                   ell(100, MOUTH_Y + 6, 9, 4, MOUTHIN)),
}
for nm, body in M.items():
    svg("mouth_" + nm, body)

# ---------------------------------------------------------------------------
svg("blush_light", mirror(lambda cx, s: ell(cx - 18 * s, BLUSH_Y, 17, 8, BLUSH)))
svg("blush_heavy", mirror(lambda cx, s: (
    ell(cx - 18 * s, BLUSH_Y, 21, 11, BLUSH) + "\n" +
    ell(cx - 18 * s, BLUSH_Y, 13, 6, "#e89a94"))))

# ---------------------------------------------------------------------------
# EFFECT OVERLAYS. These sit outside the face, top-right, where the form has
# always put them.
FX_X, FX_Y = 168, 44
svg("fx_sweat", path("M %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g Z"
                     % (FX_X, FX_Y - 16, FX_X + 13, FX_Y, FX_X + 11, FX_Y + 12,
                        FX_X, FX_Y + 12, FX_X - 11, FX_Y + 12, FX_X - 13, FX_Y,
                        FX_X, FX_Y - 16), "#8ec6e4"))
svg("fx_anger", "\n".join([
    path("M %g %g L %g %g L %g %g L %g %g Z" % (FX_X - 14, FX_Y - 12, FX_X + 2, FX_Y - 12,
                                                FX_X + 2, FX_Y - 7, FX_X - 14, FX_Y - 7), "#d8564a"),
    path("M %g %g L %g %g L %g %g L %g %g Z" % (FX_X - 14, FX_Y - 12, FX_X - 9, FX_Y - 12,
                                                FX_X - 9, FX_Y + 6, FX_X - 14, FX_Y + 6), "#d8564a"),
    path("M %g %g L %g %g L %g %g L %g %g Z" % (FX_X + 2, FX_Y - 2, FX_X + 16, FX_Y - 2,
                                                FX_X + 16, FX_Y + 3, FX_X + 2, FX_Y + 3), "#d8564a"),
    path("M %g %g L %g %g L %g %g L %g %g Z" % (FX_X + 11, FX_Y - 2, FX_X + 16, FX_Y - 2,
                                                FX_X + 16, FX_Y + 16, FX_X + 11, FX_Y + 16), "#d8564a"),
]))
svg("fx_tears", mirror(lambda cx, s: (
    path("M %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g Z"
         % (cx + 14 * s, EYE_Y + 8, cx + 20 * s, EYE_Y + 22, cx + 18 * s, EYE_Y + 38,
            cx + 12 * s, EYE_Y + 40, cx + 6 * s, EYE_Y + 38, cx + 8 * s, EYE_Y + 20,
            cx + 14 * s, EYE_Y + 8), "#8ec6e4"))))
svg("fx_sparkle", "\n".join([
    path("M %g %g L %g %g L %g %g L %g %g Z"
         % (FX_X, FX_Y - 18, FX_X + 6, FX_Y, FX_X, FX_Y + 18, FX_X - 6, FX_Y), "#f2d572"),
    path("M %g %g L %g %g L %g %g L %g %g Z"
         % (FX_X - 18, FX_Y, FX_X, FX_Y - 6, FX_X + 18, FX_Y, FX_X, FX_Y + 6), "#f2d572"),
    path("M 40 74 L 44 84 L 40 94 L 36 84 Z", "#f2d572"),
    path("M 30 84 L 40 80 L 50 84 L 40 88 Z", "#f2d572"),
]))
svg("fx_gloom", "\n".join([
    path("M 32 26 L 168 26 L 168 58 C 140 42 60 42 32 58 Z", "#33303c"),
    path("M 52 30 L 58 62 L 50 62 L 46 30 Z", "#2a2833"),
    path("M 92 30 L 96 66 L 88 66 L 86 30 Z", "#2a2833"),
    path("M 132 30 L 136 62 L 128 62 L 126 30 Z", "#2a2833"),
]))
svg("fx_excl", "\n".join([
    path("M %g %g L %g %g L %g %g L %g %g Z"
         % (FX_X - 6, FX_Y - 20, FX_X + 6, FX_Y - 20, FX_X + 3, FX_Y + 4, FX_X - 3, FX_Y + 4),
       "#efe3cf"),
    ell(FX_X, FX_Y + 13, 5.5, 5.5, "#efe3cf"),
]))
svg("fx_quest", "\n".join([
    path("M %g %g C %g %g %g %g %g %g C %g %g %g %g %g %g L %g %g "
         "C %g %g %g %g %g %g C %g %g %g %g %g %g Z"
         % (FX_X - 11, FX_Y - 12, FX_X - 11, FX_Y - 24, FX_X + 13, FX_Y - 24, FX_X + 12, FX_Y - 12,
            FX_X + 11, FX_Y - 4, FX_X + 3, FX_Y - 3, FX_X + 3, FX_Y + 4,
            FX_X - 3, FX_Y + 4,
            FX_X - 3, FX_Y - 7, FX_X + 5, FX_Y - 8, FX_X + 6, FX_Y - 13,
            FX_X + 6, FX_Y - 18, FX_X - 5, FX_Y - 18, FX_X - 5, FX_Y - 12), "#efe3cf"),
    ell(FX_X, FX_Y + 13, 5.5, 5.5, "#efe3cf"),
]))

names = sorted(f for f in os.listdir(OUT) if f.endswith(".svg"))
print("wrote %d layer SVGs to assets/art/vn" % len(names))
print("  " + "  ".join(n[:-4] for n in names))
