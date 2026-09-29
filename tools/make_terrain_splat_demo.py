#!/usr/bin/env python3
"""Build Examples/TerrainSplat: a terrain with two painted texture layers.

The layers are generated here rather than borrowed, so each is unmistakable in a
capture: layer 0 a green checker, layer 1 a red and white stripe. The left third
of the ground is layer 0, the right third layer 1, and the middle blends from
one to the other -- so a capture shows at once whether both layers draw, whether
they tile (the checker repeats every 2 world units), and whether the weights
blend. A terrain that draws its plain material colour instead is the failure.

    python tools/make_terrain_splat_demo.py
"""
import json
import math
import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Examples', 'TerrainSplat')
GRID = 32
CELL = 1.0


def textures():
    tex = os.path.join(OUT, 'assets', 'textures')
    os.makedirs(tex, exist_ok=True)
    n = 64
    checker = Image.new('RGB', (n, n))
    stripe = Image.new('RGB', (n, n))
    for y in range(n):
        for x in range(n):
            c = ((x // 32) + (y // 32)) % 2
            checker.putpixel((x, y), (40, 170, 60) if c else (15, 70, 25))
            stripe.putpixel((x, y), (220, 40, 40) if (x // 16) % 2 else (240, 240, 240))
    checker.save(os.path.join(tex, 'layer_checker.png'))
    stripe.save(os.path.join(tex, 'layer_stripe.png'))


def terrain():
    heights, splat = [], []
    for z in range(GRID):
        for x in range(GRID):
            u = x / (GRID - 1)
            heights.append(0.6 * math.sin(u * 6.0) * math.cos(z / (GRID - 1) * 5.0))
            b = min(max((u - 1.0 / 3.0) * 3.0, 0.0), 1.0)   # 0 left third, 1 right third
            splat += [1.0 - b, b, 0.0, 0.0]
    return {
        'gridWidth': GRID, 'gridHeight': GRID, 'cellSize': CELL,
        'maxHeight': 10.0, 'minHeight': -10.0,
        'heightmap': heights, 'splatmap': splat,
        'layers': [
            {'texturePath': 'assets/textures/layer_checker.png', 'tileScale': 4.0},
            {'texturePath': 'assets/textures/layer_stripe.png', 'tileScale': 4.0},
            {'texturePath': '', 'tileScale': 1.0},
            {'texturePath': '', 'tileScale': 1.0},
        ],
    }


def scene():
    ident = [0.0, 0.0, 0.0, 1.0]
    def xform(p, r=ident):
        return {'position': p, 'rotation': r, 'scale': [1.0, 1.0, 1.0]}
    pitch = math.radians(-35.0)
    cam_rot = [math.sin(pitch / 2), 0.0, 0.0, math.cos(pitch / 2)]
    sun_rot = [math.sin(math.radians(-50) / 2), 0.0, 0.0, math.cos(math.radians(-50) / 2)]
    return {
        'version': '1.0',
        'formatVersion': 1,
        'entityCount': 3,
        'entities': [
            {'id': 1, 'name': {'name': 'Sun'}, 'transform': xform([0.0, 20.0, 10.0], sun_rot),
             'light': {'type': 0, 'color': [1.0, 1.0, 1.0], 'intensity': 1.2, 'castShadows': True}},
            {'id': 2, 'name': {'name': 'Game Camera'}, 'transform': xform([0.0, 16.0, 22.0], cam_rot),
             'camera': {'isActive': True, 'fieldOfView': 60.0, 'nearPlane': 0.1, 'farPlane': 500.0,
                        'priority': 10, 'clearColor': True, 'backgroundColor': [0.55, 0.7, 0.85]}},
            {'id': 3, 'name': {'name': 'Terrain'}, 'transform': xform([0.0, 0.0, 0.0]),
             'material': {'baseColor': [1.0, 1.0, 1.0], 'metallic': 0.0, 'roughness': 0.9},
             'terrain': terrain()},
        ],
    }


def main():
    os.makedirs(os.path.join(OUT, 'scenes'), exist_ok=True)
    textures()
    with open(os.path.join(OUT, 'scenes', 'Main.enjin'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(scene(), f, indent=1)
    project = {'name': 'TerrainSplat', 'version': '1.0',
               'scenes': [{'path': 'scenes/Main.enjin', 'name': 'Main', 'buildIndex': 0, 'isStartScene': True}]}
    with open(os.path.join(OUT, 'TerrainSplat.enjinproject'), 'w', encoding='utf-8', newline='\n') as f:
        json.dump(project, f, indent=1)
    print('wrote', OUT)


if __name__ == '__main__':
    main()
