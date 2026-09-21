#!/usr/bin/env python3
"""Generate the demo projects that give untested engine features capture coverage.

WHY THIS EXISTS. tools/harness.py can only catch a feature that some project
actually uses, and an audit on 2026-09-21 found a set of features that NO example
project exercised at all:

    surfaceParam band 100   dithered gradient
    surfaceParam band 200   dithered transparency
    surfaceParam band 300   elemental surface
    surfaceParam band 400   procedural surface noise
    ArtStyleComponent       PrePBR / CelToon / Retro / MaterialExpression

That gap is not academic. Two bugs found the same day lived exactly there: the
main render pass was missing the ArtStyle overrides the editor viewport had, and
the surfaceParam cascade had drifted between its three copies. Neither could be
caught by capture, because nothing rendered them.

GENERATED rather than hand-written, for two reasons. Scene JSON carries inline
vertex arrays, so a cube is 24 vertices of hand-typed floats and a row of eight
cubes is unreviewable. And a generated scene can be regenerated when a component
gains a field, which a hand-edited one cannot.

    python tools/make_coverage_demos.py

Writes Examples/<Name>/<Name>.enjinproject and Examples/<Name>/scenes/Main.enjin.
Registering them with the harness is a separate, deliberate step: see
tools/harness_manifest.json.

TRAPS THIS FILE HAS ALREADY HIT, so the next person does not:
  - The scene's top-level "version" MUST be the STRING "1.0". A bare number fails
    the ENTIRE load with `type must be string, but is number`.
  - Unknown entity keys are silently ignored on load and erased by the next save,
    so every key below is one the serializer actually reads.
  - ElementalSurfaceComponent's charAmount/wetness/snowCoverage/frostAmount are
    DERIVED by the elemental system and deliberately not serialized. Authoring
    band 300 means authoring `accumulation`, and letting the system derive.
"""
import json
import math
import os

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# --------------------------------------------------------------------------
# Geometry
# --------------------------------------------------------------------------

def cube(size=1.0):
    """A 24-vertex cube: four vertices per face, so each face gets a flat normal.

    Flat normals matter here. Several of the modes these demos exist to cover are
    shading modes, and a cube with averaged corner normals hides the difference
    between flat and smooth shading -- which is one of the things being tested.
    """
    h = size * 0.5
    faces = [
        ((0, 0, 1),  [(-h, -h, h), (h, -h, h), (h, h, h), (-h, h, h)]),
        ((0, 0, -1), [(h, -h, -h), (-h, -h, -h), (-h, h, -h), (h, h, -h)]),
        ((1, 0, 0),  [(h, -h, h), (h, -h, -h), (h, h, -h), (h, h, h)]),
        ((-1, 0, 0), [(-h, -h, -h), (-h, -h, h), (-h, h, h), (-h, h, -h)]),
        ((0, 1, 0),  [(-h, h, h), (h, h, h), (h, h, -h), (-h, h, -h)]),
        ((0, -1, 0), [(-h, -h, -h), (h, -h, -h), (h, -h, h), (-h, -h, h)]),
    ]
    uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
    verts, idx = [], []
    for n, corners in faces:
        base = len(verts)
        for c, uv in zip(corners, uvs):
            verts.append({"position": list(c), "normal": list(n), "uv": list(uv)})
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return {"vertexCount": len(verts), "indexCount": len(idx),
            "vertices": verts, "indices": idx}


def uv_sphere(radius=1.0, segments=24, rings=12):
    """A UV sphere. `segments`/`rings` are the LOD knob: a coarse sphere has a
    visibly polygonal SILHOUETTE, which is the only part of a LOD swap a capture
    can see. A cube cannot show this -- every level of a cube is a cube."""
    verts, idx = [], []
    for r in range(rings + 1):
        v = r / rings
        phi = v * math.pi
        for sgm in range(segments + 1):
            u = sgm / segments
            theta = u * math.tau
            nx = math.sin(phi) * math.cos(theta)
            ny = math.cos(phi)
            nz = math.sin(phi) * math.sin(theta)
            verts.append({"position": [nx * radius, ny * radius, nz * radius],
                          "normal": [nx, ny, nz], "uv": [u, v]})
    row = segments + 1
    for r in range(rings):
        for sgm in range(segments):
            a = r * row + sgm
            b = a + row
            idx += [a, b, a + 1, a + 1, b, b + 1]
    return {"vertexCount": len(verts), "indexCount": len(idx),
            "vertices": verts, "indices": idx}


def lod_component(levels, base_distance, multiplier=2.0):
    """levels: [(segments, rings), ...] coarsening outwards, level 0 first."""
    out, ratios = [], []
    for i, (sgm, rng) in enumerate(levels):
        mesh = uv_sphere(1.0, sgm, rng)
        ratio = 1.0 if i == 0 else round(len(mesh["vertices"]) /
                                         float(len(uv_sphere(1.0, *levels[0])["vertices"])), 4)
        ratios.append(ratio)
        out.append({"mesh": mesh,
                    "maxDistance": base_distance * (multiplier ** i),
                    "reductionRatio": ratio})
    while len(ratios) < 5:
        ratios.append(0.06)
    return {
        "levelCount": len(out),
        "baseDistance": base_distance,
        "distanceMultiplier": multiplier,
        "enabled": True,
        "autoGenerated": True,
        # Left at 0 ON PURPOSE. It is what MeshSimplifier records, and a scene that
        # does not set it is exactly the legacy case whose metric divided by an unset
        # mesh AABB, went negative, and pinned every such entity to its COARSEST level
        # in every exported game. ResolveLODSourceExtent back-fills it from level 0's
        # vertices now, and this demo is what would notice if that stopped.
        "sourceMaxExtent": 0.0,
        "hysteresisRatio": 0.1,
        "useScreenSize": True,
        "reductionRatios": ratios,
        "levels": out,
    }


