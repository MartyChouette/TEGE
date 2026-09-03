# Streaming memory stress harness

Reproduces the level-streaming memory test: a 7x7 grid of streamed sub-scenes,
each holding a heavy inline-geometry patch, swept by a scripted camera with
**distance-unload effectively disabled** (`unloadDistance` is enormous). That
makes the memory budget the only thing that can bound the resident set, so the
budgeter is measured rather than the distance hysteresis.

## Run it

```bash
# 1. generate the project (49 chunk sub-scenes + main scene + sweep script)
python tools/streamstress/generate_project.py D:/GitHub/enjin/_streamstress

# 2. pack it for web (headless; needs a built EnjinEditor + build-web/bin)
build/bin/Release/EnjinEditor.exe --build-web \
    D:/GitHub/enjin/_streamstress/StreamStress.enjinproject <outDir>

# 3. drop the harness page in and serve
cp tools/streamstress/harness.html <outDir>/index.html
cd <outDir> && python serve.py --port 9094
```

Open the page, let it sweep, then click a budget button. `?auto` runs the
scripted A/B unattended and beacons one row per second to the dev server.

## What it measures

`chunk_resident_mb` is the engine's own estimate (`StreamingManager::
GetResidentBytes`) — inline geometry counted twice for the ECS + GPU copies plus
a per-entity overhead. It is the number the budgeter acts on, not a heap
measurement. The WASM heap is reported alongside it; Emscripten never shrinks
the heap, so heap only moves when the working set forces growth.

## Knobs

`GRID`, `PATCH`, `SPACING`, `LOAD_DIST` at the top of `generate_project.py`.
`GRID=7`/`PATCH=50` gives ~0.76 MB per chunk and ~37 MB fully resident. Raise
`GRID` or `PATCH` to push past the WASM heap and force real growth.

## Requires

The scene JSON must list every chunk sub-scene in the project's `scenes` array
(with `buildIndex: -1`) or BuildPipeline never packs them — the packer walks the
project scene list, and a streamed sub-scene is otherwise invisible to it.
