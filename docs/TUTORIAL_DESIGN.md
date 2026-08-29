# TEGE Tutorial Design

Flagship #10. The goal in one line, in Marty's words:

> I want it to feel like a videogame just taught me how to use this software. Simple, guiding, and a pipeline for my own imagination.

That sentence is the whole spec. The tutorial is not a manual and not a video. It is a short game you play inside the editor, and by the time you finish playing it you have made a real thing and you know how to make more.

## What "the best tutorial anyone has ever seen" actually means

Three promises, in order of importance:

1. **Simple.** You are never asked to understand the editor before you use it. You do one small thing, it works, you feel it. No wall of text ever. One line at a time, spoken by a friendly guide, pointing at the real button.
2. **Guiding.** The tutorial highlights the real panel, waits for the real click, and celebrates the real result. It never fakes the UI. What it teaches you is the same thing the editor's own help says, so the training and the tool speak one language.
3. **A pipeline for your imagination.** Every step ends in something visible and playable that is *yours*. You are not copying steps. You are changing a living thing and watching it answer. By the end you are not "done with the tutorial," you are already building your idea.

## The feel

- You press Play in the first minute and a game runs. The win comes before the explanation.
- The guide is a character, not a modal box. It says one line, then gets out of the way. It is always dismissible and never blocks you.
- Every beat is show, then do, then celebrate. Watch it happen, do it yourself, get a small moment of joy.
- Nothing is a dead end. Everything the tutorial teaches is always available afterward through the editor's built-in help and the command palette, so the tutorial is a door, not a hoop.
- It respects experts. Fully skippable at any point, resumable later, and re-openable from Help. A returning maker never has to sit through it.

## Part A: The First 10 Minutes (interactive first-run draft)

A single unbroken flow the first time the editor opens on a fresh project. It loads a tiny pre-built scene: one character on one platform, a camera, a light. Everything below is a beat. Each beat highlights the actual UI element, waits for the actual action, then confirms.

**Beat 0 — Hello.**
The guide appears in a corner. One line: "Want the 10 minute tour, or jump straight in?" Two buttons: Take the tour / I've got it. If they skip, it never nags again and lives in Help.

**Beat 1 — It's already a game.**
"Press Play." The Play button pulses. They press it. The scene runs. The guide: "That is your game, running. Press Stop." The point of beat 1 is the feeling that this is real and it already works.

**Beat 2 — Make it yours.**
"Click the character." The character highlights in the viewport and the tree. "See the Inspector on the right? Change its color." They drag the color. "Press Play. That color is yours now." They have now touched selection, the inspector, and the play loop, and they changed the world.

**Beat 3 — Make it move.**
"Every object is made of parts. Give this one the part that lets it walk and jump." The guide points at the Add Component search, or a suggested Platformer Controller chip. They add it. "Press Play. Use the arrow keys." It moves. The guide: "You just gave it behavior. That is the whole game, parts on objects."

**Beat 4 — Change the whole vibe.**
This is the wow beat, and it is the moment TEGE shows its personality. "Open Art Style. Pick PICO-8." The entire scene snaps into a chunky palette. "Try Game Boy. Try CRT." The maker sees that look is one click, not a pipeline. This is where they fall in love.

**Beat 5 — Make a world.**
"Drag a tree from the Asset Browser into the scene." They place it. "Drop a few. Move them around." They learn the asset browser and placement by decorating, which feels like play, not work.

**Beat 6 — You made a game. Share it.**
"You have a moving character, your own look, and a little world. That is a game." Then the payoff: "Here is how to put it in a browser and get a link you can send anyone." One click to the local preview, the game runs in a browser. Ending on a shareable artifact is what turns a lesson into pride.

**Beat 7 — Where your imagination goes next.**
The guide offers branching tracks, not a syllabus: "Want to build a platformer? A top-down adventure? Make it talk? Learn to give it rules?" Each opens a focused module from Part B. The first ten minutes end with momentum, not a certificate.

Every beat has: a highlight on the real control, a wait for the real action, a one-line confirm, and a tiny celebration. No beat is a paragraph. If a maker gets stuck, the guide nudges once, then offers to do it with them.

## Part B: The Full Curriculum

Each module is short, interactive, ends with a working artifact you keep, and has an optional challenge. Modules are branch-in-any-order after the First 10 Minutes, not a locked ladder. A maker can also ignore all of it and build, and pull a single module when they hit a wall.

**Track 0 — First 10 Minutes** (Part A above). The one everyone starts with.

**Track 1 — The Editor.**
- The panels: hierarchy, inspector, viewport, console, asset browser.
- Moving around: fly camera, focus, the transform gizmos.
- Play, Pause, Stop, and what persists when you stop.
- Undo, redo, duplicate, multi-select.
- The command palette and the console.
- Projects, scenes, saving.

