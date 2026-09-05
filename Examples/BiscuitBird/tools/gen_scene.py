# Biscuit Bird - scene generator.
# Rebuilds scenes/Main.enjin, the project file and the SFX from scratch.
# Run:  python tools/gen_scene.py     (from the project root)
#
# Everything visual is built from four reused primitive meshes (cube, sphere,
# cylinder/cone, x-pivot cube) scaled by the entity transform, so the scene
# file stays small and every prop is editable in the inspector.

import json, math, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_textures

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.makedirs(os.path.join(ROOT, "scenes"), exist_ok=True)
os.makedirs(os.path.join(ROOT, "assets"), exist_ok=True)

TAU = math.pi * 2.0

# ---------------------------------------------------------------- mesh helpers

def V(p, n, uv):
    return {"position": [round(c, 5) for c in p],
            "normal":   [round(c, 5) for c in n],
            "uv":       [round(c, 5) for c in uv]}

def mesh(verts, idx):
    return {"vertexCount": len(verts), "indexCount": len(idx),
            "vertices": verts, "indices": idx}

def make_cube(xmin=-0.5, xmax=0.5):
    """Unit cube, y/z in [-0.5,0.5]; x range is settable so wings can pivot
    at the shoulder instead of at their middle."""
    a, b = xmin, xmax
    faces = [
        ((0, 0, 1),  [(a, -.5, .5), (b, -.5, .5), (b, .5, .5), (a, .5, .5)]),
        ((0, 0, -1), [(b, -.5, -.5), (a, -.5, -.5), (a, .5, -.5), (b, .5, -.5)]),
        ((1, 0, 0),  [(b, -.5, .5), (b, -.5, -.5), (b, .5, -.5), (b, .5, .5)]),
        ((-1, 0, 0), [(a, -.5, -.5), (a, -.5, .5), (a, .5, .5), (a, .5, -.5)]),
        ((0, 1, 0),  [(a, .5, .5), (b, .5, .5), (b, .5, -.5), (a, .5, -.5)]),
        ((0, -1, 0), [(a, -.5, -.5), (b, -.5, -.5), (b, -.5, .5), (a, -.5, .5)]),
    ]
    verts, idx = [], []
    for n, quad in faces:
        base = len(verts)
        uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
        for p, uv in zip(quad, uvs):
            verts.append(V(p, n, uv))
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return mesh(verts, idx)

def make_sphere(seg=14, rings=9):
    """Unit sphere, radius 0.5 (so transform scale == diameter)."""
    verts, idx = [], []
    for r in range(rings + 1):
        v = r / rings
        phi = v * math.pi
        for s in range(seg + 1):
            u = s / seg
            th = u * TAU
            nx = math.sin(phi) * math.cos(th)
            ny = math.cos(phi)
            nz = math.sin(phi) * math.sin(th)
            verts.append(V((nx * .5, ny * .5, nz * .5), (nx, ny, nz), (u, 1 - v)))
    row = seg + 1
    for r in range(rings):
        for s in range(seg):
            a = r * row + s
            b = a + row
            idx += [a, b, a + 1, a + 1, b, b + 1]
    return mesh(verts, idx)

def make_cylinder(top_scale=1.0, seg=16):
    """Unit cylinder: height 1 centred on origin, bottom radius 0.5.
    top_scale 0 gives a cone, >1 gives a bowl/flare."""
    rb, rt = 0.5, 0.5 * top_scale
    verts, idx = [], []
    # side
    for s in range(seg + 1):
        u = s / seg
        th = u * TAU
        cx, cz = math.cos(th), math.sin(th)
        slope = (rb - rt)
        n = (cx, slope, cz)
        ln = math.sqrt(n[0] ** 2 + n[1] ** 2 + n[2] ** 2) or 1.0
        n = (n[0] / ln, n[1] / ln, n[2] / ln)
        verts.append(V((cx * rb, -0.5, cz * rb), n, (u, 0)))
        verts.append(V((cx * rt, 0.5, cz * rt), n, (u, 1)))
    for s in range(seg):
        a = s * 2
        idx += [a, a + 2, a + 1, a + 1, a + 2, a + 3]
    # caps
    for y, r, nrm in ((0.5, rt, (0, 1, 0)), (-0.5, rb, (0, -1, 0))):
        if r <= 0.0001:
            continue
        c = len(verts)
        verts.append(V((0, y, 0), nrm, (.5, .5)))
        for s in range(seg + 1):
            th = (s / seg) * TAU
            cx, cz = math.cos(th), math.sin(th)
            verts.append(V((cx * r, y, cz * r), nrm, (cx * .5 + .5, cz * .5 + .5)))
        for s in range(seg):
            if nrm[1] > 0:
                idx += [c, c + 1 + s, c + 2 + s]
            else:
                idx += [c, c + 2 + s, c + 1 + s]
    return mesh(verts, idx)

