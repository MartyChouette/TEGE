# Hand IK

A first-person hand, laid on things.

Walk along the counter. The fingers settle onto the worktop, each stopping where
its own length reaches, and let go when you walk away.

## Why first person

Because it is the case where the answer cannot be faked. In third person a hand
near a counter reads as "about right" from two metres away. Held up in front of
your face, a fingertip floating a centimetre off the worktop is obvious, and so
is one that has sunk through it.

## What to look for

| Where | What should happen |
|---|---|
| Along the counter | Fingers stop **on** the worktop. The short ones stop higher |
| The gap between the two counters | Fingers **hang**. They do not snap to a surface that is not there |
| The shelf lip | Fingers curl **over** the edge rather than pressing onto it |
| The railing | A different height, taken without dragging the old pose along |
| Walking away | The hand releases rather than staying stuck |

## The placeholder

There is no rigged character in this repo, so the hand is generated: 21 bones
and one box per bone, rigidly weighted. It looks like a hand drawn by somebody
who only owns boxes.

That is enough for what is being shown. The question is whether five fingers of
different lengths settle at the right heights, and a box answers that as clearly
as a sculpted mesh. Real art drops in by replacing the skeleton and mesh and
keeping the bone names, or by importing a rig and typing its own bone names into
the Hand IK inspector.

## The numbers, which are arithmetic rather than taste

```
eye      player 0.90 + camera 0.78          = 1.68
hand     eye - 0.61                         = 1.07
counter  0.95 centre + 0.08/2 slab          = 0.99
gap                                         = 0.08

finger reaches: Thumb 0.080  Index 0.086  Middle 0.095  Ring 0.087  Little 0.070
```

The gap sits **inside** the spread of finger lengths on purpose. Index, Middle
and Ring reach it; Little does not; Thumb is exactly on the line. A hand parked
where all five reached would look identical whether the solve were per-finger or
a single canned pose, and would prove nothing.

Measured by `Tests/Unit/Animation/TestHandIKDemo.cpp`, which reads the rig's own
bone positions and the counter's own height out of this scene, so moving either
moves the expectation with it.

## Rebuilding

`build_scene.py` generates `scenes/Main.enjin`, including the skeleton and the
skinned mesh. Edit the finger table at the top to change the hand.
