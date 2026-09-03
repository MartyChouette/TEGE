"""Generate the streaming OOM stress-test project.

A GRID x GRID grid of streaming volumes, each pointing at a sub-scene holding a
heavy inline-geometry terrain patch. Distance-unload is effectively disabled
(unloadDistance is enormous), so with no memory budget every chunk the camera
ever visits stays resident forever -- the OOM case. With a budget set, the
LRU budgeter is the only thing bounding memory, and the run stays flat.
"""
import json, math, os, shutil, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'D:/GitHub/enjin/_streamstress'
GRID = 7            # 7x7 = 49 chunks
SPACING = 60.0
PATCH = 50          # 50x50 vertices per chunk patch
LOAD_DIST = 90.0
UNLOAD_DIST = 100000.0   # never unload by distance: the budget is the only bound

shutil.rmtree(OUT, ignore_errors=True)
os.makedirs(os.path.join(OUT, 'scenes', 'chunks'))
os.makedirs(os.path.join(OUT, 'scripts'))


def r3(v): return [round(v[0], 3), round(v[1], 3), round(v[2], 3)]


def make_patch(seed):
    """A PATCH x PATCH heightfield patch centred on the origin."""
    verts, idx = [], []
    half = SPACING * 0.5
    step = SPACING / (PATCH - 1)
    for j in range(PATCH):
        for i in range(PATCH):
            x = -half + i * step
            z = -half + j * step
            y = (math.sin((x + seed * 13.0) * 0.09) * 3.0
                 + math.cos((z + seed * 7.0) * 0.11) * 3.0
                 + math.sin((x + z) * 0.05 + seed) * 1.5)
            verts.append({
                "position": r3((x, y, z)),
                "normal": [0.0, 1.0, 0.0],
                "uv": [round(i / (PATCH - 1), 3), round(j / (PATCH - 1), 3)],
            })
    for j in range(PATCH - 1):
        for i in range(PATCH - 1):
            a = j * PATCH + i
            idx += [a, a + PATCH, a + 1, a + 1, a + PATCH, a + PATCH + 1]
    return verts, idx


chunk_bytes = 0
for cz in range(GRID):
    for cx in range(GRID):
        n = cz * GRID + cx
        verts, idx = make_patch(n)
        hue = n / float(GRID * GRID)
        scene = {"version": "1.0", "entities": [{
            "id": 1,
            "name": {"name": f"Patch_{cx}_{cz}"},
            "transform": {"position": [cx * SPACING, 0, cz * SPACING],
                          "rotation": [0, 0, 0, 1], "scale": [1, 1, 1], "visible": True},
            "mesh": {"vertexCount": len(verts), "indexCount": len(idx),
                     "vertices": verts, "indices": idx},
            "material": {"baseColor": [round(0.25 + 0.6 * hue, 3),
                                       round(0.75 - 0.45 * hue, 3),
                                       round(0.35 + 0.5 * ((n * 7) % 5) / 5.0, 3), 1.0],
                         "metallic": 0.0, "roughness": 0.85},
        }]}
        p = os.path.join(OUT, 'scenes', 'chunks', f'chunk_{cx}_{cz}.enjin')
        with open(p, 'w') as f:
            json.dump(scene, f, separators=(',', ':'))
        chunk_bytes += os.path.getsize(p)

# ---------------- main scene ----------------
entities = [
    {"id": 1, "name": {"name": "Cam"},
     "transform": {"position": [0, 35, -40], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1], "visible": True},
     "camera": {"projectionType": 0, "fieldOfView": 60, "nearPlane": 0.5, "farPlane": 900,
                "isActive": True, "priority": 0, "clearColor": True, "clearDepth": True,
                "backgroundColor": [0.08, 0.10, 0.16], "orthoSize": 10}},
    {"id": 2, "name": {"name": "Sun"},
     "transform": {"position": [50, 90, 50], "rotation": [-0.38, -0.12, 0.05, 0.91], "scale": [1, 1, 1], "visible": True},
     "light": {"type": 0, "color": [1.0, 0.96, 0.88], "intensity": 1.5, "castShadows": False,
               "range": 10, "innerConeAngle": 12.5, "outerConeAngle": 17.5,
               "constantAttenuation": 1, "linearAttenuation": 0.09, "quadraticAttenuation": 0.032}},
    {"id": 3, "name": {"name": "StressRunner"},
     "transform": {"position": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1], "visible": True},
     "scriptComponent": {"enabled": True, "scripts": [{"path": "scripts/StreamStress.as", "class": "StreamStress"}]}},
]
eid = 4
for cz in range(GRID):
    for cx in range(GRID):
        entities.append({
            "id": eid,
            "name": {"name": f"Vol_{cx}_{cz}"},
            "transform": {"position": [cx * SPACING, 0, cz * SPACING],
                          "rotation": [0, 0, 0, 1], "scale": [1, 1, 1], "visible": True},
            "streamingVolume": {
                "chunkId": f"chunk_{cx}_{cz}",
                "scenePath": f"scenes/chunks/chunk_{cx}_{cz}.enjin",
                "halfExtents": [SPACING * 0.5, 30.0, SPACING * 0.5],
                "loadDistance": LOAD_DIST,
                "unloadDistance": UNLOAD_DIST,
                "priority": 2},
        })
        eid += 1

