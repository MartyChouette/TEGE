# Generates Examples/Playground - the master TEGE showcase scene (task #20).
# Re-run after editing; it rebuilds scenes/Main.enjin + scripts + assets.
import json, copy, os, shutil, struct, zlib

ROOT = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(ROOT, "Examples", "Playground")
os.makedirs(os.path.join(OUT, "scenes"), exist_ok=True)
os.makedirs(os.path.join(OUT, "scripts"), exist_ok=True)
os.makedirs(os.path.join(OUT, "assets"), exist_ok=True)

# --- borrow the unit-cube mesh + collider + player from FixedTimestep -------
src = json.load(open(os.path.join(ROOT, "Examples", "FixedTimestep", "scenes", "Main.enjin")))
by = {e["name"]["name"]: e for e in src["entities"]}
CUBE = by["Ground"]["mesh"]
BOXCOL = by["Ground"]["boxCollider"]
PLAYER = copy.deepcopy(by["Player"])

# --- assets: waterfall textures from WaterFX + a generated matcap -----------
# Rigged character (Quaternius robot, CC0) - kept across regens; sourced from
# Downloads when present so a fresh checkout still regenerates everything else.
_robot_src = os.path.join("C:", os.sep, "Users", "jerma", "Downloads",
                          "Animated Robot by Quaternius", "FBX", "Robot.fbx")
_robot_dst = os.path.join(OUT, "assets", "Robot.fbx")
if os.path.exists(_robot_src) and not os.path.exists(_robot_dst):
    shutil.copyfile(_robot_src, _robot_dst)

for tex in ("water.png", "foam.png", "mist.png"):
    shutil.copyfile(os.path.join(ROOT, "Examples", "WaterFX", "assets", tex),
                    os.path.join(OUT, "assets", tex))

def write_png(path, w, h, rgba):
    def chunk(tag, data):
        c = tag + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)
    raw = b"".join(b"\x00" + bytes(rgba[y*w*4:(y+1)*w*4]) for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw))
           + chunk(b"IEND", b""))
    open(path, "wb").write(png)

# Matcap: a lit-sphere gradient (warm key light upper-left, cool rim)
W = 128
px = bytearray(W*W*4)
for y in range(W):
    for x in range(W):
        nx, ny = (x/(W-1))*2-1, (y/(W-1))*2-1
        d2 = nx*nx + ny*ny
        i = (y*W+x)*4
        if d2 > 1.0:
            px[i:i+4] = b"\x00\x00\x00\xff"
            continue
        nz = (1.0 - d2) ** 0.5
        key = max(0.0, (-0.5*nx + 0.6*(-ny) + 0.62*nz))    # upper-left key
        rim = max(0.0, 1.0 - nz) ** 2.5
        r = min(255, int(40 + 215*key + 60*rim))
        g = min(255, int(34 + 190*key + 90*rim))
        b = min(255, int(30 + 150*key + 140*rim))
        px[i:i+4] = bytes((r, g, b, 255))
write_png(os.path.join(OUT, "assets", "matcap.png"), W, W, px)

# --- entity helpers ---------------------------------------------------------
eid = [0]
E = []
def ent(name, pos, scale=(1,1,1), rot=(0,0,0,1), **extra):
    eid[0] += 1
    e = {"id": eid[0], "name": {"name": name},
         "transform": {"position": list(pos), "rotation": list(rot),
                       "scale": list(scale), "visible": True}}
    e.update(extra); E.append(e); return e

def solid(name, pos, scale, color, rough=0.8, static=True, mat_extra=None, veg=None):
    col = copy.deepcopy(BOXCOL); col["size"] = list(scale)
    mat = {"baseColor": list(color), "metallic": 0, "roughness": rough}
    if mat_extra: mat.update(mat_extra)
    e = ent(name, pos, scale, mesh=copy.deepcopy(CUBE), material=mat, boxCollider=col,
            rigidbody={"isStatic": static, "isKinematic": False,
                       "mass": 0 if static else 1, "useGravity": not static})
    if veg: e["vegetation"] = veg
    return e

