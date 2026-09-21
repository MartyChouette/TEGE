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


if __name__ == "__main__":
    main()
