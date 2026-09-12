import json, io

# Three rooms, listened to.
#
# The first version of this scene made each room out of ONE material and joined
# them with bare concrete corridors. It measured 8.84 s, 8.87 s and 5.41 s --
# the tiled kitchen and the drywall hall came out identical -- and the test that
# checks the demo's own claim caught it.
#
# Two things were wrong and both are worth knowing:
#
#   A bare tiled box really does ring for about twelve seconds. Tile absorbs 1%,
#   so Sabine says so, and an empty tiled swimming pool hall genuinely sounds
#   like that. But nobody has a kitchen made of six tiled surfaces: a real one
#   has a tiled floor, painted walls and a plasterboard ceiling. Rooms built
#   from one material are not rooms, they are test cases, and they all pin at
#   the top of the measurement window where they stop being tellable apart.
#
#   Open doorways into hard corridors couple the rooms into one space. That is
#   physically right -- connected rooms DO share a sound field -- but it means
#   the thing being measured is the building rather than the room. The corridors
#   are carpeted now, which is both what a corridor usually is and what stops it
#   acting as a reverberant extension of everything it touches.

entities = []
next_id = [1]

# SurfaceMaterial ordinals.
DEFAULT, METAL, WOOD, STONE, GLASS, FLESH, WATER, DIRT, GRASS, ICE = range(10)
CONCRETE, CARPET, DRYWALL, TILE, BRICK, FABRIC = range(10, 16)

COLOUR = {
    TILE:     (0.88, 0.90, 0.91),
    DRYWALL:  (0.83, 0.80, 0.75),
    CARPET:   (0.34, 0.26, 0.24),
    WOOD:     (0.55, 0.38, 0.22),
    FABRIC:   (0.42, 0.34, 0.44),
    CONCRETE: (0.62, 0.62, 0.60),
    BRICK:    (0.52, 0.30, 0.24),
}

def add(name, **kw):
    e = {"id": next_id[0], "name": {"name": name}}
    next_id[0] += 1
    e.update(kw)
    entities.append(e)
    return e

def xform(pos, scale=(1, 1, 1), rot=(0, 0, 0, 1)):
    return {"position": list(pos), "rotation": list(rot),
            "scale": list(scale), "visible": True}

def box(name, pos, size, surface):
    """A slab. The collider is what the acoustics reads; the mesh is what you see."""
    add(name,
        transform=xform(pos, size),
        material={"baseColor": list(COLOUR.get(surface, (0.7, 0.7, 0.7))),
                  "metallic": 0.0, "roughness": 0.85, "surfaceMaterial": surface},
        boxCollider={"size": list(size), "center": [0, 0, 0],
                     "friction": 0.6, "bounciness": 0.0},
        rigidbody={"isStatic": True, "isKinematic": False, "mass": 0,
                   "useGravity": False})

W, H, D = 10.0, 3.4, 8.0
WALL = 0.4
GAP = 1.2            # doorway width -- narrow, so rooms couple weakly

def room(label, cx, floor, walls, ceiling, doors):
    """One room. Per-surface materials, because that is what a room is."""
    hw, hh, hd = W / 2, H / 2, D / 2

    box(label + " Floor",   (cx, -WALL / 2, 0), (W, WALL, D), floor)
    box(label + " Ceiling", (cx, H + WALL / 2, 0), (W, WALL, D), ceiling)
    box(label + " Back",    (cx, hh, -hd - WALL / 2), (W, H, WALL), walls)
    box(label + " Front",   (cx, hh,  hd + WALL / 2), (W, H, WALL), walls)

    for side, sx in (("-X", cx - hw - WALL / 2), ("+X", cx + hw + WALL / 2)):
        if side in doors:
            # A doorway, as two pillars and a lintel. Sound travels through it
            # exactly as a person does, which is half of what the demo is for --
            # hearing one room from inside another.
            pillar = (D - GAP) / 2
            box(label + " Wall" + side + "A", (sx, hh, -(GAP / 2 + pillar / 2)),
                (WALL, H, pillar), walls)
            box(label + " Wall" + side + "B", (sx, hh, (GAP / 2 + pillar / 2)),
                (WALL, H, pillar), walls)
            box(label + " Lintel" + side, (sx, H - 0.5, 0), (WALL, 1.0, GAP), walls)
        else:
            box(label + " Wall" + side, (sx, hh, 0), (WALL, H, D), walls)

# --- the three rooms -------------------------------------------------------
#
# Built the way the real ones are, so the numbers are numbers a room actually
# has rather than numbers a test case has.

# A tiled kitchen: hard floor, hard walls to waist height and above, plaster
# ceiling. Live, bright, the room where you hear yourself.
room("Kitchen", -21.0, floor=TILE, walls=TILE, ceiling=DRYWALL, doors=("+X",))

