import json, io, math

# The Acoustic Range.
#
# Three rooms of identical size was enough to prove that material changes the
# sound. It is not enough to TEST acoustics, because every room in it was the
# same shape at the same scale, so the only variable ever exercised was
# absorption. A tracer can be badly wrong about volume, about aspect ratio,
# about openings, about height, and about very small and very large rooms, and
# none of that would have shown.
#
# This is a building of spaces chosen to be far apart on every axis that matters:
#
#   volume        28 m^3 (a bathroom) to 12,000 m^3 (a cathedral), 400x
#   proportion    a cube, a 44 m gallery, a 14 m vertical shaft, a flat hall
#   absorption    fabric-lined (dead) to all-stone (enormous)
#   openness      sealed, doorway-coupled, wide-coupled, and open to the sky
#
# Each space is walkable and labelled, and each one has a sound source in it, so
# a person can hear the difference. Each one is ALSO a case in
# TestAcousticRange.cpp with a hand-computed expectation, so a person does not
# have to trust their ears to know whether it is right.

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
    FABRIC:   (0.30, 0.24, 0.34),
    CONCRETE: (0.62, 0.62, 0.60),
    BRICK:    (0.52, 0.30, 0.24),
    STONE:    (0.70, 0.68, 0.63),
    METAL:    (0.55, 0.57, 0.60),
    GLASS:    (0.70, 0.82, 0.85),
    GRASS:    (0.30, 0.45, 0.22),
}


def unit_cube():
    """A unit cube, 24 vertices so each face gets its own normal.

    Authored rather than assumed: a box collider is NOT a mesh. An earlier
    version of the small demo gave every slab a collider and a material and no
    geometry, so the acoustics measured the building perfectly while the screen
    stayed black.
    """
    faces = [
        ((0, 0, 1),  [(-0.5,-0.5, 0.5), ( 0.5,-0.5, 0.5), ( 0.5, 0.5, 0.5), (-0.5, 0.5, 0.5)]),
        ((0, 0,-1),  [( 0.5,-0.5,-0.5), (-0.5,-0.5,-0.5), (-0.5, 0.5,-0.5), ( 0.5, 0.5,-0.5)]),
        ((1, 0, 0),  [( 0.5,-0.5, 0.5), ( 0.5,-0.5,-0.5), ( 0.5, 0.5,-0.5), ( 0.5, 0.5, 0.5)]),
        ((-1, 0, 0), [(-0.5,-0.5,-0.5), (-0.5,-0.5, 0.5), (-0.5, 0.5, 0.5), (-0.5, 0.5,-0.5)]),
        ((0, 1, 0),  [(-0.5, 0.5, 0.5), ( 0.5, 0.5, 0.5), ( 0.5, 0.5,-0.5), (-0.5, 0.5,-0.5)]),
        ((0,-1, 0),  [(-0.5,-0.5,-0.5), ( 0.5,-0.5,-0.5), ( 0.5,-0.5, 0.5), (-0.5,-0.5, 0.5)]),
    ]
    verts, idx = [], []
    uv = [(0, 0), (1, 0), (1, 1), (0, 1)]
    for normal, corners in faces:
        base = len(verts)
        for c, t in zip(corners, uv):
            verts.append({"position": list(c), "normal": list(normal), "uv": list(t)})
        idx += [base + 0, base + 1, base + 2, base + 0, base + 2, base + 3]
    return {"vertexCount": len(verts), "indexCount": len(idx),
            "vertices": verts, "indices": idx}


CUBE = unit_cube()
WALL = 0.5          # slab thickness


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
        mesh=dict(CUBE),
        material={"baseColor": list(COLOUR.get(surface, (0.7, 0.7, 0.7))),
                  "metallic": 0.0, "roughness": 0.85, "surfaceMaterial": surface},
        boxCollider={"size": list(size), "center": [0, 0, 0],
                     "friction": 0.6, "bounciness": 0.0},
        rigidbody={"isStatic": True, "isKinematic": False, "mass": 0,
                   "useGravity": False})