def make_wing(mirror=False):
    """A flat, swept, tapering feather panel spanning x in [0,1] (or [-1,0]
    when mirrored), so a wing entity can pivot at the shoulder. Chord runs
    along z, thickness in y."""
    stations = [(0.00, -0.50, 0.50, 0.075),
                (0.38, -0.42, 0.34, 0.060),
                (0.72, -0.26, 0.16, 0.042),
                (1.00, -0.02, -0.05, 0.022)]
    sgn = -1.0 if mirror else 1.0
    verts, idx = [], []
    top, bot = [], []
    for u, zf, zb, t in stations:
        x = u * sgn
        top.append((len(verts), len(verts) + 1))
        verts.append(V((x, t, zf), (0, 1, 0), (u, 0)))
        verts.append(V((x, t, zb), (0, 1, 0), (u, 1)))
    for u, zf, zb, t in stations:
        x = u * sgn
        bot.append((len(verts), len(verts) + 1))
        verts.append(V((x, -t, zf), (0, -1, 0), (u, 0)))
        verts.append(V((x, -t, zb), (0, -1, 0), (u, 1)))
    for i in range(len(stations) - 1):
        a, b = top[i]
        c, d = top[i + 1]
        idx += [a, c, b, b, c, d]
        a, b = bot[i]
        c, d = bot[i + 1]
        idx += [a, b, c, b, d, c]
    # rims front and back so the wing is not paper-thin from the side
    for i in range(len(stations) - 1):
        tf0, tb0 = top[i]
        tf1, tb1 = top[i + 1]
        bf0, bb0 = bot[i]
        bf1, bb1 = bot[i + 1]
        idx += [tf0, bf0, tf1, tf1, bf0, bf1]
        idx += [tb0, tb1, bb0, bb0, tb1, bb1]
    return mesh(verts, idx)

WING_L = make_wing(False)
WING_R = make_wing(True)

CUBE   = make_cube()
CUBE_PX = make_cube(0.0, 1.0)     # spans +x from its own origin (wing pivot)
CUBE_NX = make_cube(-1.0, 0.0)
SPHERE = make_sphere()
CYL    = make_cylinder()
CONE   = make_cylinder(0.0)
BOWL   = make_cylinder(1.28)
TRUNK  = make_cylinder(0.72)


# ------------------------------------------------------------- entity helpers

def quat(pitch, yaw, roll):
    """Degrees -> [x,y,z,w], matching Quaternion::FromEuler (ZYX intrinsic)."""
    hx, hy, hz = (math.radians(a) * 0.5 for a in (pitch, yaw, roll))
    cx, sx = math.cos(hx), math.sin(hx)
    cy, sy = math.cos(hy), math.sin(hy)
    cz, sz = math.cos(hz), math.sin(hz)
    q = (cz * cy * sx - sz * sy * cx,
         cz * sy * cx + sz * cy * sx,
         sz * cy * cx - cz * sy * sx,
         cz * cy * cx + sz * sy * sx)
    return [round(c, 6) for c in q]

_eid = [0]
E = []

def ent(name, pos, scale=(1, 1, 1), rot=(0, 0, 0), visible=True, parent=None, **extra):
    _eid[0] += 1
    e = {"id": _eid[0], "name": {"name": name},
         "transform": {"position": [round(c, 4) for c in pos],
                       "rotation": quat(*rot) if len(rot) == 3 else list(rot),
                       "scale": [round(c, 4) for c in scale],
                       "visible": visible}}
    if parent is not None:
        e["parent"] = parent
    e.update(extra)
    E.append(e)
    return e

def mat(color, rough=0.85, metal=0.0, **kw):
    m = {"baseColor": [round(c, 4) for c in color], "metallic": metal,
         "roughness": rough}
    m.update(kw)
    return m

def prop(name, geo, pos, scale, color, rough=0.85, rot=(0, 0, 0), parent=None,
         visible=True, **kw):
    return ent(name, pos, scale, rot, visible, parent,
               mesh=geo, material=mat(color, rough, **kw))

# ------------------------------------------------------------------- palette

GRASS   = (0.34, 0.53, 0.26)
GRASS_D = (0.28, 0.45, 0.22)
PATH    = (0.72, 0.68, 0.60)
BARK    = (0.35, 0.25, 0.17)
BARK_D  = (0.28, 0.19, 0.13)
LEAF_A  = (0.24, 0.47, 0.22)
LEAF_B  = (0.31, 0.55, 0.26)
LEAF_C  = (0.20, 0.40, 0.19)
NEST    = (0.46, 0.33, 0.19)
NEST_D  = (0.30, 0.21, 0.12)
BISCUIT = (0.94, 0.87, 0.68)
BIRD_A  = (0.20, 0.25, 0.40)   # slate blue back
BIRD_B  = (0.78, 0.83, 0.92)   # pale belly
BEAK    = (0.95, 0.66, 0.20)
EYE     = (0.06, 0.06, 0.08)
STONE   = (0.55, 0.55, 0.58)
WOOD    = (0.48, 0.33, 0.20)
LAMP    = (0.18, 0.18, 0.20)

SHIRTS = [(0.82, 0.30, 0.28), (0.26, 0.44, 0.72), (0.90, 0.72, 0.26),
          (0.42, 0.66, 0.40), (0.68, 0.36, 0.66), (0.90, 0.52, 0.24),
          (0.30, 0.62, 0.66), (0.78, 0.78, 0.80)]
PANTS  = [(0.24, 0.26, 0.34), (0.34, 0.28, 0.22), (0.20, 0.30, 0.40),
          (0.30, 0.30, 0.32)]
SKIN   = [(0.94, 0.78, 0.62), (0.80, 0.60, 0.44), (0.60, 0.42, 0.30),
          (0.42, 0.29, 0.21), (0.98, 0.85, 0.72)]

# ------------------------------------------------------------ texture paths
# Paint these in the editor and save over the same path - they hot-reload.
T_WAFER   = "assets/tex_wafer.png"
T_CLOTH   = "assets/tex_fabric.png"
T_BARK    = "assets/tex_bark.png"
T_STRAW   = "assets/tex_straw.png"
T_PAVE    = "assets/tex_paving.png"
T_FEATHER = "assets/tex_feather.png"
T_PLUME   = "assets/tex_plumage.png"
T_UVCHART = "assets/tex_uvchart.png"