def deco(name, pos, scale, color, mat_extra=None, veg=None, rot=(0,0,0,1)):
    mat = {"baseColor": list(color), "metallic": 0, "roughness": 0.85}
    if mat_extra: mat.update(mat_extra)
    e = ent(name, pos, scale, rot=rot, mesh=copy.deepcopy(CUBE), material=mat)
    if veg: e["vegetation"] = veg
    return e

ROPE_MAT = {"baseColor": [0.62, 0.45, 0.25], "metallic": 0, "roughness": 0.9, "doubleSided": True}
CHAIN_MAT = {"baseColor": [0.55, 0.55, 0.6], "metallic": 0.8, "roughness": 0.35, "doubleSided": True}

# --- camera, light, ground --------------------------------------------------
ent("MainCam", (0, 9, 30),
    rot=[-0.13, 0, 0, 0.991],
    camera={"projectionType": 0, "fieldOfView": 60, "nearPlane": 0.1, "farPlane": 600,
            "isActive": True, "priority": 0, "clearColor": True, "clearDepth": True,
            "backgroundColor": [0.5, 0.62, 0.8], "orthoSize": 10})
ent("Sun", (8, 22, 12), rot=[-0.34, -0.1, 0, 0.93],
    light={"type": 0, "color": [1, 0.96, 0.88], "intensity": 1.25, "castShadows": True,
           "range": 10, "innerConeAngle": 12.5, "outerConeAngle": 17.5,
           "constantAttenuation": 1, "linearAttenuation": 0.09, "quadraticAttenuation": 0.032})
solid("Ground", (0, -0.5, 0), (64, 1, 64), (0.4, 0.52, 0.36))

# --- NW: waterfall (F1 authored motion) -------------------------------------
solid("FallCliff", (-22, 4, -20), (8, 8, 2), (0.42, 0.4, 0.44))
deco("Waterfall", (-22, 4, -18.85), (7.0, 8.2, 0.1), (0.85, 0.92, 1.0),
     mat_extra={"opacity": 0.8, "alphaMode": 1, "doubleSided": True,
                "baseColorTexturePath": "assets/water.png", "uvScrollSpeed": [0.0, 0.9]})
deco("FallFoam", (-22, 0.25, -18.4), (7.4, 0.5, 1.4), (1, 1, 1),
     mat_extra={"opacity": 0.9, "alphaMode": 1, "baseColorTexturePath": "assets/foam.png",
                "flipbookCols": 4, "flipbookRows": 1, "flipbookFps": 8.0})
ent("FallMist", (-22, 1.2, -18.2),
    particleEmitter={"emissionRate": 26, "lifetime": 1.6, "startSpeed": 0.7,
                     "startSize": 1.1, "endSize": 2.2, "spread": 0.45,
                     "direction": [0, 1, 0], "texturePath": "assets/mist.png",
                     "startColor": [0.9, 0.95, 1.0], "startAlpha": 0.28, "endAlpha": 0.0,
                     "gravityScale": -0.02, "playing": True})

# --- N: lake with buoyancy, in its OWN basin --------------------------------
# The water doesn't float on the ground plane: a sunken basin (floor + rim
# walls) gives it real depth below grade (Marty: "water needs to change where
# the floor is"). Engine-side auto-basin on WaterVolume is a queued feature;
# this is the authored version.
ent("Lake", (-2, 0, -22),
    waterVolume={"halfExtents": [12, 3, 8], "waterType": 0,
                 "waterColor": [0.08, 0.28, 0.42], "opacity": 0.82,
                 "waveSpeed": 1.0, "waveHeight": 0.14, "enableShore": True,
                 "shoreWidth": 0.12, "foamIntensity": 0.6})
solid("LakeBed", (-2, -2.2, -22), (24, 0.5, 16), (0.25, 0.3, 0.28))
solid("LakeRimN", (-2, 0.6, -30.4), (25.6, 2.2, 0.8), (0.5, 0.48, 0.45))
solid("LakeRimS", (-2, 0.6, -13.6), (25.6, 2.2, 0.8), (0.5, 0.48, 0.45))
solid("LakeRimW", (-14.4, 0.6, -22), (0.8, 2.2, 17.6), (0.5, 0.48, 0.45))
solid("LakeRimE", (10.4, 0.6, -22), (0.8, 2.2, 17.6), (0.5, 0.48, 0.45))
for i in range(3):
    solid(f"Floater{i}", (-6 + i * 4, 1.5, -22), (1.2, 1.2, 1.2),
          (0.7, 0.5 + 0.1 * i, 0.3), static=False)