# --------------------------------------------------------------------------
# Scene pieces
# --------------------------------------------------------------------------

def transform(pos, scale=(1.0, 1.0, 1.0), rot=(0.0, 0.0, 0.0, 1.0)):
    return {"position": list(pos), "rotation": list(rot), "scale": list(scale)}


def material(**over):
    """A material with the fields the serializer reads. Only non-defaults matter,
    but writing the base explicitly keeps a generated scene readable as a diff."""
    m = {
        "baseColor": [0.72, 0.70, 0.66],
        "metallic": 0.0,
        "roughness": 0.55,
        "opacity": 1.0,
        "alphaMode": 0,
        "alphaCutoff": 0.5,
        "emissiveColor": [0.0, 0.0, 0.0],
        "emissiveStrength": 0.0,
        "castShadows": True,
        "receiveShadows": True,
    }
    m.update(over)
    return m


def pack_rgba(r, g, b, a=255):
    """Scene palette colours are packed 0xRRGGBBAA."""
    return (r << 24) | (g << 16) | (b << 8) | a


def ramp_palette(name, lo, hi, cycle=True):
    """A 16-entry ramp, optionally cycling.

    A palette-indexed material samples a ROW of the palette texture, so a scene
    with NO palettes renders one byte-identical to a plain material -- which is
    how band 500 first appeared in this demo as coverage that covered nothing.
    The control run caught it; this is the fix.
    """
    cols = []
    for i in range(16):
        t = i / 15.0
        cols.append(pack_rgba(int(lo[0] + (hi[0] - lo[0]) * t),
                              int(lo[1] + (hi[1] - lo[1]) * t),
                              int(lo[2] + (hi[2] - lo[2]) * t)))
    pal = {"name": name, "colors": cols}
    if cycle:
        pal["cycles"] = [{"first": 1, "count": 15, "speed": 6.0, "enabled": True}]
    return pal


def sun(entity_id):
    # Angled so faces catch visibly different light: a shading-mode demo lit
    # head-on shows nothing, which is how a demo passes while proving nothing.
    pitch = math.radians(-50.0)
    return {
        "id": entity_id,
        "name": {"name": "Sun"},
        "transform": transform((0.0, 14.0, 9.0),
                               rot=(math.sin(pitch / 2), 0.0, 0.0, math.cos(pitch / 2))),
        "light": {"type": 0, "color": [1.0, 0.97, 0.92], "intensity": 2.6,
                  "range": 100.0, "castShadows": True,
                  "constantAttenuation": 1.0, "linearAttenuation": 0.09,
                  "quadraticAttenuation": 0.032,
                  "innerConeAngle": 12.5, "outerConeAngle": 17.5},
    }


def camera(entity_id, pos, pitch_deg):
    p = math.radians(pitch_deg)
    return {
        "id": entity_id,
        "name": {"name": "Camera"},
        "transform": transform(pos, rot=(math.sin(p / 2), 0.0, 0.0, math.cos(p / 2))),
        "camera": {"projectionType": 0, "fieldOfView": 55.0, "nearPlane": 0.1,
                   "farPlane": 500.0, "isActive": True, "priority": 10,
                   "clearColor": True, "clearDepth": True,
                   "backgroundColor": [0.55, 0.62, 0.70],
                   "cullingMask": 4294967295, "orthoSize": 10.0,
                   "viewportX": 0.0, "viewportY": 0.0,
                   "viewportWidth": 1.0, "viewportHeight": 1.0},
    }


def viewport_camera(entity_id, name, pos, pitch_deg, rect):
    """A camera that owns part of the frame.

    The player switches to the splitscreen path when there are TWO OR MORE active
    cameras and at least one has a viewport rect that is not the full frame
    (Player/src/main.cpp). Both conditions, so a second full-frame camera does not
    do it.

    PITCH ONLY, and deliberately. The first version of this composed a yaw and a
    pitch by hand and aimed the right-hand camera at empty sky, which read as
    "splitscreen is not working" when splitscreen was working perfectly -- the half
    was simply empty. Hand-rolled axis products are a documented trap in this
    engine (the gizmo write-back bug); two cameras offset along X see the same row
    from different places, which is all this demo needs.
    """
    p = math.radians(pitch_deg)
    x, y, w, h = rect
    return {
        "id": entity_id,
        "name": {"name": name},
        "transform": transform(pos, rot=(math.sin(p / 2), 0.0, 0.0, math.cos(p / 2))),
        "camera": {"projectionType": 0, "fieldOfView": 58.0, "nearPlane": 0.1,
                   "farPlane": 500.0, "isActive": True, "priority": 10,
                   "clearColor": True, "clearDepth": True,
                   "backgroundColor": [0.55, 0.62, 0.70],
                   "cullingMask": 4294967295, "orthoSize": 10.0,
                   "viewportX": x, "viewportY": y,
                   "viewportWidth": w, "viewportHeight": h},
    }