# ------------------------------------------------- tuning shared with scripts

RING       = 19.0      # half-width of the square walkway loop
PERSON_N   = 10
BISCUIT_N  = 48
NEST_Y     = 21.20
CARRY_MAX  = 4
PERSON_H   = 1.78     # metres, head to toe - the scale everything else answers to
BIRD_S     = 0.45     # the bird is built oversized, then scaled to a ~2.2m wingspan

# ============================================================ world: the park

ent("MainCam", (0, NEST_Y + 3.5, 12), rot=(-8, 0, 0),
    camera={"projectionType": 0, "fieldOfView": 68, "nearPlane": 0.1,
            "farPlane": 500, "isActive": True, "priority": 0,
            "clearColor": True, "clearDepth": True,
            "backgroundColor": [0.55, 0.72, 0.92], "orthoSize": 10})

ent("Sun", (30, 60, 40), rot=(-48, -34, 0),
    light={"type": 0, "color": [1.0, 0.96, 0.86], "intensity": 1.15,
           "castShadows": True, "range": 10, "innerConeAngle": 12.5,
           "outerConeAngle": 17.5, "constantAttenuation": 1.0,
           "linearAttenuation": 0.09, "quadraticAttenuation": 0.032})

# lawn -------------------------------------------------------------------
prop("Lawn", CUBE, (0, -0.5, 0), (340, 1, 340), GRASS, rough=0.95)
# the square walkway the crowd follows ------------------------------------
W = 4.4
prop("PathN", CUBE, (0, 0.03, -RING), (RING * 2 + W, 1, W), PATH, rough=0.9, baseColorTexturePath=T_PAVE)
prop("PathS", CUBE, (0, 0.03,  RING), (RING * 2 + W, 1, W), PATH, rough=0.9, baseColorTexturePath=T_PAVE)
prop("PathW", CUBE, (-RING, 0.03, 0), (W, 1, RING * 2 + W), PATH, rough=0.9, baseColorTexturePath=T_PAVE)
prop("PathE", CUBE, ( RING, 0.03, 0), (W, 1, RING * 2 + W), PATH, rough=0.9, baseColorTexturePath=T_PAVE)
# spokes running out of the plaza, so the loop reads as a real park
prop("SpokeN", CUBE, (0, 0.03, -38), (3.2, 1, 40), PATH, rough=0.9, baseColorTexturePath=T_PAVE)
prop("SpokeE", CUBE, (38, 0.03, 0), (40, 1, 3.2), PATH, rough=0.9, baseColorTexturePath=T_PAVE)
prop("Plaza",  CUBE, (0, 0.02, 0), (16, 1, 16), (0.66, 0.62, 0.55), rough=0.9, baseColorTexturePath=T_PAVE)

# ---------------------------------------------------------------- home tree
# The nest sits on a spire ABOVE the crown, not inside it: the chase camera
# rides ~3.5 units over the bird and 9 behind, and anything at nest height
# would put the opening shot inside a leaf.
prop("TreeTrunk", TRUNK, (0, 10.6, 0), (2.2, 21.2, 2.2), BARK, rough=0.95,
     baseColorTexturePath=T_BARK)
for i in range(6):
    a = (i / 6.0) * TAU
    prop(f"TreeRoot{i}", CUBE, (math.cos(a) * 1.9, 0.35, math.sin(a) * 1.9),
         (1.7, 1.0, 1.0), BARK_D, rough=0.95, baseColorTexturePath=T_BARK,
         rot=(0, -math.degrees(a), 22))
for i, (bx, by, bz, byaw, bpit) in enumerate([
        (2.4, 9.2, 0.4, -12, 58), (-2.2, 11.0, 1.0, 168, 62),
        (0.6, 8.4, -2.4, 82, 55), (-0.8, 12.0, -2.0, 250, 64),
        (2.0, 13.4, -1.4, 24, 66), (-1.6, 14.2, 1.8, 205, 68)]):
    prop(f"TreeBranch{i}", TRUNK, (bx, by, bz), (0.9, 5.4, 0.9), BARK,
         rough=0.95, baseColorTexturePath=T_BARK, rot=(bpit, byaw, 0))
CANOPY = [(0, 13.4, 0, 13.0, LEAF_B), (5.6, 12.2, 1.8, 10.5, LEAF_A),
          (-5.2, 12.8, -2.0, 10.0, LEAF_C), (2.0, 11.4, 5.4, 9.5, LEAF_A),
          (-2.4, 11.8, -5.6, 9.8, LEAF_B), (4.6, 14.6, -3.6, 8.2, LEAF_C),
          (-4.8, 14.2, 3.2, 8.6, LEAF_A), (0.4, 15.0, 0.6, 8.4, LEAF_B),
          (3.4, 9.6, 4.0, 8.0, LEAF_C), (-3.8, 9.2, -3.4, 8.4, LEAF_A)]
for i, (cx, cy, cz, d, col) in enumerate(CANOPY):
    prop(f"Canopy{i}", SPHERE, (cx, cy, cz), (d, d * 0.72, d), col, rough=0.95)

# ------------------------------------------------------------------ the nest
prop("NestBowl", BOWL, (0, NEST_Y, 0), (1.70, 0.72, 1.70), NEST, rough=1.0,
     baseColorTexturePath=T_STRAW)