def wall_with_opening(name, axis, fixed, lo, hi, y0, y1, surface,
                      gap_centre, gap_width, gap_height):
    """One wall, built as pillars plus a lintel so a doorway is a real hole.

    A doorway sound can travel through is the same doorway a person walks
    through, and hearing one space from inside another is most of what makes a
    building sound like a building rather than a list of boxes.
    """
    h = y1 - y0
    yc = (y0 + y1) / 2.0
    left_w = (gap_centre - gap_width / 2.0) - lo
    right_w = hi - (gap_centre + gap_width / 2.0)

    def slab(tag, centre, width, yy, hh):
        if width <= 0.01 or hh <= 0.01:
            return
        if axis == "x":   # wall faces along X: spans Z
            box(name + tag, (fixed, yy, centre), (WALL, hh, width), surface)
        else:             # wall faces along Z: spans X
            box(name + tag, (centre, yy, fixed), (width, hh, WALL), surface)

    slab(" A", lo + left_w / 2.0, left_w, yc, h)
    slab(" B", hi - right_w / 2.0, right_w, yc, h)
    # Lintel above the opening.
    lintel_h = y1 - (y0 + gap_height)
    if lintel_h > 0.01:
        slab(" Lintel", gap_centre, gap_width, y1 - lintel_h / 2.0, lintel_h)


def space(label, centre, size, floor, walls, ceiling, openings=(), ceiling_open=False,
          skip=()):
    """One enclosed space.

    `openings` are (side, gap_centre_offset, width, height) with side in
    -X/+X/-Z/+Z. `ceiling_open` leaves the top off, which is the difference
    between a courtyard and a room and is worth a great deal acoustically: a
    space with no ceiling loses most of its energy upward on the first bounce.
    """
    cx, cz = centre
    w, h, d = size
    hw, hd = w / 2.0, d / 2.0
    x0, x1 = cx - hw, cx + hw
    z0, z1 = cz - hd, cz + hd

    # Out to the far face of the walls, for the same reason as the spine's: the
    # threshold inside a doorway needs something under it.
    box(label + " Floor", (cx, -WALL / 2.0, cz),
        (w + 2 * WALL, WALL, d + 2 * WALL), floor)
    if not ceiling_open:
        box(label + " Ceiling", (cx, h + WALL / 2.0, cz),
            (w + 2 * WALL, WALL, d + 2 * WALL), ceiling)

    holes = {o[0]: o for o in openings}
    for side in ("-X", "+X", "-Z", "+Z"):
        # A side listed in `skip` already has a wall: the spine's. Building the
        # room's own wall there too put two slabs in exactly the same plane, and
        # coincident geometry is not a harmless duplicate to a ray tracer -- a
        # ray reflecting off one is nudged into the other and absorbs twice at
        # the same surface.
        if side in skip:
            continue
        o = holes.get(side)
        if side in ("-X", "+X"):
            fixed = (x0 - WALL / 2.0) if side == "-X" else (x1 + WALL / 2.0)
            lo, hi, gc = z0, z1, cz
        else:
            fixed = (z0 - WALL / 2.0) if side == "-Z" else (z1 + WALL / 2.0)
            # Overlap the corners.
            #
            # The Z-walls sit half a slab OUTSIDE z0/z1, while the X-walls span
            # only z0..z1 -- so meeting them flush leaves a full-height slot at
            # each of the four vertical corners. In the Great Hall that was
            # 4 x 0.5 x 9 = 18 m^2 of open corner against a 3.5 m^2 doorway, and
            # it read as the tracer measuring every large room about 20% short.
            # The rooms were genuinely leaking; the instrument was right.
            lo, hi, gc = x0 - WALL, x1 + WALL, cx
        axis = "x" if side in ("-X", "+X") else "z"

        if o is None:
            span = (hi - lo)
            if axis == "x":
                box(label + " Wall" + side, (fixed, h / 2.0, (lo + hi) / 2.0), (WALL, h, span), walls)
            else:
                box(label + " Wall" + side, ((lo + hi) / 2.0, h / 2.0, fixed), (span, h, WALL), walls)
        else:
            _, off, gw, gh = o
            wall_with_opening(label + " Wall" + side, axis, fixed, lo, hi,
                              0.0, h, walls, gc + off, gw, min(gh, h))