def ground(entity_id, half=24.0):
    return {
        "id": entity_id,
        "name": {"name": "Ground"},
        "transform": transform((0.0, -0.5, 0.0), scale=(half, 1.0, half)),
        "mesh": cube(1.0),
        "material": material(baseColor=[0.34, 0.38, 0.33], roughness=0.9),
    }


def plinth(entity_id, name, x, mat, extra=None):
    """One labelled cube on the row. `extra` adds sibling components."""
    e = {
        "id": entity_id,
        "name": {"name": name},
        "transform": transform((x, 0.9, 0.0), scale=(1.4, 1.8, 1.4)),
        "mesh": cube(1.0),
        "material": mat,
    }
    if extra:
        e.update(extra)
    return e


# A plain gradient sky. Without one the frame above the horizon is BLACK, which
# reads as a broken demo rather than a deliberate one -- and a demo nobody trusts
# the look of is a demo nobody opens. type 2 is the gradient sky.
SKYBOX = {
    "type": 2,
    "topColor": [0.24, 0.42, 0.72],
    "horizonColor": [0.62, 0.74, 0.86],
    "bottomColor": [0.52, 0.55, 0.52],
    "solidColor": [0.30, 0.42, 0.58],
    "sunDirection": [0.0, 1.0, 0.0],
    "rotation": 0.0,
    "cubemapPaths": ["", "", "", "", "", ""],
}


def scene(entities, render_over=None):
    rs = {"aaMode": 1, "ambientColor": [0.22, 0.24, 0.28], "ambientIntensity": 0.35,
          "useProjectDefaults": False}
    if render_over:
        rs.update(render_over)
    return {
        # STRING, not a number. A bare 1.0 fails the entire scene load.
        "version": "1.0",
        "formatVersion": 1,
        "entityCount": len(entities),
        "entities": entities,
        "renderSettings": rs,
        "skybox": SKYBOX,
    }


def write_project(name, entities, render_over=None):
    root = os.path.join(REPO, "Examples", name)
    os.makedirs(os.path.join(root, "scenes"), exist_ok=True)
    with open(os.path.join(root, "%s.enjinproject" % name), "w", encoding="utf-8") as f:
        json.dump({"name": name, "version": "1.0",
                   "scenes": [{"path": "scenes/Main.enjin", "name": "Main",
                               "buildIndex": 0, "isStartScene": True}]}, f, indent=1)
    with open(os.path.join(root, "scenes", "Main.enjin"), "w", encoding="utf-8") as f:
        json.dump(scene(entities, render_over), f, indent=1)
    print("wrote Examples/%s (%d entities)" % (name, len(entities)))


# --------------------------------------------------------------------------
# SurfaceBands -- one cube per surfaceParam band
# --------------------------------------------------------------------------

def surface_bands():
    """Every band the cascade can claim, side by side, in band order.

    The cascade is an ORDER: each mode that claims surfaceParam1 excludes the ones
    after it that test for a free slot. A row in band order means a diff in the
    capture points at which band moved.
    """
    e = [sun(1), camera(2, (0.0, 3.4, 11.5), -12.0), ground(3)]
    xs = [-7.5, -4.5, -1.5, 1.5, 4.5, 7.5]

    # 100 -- dithered gradient. Forces flat shading; banded across the lit falloff.
    e.append(plinth(10, "Band100_DitherGradient", xs[0],
                    material(baseColor=[0.85, 0.45, 0.30], ditherGradient=True,
                             ditherGradientBands=4, ditherGradientPattern=1)))
    # 200 -- dithered transparency. Stippled see-through, no blending required.
    e.append(plinth(11, "Band200_DitherTransparency", xs[1],
                    material(baseColor=[0.35, 0.62, 0.85], ditherTransparency=True,
                             ditherTransPattern=0, ditherTransOpacity=0.45,
                             ditherTransBlendColor=[0.70, 0.85, 1.0])))
    # 300 -- elemental. charAmount and friends are DERIVED from accumulation by the
    # elemental system, so accumulation is what a scene can author.
    e.append(plinth(12, "Band300_Elemental", xs[2],
                    material(baseColor=[0.70, 0.66, 0.58]),
                    {"elementalSurface": {"accumulation": [0.85, 0.0, 0.0, 0.0],
                                          "flammability": 1.0,
                                          "accumulationRate": 1.0, "decayRate": 0.0,
                                          "maxAccumulation": 1.0,
                                          "snowDeformation": 0.0}}))
    # 400 -- procedural surface noise.
    e.append(plinth(13, "Band400_SurfaceNoise", xs[3],
                    material(baseColor=[0.55, 0.72, 0.45],
                             surfaceNoiseScale=8.0, surfaceNoiseStrength=0.6)))
    # 500 -- palette-indexed. Covered elsewhere, kept here so the row is the whole
    # cascade: a demo of five of six bands cannot show one band stealing another's.
    e.append(plinth(14, "Band500_Palette", xs[4],
                    material(baseColor=[0.80, 0.75, 0.40],
                             paletteIndexed=True, paletteSlot=0)))
    # 600 -- lightmapped is NOT here, deliberately. Without a baked lightmap it
    # renders byte-identical to a plain material, so a cube marked `lightmapped`
    # would be coverage that covers nothing -- exactly what the control run caught
    # band 500 doing. Band 600 is exercised by the Lightmap project, which has a
    # bake; that lightmapped must WIN the slot over the palette is pinned by
    # TestMaterialDrawState instead, where it costs nothing to assert.
    return e


