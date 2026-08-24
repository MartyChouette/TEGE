#include "Enjin/Editor/ComponentHelp.h"
#include "Enjin/ECS/World.h"
#include "Enjin/ECS/Components/Transform.h"
#include "Enjin/ECS/Components/Mesh.h"
#include "Enjin/ECS/Components/Material.h"
#include "Enjin/ECS/Components/Light.h"
#include "Enjin/ECS/Components/Camera.h"
#include "Enjin/ECS/Components/Script.h"
#include "Enjin/ECS/Components/Gameplay.h"   // TilemapComponent, RigidbodyComponent
#include "Enjin/ECS/Components/DungeonGenerator.h"
#include "Enjin/ECS/Components/RandomBag.h"
#include "Enjin/ECS/Components/Scatter.h"
#include "Enjin/ECS/Components/TerrainGenerator.h"
#include "Enjin/ECS/Components/Terrain.h"
#include "Enjin/ECS/Components/WFC.h"
#include <imgui.h>
#include <string>
#include <unordered_map>

namespace Enjin {
namespace Editor {

// Presence / add helpers ----------------------------------------------------
template <typename T>
static bool Has(ECS::World* w, ECS::Entity e) { return w && w->HasComponent<T>(e); }
template <typename T>
static void Add(ECS::World* w, ECS::Entity e) { if (w && !w->HasComponent<T>(e)) w->AddComponent<T>(e); }

// The central registry. Keyed by the component's serializer key so the same
// string used for save/load, the Add-Component menu, and this help all match.
static const std::unordered_map<std::string, ComponentHelp>& Registry() {
    static const std::unordered_map<std::string, ComponentHelp> reg = [] {
        std::unordered_map<std::string, ComponentHelp> r;

        // ---- Procgen suite -------------------------------------------------
        r["randomBag"] = {
            "Hands out one authored item per pull, on demand.",
            "Author the item list below, pick a mode, then pull from a script.",
            "string s = RandomBag_Draw(self);   // next item\n"
            "int  i  = RandomBag_DrawIndex(self);\n"
            "RandomBag_Reset(self);",
            {
                { RelationKind::PulledByScript, "Script", Has<ECS::ScriptComponent>, Add<ECS::ScriptComponent> },
            }
        };
        r["dungeonGenerator"] = {
            "Builds a whole level once and paints it into a Tilemap.",
            "Pick an algorithm, press Generate Now, or let it run on play start.",
            nullptr,
            {
                { RelationKind::Paints, "Tilemap", Has<ECS::TilemapComponent>, Add<ECS::TilemapComponent> },
            }
        };
        r["scatter"] = {
            "Stamps copies of a prefab across a region (foliage, rocks, props).",
            "Set a prefab, pick a distribution, press Generate Now. Instances are children of this entity.",
            "Scatter_Generate(self);   // re-roll from script\n"
            "Scatter_Clear(self);      // remove the batch",
            {
                { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
            }
        };
        r["terrainGenerator"] = {
            "Bakes a noise + erosion heightfield into a Terrain mesh.",
            "Tune the FBM and erosion, press Generate Now, or let it run on play start.",
            "TerrainGen_Generate(self);   // rebake from script",
            {
                { RelationKind::Paints, "Terrain", Has<ECS::TerrainComponent>, Add<ECS::TerrainComponent> },
            }
        };
        r["wfc"] = {
            "Fills a grid so every neighbour pairing is legal (2D tiles or 3D modules).",
            "Add tiles, label their edges, press Generate. 2D paints a Tilemap; 3D places a prefab per cell. Tiles touch where edge labels match.",
            "WFC_Generate(self);   // resolve from script (returns 1 on success)",
            {
                { RelationKind::Paints, "Tilemap", Has<ECS::TilemapComponent>, Add<ECS::TilemapComponent> },
            }
        };

        // ---- Core transform / rendering -----------------------------------
        r["transform"] = {
            "Position, rotation and scale of the entity in the world.",
            "Drag the values, or move the entity with the gizmo in the viewport.",
            "Transform_SetPosition(self, x, y, z);",
            {}
        };
        r["mesh"] = {
            "The 3D geometry drawn for this entity.",
            "Assign a mesh asset. Add a Material to control how it looks.",
            nullptr,
            {
                { RelationKind::PairsWith,   "Material",  Has<ECS::MaterialComponent>, Add<ECS::MaterialComponent> },
                { RelationKind::PairsWith,   "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr },
            }
        };
        r["material"] = {
            "How a surface looks: colour, textures, roughness, metalness.",
            "Tweak the PBR fields, or drop in textures. Drives the lit shading.",
            nullptr,
            {
                { RelationKind::PairsWith,   "Mesh",     Has<ECS::MeshComponent>, Add<ECS::MeshComponent> },
                { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr },
            }
        };
        r["light"] = {
            "Casts light into the scene (directional, point or spot).",
            "Set colour and intensity. Direction comes from the Transform's rotation.",
            nullptr,
            {
                { RelationKind::PairsWith,   "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr },
            }
        };
        r["camera"] = {
            "A viewpoint the scene can be rendered from.",
            "Set the projection and field of view. One camera is the active view.",
            "Camera_SetActive(self);",
            {
                { RelationKind::PairsWith,   "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr },
            }
        };

        // ---- Gameplay ------------------------------------------------------
        r["scriptComponent"] = {
            "Runs an AngelScript behaviour on this entity every frame.",
            "Point it at a .as script. Its OnStart/OnUpdate drive the entity.",
            "// in your .as file:\n"
            "void OnUpdate(float dt) { /* ... */ }",
            {}
        };
        r["rigidbody"] = {
            "Makes the entity obey physics (gravity, forces, collisions).",
            "Choose dynamic or kinematic. Add a Collider so it can hit things.",
            "Rigidbody_AddForce(self, x, y, z);",
            {
                { RelationKind::FeedsPhysics, "Physics",  nullptr, nullptr },
                { RelationKind::PairsWith,   "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
            }
        };

        // ---- Colliders -----------------------------------------------------
        {
            const char* howCol = "Size is in WORLD units (entity scale is ignored). "
                                 "Add a Rigidbody to make it move; without one it's static.";
            std::vector<ComponentRelation> colRel = {
                { RelationKind::FeedsPhysics, "Physics",   nullptr, nullptr },
                { RelationKind::PairsWith,    "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> },
            };
            r["boxCollider"]     = { "A box-shaped collision volume for 3D physics.",     howCol, nullptr, colRel };
            r["sphereCollider"]  = { "A sphere-shaped collision volume for 3D physics.",  howCol, nullptr, colRel };
            r["capsuleCollider"] = { "A capsule collision volume (good for characters).",  howCol, nullptr, colRel };
            r["meshCollider"]    = { "A collision shape built from the mesh geometry.",    howCol, nullptr, colRel };
        }
        r["polygonCollider2D"] = {
            "A 2D polygon collision shape (Box2D).",
            "For 2D scenes only. Pair with a 2D body to make it dynamic.",
            nullptr,
            { { RelationKind::FeedsPhysics, "Physics 2D", nullptr, nullptr } }
        };

        // ---- Sprites / 2D --------------------------------------------------
        r["sprite2D"] = {
            "Draws a 2D image (sprite) for this entity.",
            "Assign a texture. Position and size come from the Transform.",
            nullptr,
            {
                { RelationKind::PairsWith,     "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer",  nullptr, nullptr },
            }
        };
        r["animatedSprite2D"] = {
            "Plays a frame-by-frame sprite animation.",
            "Set the frames and frame rate. Great for 2D characters and effects.",
            nullptr,
            {
                { RelationKind::PairsWith,     "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer",  nullptr, nullptr },
            }
        };

        // ---- Effects -------------------------------------------------------
        r["weatherZone"] = {
            "A volume that drives rain, snow, fog and wind in an area.",
            "Set the weather type and intensity, then overlap it with your play space.",
            nullptr,
            { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } }
        };
        r["particleEmitter"] = {
            "Emits CPU particles: smoke, sparks, magic, dust.",
            "Pick a preset, or tune rate, size and lifetime by hand.",
            nullptr,
            {
                { RelationKind::PairsWith,     "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer",  nullptr, nullptr },
            }
        };

        // ---- Controllers ---------------------------------------------------
        {
            std::vector<ComponentRelation> moveRel = {
                { RelationKind::DrivenByScript, "Input",     nullptr, nullptr },
                { RelationKind::PairsWith,      "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
            };
            r["platformer2D"] = { "Side-scrolling movement: run and jump (2D).",        "Tune speed, jump height and gravity. Reads player input.", nullptr, moveRel };
            r["topDown2D"]    = { "Top-down 8-way movement (2D).",                        "Tune move speed. Reads player input.",                     nullptr, moveRel };
            r["topDown3D"]    = { "Top-down movement with a follow camera (3D).",         "cameraAngle is pitch, not yaw. Reads player input.",       nullptr, moveRel };
            r["thirdPerson"]  = { "Third-person character movement (3D).",                "Walk, run and jump behind a follow camera.",               nullptr, moveRel };
            r["firstPerson"]  = { "First-person character movement + look (3D).",         "Mouse look plus WASD. Reads player input.",                nullptr, moveRel };
            r["vehicle"]      = { "Drives a vehicle: steering, throttle, brake.",         "Tune handling. Reads player input.",                       nullptr, moveRel };
            r["surfaceAligned"] = { "Keeps the entity aligned to the surface it walks on.", "Good for planet-gravity and wall-walking.",              nullptr, moveRel };
        }
        r["aiController"] = {
            "Steers the entity with AI: pathfinding and behaviors.",
            "Give it a target or a behavior. It drives the entity's movement.",
            nullptr,
            { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } }
        };

        // ---- UI ------------------------------------------------------------
        r["uiCanvas"] = {
            "A container that lays out UI elements on the screen.",
            "Anchors are Unity-style (edge = anchor*parent + offset). Add UI children under it.",
            nullptr,
            { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } }
        };

        // ---- Audio ---------------------------------------------------------
        r["audioSource"] = {
            "Plays a sound or music clip from this entity's position.",
            "Assign a clip. Enable 3D for positional (spatial) audio.",
            "AudioSource_Play(self);",
            {
                { RelationKind::PairsWith, "Transform",      Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::PairsWith, "Audio Listener", nullptr, nullptr },
            }
        };
        r["audioListener"] = {
            "The 'ears' of the scene. Usually on the camera or player.",
            "One active listener hears every 3D audio source.",
            nullptr,
            { { RelationKind::PairsWith, "Camera", Has<ECS::CameraComponent>, Add<ECS::CameraComponent> } }
        };

        // ---- Gameplay / effects (batch 2) ---------------------------------
        r["health"] = {
            "Hit points for this entity; it dies when they reach zero.",
            "Set max health. Damage or heal it from scripts or hazards.",
            "Health_Damage(self, 10);",
            { { RelationKind::DrivenByScript, "Script", Has<ECS::ScriptComponent>, Add<ECS::ScriptComponent> } }
        };
        r["triggerZone"] = {
            "Fires an event when something enters or exits this volume.",
            "Overlap it with the play space, then hook OnTrigger in a script.",
            nullptr,
            {
                { RelationKind::FeedsPhysics,   "Physics", nullptr, nullptr },
                { RelationKind::DrivenByScript, "Script",  Has<ECS::ScriptComponent>, Add<ECS::ScriptComponent> },
            }
        };
        r["gpuParticleEmitter"] = {
            "Emits GPU particles by the thousand: fire, magic, weather.",
            "Pick a preset or tune the emitter. Cheap even at high counts.",
            nullptr,
            {
                { RelationKind::PairsWith,     "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer",  nullptr, nullptr },
            }
        };
        r["cloth"] = {
            "Simulates cloth: flags, capes, banners.",
            "Pin some edges and tune stiffness. It catches wind and weather.",
            nullptr,
            {
                { RelationKind::PairsWith,     "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer",  nullptr, nullptr },
            }
        };
        r["swarm"] = {
            "A crowd of instanced agents (boids) for crowd demos.",
            "Set the agent count and behavior; they render instanced.",
            nullptr,
            { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } }
        };
        r["waterVolume"] = {
            "A body of water with waves, color and buoyancy.",
            "Size the volume and tune the waves. Things float in it.",
            nullptr,
            { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } }
        };
        r["grassVolume"] = {
            "Scatters grass across an area on the GPU.",
            "Set density and the area. Grass sways with the wind.",
            nullptr,
            { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } }
        };
        r["reflectionProbe"] = {
            "Captures the surroundings so reflections look right nearby.",
            "Place it where reflective surfaces need accurate reflections.",
            nullptr,
            {
                { RelationKind::PairsWith,     "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> },
                { RelationKind::FeedsRenderer, "Renderer",  nullptr, nullptr },
            }
        };

        // ---- Batch 3: audio zones, 2D physics, vegetation -----------------
        {
            std::vector<ComponentRelation> xform = {
                { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> }
            };
            r["reverbZone"] = { "Adds reverb/echo to sounds inside this area.",
                "Overlap it with a space, then pick a preset like Cave or Hall.", nullptr, xform };
            r["musicZone"] = { "Switches the background music when the player enters.",
                "Set the track and overlap it with the area it should play in.", nullptr, xform };
            r["ambientSoundLayer"] = { "A looping ambient bed (wind, crowd, rain) for an area.",
                "Assign a loop; it fades in as the player nears the zone.", nullptr, xform };
        }
        {
            std::vector<ComponentRelation> phys2d = {
                { RelationKind::FeedsPhysics, "Physics 2D", nullptr, nullptr }
            };
            r["body2D"]  = { "A 2D physics body (Box2D).",
                "For 2D scenes. Pair with a 2D collider so it can hit things.", nullptr, phys2d };
            r["joint2D"] = { "Connects two 2D bodies: hinge, weld, distance.",
                "Pick the joint type and the two bodies to link.", nullptr, phys2d };
            r["perFrameCollider"] = { "A 2D collider rebuilt every frame from the shape.",
                "For fast-moving or morphing 2D shapes.", nullptr, phys2d };
        }
        {
            std::vector<ComponentRelation> rend = {
                { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr }
            };
            r["water3D"]      = { "A 3D water surface with waves, reflection and buoyancy.",
                "Size the surface, then tune waves, color and flow.", nullptr, rend };
            r["gaussianSplat"] = { "A photoreal 3D capture (Gaussian splats) placed in the scene.",
                "Point it at a .ply or .spz splat file - phone scans work. Lights don't affect it (baked radiance); art styles and post effects do.", nullptr, rend };
            r["shrubVolume"]  = { "Scatters shrubs and bushes across an area on the GPU.",
                "Set density and the area to cover.", nullptr, rend };
            r["treeVolume"]   = { "Scatters trees across an area, with seasons and wind sway.",
                "Set density, the area, and the species.", nullptr, rend };
            r["vegetation"]   = { "Places vegetation (grass, plants) on surfaces.",
                "Tune density and the target surface.", nullptr, rend };
        }

        // ---- Batch 4 (bulk): the long tail ---------------------------------
        r["notes"] = { "Holds developer notes attached to an entity, not shipped in builds.", "Type reminders or to-dos for yourself here.", nullptr, {} };
        r["text"] = { "Shows a block of text in the world or UI.", "Type the words you want displayed.", nullptr, {} };
        r["terrain"] = { "A 3D heightmap ground you can sculpt and paint.", "Set the grid size, then raise, lower, and texture the surface.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["terrain2d"] = { "A 2D ground strip built from a height profile.", "Adjust the points to shape the ground line.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["viewmodel"] = { "Holds a first-person model like held hands or a weapon that stays in front of the camera.", "Point it at the mesh you want shown in front of the player.", nullptr, { { RelationKind::PairsWith, "Camera", nullptr, nullptr } } };
        r["cameraTrigger"] = { "Switches or blends the camera when the player enters its area.", "Place it where you want the camera to change and pick the target view.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["temperatureZone"] = { "Marks an area as hot or cold so things inside react to it.", "Place it and set the temperature for that space.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["gravityZone"] = { "Changes gravity direction or strength for anything inside it.", "Set the gravity vector and drop physics objects in.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr } } };
        r["fluidVolume"] = { "Marks a body of fluid that makes objects float and drag.", "Size the volume to your water or liquid area.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr } } };
        r["recordRewind"] = { "Records this entity's motion so it can be rewound in time.", "Enable it on objects you want to scrub backward.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["sceneRewind"] = { "Records the whole scene so you can rewind everything at once.", "Put it on one entity to control scene-wide rewind.", nullptr, {} };
        r["tilemap"] = { "A grid of tiles for building 2D levels.", "Pick a tileset and paint tiles onto the grid.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["stateMachine"] = { "Runs simple states and switches between them on conditions.", "Add states and the rules that move between them.", nullptr, { { RelationKind::DrivenByScript, "Script", Has<ECS::ScriptComponent>, Add<ECS::ScriptComponent> } } };
        r["dialogue"] = { "Holds a conversation tree the player can talk through.", "Write the lines and the choices that branch them.", nullptr, {} };
        r["damage"] = { "Deals damage to whatever it touches or overlaps.", "Set how much damage it does and what it can hurt.", nullptr, { { RelationKind::PairsWith, "Health", nullptr, nullptr } } };
        r["interactable"] = { "Lets the player interact with this object using an action button.", "Set the prompt text and what happens on use.", nullptr, { { RelationKind::DrivenByScript, "Script", Has<ECS::ScriptComponent>, Add<ECS::ScriptComponent> } } };
        r["pickup"] = { "An item the player can collect by touching it.", "Choose what it gives and any sound or effect on pickup.", nullptr, { { RelationKind::PairsWith, "Inventory", nullptr, nullptr } } };
        r["inventory"] = { "Stores items the entity is carrying.", "Set the slots and starting items.", nullptr, {} };
        r["timer"] = { "Counts down (or up) and fires when it reaches the target.", "Set the duration and what happens on finish.", nullptr, {} };
        r["gameOver"] = { "Triggers the game-over flow when its condition is met.", "Set what causes it and which screen or scene to show.", nullptr, {} };
        r["followTarget"] = { "Makes this entity move to follow another entity.", "Pick the target and how closely to trail it.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["lookAtTarget"] = { "Rotates this entity to keep facing another entity.", "Pick the target it should look at.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["waypoint"] = { "Marks a point in a path other entities can follow.", "Place it in order along the route you want.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["billboard"] = { "Turns a flat sprite to always face the camera.", "Put it on flat art you want to stay readable from any angle.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["camera2DBounds"] = { "Keeps the 2D camera inside a set rectangle.", "Size the box to the area the camera may show.", nullptr, { { RelationKind::PairsWith, "Camera", nullptr, nullptr } } };
        r["tag"] = { "Labels an entity with names you can search or filter by.", "Add tags you want to find or group entities with.", nullptr, {} };
        r["spawnPoint"] = { "Marks a spot where players or objects appear.", "Place it where you want things to spawn.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["layer"] = { "Assigns the entity to a layer for grouping and filtering.", "Pick which layer it belongs to.", nullptr, {} };
        r["streamingVolume"] = { "Loads and unloads a chunk of the world as the player nears or leaves it.", "Set the chunk name and the load and unload distances.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["streamingPortal"] = { "Links two streaming chunks so they load together at a doorway.", "Point it at the two chunks it connects.", nullptr, {} };
        r["cineComponent"] = { "Runs a virtual camera shot for cutscenes and framing.", "Set the shot values and target to compose the view.", nullptr, { { RelationKind::PairsWith, "Camera", nullptr, nullptr } } };
        r["saveData"] = { "Marks values on this entity to be written into the save file.", "Choose which fields should persist across sessions.", nullptr, {} };
        r["saveLoadMenu"] = { "Shows a menu for saving and loading game slots.", "Place it where players pick a save slot.", nullptr, {} };
        r["skeleton"] = { "Holds the bones that drive a rigged model's animation.", "Attach it to a mesh that has a skeleton to animate.", nullptr, { { RelationKind::PairsWith, "Mesh", Has<ECS::MeshComponent>, nullptr } } };
        r["jellyMesh"] = { "Makes a mesh wobble and jiggle like soft jelly.", "Tune the softness and bounce to taste.", nullptr, { { RelationKind::PairsWith, "Mesh", Has<ECS::MeshComponent>, nullptr } } };
        r["tether"] = { "Keeps this entity connected to another by a rope or link.", "Pick the other end and the tether length.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["grabbable"] = { "Lets the player pick this object up and carry it.", "Enable it on objects you want to be held.", nullptr, { { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };
        r["flowerStem"] = { "Makes a flower stem sway and bend.", "Adjust the stem stiffness and sway.", nullptr, {} };
        r["flowerParticleConfig"] = { "Controls the petals and pollen particles a flower gives off.", "Set the particle look and how many spawn.", nullptr, {} };
        r["possessable"] = { "Lets the player take direct control of this entity.", "Enable it on characters you want to switch into.", nullptr, { { RelationKind::DrivenByScript, "Script", Has<ECS::ScriptComponent>, Add<ECS::ScriptComponent> } } };
        r["lock"] = { "Blocks a door or object until the matching key is used.", "Set which key or condition unlocks it.", nullptr, {} };
        r["pushable"] = { "Lets the player push this object around.", "Enable it and set how hard it is to move.", nullptr, { { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };
        r["switch"] = { "A toggle the player flips to trigger something.", "Wire its on and off states to a target.", nullptr, {} };
        r["goalZone"] = { "Marks the finish or objective area a player must reach.", "Place it where reaching it counts as success.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["conveyor"] = { "Pushes objects that stand on it in a set direction.", "Set the direction and speed of the belt.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr } } };
        r["teleporter"] = { "Moves whatever enters it to a linked destination.", "Point it at the exit location.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["destructible"] = { "Lets this object break apart when it takes enough damage.", "Set its health and what happens when it breaks.", nullptr, { { RelationKind::PairsWith, "Health", nullptr, nullptr } } };
        r["curlNoiseField"] = { "Adds a swirling wind-like force that pushes particles around.", "Size the field and set how strong the swirl is.", nullptr, {} };
        r["fractureConfig"] = { "Sets how an object shatters into pieces when destroyed.", "Choose the piece count and break pattern.", nullptr, { { RelationKind::PairsWith, "Mesh", Has<ECS::MeshComponent>, nullptr } } };
        r["movingPlatform"] = { "A platform that travels along a set path.", "Set the waypoints and the travel speed.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["damageResistance"] = { "Reduces incoming damage by type.", "Set the multiplier for each damage type.", nullptr, { { RelationKind::PairsWith, "Health", nullptr, nullptr } } };
        r["resource"] = { "Tracks a gatherable resource amount on this entity.", "Set the resource type and starting amount.", nullptr, {} };
        r["footstep"] = { "Plays footstep sounds as the entity walks.", "Assign the step sounds and pacing.", nullptr, {} };
        r["poolable"] = { "Lets this entity be reused from an object pool instead of created fresh.", "Enable it on things you spawn often, like bullets.", nullptr, {} };
        r["questState"] = { "Tracks progress through a quest on this entity.", "Set the quest and its current step.", nullptr, {} };
        r["hudWidget"] = { "Shows a piece of on-screen game UI like a health bar.", "Pick the widget type and where it sits on screen.", nullptr, {} };
        r["cinematicCamera"] = { "A scripted camera for cutscene shots and moves.", "Set the path and timing of the shot.", nullptr, { { RelationKind::PairsWith, "Camera", nullptr, nullptr } } };
        r["tween"] = { "Smoothly animates a value from one number to another over time.", "Pick what to animate and the duration.", nullptr, {} };
        r["animationRecorder"] = { "Records animation into a clip you can replay later.", "Start recording, then save the captured motion.", nullptr, {} };
        r["networkIdentity"] = { "Marks this entity so it can be tracked across the network.", "Add it to anything that must exist for all players.", nullptr, {} };
        r["networkTransform"] = { "Syncs this entity's position and rotation over the network.", "Set the sync rate for how often it updates.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["elementalSurface"] = { "Marks a surface as an element like fire, water, or ice that reacts to others.", "Pick the element this surface is made of.", nullptr, {} };
        r["elementalEmitter"] = { "Sends out an element like fire or water that affects things nearby.", "Choose the element and its range.", nullptr, {} };
        r["elementalVolume"] = { "Fills an area with an element that reacts with objects inside.", "Size the volume and pick its element.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["dynamicDifficulty"] = { "Adjusts game difficulty based on how the player is doing.", "Set the range it can scale between.", nullptr, {} };
        r["artStyle"] = { "Applies a visual style like toon or retro to rendering.", "Pick the style you want the look to follow.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["materialSlots"] = { "Holds a separate material for each sub-mesh of a multi-part model.", "Assign a material to each slot in the list.", nullptr, { { RelationKind::PairsWith, "Mesh", Has<ECS::MeshComponent>, nullptr } } };
        r["boneAttachment"] = { "Sticks this entity onto a bone of another rigged model.", "Pick the target model and the bone to attach to.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["lod"] = { "Swaps in simpler versions of a mesh as it gets farther from the camera.", "Set the distance where each detail level kicks in.", nullptr, { { RelationKind::Requires, "Mesh", Has<ECS::MeshComponent>, nullptr } } };

        // Audio (batch 4)
        r["audioSnapshotTrigger"] = { "Switches to a mixer snapshot (like Combat or Dialogue) while the listener is inside this volume.", "Set a snapshot name and size the volume around the area it should affect.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> }, { RelationKind::PairsWith, "Audio Listener", nullptr, nullptr } } };
        r["audioOcclusion"] = { "Muffles sounds that are blocked by walls or objects between the source and the listener.", "Enable it and set the cutoff. It raycasts to decide how muffled things get.", nullptr, { { RelationKind::PairsWith, "Audio Listener", nullptr, nullptr } } };
        r["lipSync"] = { "Drives a character's mouth shapes from spoken audio so the lips match the voice.", "Add it to a talking character. It can auto-follow amplitude if you have no viseme data.", nullptr, { { RelationKind::PairsWith, "Transform", Has<ECS::TransformComponent>, Add<ECS::TransformComponent> } } };
        r["audioReactive"] = { "Makes a property like light or scale pulse in time with the loudness of an audio bus.", "Pick a bus and a target property, then tune the threshold and multiplier.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["audioThresholdTrigger"] = { "Fires an effect like a light flicker or camera shake when an audio bus gets loud enough.", "Set the bus, the threshold, and which effect to fire when it crosses.", nullptr, {} };
        r["rtpc"] = { "Maps named game parameters to audio settings like volume, pitch, or low-pass in real time.", "Add a mapping, name the parameter, and set its input and output ranges.", nullptr, { { RelationKind::DrivenByScript, "Script", Has<ECS::ScriptComponent>, Add<ECS::ScriptComponent> } } };
        r["beatClock"] = { "A musical clock that counts bars and beats at a set tempo for the whole scene.", "Set the BPM and beats per bar. Other beat components read from it.", nullptr, {} };
        r["beatSync"] = { "Pulses a property like light or scale in time with a beat clock's beats.", "Pick a sync mode and a target, then set the base and pulse values.", nullptr, { { RelationKind::PairsWith, "Beat Clock", nullptr, nullptr }, { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["conductor"] = { "Blends layered music stems in and out based on gameplay state like combat or stealth.", "Add your stems and check which states each one plays in. It crossfades for you.", nullptr, {} };
        r["audioCollision"] = { "Plays impact, scrape, and roll sounds when this physics object touches things.", "Pick a surface material and tune mass and thresholds. It reacts to collisions.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr } } };
        r["sidechain"] = { "Ducks one audio bus down whenever another bus is playing, like lowering music under voice.", "Set the source bus that triggers and the target bus that gets quieter.", nullptr, {} };
        r["midiBinding"] = { "Maps knobs, faders, and notes from a connected MIDI controller to scene properties.", "Add a binding, choose the CC or note, and pick the property it should drive.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["audioFidelity"] = { "Restyles all audio with retro presets like 8-bit, lo-fi vinyl, or cassette tape.", "Pick a style preset and set the intensity, or let it match the scene's art style.", nullptr, {} };
        r["materialInteractionTable"] = { "Defines the sound for each pair of surface materials, like metal on stone.", "Put one on any entity. Every Audio Collision component shares it.", nullptr, { { RelationKind::PairsWith, "Audio Collision", nullptr, nullptr } } };

        // Physics joints + ragdoll (batch 4)
        r["ragdoll"] = { "Turns a character into a set of linked bones that flop and tumble with physics, like a body going limp.", "Add it to a skinned character, then let it activate on death or trigger it yourself.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr } } };
        r["distanceJoint"] = { "Keeps two bodies a fixed distance apart, like a rigid rod or a taut rope between them.", "Set the two bodies and the distance you want held between them.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr }, { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };
        r["hingeJoint"] = { "Pins two bodies so they swing around a shared axis, like a door on its hinges.", "Set the two bodies and the hinge axis.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr }, { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };
        r["ballSocketJoint"] = { "Links two bodies at a point they can freely rotate around, like a shoulder or a hip.", "Set the two bodies and the point where they connect.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr }, { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };
        r["springJoint"] = { "Connects two bodies with a springy link that pulls them back toward a rest length when stretched or squeezed.", "Set the two bodies, then tune the rest length and how stiff and bouncy the spring feels.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr }, { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };
        r["fixedJoint"] = { "Welds two bodies together so they move and turn as one solid piece.", "Set the two bodies you want locked together.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr }, { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };
        r["sliderJoint"] = { "Lets two bodies slide along one shared line while staying lined up, like a drawer in its rails.", "Set the two bodies and the axis they slide along.", nullptr, { { RelationKind::FeedsPhysics, "Physics", nullptr, nullptr }, { RelationKind::PairsWith, "Rigidbody", Has<ECS::RigidbodyComponent>, Add<ECS::RigidbodyComponent> } } };

        // Inspector-file components (batch 4)
        r["meshRenderer"] = { "Controls how this entity's mesh is drawn: culling, shadows, LOD, render queue, and instancing.", "Tune the draw settings here; it needs a Mesh and Material to actually render.", nullptr, { { RelationKind::Requires, "Mesh", nullptr, nullptr }, { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["visualScript"] = { "Runs a node-based logic graph on this entity without writing code.", "Add nodes and variables here, then edit the graph in the Visual Script panel.", nullptr, { { RelationKind::DrivenByScript, "Visual Script", nullptr, nullptr } } };
        r["lookAtIK"] = { "Rotates the head and neck bones so a character looks toward a target point.", "Name the head and neck bones, set a target position, and blend it in with Look Weight.", nullptr, { { RelationKind::PairsWith, "Skeleton", nullptr, nullptr } } };
        r["interactionIK"] = { "Reaches a character's hand toward nearby interactable targets using the arm bones.", "Name the shoulder, elbow, and hand bones, set a reach radius, and match an interaction tag.", nullptr, { { RelationKind::PairsWith, "Skeleton", nullptr, nullptr } } };
        r["twoBoneIK"] = { "Bends a two-bone chain like an arm or leg so its tip reaches a target.", "Name the root, mid, and tip bones, then drive the target from script or an interaction.", nullptr, { { RelationKind::PairsWith, "Skeleton", nullptr, nullptr } } };
        r["parallaxMachine"] = { "Scrolls stacked 2D background layers at different speeds to fake depth.", "Add layers with textures and per-layer speeds; they drift as the camera moves.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["interactiveWater"] = { "A rippling water surface that reacts to objects with waves, buoyancy, and current.", "Set the grid size and wave settings; drop objects with a Water Interactor to make splashes.", nullptr, { { RelationKind::PairsWith, "Water Interactor", nullptr, nullptr }, { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };
        r["waterInteractor"] = { "Lets this object push on interactive water so it splashes, floats, or sinks.", "Set density and volume, then move it over an Interactive Water surface.", nullptr, { { RelationKind::PairsWith, "Interactive Water", nullptr, nullptr } } };
        r["animator"] = { "Plays and blends skeletal animations for a rigged model.", "Add clips and a blend tree; it drives the Skeleton each frame.", nullptr, { { RelationKind::PairsWith, "Skeleton", nullptr, nullptr } } };

        // Rendering
        r["postProcessVolume"] = { "Applies post-processing (bloom, color grading, vignette) in an area or globally.", "Toggle the effects and tune them; make it global or bound to this volume.", nullptr, { { RelationKind::FeedsRenderer, "Renderer", nullptr, nullptr } } };

        return r;
    }();
    return reg;
}

const ComponentHelp* GetComponentHelp(const char* key) {
    if (!key) return nullptr;
    const auto& reg = Registry();
    auto it = reg.find(key);
    return (it == reg.end()) ? nullptr : &it->second;
}

// Short verb shown before each connection label.
static const char* RelationVerb(RelationKind k) {
    switch (k) {
        case RelationKind::Requires:       return "needs";
        case RelationKind::PairsWith:      return "pairs with";
        case RelationKind::Paints:         return "paints";
        case RelationKind::PulledByScript: return "pulled by";
        case RelationKind::DrivenByScript: return "driven by";
        case RelationKind::FeedsRenderer:  return "feeds";
        case RelationKind::FeedsPhysics:   return "feeds";
    }
    return "->";
}

void DrawComponentHelp(const char* key, ECS::World* world, ECS::Entity entity) {
    const ComponentHelp* help = GetComponentHelp(key);
    if (!help) return;

    // WHAT — one plain sentence, in a soft accent colour.
    if (help->whatItDoes) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.72f, 0.85f, 1.0f));
        ImGui::TextWrapped("%s", help->whatItDoes);
        ImGui::PopStyleColor();
    }

    // HOW — imperative, dimmed. Optional script snippet in a subtle box.
    if (help->howToUse) {
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.62f, 1.0f));
        ImGui::TextWrapped("%s", help->howToUse);
        ImGui::PopStyleColor();
        if (help->scriptSnippet) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.80f, 0.60f, 1.0f));
            ImGui::TextUnformatted(help->scriptSnippet);
            ImGui::PopStyleColor();
        }
        ImGui::Unindent();
    }

    // CONNECTS — live: which partners are present, missing, or informational.
    if (!help->relations.empty()) {
        ImGui::TextDisabled("Connects:");
        ImGui::Indent();
        for (const auto& rel : help->relations) {
            ImGui::TextDisabled("%s %s", RelationVerb(rel.kind), rel.label);
            if (rel.present) {
                bool have = rel.present(world, entity);
                ImGui::SameLine();
                if (have) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.85f, 0.45f, 1.0f));
                    ImGui::TextUnformatted("[ok]");
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.65f, 0.30f, 1.0f));
                    ImGui::TextUnformatted("(missing)");
                    ImGui::PopStyleColor();
                    if (rel.add) {
                        ImGui::SameLine();
                        ImGui::PushID(rel.label);
                        if (ImGui::SmallButton("Add")) rel.add(world, entity);
                        ImGui::PopID();
                    }
                }
            }
        }
        ImGui::Unindent();
    }
    ImGui::Separator();
}

} // namespace Editor
} // namespace Enjin