prop("NestHollow", CYL, (0, NEST_Y + 0.26, 0), (1.20, 0.18, 1.20), NEST_D, rough=1.0)
for i in range(14):
    a = (i / 14.0) * TAU
    r = 0.76 + (i % 3) * 0.07
    prop(f"NestTwig{i}", CUBE, (math.cos(a) * r, NEST_Y + 0.36 + (i % 2) * 0.09,
                                math.sin(a) * r),
         (0.60, 0.07, 0.07), NEST if i % 2 else NEST_D, rough=1.0,
         baseColorTexturePath=T_STRAW,
         rot=(0, -math.degrees(a) + 90 + (i % 5) * 6, (i % 3) * 8 - 8))
# a soft glow so the drop-off point is findable from anywhere in the park
ent("NestGlow", (0, NEST_Y + 1.2, 0),
    light={"type": 1, "color": [1.0, 0.86, 0.52], "intensity": 2.8,
           "castShadows": False, "range": 18, "innerConeAngle": 12.5,
           "outerConeAngle": 17.5, "constantAttenuation": 1.0,
           "linearAttenuation": 0.09, "quadraticAttenuation": 0.032})
prop("NestMarker", SPHERE, (0, NEST_Y + 5.6, 0), (0.85, 0.85, 0.85),
     (1.0, 0.85, 0.45), rough=0.4,
     emissiveColor=[1.0, 0.78, 0.35], emissiveStrength=2.4)

# ------------------------------------------------------- park furniture / set
def bench(idx, x, z, yaw):
    p = ent(f"Bench{idx}", (x, 0, z), rot=(0, yaw, 0))
    pid = p["id"]
    prop(f"BenchSeat{idx}", CUBE, (0, 0.44, 0), (1.9, 0.10, 0.52), WOOD,
         rough=0.9, parent=pid)
    prop(f"BenchBack{idx}", CUBE, (0, 0.74, -0.23), (1.9, 0.46, 0.08), WOOD,
         rough=0.9, parent=pid)
    for s in (-1, 1):
        prop(f"BenchLeg{idx}{'L' if s < 0 else 'R'}", CUBE,
             (0.78 * s, 0.22, 0), (0.10, 0.44, 0.48), LAMP, rough=0.7, parent=pid)

def lamp(idx, x, z):
    p = ent(f"Lamp{idx}", (x, 0, z))
    pid = p["id"]
    prop(f"LampPost{idx}", CYL, (0, 1.9, 0), (0.16, 3.8, 0.16), LAMP,
         rough=0.5, metal=0.4, parent=pid)
    prop(f"LampHead{idx}", SPHERE, (0, 4.0, 0), (0.44, 0.44, 0.44),
         (1.0, 0.94, 0.72), rough=0.3, parent=pid,
         emissiveColor=[1.0, 0.9, 0.6], emissiveStrength=1.2)

for i, (bx, bz, byaw) in enumerate([(-8, -RING - 3.4, 0), (8, RING + 3.4, 180),
                                    (-RING - 3.4, 7, 90), (RING + 3.4, -7, -90)]):
    bench(i, bx, bz, byaw)
for i, (lx, lz) in enumerate([(-RING, -RING), (RING, -RING), (RING, RING),
                              (-RING, RING), (0, -RING - 12), (26, 0)]):
    lamp(i, lx, lz)

# fountain in the north spoke
prop("FountainBase", CYL, (0, 0.32, -30), (6.4, 0.64, 6.4), STONE, rough=0.8)
prop("FountainWater", CYL, (0, 0.60, -30), (5.4, 0.16, 5.4), (0.35, 0.62, 0.78),
     rough=0.15, metal=0.2)
prop("FountainStem", CYL, (0, 1.15, -30), (0.5, 1.4, 0.5), STONE, rough=0.8)
prop("FountainTop", BOWL, (0, 1.9, -30), (1.5, 0.34, 1.5), STONE, rough=0.8)
ent("FountainSpray", (0, 2.1, -30),
    particleEmitter={"emissionRate": 34, "lifetime": 1.4, "startSpeed": 3.6,
                     "startSize": 0.14, "endSize": 0.04, "spread": 0.18,
                     "direction": [0, 1, 0], "startColor": [0.7, 0.85, 1.0],
                     "endColor": [0.6, 0.8, 1.0], "startAlpha": 0.85,
                     "endAlpha": 0.0, "gravityScale": 1.0, "playing": True})

# background woodland + groundcover, using the engine's instanced volumes
for i, (vx, vz, hx, hz, dens) in enumerate([(-46, -34, 16, 13, 9),
                                            (44, 36, 15, 15, 9),
                                            (-42, 42, 13, 11, 8),
                                            (48, -40, 11, 12, 8)]):
    ent(f"Grove{i}", (vx, 0, vz),
        treeVolume={"halfExtents": [hx, 0, hz], "density": dens, "treeType": 0,
                    "trunkHeight": 3.4, "trunkWidth": 0.34, "canopyRadius": 2.1,
                    "canopyOffset": 3.0, "windSwayStrength": 0.45,
                    "generateColliders": False})
    ent(f"GroveShrubs{i}", (vx, 0, vz),
        shrubVolume={"halfExtents": [hx + 2, 0, hz + 2], "density": 26})
for i, (gx, gz) in enumerate([(-30, -8), (30, 10), (-10, 34), (12, -36),
                              (-38, 26), (36, -30), (0, 48), (-50, 0)]):
    ent(f"GrassPatch{i}", (gx, 0, gz),
        grassVolume={"halfExtents": [12, 0, 12], "density": 2600,
                     "bladeHeight": 0.34, "windSwayStrength": 1.1})