# --------------------------------------------------------------------------
# ArtStyles -- one cube per per-entity ArtStyleComponent override
# --------------------------------------------------------------------------

def art_styles():
    """The four ArtStyleComponent styles that do something PER ENTITY.

    NPR, PixelArt and Analog are full-screen styles applied from the camera, so a
    per-entity cube would prove nothing about them and they are left out rather
    than included to make the row look complete.
    """
    e = [sun(1), camera(2, (0.0, 3.0, 9.5), -12.0), ground(3)]
    xs = [-4.5, -1.5, 1.5, 4.5]

    # PrePBR: flat + gouraud flags.
    e.append(plinth(10, "ArtStyle_PrePBR", xs[0],
                    material(baseColor=[0.80, 0.55, 0.35]),
                    {"artStyle": {"style": 1, "prePBR_flatShading": True,
                                  "prePBR_gouraudOnly": True}}))
    # CelToon: rim strength, which is the override the main pass was missing.
    e.append(plinth(11, "ArtStyle_CelToon", xs[1],
                    material(baseColor=[0.40, 0.65, 0.85]),
                    {"artStyle": {"style": 3, "cel_rimStrength": 2.5,
                                  "cel_diffuseBands": 3}}))
    # Retro: four flags plus a snap resolution that replaces the material's.
    e.append(plinth(12, "ArtStyle_Retro", xs[2],
                    material(baseColor=[0.60, 0.75, 0.45]),
                    {"artStyle": {"style": 5, "retro_flatShading": True,
                                  "retro_affineTexturing": True,
                                  "retro_vertexSnapping": True,
                                  "retro_uvQuantize": True,
                                  "retro_snapResolution": 80}}))
    # MaterialExpression: surface noise, but only into a band nobody claimed.
    e.append(plinth(13, "ArtStyle_MatExpr", xs[3],
                    material(baseColor=[0.78, 0.70, 0.85]),
                    {"artStyle": {"style": 7, "matExpr_surfaceNoiseScale": 7.0,
                                  "matExpr_surfaceNoiseStrength": 0.5}}))
    return e


# --------------------------------------------------------------------------
# Control mode
# --------------------------------------------------------------------------

BAND_KEYS = ("ditherGradient", "ditherGradientBands", "ditherGradientPattern",
             "ditherTransparency", "ditherTransPattern", "ditherTransOpacity",
             "ditherTransBlendColor", "surfaceNoiseScale", "surfaceNoiseStrength",
             "paletteIndexed", "paletteSlot", "lightmapped")


def strip_modes(entities):
    """The same scene with every band mode and art style removed.

    A demo whose cube is indistinguishable from a plain one is not coverage, it is
    the appearance of coverage -- which is the failure this whole exercise exists
    to stop. Capturing the scene twice, once stripped, and diffing per cube is the
    only way to know which bands actually reach the screen. Anything that comes
    back identical is a band that is NOT covered, whatever the scene says.
    """
    import copy
    out = copy.deepcopy(entities)
    for e in out:
        e.pop("artStyle", None)
        e.pop("elementalSurface", None)
        m = e.get("material")
        if isinstance(m, dict):
            for k in BAND_KEYS:
                m.pop(k, None)
    return out


# --------------------------------------------------------------------------
# LODLadder -- one mesh, several distances, a different level at each
# --------------------------------------------------------------------------

def lod_ladder():
    """Identical LOD'd spheres at increasing distance, so ONE frame shows the ladder.

    LODComponent was made real on 2026-09-21 and had no capture at all, which is a
    poor state for a feature whose failure mode is silent: a wrong metric does not
    crash or vanish, it just picks the wrong mesh, and the coarsest mesh of a
    smooth object still looks like the object.

    Screen-size selection divides distance by the object's SIZE, so the spheres are
    all the same world size and simply get further away.

    The first version scaled them UP with distance so each stayed a similar size on
    screen, which felt like better composition and silently defeated the feature:
    constant apparent size is constant metric, every sphere resolved to level 0, and
    a control with LOD DISABLED captured byte-identical. A demo that cannot tell the
    feature on from the feature off is not coverage. Keep them the same size.
    """
    e = [sun(1), ground(3, half=90.0)]
    e.append(camera(2, (0.0, 3.6, 14.0), -5.0))

    levels = [(28, 14), (12, 7), (7, 4), (5, 3)]
    # Spread along X as well as Z. Placed on the view axis alone they stack into one
    # overlapping column and the silhouettes -- the only thing a capture can compare --
    # cannot be seen at all.
    R = 1.6
    # Distances chosen so the metric (dist / 3.2) lands one sphere in each band, and
    # near enough that the coarsest is still big enough on screen to SEE the facets.
    for i, (x, z) in enumerate(((-5.0, 7.0), (-1.8, -1.0), (1.4, -13.0), (4.2, -29.0))):
        dist = 14.0 - z
        e.append({
            "id": 30 + i,
            "name": {"name": "LOD_at_%dm" % int(dist)},
            "transform": transform((x, R, z), scale=(R, R, R)),
            "mesh": uv_sphere(1.0, *levels[0]),
            "material": material(baseColor=[0.85, 0.62, 0.35], roughness=0.45),
            # metric = distance / (scale * sourceMaxExtent) = dist / 3.2, so the four
            # spheres sit at roughly 1.9, 4.4, 9.4 and 18.8. base 3.0 doubling gives
            # thresholds at 3, 6 and 12: one sphere per level, in order.
            "lod": lod_component(levels, base_distance=3.0),
        })
    return e


