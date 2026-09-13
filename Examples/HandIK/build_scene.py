import json, io, os

# A first-person hand, laid on things.
#
# PLACEHOLDER ON PURPOSE. There is no rigged character in this repo, so the hand
# is generated: a 16-bone skeleton and one box per bone, rigidly weighted. It
# looks like a hand drawn by somebody who only owns boxes, and that is fine --
# what is being demonstrated is whether five fingers of different lengths settle
# onto a surface at the right heights, and a box shows that as well as a mesh
# does. Swap in real art later by replacing the skeleton and mesh and keeping
# the bone names.
#
# WHY FIRST PERSON. Because it is the case where the answer cannot be faked. In
# third person a hand near a counter reads as "about right" from two metres
# away; held up in front of your face, a fingertip floating a centimetre off the
# worktop is obvious, and so is one that has sunk through it.
#
# WHAT TO LOOK FOR:
#   - the counter: fingers stop ON it, and the short ones stop higher
#   - the shelf lip: fingers curl OVER the edge rather than pressing onto it
#   - walking away: the hand releases instead of dragging along
#   - the gap between counter and shelf: fingers hang, they do not snap to a
#     surface that is not there

entities = []
next_id = [1]

# SurfaceMaterial ordinals (see ECS/Components/SurfaceMaterial.h).
WOOD, STONE, CONCRETE, DRYWALL, TILE = 2, 3, 10, 12, 13

