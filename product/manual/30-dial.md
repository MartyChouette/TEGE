# Player Three: drives the editor with a switch and a gaze tracker

**Status: framed, not written.** The anchors below are verified against the
engine, including the part where the engine does not currently hold up.

They use a sip-and-puff switch and an eye tracker. Sip and puff gives them four
distinct signals: soft sip, hard sip, soft puff, hard puff. Gaze gives them a
pointer that is fast to move and imprecise to hold, with a dwell time that turns
looking into clicking.

The question their journey has to answer: **is every operation reachable, and is
anything important hiding behind a gesture they cannot make?**

The honest answer today is that the support is real and the reach is not
finished. Both halves belong in the chapter.

## What is genuinely there

`AlternativeInputManager` is not a stub. Five device types, each with a real
configuration:

| Device | What is configurable |
|---|---|
| SwitchAccess | Scanning on or off, 0.5 to 5.0 seconds per element, start delay, 1 to 4 switches, reverse on wrap, per-switch key mapping |
| EyeTracking | Dwell 0.3 to 3.0 seconds, smoothing, dead zone in pixels, a visible gaze indicator and its size |
| SipAndPuff | Soft and hard thresholds in both directions, giving four separate signals |
| HeadTracking | Sensitivity, smoothing, dead zone in degrees, invert per axis |
| VoiceControl | Present as a type |

A `ScanTarget` carries a label, a **group** for hierarchical scanning, screen
bounds and an activate callback. The group is the important field: flat scanning
through a hundred targets at 1.5 seconds each is unusable, and hierarchical
scanning is what makes a large interface tractable.

Around it: text scale pushed through one function that knows every system that
draws text, an announcer, subtitles, colourblind palettes, a dyslexia-friendly
font, and a project's accessibility defaults shipping inside an exported game so
a player inherits sensible settings rather than a blank slate.

The scan speed and enable flag come from the same settings the rest of the engine
reads, and the editor's scan target list is kept separate from the game's on
purpose. Application chrome and game interface are different lists because they
are different jobs.

## Where it does not hold up

> **THE ENGINE OWES YOU ONE**
> The editor has a quick-access dial. Four of them: Tools, File, Play and Create,
> each holding five or six operations, always drawn in the centre of the screen.
>
> It is close to the ideal interface for this player. Few targets, large targets,
> the same place every time, no travel across the screen, no precision needed to
> land on one.
>
> They cannot open it. It opens on a gamepad button, aims on the right stick, and
> nothing else in the editor reaches it.
>
> Written up as gap G2 in `product/ENGINE_FACTS.md`.

The gesture is the deeper half of the problem, and it is the part that makes this
a design question rather than a wiring question.

The dial is **hold, aim, release**. That asks for a sustained hold and an
analogue aim at the same time. A binary switch cannot produce that combination at
all, and a gaze pointer can aim beautifully while having no concept of holding.

So routing the existing gesture to another device does not fix it. The dial needs
a second gesture that reaches the same operations:

- **Open on a discrete signal.** A hard puff, a dwell on a corner, a scan
  selection. Not a hold.
- **Select by scanning or by dwell**, with the sectors as ordinary scan targets
  carrying their own labels, so the same dial serves switch users and gaze users
  without either inheriting the other's gesture.
- **Close on the same signal that opened it**, because a gesture that requires
  releasing something is the gesture that is unavailable.

That is worth designing properly rather than patching, which is why it belongs in
`product/design/` as a flow before it belongs in C++.

## The claim to test, not assert

The user's framing for this journey was that everything should be reachable from
a dial and from the menus. The menus half is plausible and needs walking rather
than asserting: the finished chapter has to drive the editor with scanning turned
on and the mouse unplugged, and record where it stops.

Every place it stops is a gap with a name, the way G2 has a name. A chapter that
says the engine is accessible is marketing. A chapter that lists the four places
it stopped is a document somebody can fix the engine from.

## The shape to write it in

One uninterrupted session with the mouse unplugged and scanning on, doing what
Player Two did in chapter one: block out a level, add a menu, build for web.
Record every stop. Then write the chapter around the stops.
