# Prototype: product manual

## Hypothesis being tested

That TEGE's documentation is better as **three roleplayed user journeys than as a
feature list**, and that writing it that way finds real engine defects.

Two claims, and the second is the one worth testing, because a manual that only
restates the code is cheap to write and worth little. If walking a specific
person through a specific task surfaces bugs that audits missed, the format pays
for itself and should be extended to the other two journeys.

A third, smaller claim rides along: that a clickable prototype is the right
medium for deciding this, rather than a written description of one. That is the
method `_docs_internal/CREATIVE_MODE_UI.md` already settled for editor UI, and
this applies it to documentation.

## How to run it

It is a single self-contained HTML file. No build, no dependencies, no server.

```bash
start product/prototypes/product-manual/index.html     # Windows
```

**Local only. Not published anywhere.**

Fonts come from Google Fonts and the page falls back to Arial Narrow, Impact and
a system monospace with no layout damage when offline. Nothing else is fetched.
Single theme on purpose, per `product/design/HOUSE_STYLE.md`: a printed guide has
one ground.

This page is also the **reference implementation of the house style**. New HTML
prototypes copy its token block and device classes rather than inventing a look.

## Status

**In progress.** First pass, 2026-09-17.

- Player Two, the first-timer, is written in full, seven steps.
- Players One and Three are framed against verified engine anchors, not drafted.
- Three diagrams drawn. Source SVGs in `product/design/diagrams/`.

## Findings so far

The second claim is holding up. Writing one journey found four gaps, and one of
them is a live engine bug that existing audits missed.

**G4, directional gravity zones do nothing to a character controller.** Found by
looking for a components-only way to author a hover pocket, which is exactly the
kind of question a feature list never asks. `JoltBackend::ApplyGravityZones`
honours the component; `ControllerSystem` filters it out unless the mode is
`Point`. `Directional` is the default. So the default configuration of the
component does nothing, silently, for the controller every platformer uses.

It survived the silent stub audit because it is not the shape that audit swept
for. The value **is** read. It is read by a different system than the author was
aiming at, and no amount of grepping for unread setters finds that.

**G1, no hover on the character controller**, and no scripted way in either,
since velocity is read-only across all nine `Controller_` bindings.

**G2, the quick-access dial is reachable by one input device.** The gesture is
the deeper half: hold-aim-release needs a sustained hold and an analogue aim at
once, which is the one combination a binary switch cannot make.

**G3, the two overlapping creative systems**, already known, confirmed as
something a first-timer walks straight into.

All four are written up with sources in `product/ENGINE_FACTS.md`.

### What this says about the format

The gaps clustered where a journey has to **cross between systems**: component to
system, editor to runtime, input device to interface. Both G2 and G4 are wiring
failures between two things that each work. A per-system audit cannot see those,
because each side passes its own review.

That suggests the remaining two journeys are worth writing, and worth writing as
uninterrupted sessions rather than as tours.

## Open questions for review

1. Is the manual voice right, or too playful for the audience?
2. Should the gaps live in the reader-facing manual at all, or only in the
   internal file? Current bet is that they belong in both, and that admitting
   them is what makes the rest trustworthy.
3. Player Three's journey needs a real session with the mouse unplugged and
   scanning on. Nobody has run one. Everything written about it so far is read
   from source, not observed, and it is labelled as such.

## If this concludes successfully

Per the prototype rules, this page is not migrated. The findings inform the real
thing: the Markdown in `product/manual/` is already the source of truth, and
`product/build/make-docs.sh` already produces the LibreOffice document. This
prototype exists to decide the shape, not to become the artifact.