# --- NE: vegetation grove - the REAL user-facing volume components ----------
# GPU-instanced grass/shrub/tree renderers (Marty: "everything in the scene
# should be easily-made user-facing components and volumes"). Deciduous trees
# change canopy color with the seasons; everything sways in the weather wind.
ent("Grove_Trees", (20, 0, -20),
    treeVolume={"halfExtents": [7, 0, 6], "density": 10, "treeType": 0,
                "trunkHeight": 2.4, "trunkWidth": 0.18, "canopyRadius": 1.3,
                "canopyOffset": 2.0, "windSwayStrength": 0.5})
ent("Grove_Shrubs", (20, 0, -20),
    shrubVolume={"halfExtents": [8, 0, 7], "density": 40})
ent("Grove_Grass", (20, 0, -20),
    grassVolume={"halfExtents": [9, 0, 8], "density": 7000,
                 "bladeHeight": 0.35, "windSwayStrength": 1.2})

# --- ladder station (G1, the component way) ---------------------------------
solid("LadderTower", (27, 2.5, 4), (2, 5, 2), (0.55, 0.5, 0.62))
deco("LadderRungs", (27, 2.5, 5.05), (0.8, 5.6, 0.12), (0.72, 0.5, 0.25))
ent("LadderVolume", (27, 2.9, 5.35),
    ladder={"halfExtents": [0.6, 2.9, 0.7], "climbSpeed": 3.0, "topBoost": 4.5,
            "allowJumpOff": True})

# --- CW: cloth court --------------------------------------------------------
solid("FlagPole", (-24, 3, 2), (0.3, 6, 0.3), (0.5, 0.45, 0.4))
ent("Flag", (-23.8, 5.6, 2),
    material={"baseColor": [0.85, 0.25, 0.2], "metallic": 0, "roughness": 0.9, "doubleSided": True},
    cloth={"width": 2.6, "height": 1.6, "resX": 18, "resY": 12, "pin": 2,   # LeftEdge
           "useWeatherWind": True, "weatherWindScale": 1.6, "tearable": False})
solid("DrapeBarLeft", (-17, 2.5, 4), (0.3, 5, 0.3), (0.5, 0.45, 0.4))
solid("DrapeBarRight", (-12, 2.5, 4), (0.3, 5, 0.3), (0.5, 0.45, 0.4))
solid("DrapeBar", (-14.5, 5, 4), (5.6, 0.25, 0.25), (0.5, 0.45, 0.4))
ent("TearableDrape", (-14.5, 4.85, 4),
    material={"baseColor": [0.6, 0.55, 0.8], "metallic": 0, "roughness": 0.95, "doubleSided": True},
    cloth={"width": 4.6, "height": 3.4, "resX": 20, "resY": 16, "pin": 0,   # TopEdge
           "useWeatherWind": True, "weatherWindScale": 1.2,
           "tearable": True, "tearThreshold": 1.5})
solid("Table", (-20, 0.6, 9), (3.0, 1.2, 2.0), (0.4, 0.3, 0.22))
ent("TableCloth", (-20, 2.1, 9),
    material={"baseColor": [0.95, 0.95, 0.9], "metallic": 0, "roughness": 0.95, "doubleSided": True},
    cloth={"width": 4.2, "height": 3.2, "resX": 20, "resY": 16, "pin": 4,   # None -> falls + drapes
           "collide": True, "friction": 0.8, "useWeatherWind": True,
           "weatherWindScale": 0.4, "tearable": False})

# --- C: fire + elemental reactivity + fountain ------------------------------
solid("FirePit", (0, 0.2, 4), (1.6, 0.4, 1.6), (0.25, 0.22, 0.2))
ent("CampFire", (0, 0.5, 4),
    elementalEmitter={"element": [1, 0, 0, 0], "emissionRate": 14, "intensity": 0.8,
                      "lifetime": 1.6, "spread": 0.25, "speed": 2.2,
                      "direction": [0, 1, 0], "active": True})