def source(name, pos, clip="assets/projector.wav", volume=0.7, maxd=80.0):
    add(name,
        transform=xform(pos, (0.4, 0.4, 0.4)),
        mesh=dict(CUBE),
        material={"baseColor": [0.95, 0.78, 0.30], "metallic": 0.1,
                  "roughness": 0.35, "emissiveColor": [0.7, 0.45, 0.12],
                  "emissiveStrength": 2.0},
        audioSource={"clipPath": clip, "volume": volume, "pitch": 1.0,
                     "playOnAwake": False, "loop": True, "is3D": True,
                     "spatialBlend": 1.0, "minDistance": 2.0, "maxDistance": maxd})


def lamp(name, pos, intensity, rng, colour=(1.0, 0.95, 0.86)):
    add(name,
        transform=xform(pos),
        light={"type": 1, "color": list(colour), "intensity": intensity, "range": rng})


def light_space(label, centre, size, ceiling_open=False):
    """Lamps sized to the room rather than one number everywhere.

    A point light bright enough to read a 20 m cathedral would white out a 2.5 m
    bathroom. Big spaces get MORE lamps, not brighter ones -- brighter is how
    you get a blown-out floor and black corners.
    """
    cx, cz = centre
    w, h, d = size
    if ceiling_open:
        return                       # the sun does this one
    y = h - min(0.8, h * 0.25)
    cols = max(1, int(w // 12) + 1)
    rows = max(1, int(d // 12) + 1)
    # Intensity follows the ceiling height: the lamp is that far from the floor.
    inten = 0.9 + 0.16 * h
    rng = max(8.0, min(h * 3.0, 30.0))
    for i in range(cols):
        for j in range(rows):
            x = cx - w / 2.0 + w * (i + 0.5) / cols
            z = cz - d / 2.0 + d * (j + 0.5) / rows
            lamp("%s Lamp %d%d" % (label, i, j), (x, y, z), inten, rng)


# ---------------------------------------------------------------------------
# The spine.
#
# Every space opens off one corridor, so the whole range is walkable without a
# map and each space is entered the same way. It is carpeted and long on
# purpose: a hard spine would couple all eleven spaces into one enormous shared
# sound field, which is real physics and would make every measurement in here a
# measurement of the building.
# ---------------------------------------------------------------------------

SPINE_Y = 4.0
SPINE_W = 5.0
SPINE_X0, SPINE_X1 = -78.0, 82.0

# The spine's floor and ceiling run WIDER than the spine itself, out to the far
# face of its side walls.
#
# A doorway is a hole through a wall half a metre thick, and that half metre is
# a threshold you stand on. Sized to the corridor's inner width instead, the
# floor stopped at the near face of the wall and the room's floor started at the
# far face, leaving a 1.6 x 0.5 m gap under every doorway -- and another above
# it -- straight out of the building. Twenty-two doorways came to about 35 m^2
# of hole, and it measured as 7 to 34 percent of all rays escaping from inside
# rooms that a plan view says are sealed. Every large space read about 20% short
# because it genuinely was leaking.
box("Spine Floor",   ((SPINE_X0 + SPINE_X1) / 2, -WALL / 2, 0),
    (SPINE_X1 - SPINE_X0, WALL, SPINE_W + 2 * WALL), CARPET)
box("Spine Ceiling", ((SPINE_X0 + SPINE_X1) / 2, SPINE_Y + WALL / 2, 0),
    (SPINE_X1 - SPINE_X0, WALL, SPINE_W + 2 * WALL), FABRIC)
box("Spine End -X",  (SPINE_X0 - WALL / 2, SPINE_Y / 2, 0), (WALL, SPINE_Y, SPINE_W), FABRIC)
box("Spine End +X",  (SPINE_X1 + WALL / 2, SPINE_Y / 2, 0), (WALL, SPINE_Y, SPINE_W), FABRIC)

for i in range(int((SPINE_X1 - SPINE_X0) // 14) + 1):
    x = SPINE_X0 + 7.0 + i * 14.0
    if x < SPINE_X1:
        lamp("Spine Lamp %d" % i, (x, SPINE_Y - 0.7, 0), 1.3, 12.0, (1.0, 0.93, 0.82))

# ---------------------------------------------------------------------------
# The spaces.
#
# Placed either side of the spine. Every one is a case in TestAcousticRange.cpp,
# and the comment on each is the reason it is here rather than a description of
# what it looks like.
# ---------------------------------------------------------------------------

DOOR_W, DOOR_H = 1.6, 2.2
SPINE_DOORS = []          # (side, x, width, height), filled by place()
NORTH, SOUTH = -1.0, 1.0          # which side of the spine a space sits on


def place(label, x, side, size, floor, walls, ceiling,
          ceiling_open=False, wide_gap=None):
    """Put a space beside the spine and cut a doorway into the spine wall."""
    w, h, d = size
    cz = side * (SPINE_W / 2.0 + WALL + d / 2.0)
    opening_side = "+Z" if side < 0 else "-Z"
    gw = wide_gap if wide_gap else DOOR_W
    gh = min(DOOR_H if not wide_gap else h, h)
    # The room keeps its OWN wall on the corridor side, even though the spine
    # has one in the same plane.
    #
    # Dropping it looked like the right cleanup -- two coincident slabs is not
    # something a ray tracer should be asked to resolve -- and it took escape
    # from 2% to 98% in the tall spaces, because the spine's wall is only four
    # metres high and the Great Hall is nine and the Cathedral is twenty-two.
    # Removing the room's wall opened both of them to the sky above the corridor
    # roof. The duplication stays until the spaces are moved a slab further out,
    # which is a change to every hardcoded centre in TestAcousticRange.cpp.
    space(label, (x, cz), size, floor, walls, ceiling,
          openings=[(opening_side, 0.0, gw, gh)], ceiling_open=ceiling_open)
    light_space(label, (x, cz), size, ceiling_open)
    # Record the doorway. The spine's side walls are built in one pass at the
    # end, as segments BETWEEN the doors.
    #
    # They used to be built here, one stub per space spanning only that space's
    # width -- which left the corridor with no walls at all between spaces. The
    # building was open to the void along its whole length, so energy poured out
    # of every doorway and never came back, and every measurement came in short.
    # A wall you only build where the holes are is not a wall.
    SPINE_DOORS.append((side, x, gw, gh))
    return (x, cz)


# 1. The control. Lined with fabric on every surface, which is as close to an
#    anechoic chamber as an ordinary material table gets. If this one ever
#    measures long, something upstream is broken and no other reading is worth
#    reading.
c1 = place("Anechoic Cell", -70.0, NORTH, (4.0, 3.0, 4.0), FABRIC, FABRIC, FABRIC)

# 2. The smallest hard room. 28 m^3 of tile: a short tail, but a viciously
#    bright one, and the case where a tracer that quietly assumes "small means
#    dead" gets caught.
c2 = place("Tiled Bathroom", -60.0, NORTH, (3.2, 2.5, 3.5), TILE, TILE, TILE)

# 3. The reference. What most rooms a person has stood in actually are, and the
#    number every other reading here should be compared against.
c3 = place("Living Room", -47.0, NORTH, (7.0, 2.7, 6.0), WOOD, DRYWALL, DRYWALL)

# 4. Soft and ordinary. Same scale as the living room, opposite treatment, so
#    the pair isolates absorption with everything else held still.
c4 = place("Carpeted Lounge", -47.0, SOUTH, (7.0, 2.7, 6.0), CARPET, FABRIC, DRYWALL)

# 5. Tall and narrow. A 14 m shaft over a 5 m square floor: the mean free path
#    is dominated by one axis, which is the case a room-size heuristic based on
#    volume alone gets most wrong.
c5 = place("Concrete Stairwell", -33.0, NORTH, (5.0, 14.0, 5.0), CONCRETE, CONCRETE, CONCRETE)

# 6. Long and thin. 44 m of parallel walls 5 m apart -- the flutter-echo case,
#    and the one where reflections arrive in a regular train rather than a wash.
c6 = place("Long Gallery", -12.0, SOUTH, (44.0, 4.0, 5.0), CONCRETE, DRYWALL, DRYWALL)

# 7. Flat and wide. Same trick as the stairwell turned on its side: a big floor
#    under a low ceiling, where almost every early reflection is vertical.
c7 = place("Low Warehouse", -18.0, NORTH, (26.0, 3.2, 18.0), CONCRETE, CONCRETE, METAL)

# 8. Big and hard. The one people mean when they say "reverberant".
c8 = place("Great Hall", 14.0, NORTH, (24.0, 9.0, 16.0), STONE, BRICK, WOOD)

# 9. Biggest. 30 x 22 x 18 of stone is about 12,000 m^3 and should measure in
#    seconds -- the top of the range, and the case that catches a tracer running
#    out of bounces and reporting a short tail with a straight face.
c9 = place("Cathedral", 52.0, NORTH, (30.0, 22.0, 18.0), STONE, STONE, STONE)

# 10. No ceiling. Walls on four sides and open sky above: most of the energy
#     leaves on the first bounce, so it should read as nearly dry despite being
#     enclosed on every side a plan view can show.
c10 = place("Open Courtyard", 20.0, SOUTH, (20.0, 6.0, 16.0), STONE, BRICK, STONE,
            ceiling_open=True)

# 11. Two rooms, one opening, opposite treatments. Coupled-room decay: the
#     tail is not one exponential, it is a hard room feeding a soft one, and a
#     single RT60 cannot describe it honestly. Standing in the doorway is the
#     interesting place.
c11a = place("Coupled Hard", 62.0, SOUTH, (9.0, 4.0, 9.0), TILE, TILE, TILE)
c11b_z = SOUTH * (SPINE_W / 2.0 + WALL + 9.0 + WALL + 4.5)
space("Coupled Soft", (62.0, c11b_z), (9.0, 4.0, 9.0), CARPET, FABRIC, FABRIC,
      openings=[("-Z", 0.0, 3.6, 3.4)])
light_space("Coupled Soft", (62.0, c11b_z), (9.0, 4.0, 9.0))
# The shared opening, cut through the wall the two rooms have in common.
wall_with_opening("Coupled Link", "z", SOUTH * (SPINE_W / 2.0 + WALL + 9.0 + WALL / 2.0),
                  62.0 - 4.5, 62.0 + 4.5, 0.0, 4.0, TILE, 62.0, 3.6, 3.4)

# ---------------------------------------------------------------------------
# The spine's side walls, built last, as the solid stretches between doorways.
# ---------------------------------------------------------------------------

for wall_side in (NORTH, SOUTH):
    doors = sorted([d for d in SPINE_DOORS if d[0] == wall_side], key=lambda d: d[1])
    zf = wall_side * (SPINE_W / 2.0 + WALL / 2.0)
    cursor = SPINE_X0
    for i, (_, dx, dw, dh) in enumerate(doors):
        left = dx - dw / 2.0
        if left - cursor > 0.01:
            box("Spine Wall %s %d" % ("N" if wall_side < 0 else "S", i),
                ((cursor + left) / 2.0, SPINE_Y / 2.0, zf),
                (left - cursor, SPINE_Y, WALL), FABRIC)
        # Lintel over the doorway, so the hole is a doorway and not a slot to
        # the ceiling.
        if SPINE_Y - dh > 0.01:
            box("Spine Lintel %s %d" % ("N" if wall_side < 0 else "S", i),
                (dx, dh + (SPINE_Y - dh) / 2.0, zf), (dw, SPINE_Y - dh, WALL), FABRIC)
        cursor = dx + dw / 2.0
    if SPINE_X1 - cursor > 0.01:
        box("Spine Wall %s end" % ("N" if wall_side < 0 else "S"),
            ((cursor + SPINE_X1) / 2.0, SPINE_Y / 2.0, zf),
            (SPINE_X1 - cursor, SPINE_Y, WALL), FABRIC)

# ---------------------------------------------------------------------------
# What makes the sound.
#
# One per space, a metre from a wall so the direct path and the bounce behind it
# differ by two metres -- a reflection about six milliseconds later, arriving
# from the wall. None of them plays on awake: this range is auditioned with a
# clap and then silence, because a tail is what you hear AFTER a sound stops and
# a continuous drone fills exactly that gap.
# ---------------------------------------------------------------------------

SPACES = [
    ("Anechoic Cell", c1, 3.0), ("Tiled Bathroom", c2, 2.5),
    ("Living Room", c3, 2.7), ("Carpeted Lounge", c4, 2.7),
    ("Concrete Stairwell", c5, 14.0), ("Long Gallery", c6, 4.0),
    ("Low Warehouse", c7, 3.2), ("Great Hall", c8, 9.0),
    ("Cathedral", c9, 22.0), ("Open Courtyard", c10, 6.0),
    ("Coupled Hard", c11a, 4.0),
]
for label, (sx, sz), sh in SPACES:
    source("Source - " + label, (sx, min(1.4, sh * 0.4), sz), volume=0.75)

# ---------------------------------------------------------------------------
# Listener, camera, sun, trigger.
# ---------------------------------------------------------------------------

add("Player",
    transform=xform((-70.0, 0.9, 0.0), (0.7, 1.7, 0.7)),
    mesh=dict(CUBE),
    material={"baseColor": [0.92, 0.92, 0.96], "metallic": 0.0, "roughness": 0.6},
    thirdPerson={"moveSpeed": 7.0, "jumpForce": 6.5,
                 "cameraDistance": 4.0, "cameraHeight": 1.7})

# An explicit listener, on the player.
#
# Not decoration. Everything in AudioReactiveSystem -- room measurement,
# occlusion, reverb zones -- resolves the listener from this component, and when
# a scene has none it used to fall back to the world origin in silence, so the
# room being measured was always the one at (0,0,0). That is fixed in the engine
# (it falls back to the active camera now, and warns when there is nothing at
# all), but a third-person camera sits four metres behind your head, and in a
# range built to be measured the measuring point should be stated rather than
# inferred.
add("Listener",
    transform=xform((-70.0, 1.6, 0.0)),
    audioListener={"isActive": True})

add("MainCam",
    transform=xform((-70.0, 2.2, 8.0), rot=(-0.07, 0, 0, 0.9975)),
    camera={"projectionType": 0, "fieldOfView": 65, "nearPlane": 0.1,
            "farPlane": 900, "isActive": True, "priority": 0,
            "clearColor": True, "clearDepth": True,
            "backgroundColor": [0.05, 0.06, 0.09], "orthoSize": 10})

add("Sun",
    transform=xform((0, 30, 0), rot=(-0.38, 0.22, 0.09, 0.89)),
    light={"type": 0, "color": [1.0, 0.97, 0.92], "intensity": 0.20})

add("Room Clap",
    transform=xform((0.0, 1.0, 0.0)),
    scriptComponent={"scripts": [{"path": "scripts/RoomClap.as", "class": "RoomClap",
                                  "enabled": True, "properties": {}}]})

scene = {"version": "1.0", "entities": entities}
io.open("Examples/RoomAcoustics/scenes/Range.enjin", "w", encoding="utf-8").write(
    json.dumps(scene, indent=1))

project = {
    "name": "Room Acoustics",
    "version": "1.0",
    "scenes": [
        {"path": "scenes/Range.enjin", "buildIndex": 0, "isStartScene": True},
        {"path": "scenes/Main.enjin", "buildIndex": 1, "isStartScene": False},
    ],
}
io.open("Examples/RoomAcoustics/RoomAcoustics.enjinproject", "w", encoding="utf-8").write(
    json.dumps(project, indent=1))

# The volumes, printed, because these are the numbers the tests assert against
# and a silent generator is a generator nobody checks.
print("entities:", len(entities))
print()
print("%-20s %8s %8s %8s" % ("space", "volume", "surface", "4V/S"))
for label, sz in (("Anechoic Cell", (4.0, 3.0, 4.0)),
                  ("Tiled Bathroom", (3.2, 2.5, 3.5)),
                  ("Living Room", (7.0, 2.7, 6.0)),
                  ("Carpeted Lounge", (7.0, 2.7, 6.0)),
                  ("Concrete Stairwell", (5.0, 14.0, 5.0)),
                  ("Long Gallery", (44.0, 4.0, 5.0)),
                  ("Low Warehouse", (26.0, 3.2, 18.0)),
                  ("Great Hall", (24.0, 9.0, 16.0)),
                  ("Cathedral", (30.0, 22.0, 18.0)),
                  ("Open Courtyard", (20.0, 6.0, 16.0)),
                  ("Coupled Hard", (9.0, 4.0, 9.0))):
    w, h, d = sz
    v = w * h * d
    s = 2 * (w * d + w * h + h * d)
    print("%-20s %8.0f %8.0f %8.2f" % (label, v, s, 4 * v / s))