# ------------------------------------------------------------------- crowd
def person(i):
    """A 1.78 m pedestrian: hips at 0.76, shoulders at 1.34, eyes at 1.62."""
    shirt = SHIRTS[i % len(SHIRTS)]
    pants = PANTS[i % len(PANTS)]
    skin  = SKIN[i % len(SKIN)]
    h = PERSON_H
    p = ent(f"Person{i}", (0, 0, -100 - i * 3))
    pid = p["id"]
    prop(f"P{i}Hips", CUBE, (0, 0.76 * h / 1.78, 0), (0.34, 0.30, 0.24), pants, parent=pid, baseColorTexturePath=T_CLOTH)
    for s2, tag in ((-1, "L"), (1, "R")):
        prop(f"P{i}Leg{tag}", CUBE, (0.10 * s2, 0.36, 0), (0.15, 0.74, 0.17),
             pants, parent=pid, baseColorTexturePath=T_CLOTH)
        prop(f"P{i}Arm{tag}", CUBE, (0.25 * s2, 1.06, 0), (0.11, 0.58, 0.13),
             shirt, parent=pid, baseColorTexturePath=T_CLOTH)
    prop(f"P{i}Body", CUBE, (0, 1.14, 0), (0.42, 0.62, 0.26), shirt, parent=pid, baseColorTexturePath=T_CLOTH)
    prop(f"P{i}Neck", CUBE, (0, 1.48, 0), (0.13, 0.09, 0.13), skin, parent=pid)
    prop(f"P{i}Head", SPHERE, (0, 1.64, 0), (0.21, 0.26, 0.22), skin, parent=pid)
    if i % 3 == 0:
        prop(f"P{i}Hat", CYL, (0, 1.78, 0), (0.26, 0.10, 0.26),
             SHIRTS[(i + 3) % len(SHIRTS)], parent=pid)
    # the bag of biscuits every pedestrian is carrying
    prop(f"P{i}Bag", CUBE, (0.30, 0.82, 0.04), (0.20, 0.28, 0.14),
         (0.78, 0.70, 0.52), parent=pid)

for i in range(PERSON_N):
    person(i)

# ---------------------------------------------------------------- biscuits
for i in range(BISCUIT_N):
    prop(f"Biscuit{i}", CYL, (0, -8, 0), (0.30, 0.08, 0.30), (1, 1, 1),
         rough=0.75, visible=False, baseColorTexturePath=T_WAFER)

# -------------------------------------------------------------------- the bird
bird = ent("Bird", (0, NEST_Y + 0.80, 0), (BIRD_S, BIRD_S, BIRD_S))
BID = bird["id"]
prop("BirdBody",  SPHERE, (0, 0, 0),        (1.45, 1.28, 2.05),  BIRD_A, rough=0.7, parent=BID, baseColorTexturePath=T_PLUME)
prop("BirdBelly", SPHERE, (0, -0.30, 0.06), (1.26, 0.86, 1.72), BIRD_B, rough=0.7, parent=BID, baseColorTexturePath=T_PLUME)
prop("BirdHead",  SPHERE, (0, 0.58, -0.94), (1.08, 1.08, 1.08), BIRD_A, rough=0.7, parent=BID, baseColorTexturePath=T_PLUME)
prop("BirdBeak",  CONE,   (0, 0.47, -1.47), (0.42, 0.7, 0.42),    BEAK,   rough=0.5,
     rot=(-90, 0, 0), parent=BID)
prop("BirdEyeL",  SPHERE, (0.33, 0.78, -1.27), (0.24, 0.24, 0.24), EYE, rough=0.25, parent=BID)
prop("BirdEyeR",  SPHERE, (-0.33, 0.78, -1.27), (0.24, 0.24, 0.24), EYE, rough=0.25, parent=BID)
prop("BirdTailL", WING_L, (0.02, 0.14, 1.02), (0.62, 1.0, 0.95), BIRD_A, rough=0.7,
     rot=(-10, -68, 0), parent=BID, baseColorTexturePath=T_FEATHER)
prop("BirdTailR", WING_R, (-0.02, 0.14, 1.02), (0.62, 1.0, 0.95), BIRD_A, rough=0.7,
     rot=(-10, 68, 0), parent=BID, baseColorTexturePath=T_FEATHER)
prop("BirdWingL", WING_L, (0.5, 0.16, 0.02), (2.5, 1.0, 1.5), BIRD_A, rough=0.7,
     parent=BID, baseColorTexturePath=T_FEATHER)
prop("BirdWingR", WING_R, (-0.5, 0.16, 0.02), (2.5, 1.0, 1.5), BIRD_A, rough=0.7,
     parent=BID, baseColorTexturePath=T_FEATHER)
for i in range(CARRY_MAX):
    prop(f"Carry{i}", CYL, (0, -0.86 - i * 0.26, -0.34), (0.72, 0.18, 0.72),
         (1, 1, 1), rough=0.75, visible=False, parent=BID,
         baseColorTexturePath=T_WAFER)
prop("BirdShadow", CYL, (0, 0.05, 0), (1.6, 0.02, 1.6), (0.05, 0.07, 0.05),
     rough=1.0, opacity=0.34, alphaMode=1)

