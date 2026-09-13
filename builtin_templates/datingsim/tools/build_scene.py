#!/usr/bin/env python3
"""The VN / dating-sim template scene: you and two prospects.

A SCENE GENERATOR, not a content authoring tool. It lays out the furniture every
visual novel needs, wires two PortraitRigs and one VNScene, and stops. The
CONTENT is a data asset (data/conversations/*.enjdata against
data/schemas/conversation.enjschema), authored in the editor's Data Assets panel,
so writing a new scene never means touching this file.

The layout is the form's oldest arrangement, and it is that way because it works:
two busts facing in, the speaker lit and the other dimmed, a name above a box of
text along the bottom, the answers under it, and each prospect's standing shown
above their own head rather than in a menu you have to go and look at.

    python tools/build_scene.py                 play it by hand
    python tools/build_scene.py --auto 1.2      walk itself, for a capture

Portrait layers are flat coloured quads. Swap each one's material for real art in
the editor and nothing else changes; PortraitRig finds them by name.
"""
import json, math, os, sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)                      # the template folder

# renderSettings comes from the engine's OWN baseline template, not from a game.
# A template that reaches into somebody's project for its render config is a
# template that only builds on the machine that made it.
REF = json.load(open(os.path.join(os.path.dirname(ROOT), "blank", "scene.enjin")))

# EMPTY fontPath = the engine's bundled default. A template must not name a font
# file, because it does not ship one, and a fontPath that fails to load renders
# NOTHING with no warning (see the WYSIWYG audit): every line of dialogue in this
# scene would silently disappear in a new project.
BFONT = ""
CONVERSATION = "conversations/first_evening"

AUTO = 0.0
if "--auto" in sys.argv:
    AUTO = float(sys.argv[sys.argv.index("--auto") + 1])

entities = []
_sid = [9000]


def sid():
    _sid[0] += 1
    return _sid[0]


def add(name, **comps):
    e = {"id": len(entities) + 1, "name": {"name": name}, "stableId": {"id": sid()}}
    e.update(comps)
    entities.append(e)
    return e


def xform(pos, scale=(1, 1, 1)):
    return {"position": list(pos), "rotation": [0, 0, 0, 1], "scale": list(scale)}


def quad():
    return {"vertexCount": 4, "indexCount": 6,
            "vertices": [{"position": [-0.5, -0.5, 0], "normal": [0, 0, 1], "uv": [0, 1]},
                         {"position": [0.5, -0.5, 0], "normal": [0, 0, 1], "uv": [1, 1]},
                         {"position": [0.5, 0.5, 0], "normal": [0, 0, 1], "uv": [1, 0]},
                         {"position": [-0.5, 0.5, 0], "normal": [0, 0, 1], "uv": [0, 0]}],
            "indices": [0, 1, 2, 0, 2, 3]}


def disc(n=28, r=0.5):
    v = [{"position": [0, 0, 0], "normal": [0, 0, 1], "uv": [0.5, 0.5]}]
    for i in range(n):
        a = 2 * math.pi * i / n
        v.append({"position": [r * math.cos(a), r * math.sin(a), 0],
                  "normal": [0, 0, 1], "uv": [0.5, 0.5]})
    idx = []
    for i in range(n):
        idx += [0, 1 + i, 1 + (i + 1) % n]
    return {"vertexCount": n + 1, "indexCount": len(idx), "vertices": v, "indices": idx}


def srgb(c):
    # Emissive is taken as LINEAR and the framebuffer is sRGB, so a colour
    # authored at 0.15 leaves the screen at 0.44. These are colours as they
    # should LOOK, converted on the way in.
    return [round(v ** 2.2, 5) for v in c]


def flat(color, alpha=None):
    m = {"baseColor": [0, 0, 0], "metallic": 0.0, "roughness": 1.0,
         "emissiveColor": srgb(color), "emissiveStrength": 1.0,
         "alphaMode": 0, "castShadows": False}
    if alpha is not None:
        # AlphaMode{Opaque 0, Mask 1, Blend 2} and the serialized field is
        # `opacity`. An opaque dimmer does not read as "dimmed", it reads as a
        # black rectangle where a person used to be.
        m["alphaMode"] = 2
        m["opacity"] = alpha
    return m


def text(s, wh, color, wrap=4000.0, align=0):
    return {"text": s, "fontPath": BFONT, "fontSize": 48.0, "wrapWidth": wrap,
            "textColor": srgb(color), "bgOpacity": 0.0, "horizontalAlign": align,
            "sdfText": True, "worldHeight": wh, "lit": False}


# ---------------------------------------------------------------------------
# PORTRAIT LAYERS, as SVG mounted with displayGraphic (build_vn_art.py draws
# them). Every layer is a full-face document on one 200x260 viewBox with only
# its own piece drawn, so ALL of them mount at the identical transform and the
# alignment lives in the art instead of in a table of offsets here. Swapping a
# brow cannot nudge it out of place.
ART = "assets/art/vn/"
DOC_W, DOC_H = 200.0, 260.0
BUST_H = 4.00                       # world height of the whole face document
BUST_W = BUST_H * DOC_W / DOC_H
BUST_TOP = 2.66                     # displayGraphic anchors TOP-LEFT at the origin
                                    # (bottom lands at -1.48, just under the box top)

