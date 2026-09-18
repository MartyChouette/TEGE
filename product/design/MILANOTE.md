# Milanote board: TEGE product and UX

Structure for the board, written so it can be built by pasting rather than
retyped. Milanote is where the loose work lives: references, marked-up
screenshots, arrangements nobody has committed to yet. This file is the index of
what belongs there, not a copy of it.

The split that keeps the two from rotting:

- **Milanote holds the undecided.** References, screenshots with scribbles on
  them, three arrangements of the same panel, anything still being argued about.
- **`product/` holds the decided.** Once a board column stops moving, it becomes
  a diagram in `design/diagrams/` or a section in `manual/`, and the board card
  gets a link to it.

A card that has been settled for a month and is still only on the board is a
decision nobody wrote down.

---

## Column 1: The three players

One card per player. Keep them short enough to read at a glance on the board.

**Player One, has shipped games before**
Wants the fast path. Scripting API within reach, a debugger worth opening, a
broken texture fixed in the engine instead of round-tripped. The measure is how
many times the engine makes them leave it.

**Player Two, never opened a game engine**
Creative Mode to a playable level to a link a friend taps on a phone, with no
command line and no settings window. The measure is whether that path has a break
in it.

**Player Three, sip-and-puff switch and gaze tracker**
Every operation reachable from a dial and from the menus. The measure is where a
session with the mouse unplugged actually stops.

## Column 2: Screens to mark up

Screenshot each, then scribble. Per `_docs_internal/CREATIVE_MODE_UI.md`, marked
up screenshots are the fastest way to say what is wrong.

- Creative Mode rail, all thirteen tools, at 100% and at 190% UI scale
- The Build Game dialog, Platform set to Web
- The four radial dials, one card each
- Pause menu and the save list, including the empty state
- Touch overlay in Simulate Touch Controls
- Inspector on an entity with a `GravityZoneComponent`

The 190% card matters more than it looks. Hand-drawn editor surfaces do not
inherit the UI scale automatically, and a fixed list of rows at 190% can be
taller than its window, so whatever was pinned to the bottom is what goes
missing.

## Column 3: The five questions, per screen

From the method doc. Answer these on a card per screen and the layout mostly
falls out.

1. Who is at this screen and what are they trying to finish?
2. What must be visible without a click?
3. What is allowed to be one click away?
4. What must never steal focus or interrupt?
5. How do they know it worked?

Question five is the one that gets skipped and the one that makes software feel
broken.

## Column 4: References, each with a reason

Not "make it like Blender". The reason transfers, the look does not. Every
reference card needs a second line saying what specifically is being borrowed.

## Column 5: Open gaps

One card per gap in `product/ENGINE_FACTS.md`, linked back to it. Currently G1
hover, G2 dial reach, G3 overlapping creative systems, G4 directional gravity
zones. A gap card gets closed on the board only when the engine changes, never
when the document is updated.

## Column 6: Dial redesign

The live design problem, and the one worth doing on a board before anyone writes
C++.

The dial is hold, aim, release. That needs a sustained hold and an analogue aim
at the same time, which is exactly what a binary switch cannot produce and what a
gaze pointer has no concept of. Routing the existing gesture to another device
does not fix it.

Arrangements to try as separate cards:

- Open on a discrete signal, a hard puff or a corner dwell, not a hold
- Sectors as ordinary scan targets, so switch scanning walks them with labels
- Dwell to select for gaze, with the dwell time already configurable
- Close on the same signal that opened it, since releasing is the unavailable part

## Uploads

`design/diagrams/*.svg` drop straight onto a board and stay editable in Inkscape
afterwards. Upload rather than screenshot, so the board and the repo do not
diverge into a picture and a source that disagree.