# ------------------------------------------------------------------ world sign
ent("HelpSign", (-2.65, 2.6, 13.5),
    text={"text": chr(10).join([
              "BISCUIT BIRD",
              "TAP / SPACE to flap - hold left or right of centre to steer",
              "Dive at the people, then carry the biscuits home to the nest"]),
          "sdfText": True, "lit": False, "worldHeight": 0.34,
          "textColor": [0.10, 0.13, 0.10], "bgColor": [0.95, 0.95, 0.90],
          "bgOpacity": 0.0, "horizontalAlign": 1})

# ------------------------------------------------------------------- the HUD
def ui_el(eid, name, amin, amax, text="", size=34, color=(1, 1, 1),
          align=0, bg=(0, 0, 0), bg_alpha=0.0):
    return {
        "id": eid, "name": name, "type": 2, "parentId": 0, "childIds": [],
        "enabled": True, "visible": True, "focusable": False, "tabOrder": 0,
        "onClickEvent": "", "onSubmitEvent": "", "onValueChangedEvent": "",
        "anchor": {"anchorMin": list(amin), "anchorMax": list(amax),
                   "pivot": [0.5, 0.5], "offsetLeft": 0.0, "offsetRight": 0.0,
                   "offsetTop": 0.0, "offsetBottom": 0.0},
        "style": {"bgColor": list(bg), "bgAlpha": bg_alpha,
                  "borderColor": [0.0, 0.0, 0.0], "borderRadius": 6.0,
                  "borderWidth": 0.0, "focusColor": [-1.0, -1.0, -1.0],
                  "fontSize": size, "textColor": list(color)},
        "data": {"text": text, "textAlignH": align, "textAlignV": 1,
                 "activeTabIndex": 0, "checked": 0, "gridColumns": 2,
                 "imageAlpha": 1.0, "imagePath": "", "imageTint": [1, 1, 1],
                 "inputText": "", "listSelectedIndex": -1, "placeholder": "",
                 "progressFillColor": [-1, -1, -1], "progressValue": 0.0,
                 "selectedOption": 0, "sliderMax": 1.0, "sliderMin": 0.0,
                 "sliderValue": 0.0, "tooltipDelay": 0.5},
    }

HUD_ELEMENTS = [
    ui_el(1, "Score",   (0.020, 0.030), (0.360, 0.098),
          "BISCUITS  0", 54, (1.0, 0.94, 0.72), 0, (0.05, 0.09, 0.06), 0.42),
    ui_el(2, "Carry",   (0.020, 0.104), (0.360, 0.158),
          "CARRYING  0 / 4", 34, (0.86, 0.93, 0.82), 0, (0.05, 0.09, 0.06), 0.34),
    ui_el(3, "Clock",   (0.660, 0.030), (0.980, 0.098),
          "2:00", 54, (1.0, 1.0, 1.0), 2, (0.05, 0.09, 0.06), 0.42),
    ui_el(4, "Banner",  (0.140, 0.330), (0.860, 0.470),
          "BISCUIT BIRD", 92, (1.0, 0.93, 0.66), 1, (0.04, 0.07, 0.05), 0.62),
    ui_el(5, "Sub",     (0.140, 0.478), (0.860, 0.560),
          "TAP or press SPACE to leave the nest", 40, (0.92, 0.96, 0.90), 1,
          (0.04, 0.07, 0.05), 0.5),
    ui_el(6, "Hint",    (0.140, 0.905), (0.860, 0.960),
          "TAP to flap - tap left / right of centre to steer", 32,
          (0.88, 0.92, 0.86), 1, (0.04, 0.07, 0.05), 0.34),
    ui_el(7, "Toast",   (0.240, 0.640), (0.760, 0.712),
          "", 44, (1.0, 0.88, 0.55), 1, (0.04, 0.07, 0.05), 0.0),
]

ent("HUD", (0, 0, 0), uiCanvas={
    "canvasName": "BiscuitHUD", "designWidth": 1920.0, "designHeight": 1080.0,
    "scaleMode": 0, "sortOrder": 10, "visible": True,
    "nextElementId": len(HUD_ELEMENTS) + 1, "elements": HUD_ELEMENTS})

# ------------------------------------------------------------------- scripts
ent("BirdBrain", (0, 0, 0), scriptComponent={"scripts": [
    {"path": "scripts/BirdFlight.as", "class": "BirdFlight", "enabled": True,
     "properties": {}}]})
ent("Director", (0, 0, 0), scriptComponent={"scripts": [
    {"path": "scripts/BiscuitGame.as", "class": "BiscuitGame", "enabled": True,
     "properties": {}}]})

# ============================================================== write it out

SCENE = {
    "version": "1.0",
    "formatVersion": 1,
    "entityCount": len(E),
    "skybox": {"type": 2,
               "topColor": [0.16, 0.38, 0.72], "horizonColor": [0.72, 0.85, 0.96],
               "bottomColor": [0.48, 0.56, 0.52],
               "sunDirection": [-0.42, -0.66, -0.62], "sunIntensity": 1.0,
               "sunSize": 0.035, "sunColor": [1.0, 0.95, 0.80],
               "cloudCoverage": 0.32, "cloudScale": 2.4,
               "cloudColor": [1.0, 1.0, 1.0]},
    "renderSettings": {"useProjectDefaults": False,
                       "ambientColor": [0.40, 0.48, 0.60], "ambientIntensity": 0.40,
                       "shadowsEnabled": True, "shadowResolution": 2048,
                       "shadowDistance": 90.0, "shadowStrength": 0.75,
                       "fxaaEnabled": True, "bloomEnabled": True,
                       "bloomThreshold": 1.05, "bloomIntensity": 0.35,
                       "toneMappingMode": 3, "exposure": 0.92, "saturation": 1.20,
                       "fogDensity": 0.006, "fogColor": [0.72, 0.83, 0.94],
                       "fogStart": 70.0, "fogEnd": 220.0,
                       "backfaceCulling": False},
    "entities": E,
}
with open(os.path.join(ROOT, "scenes", "Main.enjin"), "w") as f:
    json.dump(SCENE, f, separators=(",", ":"))