# --------------------------------------------------------------------------
# Splitscreen -- the band row, seen through two viewports
# --------------------------------------------------------------------------

def splitscreen():
    """Two viewports over the SAME band row, on purpose.

    RenderSplitscreen is the third push-constant builder and it holds its own copy
    of the surfaceParam cascade -- the copy that was missing the ArtStyle overrides.
    Until this project existed, NOTHING in the harness rendered through it at all,
    so that builder could not be migrated to the shared cascade with any proof, and
    a bug in it was invisible. Putting the band cubes in front of it is what makes
    the migration checkable rather than a leap.

    The two cameras look from different angles so the halves are visibly different
    frames. Two halves that matched would pass a draws claim while proving only
    that one view got copied twice.
    """
    e = [x for x in surface_bands() if "camera" not in x]
    # Offset along X and at different heights, so the halves are visibly different
    # frames of the same row. Two halves that matched would pass a draws claim while
    # proving only that one view got copied twice.
    # Ids 20 and 21, NOT 2 and 3. The ground is entity 3, and reusing that id made it
    # vanish from both halves -- which read as "splitscreen loses the ground" when it
    # was two entities claiming one id.
    e.append(viewport_camera(20, "CameraLeft",  (-3.2, 3.0, 9.5), -10.0,
                             (0.0, 0.0, 0.5, 1.0)))
    e.append(viewport_camera(21, "CameraRight", (3.2, 5.5, 13.0), -18.0,
                             (0.5, 0.0, 0.5, 1.0)))
    return e


# --------------------------------------------------------------------------
# Animation LOD -- identical rigs at increasing distance
# --------------------------------------------------------------------------

def _bone_chain(count, seg):
    """A straight chain of `count` bones, each `seg` tall, rooted at the origin."""
    bones = []
    for i in range(count):
        bones.append({
            "name": "B%d" % i,
            "parentIndex": i - 1,
            "bindPosition": [0.0, 0.0 if i == 0 else seg, 0.0],
            "bindRotation": [0, 0, 0, 1],
            "bindScale": [1, 1, 1],
            # Bind pose is a pure translation up the chain, so the inverse is a
            # pure translation down it.
            "inverseBindMatrix": [1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,
                                  -0.0, -seg * i, -0.0, 1],
        })
    return bones


def _skinned_column(count, seg, half=0.22):
    """A stack of boxes, one per bone, each rigidly weighted to its own bone.

    Rigid weights on purpose: a smooth skin blends two bones per vertex and a
    frozen deep bone still moves a little, which is exactly the difference a
    capture would then fail to see. One bone per box makes a gated bone
    UNAMBIGUOUSLY still.
    """
    verts, idx = [], []
    for b in range(count):
        y0, y1 = b * seg, (b + 1) * seg
        corners = [(-half, y0, -half), (half, y0, -half), (half, y1, -half), (-half, y1, -half),
                   (-half, y0,  half), (half, y0,  half), (half, y1,  half), (-half, y1,  half)]
        base = len(verts)
        for c in corners:
            n = (c[0], 0.0, c[2])
            verts.append({"position": list(c), "normal": list(n), "uv": [0.0, 0.0],
                          "boneWeights": [1.0, 0.0, 0.0, 0.0],
                          "boneIndices": [b, 0, 0, 0]})
        for a, bb, cc in ((0,1,2),(0,2,3),(4,6,5),(4,7,6),
                          (0,4,5),(0,5,1),(1,5,6),(1,6,2),
                          (2,6,7),(2,7,3),(3,7,4),(3,4,0)):
            idx += [base + a, base + bb, base + cc]
    return {"vertexCount": len(verts), "indexCount": len(idx),
            "vertices": verts, "indices": idx}


def _sway_clip(count, duration=2.0, keys=24):
    """A clip that swings every bone about Z, deeper bones further.

    The amplitude GROWS with depth so the tip travels furthest, which is what
    makes a dropped update rate visible: a low-Hz band holds a pose, and the
    part of the silhouette that moved most is the part that is now visibly
    somewhere else.
    """
    import math
    tracks = []
    for b in range(count):
        amp = 0.10 + 0.05 * b
        times, rots = [], []
        for k in range(keys + 1):
            t = duration * k / keys
            a = amp * math.sin(2.0 * math.pi * t / duration + b * 0.4)
            times.append(round(t, 5))
            rots.append([0.0, 0.0, round(math.sin(a * 0.5), 6), round(math.cos(a * 0.5), 6)])
        tracks.append({
            # boneName, not boneIndex: AddAnimation re-resolves indices from the
            # skeleton by NAME, so an index written here is discarded and the
            # track silently binds to bone 0.
            "boneName": "B%d" % b,
            "boneIndex": b,
            "positionTimes": [], "positions": [],
            "rotationTimes": times, "rotations": rots,
            "scaleTimes": [], "scales": [],
        })
    return {"duration": duration, "ticksPerSecond": 1.0, "playMode": 0, "tracks": tracks}


