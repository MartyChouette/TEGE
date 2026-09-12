# Room Acoustics

Three rooms, listened to.

They are the same size and shape. They differ only in what they are made of,
and **there is not one reverb setting in this scene** — no `ReverbZone`, no
decay time, no room size. Everything you hear is measured from the geometry you
are standing in.

Walk between them with the projector running and listen to the tail.

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

## The projectors

One in each room, a metre from the back wall. That gap is deliberate: the direct
sound travels one metre less than the bounce off the wall behind it, so the
reflection lands about six milliseconds later, arriving from the direction of the
wall. That delay and that direction are what place the projector against the
wall rather than merely somewhere in the room.

Stand in front of one, then walk to the side of it, and the early reflections
change while the tail does not.

## Things it will teach you that are true and surprising

**A bare tiled box rings for about twelve seconds.** The first version of this
scene made each room out of a single material, and the tiled one measured 8.8 s
against the eight second measurement ceiling. That is not a bug — tile absorbs
1%, Sabine says twelve seconds, and an empty tiled pool hall really does sound
like that. It is just not a kitchen. Real rooms are made of several things.

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