ent("DripWater", (0.4, 3.2, 4),
    elementalEmitter={"element": [0, 1, 0, 0], "emissionRate": 6, "intensity": 1.0,
                      "lifetime": 2.0, "spread": 0.12, "speed": 0.6,
                      "direction": [0, -1, 0], "active": True})
ent("Fountain", (5, 0.4, 8),
    particleEmitter={"emissionRate": 42, "lifetime": 1.5, "startSpeed": 4.2,
                     "startSize": 0.16, "endSize": 0.05, "spread": 0.14,
                     "direction": [0, 1, 0], "startColor": [0.65, 0.8, 1.0],
                     "startAlpha": 0.9, "endAlpha": 0.0, "gravityScale": 1.0,
                     "playing": True})

# --- CE: ropes, chains, clothesline -----------------------------------------
solid("RopeTower", (14, 3.5, 6), (2, 7, 2), (0.55, 0.5, 0.62))
solid("RopeArm", (14, 6.8, 8.2), (0.4, 0.4, 2.6), (0.5, 0.45, 0.4))
ent("ClimbRope", (14, 6.6, 9.2), material=dict(ROPE_MAT),
    rope={"length": 6.0, "segments": 20, "thickness": 0.07, "iterations": 10,
          "useWeatherWind": True, "weatherWindScale": 0.7, "collide": True},
    ladder={"halfExtents": [0.7, 3.8, 0.7], "climbSpeed": 2.5, "topBoost": 4.5,
            "allowJumpOff": True})
for i, lx in enumerate((9, 12)):
    ent(f"LanternChain{i}", (lx, 6.9, 2), material=dict(CHAIN_MAT),
        rope={"style": 1, "length": 2.6, "segments": 8, "thickness": 0.08,
              "iterations": 12, "endMass": 2.0, "endAttachName": f"Lantern{i}",
              "collide": False})
    ent(f"Lantern{i}", (lx, 4.1, 2), (0.4, 0.5, 0.4), mesh=copy.deepcopy(CUBE),
        material={"baseColor": [1.0, 0.85, 0.4], "metallic": 0, "roughness": 0.4,
                  "emissiveColor": [1.0, 0.75, 0.3], "emissiveStrength": 1.6})
solid("ChainBeam", (10.5, 7, 2), (4.4, 0.35, 0.35), (0.5, 0.45, 0.4))
solid("LinePostA", (20, 2, 12), (0.3, 4, 0.3), (0.45, 0.4, 0.35))
solid("LinePostB", (27, 2, 12), (0.3, 4, 0.3), (0.45, 0.4, 0.35))
ent("LineEnd", (27, 3.9, 12))
ent("ClothesLine", (20, 3.9, 12), material=dict(ROPE_MAT),
    rope={"length": 8.6, "segments": 26, "thickness": 0.03, "iterations": 10,
          "endAttachName": "LineEnd", "pinBottom": True,
          "useWeatherWind": True, "weatherWindScale": 1.8, "collide": False})
for i, cx in enumerate((21.6, 23.5, 25.4)):
    ent(f"Laundry{i}", (cx, 3.55, 12),
        material={"baseColor": [[0.9, 0.9, 0.95], [0.4, 0.6, 0.85], [0.9, 0.75, 0.5]][i],
                  "metallic": 0, "roughness": 0.95, "doubleSided": True},
        cloth={"width": 1.2, "height": 1.5, "resX": 10, "resY": 12, "pin": 0,  # TopEdge
               "useWeatherWind": True, "weatherWindScale": 2.0,
               "tearable": True, "tearThreshold": 1.45})