def anim_lod():
    """Four identical rigs at increasing distance, each swaying, LOD'd by distance.

    AnimationLODComponent shipped on 2026-09-21 with unit tests and NO capture,
    because nothing in the harness had a rigged model in it. Unit tests pin the
    band arithmetic; they cannot tell you the gate is wired to the animator, and
    a rate gate that is computed and never applied looks exactly like a rate gate
    that is working -- the animation plays, it just plays at full rate.

    The control is the component DISABLED, which is the honest A/B: same rigs,
    same clip, same frame, every bone updated every frame. If the captures match,
    the feature did nothing.

    The rigs are the same world size and simply get further away, for the reason
    written up in lod_ladder: apparent size held constant is a metric held
    constant, and the demo covers nothing.
    """
    BONES, SEG = 6, 0.55
    e = [sun(1), ground(3, half=110.0)]
    e.append(camera(2, (0.0, 3.0, 12.0), -4.0))

    skel = {"name": "SwayChain", "sourceAssetPath": "", "bones": _bone_chain(BONES, SEG)}
    mesh = _skinned_column(BONES, SEG)
    clip = _sway_clip(BONES)

    # Bands in HERTZ, and aggressive on purpose: the point is to be SEEN in a
    # single frame, not to be a sensible shipping default. 0 Hz holds the pose.
    bands = [
        {"beginDistance": 0.0,  "updateHz": 0.0,  "ik": True,  "blendTrees": True,
         "interpolate": True,  "maxBoneDepth": 0},
        {"beginDistance": 12.0, "updateHz": 6.0,  "ik": False, "blendTrees": False,
         "interpolate": True,  "maxBoneDepth": 0},
        {"beginDistance": 26.0, "updateHz": 2.0,  "ik": False, "blendTrees": False,
         "interpolate": False, "maxBoneDepth": 3},
        {"beginDistance": 46.0, "updateHz": 0.5,  "ik": False, "blendTrees": False,
         "interpolate": False, "maxBoneDepth": 1},
    ]

    # Spread across X as well as Z: stacked on the view axis the silhouettes
    # overlap and the only thing a capture can compare is hidden.
    for i, (x, z) in enumerate(((-4.5, 6.0), (-1.6, -6.0), (1.8, -22.0), (5.0, -44.0))):
        e.append({
            "id": 40 + i,
            "name": {"name": "Rig_at_%dm" % int(12.0 - z)},
            "transform": transform((x, 0.0, z)),
            "mesh": mesh,
            "material": material(baseColor=[0.78, 0.44, 0.52], roughness=0.5),
            "skeleton": skel,
            "animator": {"speed": 1.0, "currentAnimation": "Sway",
                         "animations": {"Sway": clip}},
            "animationLOD": {"enabled": True, "bandCount": 4, "cullDistance": 0.0,
                             "bands": bands},
        })
    return e


# --------------------------------------------------------------------------
# Water3D foam -- the one setting the MAIN pass never applied
# --------------------------------------------------------------------------

def water_foam():
    """A Water3D plane with foam on, and nothing else in the scene competing.

    `Water3DSettings::enableFoam` had no capture anywhere, and the reason it
    needed one is the reason it was broken: the foam block existed in
    `RenderToTarget` (the editor viewport) and `RenderSplitscreen`, and NOT in
    `RenderEntity` -- which is editor play mode and every exported game. So foam
    appeared while you authored it and vanished from the build, which reads as an
    art problem rather than a missing branch.

    The control is `enableFoam` OFF with every other water setting identical, so
    a byte-identical pair means the flag reached no shader.

    VertexWave style, not Refractive: the foam block is explicitly skipped for
    Refractive water (it takes the surfaceParams for its refraction split), so a
    demo authored Refractive would prove nothing and look deliberate.

    The plane sits at the ORIGIN with its placement in settings.position, because
    the surface mesh is generated in WORLD space around that and the entity
    transform is applied on top -- putting the offset in both moves the water
    twice as far as intended.
    """
    e = [sun(1)]
    e.append(camera(2, (0.0, 5.5, 13.0), -18.0))

    # A dark floor under the water so the surface reads against something. Foam is
    # a light value on the crests; over a light ground the contrast is smaller and
    # so is the thing the capture has to see.
    e.append({
        "id": 3,
        "name": {"name": "Seabed"},
        "transform": transform((0.0, -1.2, 0.0), scale=(40.0, 0.2, 40.0)),
        "mesh": cube(1.0),
        "material": material(baseColor=[0.10, 0.12, 0.16], roughness=0.95),
    })

    e.append({
        "id": 10,
        "name": {"name": "Water"},
        # Transform stays at the ORIGIN. See the docstring.
        "transform": transform((0.0, 0.0, 0.0)),
        "water3D": {
            "position": [0.0, 0.0, 0.0],
            "width": 26.0,
            "depth": 26.0,
            "tileSize": 0.5,
            "style": 2,              # VertexWave -- foam is skipped for Refractive
            "shallowColor": [0.10, 0.42, 0.55],
            "deepColor": [0.03, 0.12, 0.22],
            "opacity": 0.85,
            "waveSpeed": 1.1,
            "waveHeight": 0.45,      # tall enough that crests actually cross the
            "waveFrequency": 1.6,    # foam threshold below
            "waveDirection": [1.0, 0.35],
            "gerstnerWaves": True,
            "waveSteepness": 0.6,
            "enableFoam": True,
            "foamThreshold": 0.30,   # crest height at which foam starts
            "foamScale": 9.0,
        },
    })
    return e