**Track 2 — Objects and Their Parts.**
- Entities and components, taught as objects and the parts they carry.
- The wiring board and the built-in help: what a part does, how to use it, what it connects to.
- Transform and parenting.
- Prefabs: make once, reuse everywhere, override one and only that one changes.

**Track 3 — Make It Move.**
- Character controllers: 2D platformer, top-down, 3D.
- Input and input actions, remappable from day one.
- Physics: colliders, triggers, sensors, ground checks.
- Health, damage, hazards.
- Cameras and the camera director for shots that follow and frame.

**Track 4 — Make It Look Good.**
- Materials: PBR plus the stylistic knobs.
- Art style presets: PSX, palettes, CRT, cel, low poly, and the rest.
- Lighting: lights, shadows, ambient, and baked lighting for the prerendered look.
- Reflections: the hand-crafted styles, matcap through planar mirrors.
- Post-processing, weather, water.
- Particles and VFX.

**Track 5 — Make It Feel Alive.**
- Audio: sound effects, music, spatial sound, surface response.
- Juice: tweens, screen shake, hit feel.
- UI and HUD, menus, dialogue.

**Track 6 — Logic Without Code.**
- Visual scripting: nodes and events.
- Behavior trees, AI, navmesh.
- Quests, save and load.

**Track 7 — Scripting.**
- The TegeBehavior skeleton and the scripting API.
- Hot reload, events, coroutines.

**Track 8 — Procgen and Big Worlds.**
- The procgen suite: dungeons, scatter, terrain, wave function collapse.
- Level streaming and large worlds.

**Track 9 — For Everyone.**
- Accessibility: colorblind modes, subtitles, remapping, reduced motion.
- Localization.
- Options menus that actually work.

**Track 10 — Ship It.**
- The startup flow and boot sequence.
- Building for Windows, Linux, and the web.
- One click to a shareable web link.
- Save systems and settings that persist.

## How it is built in the engine

- **One teaching language.** The tutorial uses the same what/how/connects help the editor already shows on every component. The tutorial and the always-on help are the same system, so learning in the tutorial and learning on your own feel identical.
- **The guide is the mascot.** The character that walks you through the First 10 Minutes is the TEGE mascot, and its Clippy-style helper mode is the same character offering optional contextual tips later. One friendly face, always dismissible, always off-switchable.
- **Real UI, real actions.** Steps drive and highlight the actual editor controls and wait for the actual action. No fake screenshots, no simulated panels.
- **Skippable and resumable.** Any step can be skipped, the whole thing can be exited, progress is saved, and it re-opens from Help. Experts are never trapped.
- **Ends in artifacts.** Every module leaves the maker with a working, playable, keepable result, and the First 10 Minutes ends with a shareable link. Pride is the retention mechanism.

## Smart, state-aware logic (never dumb, never redundant)

The tutorial and every autonomous wizard, quick-setup, and one-click Add button must inspect the world before they touch it. A wizard that adds a second controller, drops a rigidbody on something that already has one, or mixes 2D and 3D is worse than no wizard. The rule is: read the current state, then only offer or apply the thing that actually fits. Concretely:

- **Inspect before acting.** Before adding any component, check whether it or an equivalent already exists on the entity. If it does, configure the existing one, never add a duplicate. No two controllers, no two rigidbodies, no two colliders that fight.
- **Respect the scene's dimension.** Detect whether the scene is 2D, 2.5D, or 3D and only ever offer matching parts. 2D scenes use the 2D physics and the 2D controllers, 3D scenes use the 3D physics and the 3D controllers, and the wizard never mixes them. This follows the engine's strict 2D and 3D separation. A step that would put a 3D character controller in a 2D scene simply does not appear.
- **No conflicts.** Do not add parts that contradict what is already there. If the object already walks, the "make it move" step offers to tune it, not to bolt on a second way to move. If it already has a collider, offer to resize it, not to stack another.
- **Idempotent steps.** Running a step twice, or coming back to a module later, must not pile up junk. A step that is already satisfied shows as done and does nothing on repeat.
- **Suggest for the context, not from a script.** The guide reads what the maker actually has and proposes the next real thing for that scene. It is decision logic over the live scene, not a fixed sequence played blind. This is the cutting-edge part: the tutorial adapts to the project instead of assuming a clean slate.
- **Explain the why when it declines.** When the wizard chooses not to add something because it would conflict, it says so in one friendly line, so the maker learns the rule instead of being confused by a missing step.

This logic is shared. The same state-aware checks power the tutorial steps, the ComponentHelp "Add" buttons, and any future auto-wiring, so nothing in the editor can produce a redundant or contradictory setup.

## The one test

After the First 10 Minutes, the maker should not think "I finished the tutorial." They should think "I want to build the thing I just imagined, and I know where to start." If that is true, it is the best tutorial anyone has seen.