# --- S: render-style gallery -------------------------------------------------
STYLES = [
    ("MatcapSphere",  {"matcapTexturePath": "assets/matcap.png", "roughness": 0.4}),
    ("ScrollReflect", {"scrollReflectionTexturePath": "assets/water.png",
                       "scrollReflectionSpeed": [0.06, 0.02], "scrollReflectionStrength": 0.8,
                       "metallic": 0.6, "roughness": 0.3}),
    ("FlatShaded",    {"flatShading": True}),
    ("PS1Cube",       {"affineTexturing": True, "vertexSnapping": True,
                       "baseColorTexturePath": "assets/water.png"}),
    ("Stipple",       {"stippleTransparency": True, "opacity": 0.55}),
    ("Conveyor",      {"baseColorTexturePath": "assets/water.png",
                       "uvScrollSpeed": [0.35, 0.0]}),
]
for i, (nm, extra) in enumerate(STYLES):
    x = -15 + i * 6
    solid(f"Pedestal{i}", (x, 0.5, 20), (1.6, 1.0, 1.6), (0.35, 0.35, 0.4))
    deco(nm, (x, 1.8, 20), (1.3, 1.3, 1.3),
         (0.8, 0.8, 0.85) if "baseColorTexturePath" not in extra else (1, 1, 1),
         mat_extra=extra)
# reflective wet patch + subject
ent("WetPatch", (24, 0.02, 20), (6, 1, 6),
    mesh=copy.deepcopy(CUBE),
    material={"baseColor": [0.1, 0.12, 0.16], "metallic": 0.1, "roughness": 0.2,
              "opacity": 0.55, "alphaMode": 1},
    reflectivePlane={"reflectionStrength": 0.55, "tint": [0.8, 0.9, 1.0],
                     "blur": 0, "fresnelPower": 2.0, "resolution": 512,
                     "clipBias": 0.02, "active": True})
deco("WetSubject", (24, 1.5, 18.5), (1.0, 2.4, 1.0), (0.85, 0.3, 0.25))

# --- player + signs + script ------------------------------------------------
PLAYER["transform"]["position"] = [0, 0.8, 14]
eid[0] += 1; PLAYER["id"] = eid[0]
E.append(PLAYER)
ent("WelcomeSign", (0, 6.5, -6), (7, 3.4, 1),
    text={"text": "TEGE PLAYGROUND\nWASD move - Space jump - climb the ROPE and the LADDER (walk in + W)\nWeather evolves (watch the SKY change) - C colorblind - B BULLET TIME\nTear the purple drape and the laundry by running through",
          "fontSize": 38, "textureWidth": 1024, "textureHeight": 512,
          "textColor": [1, 1, 1], "bgColor": [0.05, 0.08, 0.12], "bgOpacity": 0.75,
          "horizontalAlign": 1, "wrapWidth": 1000})
ent("CharacterNote", (8, 2.2, 14), (4, 1.6, 1),
    text={"text": "Animated character station:\ndrag assets/Robot.fbx from the\nAsset Browser to HERE",
          "fontSize": 34, "textureWidth": 512, "textureHeight": 256,
          "textColor": [1, 0.95, 0.7], "bgColor": [0.1, 0.1, 0.05], "bgOpacity": 0.6,
          "horizontalAlign": 1, "wrapWidth": 480})
ent("Director", (0, 0, 0), scriptComponent={"scripts": [{"path": "scripts/Playground.as",
                                                         "class": "Playground", "enabled": True}]})

SCENE = {"version": "1.0",
         "skybox": {"type": 2, "topColor": [0.12, 0.24, 0.5],
                    "horizonColor": [0.5, 0.65, 0.82], "bottomColor": [0.45, 0.5, 0.55],
                    "sunDirection": [0.35, -0.7, 0.4], "sunIntensity": 1.0,
                    "sunSize": 0.03, "sunColor": [1, 0.95, 0.82],
                    "cloudCoverage": 0.35, "cloudScale": 2.0, "cloudColor": [1, 1, 1]},
         "entities": E}
json.dump(SCENE, open(os.path.join(OUT, "scenes", "Main.enjin"), "w"), indent=1)

json.dump({"name": "Playground", "version": "1.0",
           "scenes": [{"path": "scenes/Main.enjin", "buildIndex": 0, "isStartScene": True}]},
          open(os.path.join(OUT, "Playground.enjinproject"), "w"), indent=1)