# ====================================================== scene 2: PaintTest
# A bench for the texture-painting round trip. Nothing moves and nothing is
# scripted - open it, orbit, paint a texture, and watch it come back.
#
# Top row  : one of every primitive Biscuit Bird uses, all wearing the UV chart,
#            so you can read off exactly where a texel lands on each shape.
# Bottom row: the real game assets at inspection size, wearing the real maps.

SCENE_MAIN = E[:]
E = []
_eid[0] = 0

ent("MainCam", (0, 4.6, 19.0), rot=(-9, 0, 0),
    camera={"projectionType": 0, "fieldOfView": 58, "nearPlane": 0.1,
            "farPlane": 300, "isActive": True, "priority": 0,
            "clearColor": True, "clearDepth": True,
            "backgroundColor": [0.18, 0.20, 0.24], "orthoSize": 10})
ent("Sun", (8, 14, 10), rot=(-42, -28, 0),
    light={"type": 0, "color": [1.0, 0.97, 0.92], "intensity": 1.25,
           "castShadows": True, "range": 10, "innerConeAngle": 12.5,
           "outerConeAngle": 17.5, "constantAttenuation": 1.0,
           "linearAttenuation": 0.09, "quadraticAttenuation": 0.032})
ent("Fill", (-10, 6, 8), rot=(-30, 40, 0),
    light={"type": 0, "color": [0.70, 0.78, 0.92], "intensity": 0.55,
           "castShadows": False, "range": 10, "innerConeAngle": 12.5,
           "outerConeAngle": 17.5, "constantAttenuation": 1.0,
           "linearAttenuation": 0.09, "quadraticAttenuation": 0.032})
prop("Floor", CUBE, (0, -0.5, 0), (60, 1, 60), (0.30, 0.32, 0.35), rough=0.95)

# --- the standing canvas: the UV chart at a size you cannot miss -----------
prop("CanvasBoard", CUBE, (0, 3.2, -4.2), (7.6, 4.4, 0.16), (1, 1, 1),
     rough=0.9, baseColorTexturePath=T_UVCHART, doubleSided=True)
prop("CanvasPostL", CUBE, (-3.6, 1.6, -4.2), (0.16, 3.2, 0.16), (0.35, 0.30, 0.26))
prop("CanvasPostR", CUBE, (3.6, 1.6, -4.2), (0.16, 3.2, 0.16), (0.35, 0.30, 0.26))

# --- top row: every primitive wearing the UV chart -------------------------
CHART_ROW = [
    ("Chart_Cube",     CUBE,   (1.3, 1.3, 1.3), (0, 0, 0)),
    ("Chart_Sphere",   SPHERE, (1.5, 1.5, 1.5), (0, 0, 0)),
    ("Chart_Cylinder", CYL,    (1.3, 1.5, 1.3), (0, 0, 0)),
    ("Chart_Cone",     CONE,   (1.4, 1.6, 1.4), (0, 0, 0)),
    ("Chart_Bowl",     BOWL,   (1.5, 1.1, 1.5), (0, 0, 0)),
    ("Chart_Wing",     WING_L, (2.0, 1.0, 1.6), (0, 0, 0)),
]
for i, (nm, geo, sc, rot) in enumerate(CHART_ROW):
    x = -8.0 + i * 3.2
    prop(f"Pedestal{i}", CYL, (x, 0.35, 0.0), (1.7, 0.7, 1.7), (0.22, 0.23, 0.26))
    prop(nm, geo, (x, 1.6, 0.0), sc, (1, 1, 1), rough=0.85, rot=rot,
         baseColorTexturePath=T_UVCHART, doubleSided=True)

# --- bottom row: the actual game assets, at inspection size ---------------
ASSET_ROW = [
    ("Look_Biscuit", CYL,    (2.6, 0.7, 2.6), (0, 0, 0), (1, 1, 1),   T_WAFER),
    ("Look_Shirt",   CUBE,   (1.7, 2.5, 1.1), (0, 0, 0), SHIRTS[0],   T_CLOTH),
    ("Look_Bark",    TRUNK,  (1.5, 2.6, 1.5), (0, 0, 0), BARK,        T_BARK),
    ("Look_Nest",    BOWL,   (2.3, 1.0, 2.3), (0, 0, 0), NEST,        T_STRAW),
    ("Look_Paving",  CUBE,   (2.6, 0.3, 2.6), (0, 0, 0), PATH,        T_PAVE),
    ("Look_Wing",    WING_L, (2.4, 1.0, 1.8), (0, 0, 0), BIRD_A,      T_FEATHER),
    ("Look_Plumage", SPHERE, (1.7, 1.5, 2.2), (0, 0, 0), BIRD_A,      T_PLUME),
]
for i, (nm, geo, sc, rot, col, tex) in enumerate(ASSET_ROW):
    x = -9.6 + i * 3.2
    prop(nm, geo, (x, 1.1, 7.2), sc, col, rough=0.8, rot=rot,
         baseColorTexturePath=tex, doubleSided=True)