VARIANTS = {
    "brow":  ["neutral", "raised", "furrowed", "worried", "skeptical"],
    "eye":   ["open", "half", "happy", "wide", "left", "right", "down",
              "teary", "squeezed", "closed"],
    "mouth": ["neutral", "smile", "open-smile", "big-open", "frown", "small-o",
              "gritted", "pout", "wavy", "mid"],
    "blush": ["light", "heavy"],
    "fx":    ["sweat", "anger", "tears", "sparkle", "gloom", "excl", "quest"],
}
# Paint order, back to front. displayGraphic is alpha blended, so this is what
# stops a blush landing on top of a mouth.
Z = {"blush": 0.10, "brow": 0.20, "eye": 0.20, "mouth": 0.20, "fx": 0.30}

C_BG    = (0.055, 0.052, 0.075)
C_WALL  = (0.105, 0.098, 0.135)
C_POOL  = (0.118, 0.110, 0.147)
C_FLOOR = (0.075, 0.068, 0.095)
C_BOX   = (0.125, 0.118, 0.155)
C_BOXLO = (0.085, 0.080, 0.110)
C_TX    = (0.91, 0.89, 0.85)
C_MUT   = (0.50, 0.48, 0.55)
C_NAME  = (0.95, 0.80, 0.44)
C_OPT   = (0.68, 0.85, 0.94)
C_DIM   = (0.055, 0.052, 0.075)
C_TRACK = (0.19, 0.18, 0.23)
C_RULE  = (0.26, 0.24, 0.31)
C_MARK  = (0.86, 0.84, 0.78)
C_NOTE  = (0.52, 0.50, 0.57)
FILLS   = [(0.90, 0.55, 0.48), (0.48, 0.76, 0.88)]


def graphic(src, world_h):
    return {"sourcePath": src, "worldHeight": world_h, "curveTolerance": 0.15}


def bust(prefix, cx, head_svg):
    """One character. base_head is the only piece that differs between the two;
    every other layer is ink and reads the same on either face, so both busts
    mount the SAME file under their own prefixed entity name."""
    x = cx - BUST_W / 2.0
    add(prefix + "base_head", transform=xform((x, BUST_TOP, 0.0)),
        displayGraphic=graphic(ART + head_svg + ".svg", BUST_H))
    for slot, names in VARIANTS.items():
        for v in names:
            add(prefix + slot + "_" + v, transform=xform((x, BUST_TOP, Z[slot])),
                displayGraphic=graphic(ART + slot + "_" + v + ".svg", BUST_H))


# ---- camera ---------------------------------------------------------------
add("Camera", transform=xform((0, 0, 15)),
    camera={"projectionType": 1, "orthoSize": 4.5, "nearPlane": 0.05,
            "farPlane": 100.0, "priority": 10, "isActive": True,
            "backgroundColor": list(C_BG)})

# ---- room -----------------------------------------------------------------
# Three flat bands and two pools of light. The pools are the only reason the
# busts do not read as cut-outs pasted on a colour: something in the room has to
# be behind them.
LX, RX = -3.95, 3.95
add("Wall",  transform=xform((0, 0.0, -2.0), (18.0, 10.0, 1)),
    mesh=quad(), material=flat(C_WALL))
add("Floor", transform=xform((0, -3.1, -1.9), (18.0, 2.8, 1)),
    mesh=quad(), material=flat(C_FLOOR))
add("Skirting", transform=xform((0, -1.69, -1.85), (18.0, 0.05, 1)),
    mesh=quad(), material=flat(C_RULE))
for side, cx in (("Left", LX), ("Right", RX)):
    add("Pool" + side, transform=xform((cx, 0.30, -1.8), (7.2, 7.2, 1)),
        mesh=disc(40), material=flat(C_POOL))

# ---- the two prospects ----------------------------------------------------
bust("L_", LX, "base_head_ren")
bust("R_", RX, "base_head_mira")

# The dimmer sits IN FRONT of its bust. Whoever is not talking goes dark, which
# does more for readability than any amount of motion.
for side, cx, who in (("Left", LX, "ren"), ("Right", RX, "mira")):
    add("Dim" + side, transform=xform((cx - BUST_W / 2.0, BUST_TOP, 0.55)),
        displayGraphic=graphic(ART + "dim_" + who + ".svg", BUST_H))

# ---- affinity meters, one above each head ---------------------------------
METER_W = 2.6
for side, cx, fill in (("Left", LX, FILLS[0]), ("Right", RX, FILLS[1])):
    add("MeterName" + side, transform=xform((cx - METER_W / 2, 3.42, 0.5)),
        text=text("", 0.20, C_MUT))
    add("MeterVal" + side, transform=xform((cx + METER_W / 2 - 0.30, 3.42, 0.5)),
        text=text("", 0.20, C_MUT))
    add("MeterTrack" + side, transform=xform((cx, 3.22, 0.4), (METER_W, 0.075, 1)),
        mesh=quad(), material=flat(C_TRACK))
    # VNScene rescales this from its left edge, so the authored width is the
    # zero state and the script moves the centre as it grows.
    add("MeterFill" + side, transform=xform((cx - METER_W / 2, 3.22, 0.45), (0.001, 0.075, 1)),
        mesh=quad(), material=flat(fill))
    # Where this character expects to be left. VNScene slides it along the track
    # from the fill's left edge. A meter without one is a number going up.
    add("MeterMark" + side, transform=xform((cx, 3.22, 0.5), (0.035, 0.20, 1)),
        mesh=quad(), material=flat(C_MARK))