open(os.path.join(OUT, "scripts", "Playground.as"), "w").write('''// Evolving weather + accessibility for the TEGE Playground.
class Playground : TegeBehavior {
    float t = 0.0f;
    int phase = -1;          // -1 forces the first announce
    int cbMode = 0;
    bool bullet = false;
    float rain = 0.0f, snow = 0.0f;      // current, eased toward targets
    float rainT = 0.0f, snowT = 0.0f;    // targets per phase

    void OnStart() {
        Subtitle_Show("Welcome to the TEGE Playground", "", 4.0f);
        Announcer_Announce("Playground loaded. Weather will evolve on its own.");
    }

    void OnUpdate(float dt) {
        // ---- evolving weather: 18s phases, intensities EASE between them ----
        t += dt;
        int p = int(t / 18.0f) % 4;
        if (p != phase) {
            phase = p;
            // Weather_Set drives the TYPE (the sim lerps intensities toward
            // the type's profile - setting intensity alone fights Clear).
            if (p == 0) { rainT = 0.0f; snowT = 0.0f; Weather_Set(0, 3.0f); Subtitle_Show("Weather: clear skies", "", 2.5f); }
            if (p == 1) { rainT = 0.85f; snowT = 0.0f; Weather_Set(2, 3.0f); Subtitle_Show("Weather: rain rolling in", "", 2.5f); Render_SetRainActive(true); }
            if (p == 2) { rainT = 0.0f; snowT = 0.8f; Weather_Set(4, 3.0f); Subtitle_Show("Weather: turning to snow", "", 2.5f); }
            if (p == 3) { rainT = 0.0f; snowT = 0.0f; Weather_Set(0, 3.0f); Subtitle_Show("Weather: clearing up", "", 2.5f); }
        }
        rain += (rainT - rain) * min(dt * 0.6f, 1.0f);
        snow += (snowT - snow) * min(dt * 0.6f, 1.0f);
        Weather_SetRainIntensity(rain);
        Weather_SetSnowIntensity(snow);
        if (rain < 0.02f && rainT == 0.0f) Render_SetRainActive(false);

        // ---- bullet time: B slows the world, the player stays at wall speed ----
        if (Input_GetKeyDown(Key::B)) {
            bullet = !bullet;
            uint64 me = Scene_FindEntity("Player");
            Time_SetScale(bullet ? 0.25f : 1.0f);
            if (me != 0) Controller_SetIgnoreTimeScale(me, bullet);
            Subtitle_Show(bullet ? "BULLET TIME - the world slows, you don't"
                                 : "Bullet time off", "", 2.5f);
        }

        // ---- accessibility: C cycles colorblind simulation modes ----
        if (Input_GetKeyDown(Key::C)) {
            cbMode = (cbMode + 1) % 4;
            Colorblind_SetMode(cbMode);
            string name = "off";
            if (cbMode == 1) name = "protanopia";
            if (cbMode == 2) name = "deuteranopia";
            if (cbMode == 3) name = "tritanopia";
            Subtitle_Show("Colorblind mode: " + name, "", 2.0f);
            Announcer_Announce("Colorblind mode " + name);
        }
    }

    float min(float a, float b) { return a < b ? a : b; }
}
''')

open(os.path.join(OUT, "README.md"), "w").write('''# TEGE Playground

The master showcase scene - one plaza with every major system live:

- NW waterfall (F1 authored motion: UV scroll + foam flipbook + mist particles)
- N lake with buoyant crates (WaterVolume + dynamic rigidbodies)
- NE grove: trees, shrubs, grass tufts swaying in the weather wind
- CW cloth court: flag (edge-pinned), TEARABLE purple drape (run through it),
  tablecloth draped over a table (collide + friction)
- C fire pit (elemental fire) with water dripping into it = steam reactivity,
  plus a classic particle fountain
- CE: CLIMBABLE rope (walk into it and push W - a ladder volume rides the rope),
  lanterns swinging on chains, a clothesline with tearable laundry
- S render-style gallery: matcap, scrolling reflection, flat shading, PS1
  affine+vertex-snap, stipple transparency, UV-scroll conveyor, and a
  reflective wet patch
- Weather EVOLVES on a timer (clear -> rain -> snow -> clear), eased, with
  subtitles + screen-reader announcements; C cycles colorblind modes

Drop a rigged .glb at the marked station for the animated-character exhibit.
''')

print("playground generated:", len(E), "entities")