# --------------------------------------------------------------------------
# Custom shaders -- a persisted graph result, in a BUILD
# --------------------------------------------------------------------------

# The interface a custom shader must match, copied from what the Shader Graph
# emits (Engine/src/Editor/ShaderGraph.cpp). It is not a free-form shader slot:
# the compiled pipeline SHARES the main pipeline layout, so the descriptor sets
# and push constants bound for an ordinary entity stay valid for this one. Get a
# binding number or a push-constant field wrong and it is not a compile error,
# it is garbage read at the wrong offset.
_CS_VERT = """#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(set = 0, binding = 0) uniform ViewProjectionUBO {
    mat4 view;
    mat4 proj;
} vp;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor; float metallic;
    vec3 emissiveColor; float roughness;
    float emissiveStrength, opacity, alphaCutoff;
    int flags;
    float parallaxScale;
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec4 fragColor;
layout(location = 3) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    gl_Position = vp.proj * vp.view * worldPos;
    fragNormal = mat3(pc.model) * inNormal;
    fragUV = inUV;
    fragColor = inColor;
}
"""

# Hard world-space bands plus a facing term. Chosen to be unmistakable rather
# than pretty: a capture has to tell this apart from the ordinary lit material
# the control renders, and a subtle tint would be indistinguishable from a
# lighting difference.
#
# It reads pc.baseColor and lighting.cameraPos so the capture also proves the
# SHARED layout is intact -- a custom pipeline that bound its own descriptors
# would still draw, just with nonsense in those.
_CS_FRAG = """#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec4 fragColor;
layout(location = 3) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform LightingUBO {
    vec3 ambientColor; float ambientIntensity;
    vec3 cameraPos; float _pad0;
} lighting;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec3 baseColor; float metallic;
    vec3 emissiveColor; float roughness;
    float emissiveStrength, opacity, alphaCutoff;
    int flags;
    float parallaxScale;
} pc;

void main() {
    float band = step(0.5, fract(fragWorldPos.y * 2.0));
    vec3 toCam = normalize(lighting.cameraPos - fragWorldPos);
    float facing = max(dot(normalize(fragNormal), toCam), 0.0);
    vec3 a = pc.baseColor;
    vec3 b = vec3(1.0) - pc.baseColor;
    outColor = vec4(mix(a, b, band) * (0.35 + 0.65 * facing), 1.0);
}
"""


def custom_shader():
    """Two identical spheres, one wearing a persisted custom shader.

    CustomShaderComponent had no capture, which matters more than it looks:
    the shader graph is an EDITOR tool, and the thing it produces is meant to
    survive into a build. A persisted shader is recompiled at the next
    FlushPendingChanges in whatever runtime loads the scene -- so the question a
    capture answers is not "does the graph work" but "does its output reach a
    game", which is the question this engine keeps getting wrong.

    The second sphere is the built-in material, unchanged, as an in-frame
    reference: if BOTH spheres end up looking the same, the custom pipeline was
    never bound, and that is visible in one picture without a control run.

    The control (--control) drops the component, so the shaded sphere falls back
    to its ordinary material.
    """
    e = [sun(1), ground(3, half=30.0)]
    e.append(camera(2, (0.0, 3.4, 9.5), -10.0))

    for i, (x, custom) in enumerate(((-2.2, True), (2.2, False))):
        ent = {
            "id": 20 + i,
            "name": {"name": "Custom" if custom else "Reference"},
            "transform": transform((x, 1.5, 0.0), scale=(1.4, 1.4, 1.4)),
            "mesh": uv_sphere(1.0, 32, 16),
            "material": material(baseColor=[0.85, 0.30, 0.20], roughness=0.5),
        }
        if custom:
            ent["customShader"] = {
                "vs": _CS_VERT,
                "fs": _CS_FRAG,
                "label": "CoverageBands",
            }
        e.append(ent)
    return e


# --------------------------------------------------------------------------
# MeshRenderer filters -- three switches that were each inert once
# --------------------------------------------------------------------------