# A hall: boards underfoot, plaster around. The ordinary case, and the one that
# has to sit between the other two or the demo is only showing extremes.
room("Hall", 0.0, floor=WOOD, walls=DRYWALL, ceiling=DRYWALL, doors=("-X", "+X"))

# A basement: carpet down, soft hangings on the walls, plaster over. Dead.
room("Basement", 21.0, floor=CARPET, walls=FABRIC, ceiling=FABRIC, doors=("-X",))

# --- the corridors ---------------------------------------------------------
#
# Carpeted, which is what a corridor usually is and what stops it acting as a
# reverberant extension of both rooms it joins. Concrete here was most of why
# the first version measured three rooms as one.

for label, cx in (("Corridor West", -10.7), ("Corridor East", 10.7)):
    box(label + " Floor",   (cx, -WALL / 2, 0), (11.0, WALL, 2.0), CARPET)
    box(label + " Ceiling", (cx, H + WALL / 2, 0), (11.0, WALL, 2.0), FABRIC)
    box(label + " Side-Z",  (cx, H / 2, -1.2), (11.0, H, WALL), FABRIC)
    box(label + " Side+Z",  (cx, H / 2,  1.2), (11.0, H, WALL), FABRIC)

# --- what makes the sound --------------------------------------------------
#
# One metre from each room's back wall, which is the acceptance case: the direct
# path and the bounce off the wall behind it differ by two metres, so the
# reflection lands about six milliseconds later, arriving from the wall. That
# delay and that direction are what place the thing against the wall.

def source(name, pos, clip, volume=1.0):
    add(name,
        transform=xform(pos, (0.4, 0.4, 0.4)),
        material={"baseColor": [0.95, 0.78, 0.30], "metallic": 0.1,
                  "roughness": 0.35, "emissiveColor": [0.7, 0.45, 0.12],
                  "emissiveStrength": 2.0},
        audioSource={"clipPath": clip, "volume": volume, "pitch": 1.0,
                     "playOnAwake": True, "loop": True, "is3D": True,
                     "spatialBlend": 1.0, "minDistance": 2.0, "maxDistance": 70.0})

BACK = -D / 2 + 1.0
source("Projector - tiled kitchen",   (-21.0, 1.2, BACK), "assets/projector.wav", 0.9)
source("Projector - wooden hall",     (  0.0, 1.2, BACK), "assets/projector.wav", 0.9)
source("Projector - carpeted basement", (21.0, 1.2, BACK), "assets/projector.wav", 0.9)

# --- listener and lighting -------------------------------------------------

add("Player",
    transform=xform((0.0, 0.9, 2.4), (0.7, 1.7, 0.7)),
    material={"baseColor": [0.92, 0.92, 0.96], "metallic": 0.0, "roughness": 0.6},
    thirdPerson={"moveSpeed": 5.0, "jumpForce": 6.5,
                 "cameraDistance": 4.0, "cameraHeight": 1.7})

add("MainCam",
    transform=xform((0.0, 2.2, 8.0), rot=(-0.07, 0, 0, 0.9975)),
    camera={"projectionType": 0, "fieldOfView": 60, "nearPlane": 0.1,
            "farPlane": 500, "isActive": True, "priority": 0,
            "clearColor": True, "clearDepth": True,
            "backgroundColor": [0.04, 0.04, 0.06], "orthoSize": 10})

add("Sun",
    transform=xform((0, 12, 0), rot=(-0.38, 0.22, 0.09, 0.89)),
    light={"type": 0, "color": [1.0, 0.97, 0.92], "intensity": 1.2})

for label, cx in (("Kitchen Lamp", -21.0), ("Hall Lamp", 0.0), ("Basement Lamp", 21.0)):
    add(label,
        transform=xform((cx, H - 0.5, 0)),
        light={"type": 1, "color": [1.0, 0.95, 0.86], "intensity": 16.0, "range": 15.0})
for label, cx in (("Corridor West Lamp", -10.7), ("Corridor East Lamp", 10.7)):
    add(label,
        transform=xform((cx, H - 0.6, 0)),
        light={"type": 1, "color": [1.0, 0.93, 0.82], "intensity": 8.0, "range": 8.0})

scene = {"version": "1.0", "entities": entities}
io.open("Examples/RoomAcoustics/scenes/Main.enjin", "w", encoding="utf-8").write(
    json.dumps(scene, indent=1))

project = {
    "name": "Room Acoustics",
    "version": "1.0",
    "scenes": [{"path": "scenes/Main.enjin", "buildIndex": 0, "isStartScene": True}],
}
io.open("Examples/RoomAcoustics/RoomAcoustics.enjinproject", "w", encoding="utf-8").write(
    json.dumps(project, indent=1))

print("entities:", len(entities))
