# Sprite Pivot & Rotation

A check you can read at a glance. Open `SpritePivot.enjinproject` and press
Play, or just look at the scene in the editor viewport.

Every sprite here is untextured and tinted, so nothing you see comes from an
image file. The geometry is the whole point.

The small **white square** on each sprite marks that entity's **origin** — the
position in its `TransformComponent`. Where the coloured bar sits relative to
that white square is what the pivot decides.

## Top row: pivot

Three identical sprites. Same size (2.2 x 4.0), same rotation (35 degrees),
laid out along the row. The only difference is `pivot`:

| Colour | Pivot | Where the origin lands |
|--------|-------|------------------------|
| Green  | `0, 0`     | bottom-left corner of the bar |
| Red    | `0.5, 0.5` | dead centre |
| Blue   | `1, 1`     | top-right corner |

The bars sit in three visibly different places relative to their white dots.

**If all three looked the same, `pivot` is being ignored.** It used to be: the
field existed, the inspector edited it, and nothing read it. Anything authored
against that behaviour was compensated for by moving the entity instead, so a
scene fixed that way will have shifted when the pivot started working.

## Bottom row: rotation of a non-square sprite

The same 2.2 x 4.0 bar at 0, 30, 45, 60 and 90 degrees.

Read the last one. At 90 degrees a bar that is 2.2 wide and 4.0 tall has to
come out **4.0 wide and 2.2 tall**. It does.

**If all five drew upright and identical, the sprite is keeping an
axis-aligned silhouette** and only its contents are turning. That was the old
behaviour, and it is the reason this row exists: a rotated square sprite looks
correct under both the bug and the fix, so a square would prove nothing.

## Why this scene is shaped like this

Both fixes are the kind that a screenshot of a normal scene cannot confirm.
Each row is built so the broken version and the working version look
*obviously* different rather than subtly different, and every sprite carries
its own origin marker so you are comparing against a known point instead of
against your memory of where it used to be.