ent("PaintHelp", (-3.5, 10.4, -4.1),
    text={"text": chr(10).join([
              "TEXTURE PAINT TEST",
              "Double-click a texture on any object to paint it.",
              "Save writes back over the same file and the viewport reloads it."]),
          "sdfText": True, "lit": False, "worldHeight": 0.42,
          "textColor": [0.92, 0.95, 1.0], "bgColor": [0.06, 0.07, 0.10],
          "bgOpacity": 0.0, "horizontalAlign": 1})

PAINT_SCENE = {
    "version": "1.0", "formatVersion": 1, "entityCount": len(E),
    "skybox": {"type": 2, "topColor": [0.10, 0.12, 0.18],
               "horizonColor": [0.30, 0.33, 0.40],
               "bottomColor": [0.16, 0.17, 0.20],
               "sunDirection": [-0.4, -0.7, -0.6], "sunIntensity": 0.5,
               "sunSize": 0.02, "sunColor": [1, 1, 1],
               "cloudCoverage": 0.0, "cloudScale": 2.0, "cloudColor": [1, 1, 1]},
    "renderSettings": {"useProjectDefaults": False,
                       "ambientColor": [0.55, 0.58, 0.66], "ambientIntensity": 0.55,
                       "shadowsEnabled": True, "shadowResolution": 2048,
                       "shadowDistance": 60.0, "shadowStrength": 0.6,
                       "fxaaEnabled": True, "bloomEnabled": False,
                       "toneMappingMode": 3, "exposure": 0.95, "saturation": 1.0,
                       "fogDensity": 0.0, "backfaceCulling": False},
    "entities": E,
}
with open(os.path.join(ROOT, "scenes", "PaintTest.enjin"), "w") as f:
    json.dump(PAINT_SCENE, f, separators=(",", ":"))
PAINT_COUNT = len(E)
E = SCENE_MAIN


PROJECT = {
    "projectName": "BiscuitBird",
    "version": "1.0",
    "projectMode": 1,
    "physicsBackend": 0,
    "buildConfig": {"windowTitle": "Biscuit Bird", "windowWidth": 1280,
                    "windowHeight": 720, "fullscreen": False},
    "frameSettings": {"targetFrameRate": 0, "vSync": True,
                      "backgroundBehavior": 1},
    "scenes": [{"name": "Main", "path": "scenes/Main.enjin",
                "buildIndex": 0, "isStartScene": True},
               {"name": "PaintTest", "path": "scenes/PaintTest.enjin",
                "buildIndex": -1, "isStartScene": False}],
}
with open(os.path.join(ROOT, "BiscuitBird.enjinproject"), "w") as f:
    json.dump(PROJECT, f, indent=1)

# ------------------------------------------------------------------ the SFX
SR = 22050

def wav(name, samples):
    data = b"".join(struct.pack("<h", max(-32000, min(32000, int(s * 32000))))
                    for s in samples)
    hdr = (b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVEfmt " +
           struct.pack("<IHHIIHH", 16, 1, 1, SR, SR * 2, 2, 16) +
           b"data" + struct.pack("<I", len(data)))
    open(os.path.join(ROOT, "assets", name), "wb").write(hdr + data)

def env(i, n, attack=0.02, release=0.6):
    a = max(1, int(n * attack))
    if i < a:
        return i / a
    return max(0.0, (1.0 - (i - a) / max(1, n - a))) ** (1.0 / max(0.05, release))

_seed = [12345]
def noise():
    _seed[0] = (_seed[0] * 1103515245 + 12345) & 0x7fffffff
    return (_seed[0] / 0x3fffffff) - 1.0

def tone(f, t):
    return math.sin(TAU * f * t)

# flap: a filtered noise whoosh with a low body thump
n = int(SR * 0.20)
buf, lp = [], 0.0
for i in range(n):
    t = i / SR
    lp += (noise() - lp) * 0.16
    s = lp * 0.75 * env(i, n, 0.05, 0.45) + tone(120 - 40 * t / 0.2, t) * 0.25 * env(i, n, 0.01, 0.3)
    buf.append(s * 0.55)
wav("flap.wav", buf)

# pickup: two quick bell notes
n = int(SR * 0.28)
buf = []
for i in range(n):
    t = i / SR
    f = 1320 if t < 0.07 else 1760
    e = env(i, n, 0.005, 0.25)
    buf.append((tone(f, t) * 0.6 + tone(f * 2, t) * 0.2) * e * 0.5)
wav("pickup.wav", buf)

# deposit: a rising four-note flourish
n = int(SR * 0.62)
buf = []
notes = [523.25, 659.25, 783.99, 1046.5]
for i in range(n):
    t = i / SR
    k = min(3, int(t / 0.145))
    f = notes[k]
    lt = t - k * 0.145
    e = max(0.0, 1.0 - lt / 0.145) ** 1.4
    buf.append((tone(f, t) * 0.55 + tone(f * 2, t) * 0.18) * e * 0.5)
wav("deposit.wav", buf)

# yelp: a startled downward chirp
n = int(SR * 0.26)
buf = []
for i in range(n):
    t = i / SR
    f = 760 - 420 * (t / 0.26)
    e = env(i, n, 0.02, 0.4)
    buf.append((tone(f, t) * 0.5 + tone(f * 1.5, t) * 0.2 + noise() * 0.08) * e * 0.5)
wav("yelp.wav", buf)

for _t in gen_textures.generate_all():
    pass
print(f"BiscuitBird generated: {len(E)} entities -> scenes/Main.enjin")
print(f"                     {PAINT_COUNT} entities -> scenes/PaintTest.enjin")