main = {"version": "1.0", "entities": entities,
        "skybox": {"type": 0, "topColor": [0.16, 0.22, 0.40], "bottomColor": [0.55, 0.60, 0.68],
                   "horizonColor": [0.40, 0.45, 0.55], "solidColor": [0.1, 0.1, 0.15], "rotation": 0.0}}
with open(os.path.join(OUT, 'scenes', 'Main.enjin'), 'w') as f:
    json.dump(main, f, separators=(',', ':'))

# ---------------- sweep script ----------------
SPAN = SPACING * (GRID - 1)
script = f'''// Streaming OOM stress harness.
// Walks the camera along a serpentine path across the whole {GRID}x{GRID} chunk grid.
// Chunk unloadDistance is effectively infinite, so with no memory budget every
// chunk visited stays resident (the OOM case); with a budget the LRU evictor is
// the only thing keeping the resident set flat. The page drives the budget via
// setStreamingBudgetMB(), so one session can A/B both.
class StreamStress : TegeBehavior {{
    float t = 0.0f;
    uint64 cam = 0;
    int laps = 0;

    void OnStart() {{
        cam = Scene_FindEntity("Cam");
        Camera_TakeManualControl(cam);
        Streaming_SetMemoryBudgetMB(0);   // start unbounded; the page can cap it live
        Debug_Log("StreamStress: sweeping {GRID}x{GRID} chunks, budget unlimited");
    }}

    void OnUpdate(float dt) {{
        t += dt;
        float speed = 55.0f;
        float span = {SPAN:.1f}f;
        float rowLen = span;
        float legs = float({GRID});
        float lapLen = legs * rowLen + (legs - 1.0f) * {SPACING:.1f}f;
        float d = t * speed;
        int lap = int(d / lapLen);
        if (lap != laps) {{ laps = lap; Debug_Log("StreamStress: lap " + laps); }}
        d = d - float(lap) * lapLen;

        // Serpentine: traverse a row, hop to the next, reverse direction.
        float x = 0.0f;
        float z = 0.0f;
        float seg = rowLen + {SPACING:.1f}f;
        int row = int(d / seg);
        if (row > {GRID - 1}) row = {GRID - 1};
        float within = d - float(row) * seg;
        z = float(row) * {SPACING:.1f}f;
        if (within > rowLen) {{
            // hop between rows
            z += (within - rowLen);
            within = rowLen;
        }}
        x = (row % 2 == 0) ? within : (rowLen - within);

        Entity_SetPosition(cam, Vector3(x, 34.0f, z - 42.0f));
        Entity_SetRotation(cam, Vector3(-22.0f, 0.0f, 0.0f));
    }}
}}
'''
with open(os.path.join(OUT, 'scripts', 'StreamStress.as'), 'w') as f:
    f.write(script)

# ---------------- project manifest ----------------
# Every chunk sub-scene must be listed here or BuildPipeline never packs it
# (the packer walks the project's scene list). buildIndex -1 keeps them out of
# the build ORDER while still shipping them in the .enjpak, which is exactly
# what a streamed sub-scene wants.
scenes = [{"name": "Main", "path": "scenes/Main.enjin", "buildIndex": 0, "isStartScene": True}]
for cz in range(GRID):
    for cx in range(GRID):
        scenes.append({"name": f"chunk_{cx}_{cz}",
                       "path": f"scenes/chunks/chunk_{cx}_{cz}.enjin",
                       "buildIndex": -1, "isStartScene": False})
proj = {"name": "StreamStress", "version": "1.0", "scenes": scenes}
with open(os.path.join(OUT, 'StreamStress.enjinproject'), 'w') as f:
    json.dump(proj, f, indent=2)

vtx_stride = 136
per_chunk_est = PATCH * PATCH * vtx_stride * 2 + (PATCH - 1) ** 2 * 6 * 4 * 2 + 256
print(f"grid={GRID}x{GRID} ({GRID*GRID} chunks), patch={PATCH}x{PATCH} ({PATCH*PATCH} verts)")
print(f"chunk scene JSON on disk: {chunk_bytes/1024/1024:.1f} MB")
print(f"engine estimate per chunk: {per_chunk_est/1024/1024:.2f} MB")
print(f"all chunks resident (no budget): {GRID*GRID*per_chunk_est/1024/1024:.1f} MB")
print(OUT)