def mesh_renderer_filters():
    """Four spheres. Three of them should be INVISIBLE, each for a different reason.

    `MeshRendererComponent` is authored by no project in the whole corpus --
    measured with tools/feature_coverage.py, which is how this gap was found
    rather than remembered. That matters more than an ordinary coverage hole,
    because all three of the filters it carries were recently INERT:

      enabled          "It did nothing at all: an author could untick Enabled
                        and watch the mesh carry on drawing."
      renderLayerMask  "Both halves of this were inert" -- the mask and the
                        camera's cullingMask were never compared.
      maxDrawDistance  guarded so 0 means infinite, which is the kind of test
                        that silently inverts.

    Each was fixed, and nothing since could have told you if one broke again.

    The demo is self-checking in a single frame: a filter that stops working
    makes a sphere APPEAR. That is a better shape than a subtle shading
    difference, because the failure adds something obvious rather than removing
    something you have to look for.

    The reference sphere is deliberately first and unfiltered. A capture showing
    nothing at all would otherwise be indistinguishable from a scene that failed
    to load -- which the draws claim would report as a broken project rather
    than as the interesting answer.

    Control (--control) makes every filter permissive, so all four draw. If the
    two captures match, none of the filters did anything.
    """
    e = [sun(1), ground(3, half=60.0)]

    # cullingMask = layers 0 and 1 only. Sphere 3 sits on layer 2, so the camera
    # must not render it. Picking a mask rather than leaving it at all-ones is
    # the point: an inert comparison passes trivially when everything is 0xFFFFFFFF.
    cam = camera(2, (0.0, 3.2, 12.0), -8.0)
    cam["camera"]["cullingMask"] = 0b011
    e.append(cam)

    spheres = [
        # (x, label, meshRenderer, permissive meshRenderer for the control)
        (-4.8, "Visible",
         {"enabled": True, "renderLayerMask": 1, "maxDrawDistance": 0.0},
         {"enabled": True, "renderLayerMask": 1, "maxDrawDistance": 0.0}),
        (-1.6, "HiddenByEnabled",
         {"enabled": False, "renderLayerMask": 1, "maxDrawDistance": 0.0},
         {"enabled": True,  "renderLayerMask": 1, "maxDrawDistance": 0.0}),
        (1.6, "HiddenByLayer",
         {"enabled": True, "renderLayerMask": 0b100, "maxDrawDistance": 0.0},
         {"enabled": True, "renderLayerMask": 0b001, "maxDrawDistance": 0.0}),
        # Roughly 12.4 units from the camera, so a 4-unit draw distance excludes
        # it while 0 (infinite) keeps it.
        (4.8, "HiddenByDistance",
         {"enabled": True, "renderLayerMask": 1, "maxDrawDistance": 4.0},
         {"enabled": True, "renderLayerMask": 1, "maxDrawDistance": 0.0}),
    ]

    for i, (x, label, mr, _permissive) in enumerate(spheres):
        e.append({
            "id": 30 + i,
            "name": {"name": label},
            "transform": transform((x, 1.3, 0.0), scale=(1.3, 1.3, 1.3)),
            "mesh": uv_sphere(1.0, 28, 14),
            "material": material(baseColor=[0.90, 0.55, 0.20], roughness=0.45),
            "meshRenderer": mr,
        })
    return e


def mesh_renderer_filters_permissive(entities):
    """The control: same scene, every filter switched to permissive."""
    for ent in entities:
        if "meshRenderer" in ent:
            ent["meshRenderer"] = {"enabled": True, "renderLayerMask": 1,
                                   "maxDrawDistance": 0.0}
    return entities


def main():
    import sys
    control = "--control" in sys.argv
    sb, art = surface_bands(), art_styles()
    palettes = {"scenePalettes": [ramp_palette("Ember", (40, 10, 60), (255, 200, 90))]}
    if control:
        sb, art = strip_modes(sb), strip_modes(art)
        # The palette stays in the CONTROL scene. Stripping it too would make the
        # control differ for two reasons at once, and the whole point of a control
        # is that exactly one thing changed.
        print("CONTROL: band modes and art styles stripped")
    write_project("SurfaceBands", sb, palettes)
    write_project("ArtStyles", art)
    ss = splitscreen()
    if control:
        ss = strip_modes(ss)
    write_project("Splitscreen", ss, palettes)

    ladder = lod_ladder()
    if control:
        # The control for LOD is LOD TURNED OFF, which pins every entity to level 0.
        # Stripping the component instead would change the mesh source as well and
        # the diff would mean two things at once.
        for ent in ladder:
            if "lod" in ent:
                ent["lod"]["enabled"] = False
    write_project("LODLadder", ladder)

    rigs = anim_lod()
    if control:
        # The control is the GATE turned off, not the component removed: removing
        # it would also remove whatever defaults the system applies, and the diff
        # would mean two things at once.
        for ent in rigs:
            if "animationLOD" in ent:
                ent["animationLOD"]["enabled"] = False
    write_project("AnimationLOD", rigs)

    foam = water_foam()
    if control:
        # The control is the FLAG off and every other water setting untouched, so
        # the pair differs for exactly one reason.
        for ent in foam:
            if "water3D" in ent:
                ent["water3D"]["enableFoam"] = False
    write_project("WaterFoam", foam)

    shaded = custom_shader()
    if control:
        # Control = no custom shader at all, so the left sphere renders through
        # the ordinary material path and the pair differs for one reason.
        for ent in shaded:
            ent.pop("customShader", None)
    write_project("CustomShader", shaded)

    filters = mesh_renderer_filters()
    if control:
        filters = mesh_renderer_filters_permissive(filters)
    write_project("MeshRendererFilters", filters)


if __name__ == "__main__":
    main()