# ---- the box --------------------------------------------------------------
BOX_TOP, BOX_BOT = -1.30, -4.35
add("Box", transform=xform((0, (BOX_TOP + BOX_BOT) / 2, 0.7),
                           (15.6, BOX_TOP - BOX_BOT, 1)),
    mesh=quad(), material=flat(C_BOX))
add("BoxRule", transform=xform((0, BOX_TOP - 0.44, 0.75), (15.6, 0.02, 1)),
    mesh=quad(), material=flat(C_TRACK))

add("Nameplate", transform=xform((-7.5, BOX_TOP - 0.10, 0.8)), text=text("", 0.26, C_NAME))
add("BeatCount", transform=xform((6.9, BOX_TOP - 0.10, 0.8)), text=text("", 0.19, C_MUT))
# One wrapped text entity, revealed a character at a time. wrapWidth is in
# authored pixels against worldHeight, so this is the measured equivalent of
# ~14.8 world units at a 0.24 line height.
add("Line", transform=xform((-7.5, BOX_TOP - 0.62, 0.8)),
    text=text("", 0.24, C_TX, wrap=2700.0))

# Each answer gets its consequence directly beneath it, dimmer and indented.
# Three options with three notes is the whole choice visible at once, which is
# the difference between a decision and a guess.
for i in range(3):
    y = BOX_TOP - 1.16 - i * 0.60
    add("Opt%d" % i,  transform=xform((-7.1, y, 0.8)), text=text("", 0.22, C_OPT))
    add("Note%d" % i, transform=xform((-6.65, y - 0.26, 0.8)),
        text=text("", 0.165, C_NOTE))

# One row per character at the end, read off the gap to their mark.
for i in range(2):
    add("Verdict%d" % i, transform=xform((-7.5, BOX_TOP - 1.30 - i * 0.46, 0.85)),
        text=text("", 0.20, C_TX, wrap=2700.0))

add("Prompt", transform=xform((-7.5, BOX_BOT + 0.14, 0.8)), text=text("", 0.18, C_MUT))
# The closing line goes IN the box, not across the busts. It is the last thing
# read, and the box is where every other line has been read all scene.
add("Closing", transform=xform((-7.5, BOX_TOP - 0.62, 0.85)),
    text=text("", 0.26, C_TX, wrap=2700.0))

# ---- the rigs and the driver ---------------------------------------------
def rig(name, speaker, prefix, pos):
    add(name, transform=xform(pos),
        scriptComponent={"scripts": [{
            "class": "PortraitRig", "path": "scripts/enjin_api/PortraitRig.as",
            "enabled": True, "properties": {
                "speakerId":   {"type": 3, "value": speaker},
                "layerPrefix": {"type": 3, "value": prefix},
                "startEmote":  {"type": 3, "value": "neutral"},
                "emoteTable":  {"type": 3, "value": ""},
                "blinkEvery":  {"type": 1, "value": 3.4},
                "blinkHold":   {"type": 1, "value": 0.09},
                "mouthRate":   {"type": 1, "value": 11.0}}}]})


rig("PortraitLeft",  "ren",  "L_", (LX, 0, 0))
rig("PortraitRight", "mira", "R_", (RX, 0, 0))

add("VN", transform=xform((0, 0, 0)),
    scriptComponent={"scripts": [{
        "class": "VNScene", "path": "scripts/enjin_api/VNScene.as",
        "enabled": True, "properties": {
            "conversation":  {"type": 3, "value": CONVERSATION},
            "fallbackLine":  {"type": 3, "value": "No conversation asset loaded. Press Scan Assets in the Data Assets panel."},
            "revealRate":    {"type": 1, "value": 42.0},
            "meterWidth":    {"type": 1, "value": METER_W},
            "startAffinity": {"type": 0, "value": 40},
            "resetOnStart":  {"type": 0, "value": 1},
            "maxAffinity":   {"type": 0, "value": 100},
            "autoBeat":      {"type": 1, "value": AUTO},
            "autoPick":      {"type": 0, "value": 1}}}]})

scene = {"version": REF.get("version", 1),
         "formatVersion": REF.get("formatVersion", 1),
         "entityCount": len(entities),
         "renderSettings": REF["renderSettings"],
         "entities": entities}

out = os.path.join(ROOT, "scene.enjin")
json.dump(scene, open(out, "w"), indent=1)
layers = sum(len(v) for v in VARIANTS.values()) * 2 + 2
print("wrote %s  -  %d entities (%d portrait layers across 2 busts)"
      % (out, len(entities), layers))
