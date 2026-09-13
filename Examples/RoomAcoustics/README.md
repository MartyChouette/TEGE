# Room Acoustics

Three rooms, listened to.

They are the same size and shape. They differ only in what they are made of,
and **there is not one reverb setting in this scene** — no `ReverbZone`, no
decay time, no room size. Everything you hear is measured from the geometry you
are standing in.

## Playing it

Open the project, press **Play**, and:

| Key | What it does |
|-----|--------------|
| **Q** | Clap where you are standing |
| **P** | Projectors on and off |
| **R** | Print the listener position to the console |
| WASD, Space, Shift | walk, jump, sprint |

**Clap, then shut up and listen.** A reverb tail is what you hear *after* a
sound stops, so a room is auditioned with a transient and then silence. The
projectors are useful for placing a source against a wall and useless for
hearing a tail — a continuous drone fills the gap the tail is supposed to live
in, and every room ends up sounding like a drone. Press **P** to kill them, then
walk from the kitchen to the basement pressing **Q**.

The console prints the measured RT60 of whatever room you are standing in each
time the acoustics re-measure, so you can read the number you are hearing.

## What it measures

| Room     | Made of                                 | RT60 (low / mid / high) |
|----------|-----------------------------------------|-------------------------|
| Kitchen  | tiled floor and walls, plaster ceiling  | 3.56 / 3.72 / 3.16 s    |
| Hall     | boards underfoot, plaster around        | 2.11 / 2.06 / 2.30 s    |
| Basement | carpet down, soft hangings, soft ceiling| 1.32 / 0.77 / 0.73 s    |

Nearly five times between the kitchen and the basement, from nothing but the
material on each surface.

The basement is the interesting one. Its low band rings almost twice as long as
its top — that is what "dull rather than quiet" means, and it is why a carpeted
room sounds different rather than just sounding smaller. Carpet returns 92% of
the lows and 40% of the highs; the simulation is doing nothing more exotic than
believing those numbers.

## Per-source reflections

The shared reverb bus renders one reflection pattern, traced with the source at
your own position. That is exactly right for a clap at your feet and gets worse
the further away a sound is, because which walls answer and how long after the
direct sound they arrive are properties of the path from *that* source to your
ears.

The eight nearest audible sources get their own, traced from where they actually
are. One trace per frame, round-robin, a quarter second apart per source, so the
cost is the same whether three sounds are playing or three hundred. Everything
else keeps the shared pattern, which is a fair approximation and not a failure.

Measured, in a 20 x 6 x 16 tiled room with the source a metre off the back wall:
the direct path is 7.00 m, the path via the wall is 9.00 m, and the reflection
lands **5.8 ms** after the direct sound, arriving from the wall. That delay and
that direction are the whole cue.

## The projectors

One in each room, a metre from the back wall. That gap is deliberate: the direct
sound travels one metre less than the bounce off the wall behind it, so the
reflection lands about six milliseconds later, arriving from the direction of the
wall. That delay and that direction are what place the projector against the
wall rather than merely somewhere in the room.

Stand in front of one, then walk to the side of it, and the early reflections
change while the tail does not.

## What was wrong with it, and for how long

This demo was reported as sounding identical in all three rooms. It did. Three
separate bugs sat on top of each other, and the acoustics were correct
underneath all of them:

**There was no reverb in the build at all.** The environmental reverb bus was
created inside `#ifdef ENJIN_AUDIO_STEAM_AUDIO`, and that CMake option defaults
to `OFF`. So `reverbReady` was false, and every call that fed the bus returned
at its first line: the measured rooms, the early reflections, the Freeverb, and
the `ReverbZone` components that predate all of it. Every scene in this engine
has played bone dry since the feature was written. Twelve test suites were green
over it, because every one of them exercised a DSP class directly and none went
through `AudioEngine`, which is the one place it was switched off.

**The listener never moved.** miniaudio's spatializer follows the active camera,
so panning and distance always worked. Everything else -- room measurement,
occlusion, reverb zones -- read an `AudioListenerComponent` and fell back to the
world origin when a scene had none, silently. Nothing requires that component.
So the room being measured was always the one at (0,0,0) no matter where you
walked.

**The early reflections were never connected.** The image-source tracer and its
per-band material gains existed, and were tested, and nothing called them. The
live reverb node had a diffuse tail and a single pre-delay, which is why rooms
could only differ by decay time -- the same wash, longer or shorter.

All three are fixed, and each has a test at the seam it broke at rather than one
layer down.

## Things it will teach you that are true and surprising

**A bare tiled box rings for about twelve seconds.** The first version of this
scene made each room out of a single material, and the tiled one measured 8.8 s
against the eight second measurement ceiling. That is not a bug — tile absorbs
1%, Sabine says twelve seconds, and an empty tiled pool hall really does sound
like that. It is just not a kitchen. Real rooms are made of several things.

**One bounce off carpet is barely quieter than one bounce off tile.** In the
mids, heavy carpet absorbs 0.30 of the energy, so it returns sqrt(0.70) = 0.84
of the amplitude against tile's sqrt(0.99) = 0.995 -- about 16% down. Measured
off the floor here: 0.261 against 0.311. What a single reflection *does* carry
is the top end, where tile absorbs 0.02 and carpet absorbs 0.60. The five-fold
difference in the table above lives in the tail, because a tail is a hundred of
those bounces multiplied together, and 0.99^100 against 0.70^100 is everything.
That is why reverb time is the headline number and a single echo is not.

**Open doorways couple rooms into one space.** The same first version joined the
rooms with bare concrete corridors, and all three measured nearly the same:
sound left through the doorways, rang around the building, and came back. That is
real — it is called coupled-room decay, and it is why the corridors here are
carpeted and long and the doorways are narrow.

Both of those were found by a test that measures this scene and checks it still
demonstrates what it claims (`Tests/Unit/Acoustics/TestRoomAcousticsDemo.cpp`).
A demo that quietly stops demonstrating its point is worse than no demo, because
it becomes evidence for something untrue.

## Rebuilding the scene

`build_scene.py` generates `scenes/Main.enjin`. Edit the materials there and run
it to hear a different building.

It regenerates the WHOLE scene, including the lighting, so anything tuned by
hand in the editor has to be copied back into the script before rerunning it —
the lamp intensities in there now were set that way. `Tests/Unit/Scripting/
TestExampleScripts.cpp` compiles every script under `Examples/` against the real
bindings, so a demo script calling a function the engine does not have fails in
CI rather than in front of whoever opened the project.
