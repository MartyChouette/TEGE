# Design direction: how we make gameplay charts

Written 2026-09-17, revised the same day after going through slides from the
one-page design talk. Companion to `ONE_PAGE_DESIGN.md`. Same tradition: a chart
is not a picture of data, it is an argument you can point at.

---

## 1. What a gameplay chart is for

A gameplay chart makes a system's **behaviour across a range** visible, so tuning
becomes a conversation about a shape rather than about adjectives.

"Jumping feels floaty" is unarguable and unfixable; two people can hold that
sentence and mean opposite things. A plotted descent curve with real numbers on
it can be pointed at, disagreed with, and changed.

The test:

> Does it turn an adjective into a shape somebody can argue with?

If no, write the sentence instead.

## 2. The catalogue

Most gameplay questions land in one of these. Reach for the matching form rather
than inventing one. The first group is measured, the second is structural, and
the difference matters: one is evidence and the other is a proposal.

### Measured

**Progression curve.** Power, cost or difficulty against time or level. Draw
**both** the cost curve and the reward curve on the same axes, because a
progression problem is almost always the gap between them, and that gap is
invisible when the two live on separate charts.

**Pacing chart.** Intensity against progress through a level or session. Usually
drawn by hand as a statement of intent, which is legitimate as long as it is
labelled as intent. Pair it with the measured version once the level exists; the
difference between the two is the most useful chart in the set.

**Economy flow.** Where a resource comes from and where it goes. Boxes for
stores, arrows for flows, and **a rate on every arrow**. An unlabelled economy
diagram is a picture of nouns. Sources left, sinks right, loop drawn as a loop,
and if they do not balance the drawing should show that rather than require
arithmetic.

### Structural

**Constraint triangle.** Three opposed attributes on the corners, archetypes
placed according to what they trade away. Heavy against fast against damage;
physical against creative against playful. Extremely good for factions, classes
and creature types, because it makes the trade-off the subject rather than the
stats.

Two devices to copy. Put the **governing rules in the empty centre** of the
triangle, where the eye already is. And give each archetype a **vocabulary
sidebar** under fixed headings, something like traits, materials, effects,
sounds, reference. Fixed headings make three factions comparable at a glance, and
it is a far better brief for an artist than a paragraph.

**Radial progression fan.** Tiers as concentric arcs, branching outward from one
root toward the end archetypes. Shows tier depth and branching factor at the same
time, which a normal tree laid out left to right does not. Use it when the
question is "how many ways through" rather than "what unlocks what".

**Taxonomy sheet.** A grid of variants, each with a tiny diagram and its own
short parameter list. The right form for defining a vocabulary: attack delivery
shapes, movement types, damage volumes. Keep the parameters in a list beside the
diagram, never in prose, so an implementer does not have to extract numbers from
sentences.

**Interaction matrix.** Every ability against every status, every tool against
every surface. The value is not the filled cells. It is the **empty** ones, which
are the combinations nobody considered, and the **duplicated** ones, which are
two things doing one job.

**Storyboard grid.** The same frame repeated across a grid, banded by phase.
Anything that changes over time gets panels rather than an adjective. The ability
sheets in the reference material do this in three panels: early, middle, late.

**Flow map.** The route a player can take. Nodes for places or states, edges for
moves. Mark the critical path, the optional, and the one-way doors.

**State machine.** Every state, every transition, every trigger, and critically
**every state's exit.** A state with no drawn exit is a softlock waiting to be
found by somebody else.

**Module map.** Every part of the product and how they connect, with the rules
for moving between them alongside. One page that answers "what is this thing
made of".

**Reach chart.** Ours. Which input devices can perform which operations. See
`SYMBOL_STANDARDS.md` for the strip form. Most projects never draw this, which is
why most projects cannot answer it.

## 2a. Worked examples, ours

- `diagrams/onepager-move-set.svg` is a taxonomy sheet. Ten moves, each with its
  delivery shape and its parameters listed beside the diagram.
- `diagrams/onepager-kettle-hollow-timeline.svg` is a map with a time axis, the
  fused form, so layout and pacing can be argued against each other.
- `diagrams/onepager-windmill-yard-events.svg` separates sequenced from free
  events by drawing the chain, which makes cuttable content obvious.

## 3. Rules for drawing them

**One chart, one claim, and the claim is the title.** Not "Jump tuning" but
"Hover crosses a 6m gap only above 0.4s hold". A chart titled with a topic has
not decided what it is saying.

**Label the axes and name the units.** Seconds, metres, metres per second, gold
per minute. A number with no unit is decoration.

