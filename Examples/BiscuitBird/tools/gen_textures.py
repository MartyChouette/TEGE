# Biscuit Bird - paintable texture set.
#
# Everything here is a STARTING POINT to paint over, not final art. The maps are
# authored so a brush stroke lands somewhere meaningful:
#
#   tex_fabric  greyscale weave  -> tinted by each shirt's baseColor
#   tex_bark    greyscale grain  -> tinted by the trunk colour
#   tex_straw   greyscale weave  -> tinted by the nest colour
#   tex_paving  greyscale stone  -> tinted by the path colour
#   tex_feather greyscale barbs  -> tinted by the wing colour
#   tex_plumage greyscale down   -> tinted by the body colour (lat/long)
#   tex_wafer   full colour      -> the biscuit face itself
#   tex_uvchart UV reference     -> the PaintTest scene, to see where texels land
#
# Paint them in the editor (View > Pixel Editor > Load, or the material
# inspector's "Edit in external app") and save over the same path. RenderSystem
# polls texture files every 5 seconds and reloads them live - no re-import.
#
# Run from the project root:  python tools/gen_textures.py
# (tools/gen_scene.py calls this for you.)

import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")

PNG_MAGIC = bytes([137, 80, 78, 71, 13, 10, 26, 10])


def write_png(name, w, h, rgba):
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)

    raw = b"".join(bytes([0]) + bytes(rgba[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    png = (PNG_MAGIC
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw))
           + chunk(b"IEND", b""))
    os.makedirs(ASSETS, exist_ok=True)
    open(os.path.join(ASSETS, name), "wb").write(png)


_seed = [98765]


def rnd():
    _seed[0] = (_seed[0] * 1103515245 + 12345) & 0x7fffffff
    return _seed[0] / 0x7fffffff


def clamp8(v):
    return max(0, min(255, int(v)))


def put(buf, w, x, y, r, g, b, a=255):
    i = (y * w + x) * 4
    buf[i] = clamp8(r)
    buf[i + 1] = clamp8(g)
    buf[i + 2] = clamp8(b)
    buf[i + 3] = clamp8(a)


def wafer(size=128):
    """The biscuit face. The cylinder cap UVs are a disc, so this maps as a
    circle onto the top and bottom of every biscuit."""
    buf = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            nx = (x / (size - 1)) * 2 - 1
            ny = (y / (size - 1)) * 2 - 1
            d = math.sqrt(nx * nx + ny * ny)
            if d > 1.0:
                put(buf, size, x, y, 232, 214, 170)   # outside the disc: crumb
                continue
            v = 240 - 26 * d * d                      # domed centre
            v += math.sin(d * 22.0) * 7.0             # pressed rings
            v += (rnd() - 0.5) * 9.0                  # bake speckle
            v -= 32.0 * max(0.0, d - 0.86) / 0.14     # toasted rim
            put(buf, size, x, y, v, v * 0.90, v * 0.70)
    write_png("tex_wafer.png", size, size, buf)


def fabric(size=128):
    """Greyscale weave. Shirt colour stays in the material baseColor, so one
    texture serves all eight pedestrians."""
    buf = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            weave = 10.0 * (((x >> 1) + (y >> 1)) % 2)
            thread = 6.0 * math.sin(x * 0.8) + 6.0 * math.sin(y * 0.8)
            v = 214 + weave + thread + (rnd() - 0.5) * 8.0
            put(buf, size, x, y, v, v, v)
    # a clean chest panel, sized to the cube face UVs, to drop a logo into
    for y in range(int(size * 0.30), int(size * 0.70)):
        for x in range(int(size * 0.30), int(size * 0.70)):
            put(buf, size, x, y, 244, 244, 244)
    write_png("tex_fabric.png", size, size, buf)


def bark(size=128):
    buf = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            groove = math.sin(x * 0.55 + math.sin(y * 0.07) * 2.4) * 26.0
            v = 196 + groove + (rnd() - 0.5) * 20.0
            put(buf, size, x, y, v, v * 0.98, v * 0.94)
    write_png("tex_bark.png", size, size, buf)


def straw(size=128):
    buf = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            strand = math.sin(x * 0.9 + y * 2.6) * 20.0 + math.sin(y * 0.5) * 12.0
            v = 200 + strand + (rnd() - 0.5) * 24.0
            put(buf, size, x, y, v, v * 0.97, v * 0.90)
    write_png("tex_straw.png", size, size, buf)


def paving(size=128):
    buf = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            grout = (x % 64) < 3 or (y % 32) < 3
            v = 150 if grout else 214 + (rnd() - 0.5) * 16.0
            put(buf, size, x, y, v, v * 0.99, v * 0.96)
    write_png("tex_paving.png", size, size, buf)


def uvchart(size=256):
    """Four coloured quadrants, an 8x8 checker, a dark border and a white centre
    cross. Paint a stroke on this and you can read straight off the model which
    part of the UV square you hit."""
    quad = [(214, 80, 70), (80, 150, 214), (240, 190, 70), (90, 190, 110)]
    buf = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            u, v = x / (size - 1), y / (size - 1)
            r, g, b = quad[(0 if u < 0.5 else 1) + (0 if v < 0.5 else 2)]
            k = 1.0 if ((x * 8 // size) + (y * 8 // size)) % 2 else 0.72
            if x < 4 or y < 4 or x > size - 5 or y > size - 5:
                r = g = b = 24
                k = 1.0
            if abs(x - size // 2) < 2 or abs(y - size // 2) < 2:
                r = g = b = 255
                k = 1.0
            put(buf, size, x, y, r * k, g * k, b * k)
    write_png("tex_uvchart.png", size, size, buf)


def feather(size=128):
    """Wing and tail. The feather panel UVs run u along the span and v across
    the chord, so these bars read as flight feathers from root to tip."""
    buf = bytearray(size * size * 4)
    for y in range(size):
        for x in range(size):
            u = x / (size - 1)
            v = y / (size - 1)
            bar = math.sin(u * 46.0) * 16.0            # feather separations
            shaft = -22.0 * math.exp(-((v - 0.18) ** 2) / 0.004)
            tip = -30.0 * max(0.0, u - 0.78) / 0.22    # dark wingtips
            g = 226 + bar + shaft + tip + (rnd() - 0.5) * 7.0
            put(buf, size, x, y, g, g, g)
    write_png("tex_feather.png", size, size, buf)


def plumage(w=256, h=128):
    """Body and head. Sphere UVs are lat/long: u wraps around, v runs pole to
    pole, so v near 1 is the belly. Paint eye stripes and bibs here."""
    buf = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            v = y / (h - 1)
            belly = 46.0 * max(0.0, v - 0.55) / 0.45   # pale underside
            speck = (rnd() - 0.5) * 10.0
            band = math.sin(v * 26.0) * 5.0
            g = 196 + belly + band + speck
            put(buf, w, x, y, g, g * 0.99, g)
    write_png("tex_plumage.png", w, h, buf)


def generate_all():
    wafer()
    fabric()
    bark()
    straw()
    paving()
    feather()
    plumage()
    uvchart()
    return ["tex_wafer.png", "tex_fabric.png", "tex_bark.png",
            "tex_straw.png", "tex_paving.png", "tex_feather.png",
            "tex_plumage.png", "tex_uvchart.png"]


if __name__ == "__main__":
    for n in generate_all():
        print("wrote assets/" + n)