COLOUR = {
    WOOD:     (0.55, 0.38, 0.22),
    STONE:    (0.70, 0.68, 0.63),
    CONCRETE: (0.62, 0.62, 0.60),
    DRYWALL:  (0.83, 0.80, 0.75),
    TILE:     (0.88, 0.90, 0.91),
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


def unit_cube():
    """24 vertices so every face gets its own normal."""
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


def box(name, pos, size, surface, collide=True):
    """A slab. The collider is what the hand casts against; the mesh is what you see."""
    kw = dict(
        transform=xform(pos, size),
        mesh=dict(CUBE),
        material={"baseColor": list(COLOUR.get(surface, (0.7, 0.7, 0.7))),
                  "metallic": 0.0, "roughness": 0.85, "surfaceMaterial": surface},
    )
    if collide:
        kw["boxCollider"] = {"size": list(size), "center": [0, 0, 0],
                             "friction": 0.6, "bounciness": 0.0}
        kw["rigidbody"] = {"isStatic": True, "isKinematic": False, "mass": 0,
                           "useGravity": False}
    add(name, **kw)


# ---------------------------------------------------------------------------
# THE HAND RIG
#
# Canonical local space, and the whole rig depends on agreeing about it:
#   +Z  the way the fingers point (away from you)
#   -Y  the way the palm faces     (down, at the counter)
#   +X  toward the thumb           (this is a RIGHT hand)
#
# Getting the palm axis wrong does not throw. It casts the fingertip rays out
# through the back of the hand and every finger quietly reports no contact,
# which is why HandIKComponent asks for it explicitly rather than guessing.
# ---------------------------------------------------------------------------

FINGERS = [
    # name,     knuckle x,  knuckle z, three bone lengths
    ("Thumb",   0.040,  0.010, (0.032, 0.026, 0.022)),
    ("Index",   0.021,  0.052, (0.040, 0.026, 0.020)),
    ("Middle",  0.000,  0.055, (0.044, 0.029, 0.022)),
    ("Ring",   -0.021,  0.051, (0.040, 0.027, 0.020)),
    ("Little", -0.040,  0.044, (0.032, 0.021, 0.017)),
]

WRIST = "Hand_Wrist"


def build_hand():
    """Skeleton bones plus a skinned box mesh, in bind pose.

    Bone bindPosition is LOCAL to the parent. The inverse bind matrix is the
    inverse of the bone's world bind transform, which here is a pure
    translation, so it is the identity with the negated world position in the
    translation column (m[12..14], the GL layout this engine uses).
    """
    bones = []
    world = {}          # bone name -> world bind position

    def bone(name, parent_name, local):
        parent_index = -1 if parent_name is None else next(
            i for i, b in enumerate(bones) if b["name"] == parent_name)
        pw = (0.0, 0.0, 0.0) if parent_name is None else world[parent_name]
        wpos = (pw[0] + local[0], pw[1] + local[1], pw[2] + local[2])
        world[name] = wpos
        inv = [1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,
               -wpos[0], -wpos[1], -wpos[2], 1]
        bones.append({
            "name": name,
            "parentIndex": parent_index,
            "bindPosition": list(local),
            "bindRotation": [0, 0, 0, 1],
            "bindScale": [1, 1, 1],
            "inverseBindMatrix": inv,
        })
        return name

    bone(WRIST, None, (0.0, 0.0, 0.0))

    for fname, kx, kz, lengths in FINGERS:
        prev = bone("Hand_%s_1" % fname, WRIST, (kx, 0.0, kz))
        prev = bone("Hand_%s_2" % fname, prev, (0.0, 0.0, lengths[0]))
        prev = bone("Hand_%s_3" % fname, prev, (0.0, 0.0, lengths[1]))
        # A leaf bone at the fingertip. Named, so the solver READS the tip
        # instead of extrapolating past the distal joint -- which is right for a
        # straight finger and drifts as it curls.
        bone("Hand_%s_tip" % fname, prev, (0.0, 0.0, lengths[2]))

    index_of = {b["name"]: i for i, b in enumerate(bones)}

    # The mesh: one box per bone, in BIND-POSE WORLD space, rigidly weighted.
    #
    # World space because the skinning matrix is worldTransform *
    # inverseBindMatrix, so a vertex authored at its bind world position lands
    # where the bone takes it. Rigid (one bone, weight 1) because this is a
    # placeholder: smooth weights would hide exactly the joint positions the
    # demo exists to show.
    verts, idx = [], []

    def slab(centre, size, bone_name):
        bi = index_of[bone_name]
        base = len(verts)
        for v in CUBE["vertices"]:
            verts.append({
                "position": [centre[0] + v["position"][0] * size[0],
                             centre[1] + v["position"][1] * size[1],
                             centre[2] + v["position"][2] * size[2]],
                "normal": list(v["normal"]),
                "uv": list(v["uv"]),
                "boneWeights": [1.0, 0.0, 0.0, 0.0],
                "boneIndices": [bi, 0, 0, 0],
            })
        for i in CUBE["indices"]:
            idx.append(base + i)

    # Palm, on the wrist.
    slab((0.0, 0.0, 0.030), (0.090, 0.022, 0.070), WRIST)

    # One segment per finger bone, spanning from that joint to the next.
    for fname, kx, kz, lengths in FINGERS:
        for j, ln in enumerate(lengths):
            here = world["Hand_%s_%d" % (fname, j + 1)]
            centre = (here[0], here[1], here[2] + ln * 0.5)
            thick = 0.016 - 0.002 * j
            slab(centre, (thick, thick, ln * 0.9), "Hand_%s_%d" % (fname, j + 1))

    mesh = {"vertexCount": len(verts), "indexCount": len(idx),
            "vertices": verts, "indices": idx}
    return bones, mesh


BONES, HAND_MESH = build_hand()


# ---------------------------------------------------------------------------
# The room, and the things to put a hand on.
# ---------------------------------------------------------------------------

W, H, D = 14.0, 3.0, 9.0
T = 0.3

box("Floor",     (0, -T / 2, 0),        (W, T, D),  CONCRETE)
box("Ceiling",   (0, H + T / 2, 0),     (W, T, D),  DRYWALL)
box("Wall -Z",   (0, H / 2, -D / 2),    (W, H, T),  DRYWALL)
box("Wall +Z",   (0, H / 2, D / 2),     (W, H, T),  DRYWALL)
box("Wall -X",   (-W / 2, H / 2, 0),    (T, H, D),  DRYWALL)
box("Wall +X",   (W / 2, H / 2, 0),     (T, H, D),  DRYWALL)

# THE COUNTER. 0.95 high, which is a kitchen worktop, and long enough to walk
# the length of while the hand stays on it.
box("Counter Top",   (-3.5, 0.95, -3.2), (7.0, 0.08, 0.65), STONE)
box("Counter Body",  (-3.5, 0.46, -3.3), (7.0, 0.90, 0.55), WOOD)

# THE GAP. Deliberate, and the only part of this scene that tests a NEGATIVE:
# over open air the fingers must hang where the animation left them rather than
# snapping to a surface that is not there.
box("Counter Top 2", (2.6, 0.95, -3.2),  (2.6, 0.08, 0.65), STONE)
box("Counter Body 2",(2.6, 0.46, -3.3),  (2.6, 0.90, 0.55), WOOD)

# THE SHELF LIP. A plank with its edge in free air, which is the case a point
# target cannot express: fingers have to curl OVER it.
box("Shelf Plank",   (3.0, 1.25, 2.6),   (5.0, 0.06, 0.40), WOOD)
box("Shelf Post L",  (0.8, 0.62, 2.7),   (0.10, 1.25, 0.20), WOOD)
box("Shelf Post R",  (5.2, 0.62, 2.7),   (0.10, 1.25, 0.20), WOOD)

# A RAILING at a different height, so the hand has to deal with a surface that
# is not the one it just left.
box("Rail",          (-4.0, 1.05, 2.4),  (5.0, 0.07, 0.07), STONE)
box("Rail Post L",   (-6.3, 0.52, 2.4),  (0.08, 1.05, 0.08), STONE)
box("Rail Post R",   (-1.7, 0.52, 2.4),  (0.08, 1.05, 0.08), STONE)

# ---------------------------------------------------------------------------
# The player, the camera, and the hand hanging off it.
# ---------------------------------------------------------------------------

player = add("Player",
             transform=xform((-6.0, 0.9, -2.0), (0.6, 1.7, 0.6)),
             firstPerson={"moveSpeed": 3.2, "jumpForce": 6.0,
                          "mouseSensitivity": 2.0, "eyeHeight": 1.65},
             rigidbody={"isStatic": False, "isKinematic": False, "mass": 70.0,
                        "useGravity": True},
             capsuleCollider={"radius": 0.3, "height": 1.1, "center": [0, 0, 0]})

cam = add("MainCam",
          transform=xform((0.0, 0.78, 0.0), rot=(0, 0, 0, 1)),
          parent=player["id"],
          camera={"projectionType": 0, "fieldOfView": 70, "nearPlane": 0.05,
                  "farPlane": 200, "isActive": True, "priority": 0,
                  "clearColor": True, "clearDepth": True,
                  "backgroundColor": [0.06, 0.07, 0.09], "orthoSize": 10})

# THE HAND. Parented to the camera, held out and to the right, palm down.
#
# Parenting to the camera rather than the player is what makes it first person:
# the hand goes where you look, and the IK re-solves against whatever ends up
# underneath it.
# Height is the whole experiment, so it is arithmetic rather than taste.
#
#   eye     player 0.90 + camera 0.78            = 1.68
#   hand    eye - 0.61                           = 1.07
#   counter top face, 0.95 centre + 0.08/2 slab  = 0.99
#   gap                                          = 0.08
#
# Finger reaches are 0.070 to 0.095, so the gap sits INSIDE that spread on
# purpose: Index, Middle and Ring make it, Little does not, Thumb is on the
# line. A hand parked where every finger reaches would look identical whether
# the solve were per-finger or a single canned pose, and prove nothing.
add("RightHand",
    transform=xform((0.22, -0.61, 0.45), rot=(0, 0, 0, 1)),
    parent=cam["id"],
    mesh=HAND_MESH,
    material={"baseColor": [0.86, 0.68, 0.60], "metallic": 0.0, "roughness": 0.75},
    skeleton={"name": "PlaceholderHand", "sourceAssetPath": "", "bones": BONES},
    # An animator with no clip. The entity has to have one to be picked up by
    # the renderer's animation pass at all, and IK no longer requires anything
    # to be playing -- a hand resting on a counter is exactly when nothing is.
    animator={"speed": 1.0},
    handIK={
        "handBone": WRIST,
        # Palm faces -Y in the rig's own space. Authored, not inferred: the same
        # hand is -Y in one export and +Z in another.
        "palmNormalLocal": {"x": 0.0, "y": -1.0, "z": 0.0},
        "mode": 1,                  # SurfacePoint
        "engageDistance": 0.45,
        "edgeDirection": {"x": 1.0, "y": 0.0, "z": 0.0},
        "approachRate": 5.0,
        "releaseRate": 8.0,
        "weight": 1.0,
        "fingers": [
            {"proximal": "Hand_%s_1" % f, "intermediate": "Hand_%s_2" % f,
             "distal": "Hand_%s_3" % f, "tip": "Hand_%s_tip" % f,
             # Fingers hinge toward the palm, which is -Y here.
             "curlDirection": {"x": 0.0, "y": -1.0, "z": 0.0}}
            for f, _, _, _ in FINGERS
        ],
    })

add("Sun",
    transform=xform((0, 6, 0), rot=(-0.38, 0.22, 0.09, 0.89)),
    light={"type": 0, "color": [1.0, 0.97, 0.92], "intensity": 0.6})

for i, x in enumerate((-5.0, 0.0, 5.0)):
    add("Lamp %d" % i,
        transform=xform((x, H - 0.5, 0.0)),
        light={"type": 1, "color": [1.0, 0.95, 0.88], "intensity": 2.4, "range": 12.0})

scene = {"version": "1.0", "entities": entities}
out_dir = "Examples/HandIK"
os.makedirs(out_dir + "/scenes", exist_ok=True)
io.open(out_dir + "/scenes/Main.enjin", "w", encoding="utf-8").write(
    json.dumps(scene, indent=1))

project = {
    "name": "Hand IK",
    "version": "1.0",
    "scenes": [{"path": "scenes/Main.enjin", "buildIndex": 0, "isStartScene": True}],
}
io.open(out_dir + "/HandIK.enjinproject", "w", encoding="utf-8").write(
    json.dumps(project, indent=1))

print("entities:", len(entities))
print("bones:", len(BONES), " hand vertices:", HAND_MESH["vertexCount"])
print()
print("finger reaches (metres):")
for fname, kx, kz, lengths in FINGERS:
    print("  %-7s %.3f" % (fname, sum(lengths)))