**Real values across the whole range.** Plot what the system does across the
range a player will meet, not one comfortable value. The interesting behaviour is
almost always at the ends.

**Annotate on the chart, not underneath it.** Callout on a leader line, where the
thing happens. A caption saying "note the dip at 12s" is asking the reader to do
work the drawing should have done.

**Mark the tuning knobs.** Show which numbers are fields somebody can change and
their current values. A chart of behaviour with no visible controls says what is
happening but not what to do about it.

**Date it, and say whether it is intent or measurement.** Intent is a proposal,
measurement is evidence. Mixing them silently is the fastest way to make a team
confident about something nobody checked.

**Say when it is not to scale.** A drawing that looks measured and is not will be
measured by somebody.

**Repeat the unit, never scale it.** Seven things means seven marks, not one mark
at seven times the size. Scaling reads as a different kind of thing, not more of
the same thing.

**Never colour alone.** Green and red measure 1.01:1 against each other in this
palette, so any chart separating two series by hue alone separates them by
nothing once printed or seen by a colourblind reader. Use the screened hatch, a
dash pattern, or a shape on the line. Colour is the second channel, never the
first.

**Put it where the decision is.** On the wall, in the one pager, beside the panel
it describes. A chart in a folder is a chart nobody consults.

## 4. What a gameplay chart is not

**Not a dashboard.** A dashboard monitors a running thing; a chart argues for a
change. If it has eight panels it is a dashboard and it has stopped arguing.

**Not a screenshot of a spreadsheet.** A table is for looking values up, a chart
is for seeing a shape. Pasting a grid of numbers gives you neither.

## 5. Four charts this project should draw next

Each answers a question we currently answer with a paragraph, or not at all.

**Switch-access reach time.** Scan speed against number of targets, plotted as
time to reach an operation. `AlternativeInput.h` allows 0.5 to 5.0 seconds per
element, so at the default 1.5s a flat list of forty editor targets takes a
**minute** to cross. That one chart is the whole argument for hierarchical
scanning and for the dial, it would tell us what the group sizes should be, and
it is drawn nowhere.

**Hover budget against gap width.** The curve behind gap G1. How long a hover
must last, and how hard the descent clamp must be, to cross a gap of a given
width at a given run speed. Until it exists, "how floaty" is an adjective.

**Gravity zone reader matrix.** Not a curve, a four cell grid: zone mode against
consumer. `Directional` and `Point` against the Jolt backend and
`ControllerSystem`. Three cells work and one does nothing. Gap G4 took a session
to find and would be a glance on that grid. **The fastest version of a bug report
is often a four cell matrix.**

**Touch scheme fingerprint.** The inputs that rebuild the touch scheme drawn as a
flow: controller preset, plus each `ActionTriggerComponent` button, plus project
overrides, into the built scheme and the controls hint. It explains why dropping
a component into a scene makes a button appear, which currently surprises people
who then assume it is a bug.

A fifth, once the engine grows: a **constraint triangle for the three players**.
Veteran, first-timer and switch user pull the editor in different directions, and
a feature that serves one often costs another. That trade-off is currently
implicit and would be better drawn.

## 6. How we make them

1. **Sketch on paper.** Axes and the shape of the curve, by hand, before any
   numbers. If the shape is not interesting the chart is not worth making.
2. **Get the real numbers** from source or measurement, and record where they
   came from. `ENGINE_FACTS.md` applies: a chart is a claim.
3. **Draw it in Inkscape** from `ux-kit/`, in the house palette, with the
   screened hatch for series separation and the symbol set for any marks.
4. **State the claim in the title**, and put the date and source in the corner.
5. **Put it on the one pager** for the system it belongs to.

A chart generated from data should still be re-authored into house style before
it goes on a page. A default plot carries its tool's opinions, not ours, and it
will not sit next to the rest of the work.

---

## Sources

Slides from the one-page design talk, reviewed 2026-09-17 as screenshots supplied
for this purpose. Supporting published material:

- [Video: One-page designs](https://www.gamedeveloper.com/design/video-one-page-designs)
- ['The goal of design is to efficiently communicate ideas'](https://www.gamedeveloper.com/design/-the-goal-of-design-is-to-efficiently-communicate-ideas-)
- [GDC Vault: Simulating a City, One Page at a Time](https://gdcvault.com/play/1017708/Simulating-a-City-One-Page)
- [One Page Design Philosophy](https://www.youtube.com/watch?v=E9_wLks1kAg)

The four charts in section 5 are ours, drawn from findings in `ENGINE_FACTS.md`,
and none of them exist yet.
