# Design direction: how we write a game design document

Written 2026-09-17, revised the same day after going through slides from the
one-page design talk. The position follows the one-page design tradition set out
by Stone Librande at GDC 2010 and developed across his work. Sources at the end.

---

## 1. The thesis

**The goal of design is to communicate ideas efficiently. It is not to produce
documentation.**

A document is not an achievement. It is an attempt to get an idea into somebody
else's head accurately enough that they can build it. Judge it only on whether
that happened.

Two observations follow, both uncomfortable. **Long documents do not get read**,
not by busy people, not twice, and almost never by the person implementing the
thing at the moment they implement it. And **wikis break the relationships
between elements**, which is fatal, because game systems are almost entirely
relationships. Split them across pages and the part that mattered is the part
that is now invisible.

## 2. The anatomy of a one pager

This is the layout, and it is worth following closely because every good example
uses it:

```
  Title                              Lots of whitespace
  date
                    +-------------------+        +-----------+
     Callout -------|                   |--------|  Detail   |
                    |       MAIN        |        | inset     |
  +----------+      |    ILLUSTRATION   |        +-----------+
  | Sidebar  |      |                   |            Notes
  | - bullet |      |                   |
  | - bullet |      +-------------------+
  | - bullet |            |
  +----------+       Callout
                       Description
```

Seven parts, each doing a job:

- **Title and date, top left.** Every example carries a date. Not a version
  number, a date. It is how a reader knows whether to trust the page against the
  build.
- **One dominant main illustration.** The page has a subject and you can see it
  from across the room. This is the part that makes it a one pager rather than a
  compressed document.
- **Callouts on leader lines into the illustration.** The note sits where the
  thing is. Nothing is explained in a paragraph that could be labelled in place.
- **A detail inset** for the one thing that needs a closer look, with its own
  notes underneath it.
- **A sidebar of bullets** for the rules that are not spatial.
- **A description** under the illustration, short.
- **Whitespace, deliberately.** The reference layout labels it as a feature.
  Space is where somebody writes on the printout.

## 3. The archetypes

Going through the examples, the same small number of page types keep recurring.
Pick the one that matches the question rather than inventing a layout.

| Archetype | Looks like | Use it for |
|---|---|---|
| **Screen flow** | Every screen drawn small, arrows for every transition, the whole flow on one sheet | Menus, onboarding, anything with modes. One example covered an entire login and account system including every error state |
| **World structure** | An exploded stack of levels with an annotation band down the side | Level progression, what changes floor to floor |
| **Annotated map** | A map with a roster box per area, connected by leader lines | Content distribution. What lives where |
| **Level sheet** | One level's map, numbered locations, a spawn list per location, objectives down the right | A single level, start to finish |
| **Interior plan** | A floorplan with an inset showing a step sequence | A small space with a secret or a specific interaction in it |
| **Taxonomy sheet** | A grid of variants, each with a tiny diagram and its own parameter list | Defining a vocabulary. Attack delivery types, movement types, damage shapes |
| **Ability spec** | One ability, described, with a three panel diagram showing it over time | A single mechanic in detail, including the UI for it |
| **Storyboard grid** | The same frame repeated across a grid, banded by phase | An arc over time. A creature growing up, a session, a tutorial |
| **Module map** | Every module of the game, how they connect, with the click rules alongside | The shape of the whole product |
| **Constraint triangle** | Three opposed attributes, archetypes placed between them | Factions, classes, anything defined by trade-offs |

## 3a. Worked examples, ours

Four sheets in `diagrams/` are original pages built with these archetypes, and
they are the fastest way to see the form rather than read about it:

| Sheet | Archetype | What it demonstrates |
|---|---|---|
| `onepager-how-to-make-a-gdd.svg` | Relationship | This method, drawn as a flow chart with two exits |
| `onepager-kettle-hollow-timeline.svg` | Map plus timeline | A whole location against a gameplay-minutes ruler |
| `onepager-move-set.svg` | Taxonomy sheet | Ten moves as source, target and the shape between |
| `onepager-windmill-yard-events.svg` | Level sheet | Fourteen events, and which of them have an order |

`manual-contents-mockup.svg` and `iconography-study.svg` are the same discipline
turned on our own documents.

## 4. Devices worth stealing

**Date everything.** Every example is dated. This is the single cheapest habit on
the list and the one that keeps a wall of pages honest.

**Say when a drawing is not to scale.** One map labels itself NOT TO SCALE in the
title block. A drawing that looks measured and is not will be measured by
somebody.

**The vocabulary sidebar.** One faction sheet defines each faction not by stats
but by a short list under fixed headings: traits, materials, effects, sounds,
reference. That is a much better brief for an artist or a sound designer than a
paragraph, and the fixed headings make three factions comparable at a glance.

**Parameters listed beside the diagram, not in prose.** The taxonomy sheets put
the tunable values in a small italic list next to each variant. A reader who
wants to implement it does not have to extract the numbers from sentences.

**A sequence in panels.** The ability sheet shows the mechanic in three panels,
early, middle, late. Anything that changes over time gets panels rather than an
adjective.

**Rules in the middle of the diagram.** The trait triangle puts its governing
rules in the empty centre of the shape, where the eye already is.

## 5. What a TEGE one pager adds

Our documents have a job the general form does not: they must say what is real.
So a one pager here also carries the marks from `SYMBOL_STANDARDS.md`.

Put **verdict marks on the parts themselves**, not in a key: works, will surprise
you, missing. A normal design document describes an intention. Ours describe a
mixture of what exists and what is intended, and **a reader who cannot tell those
apart will build on the wrong one.**

Where reach is in question, put a **reach strip** on the page. And keep the
`ENGINE_FACTS.md` rule: no claim goes on a page unless it was read from source,
with the date it was verified.

## 6. Start on paper

Draw the first version by hand, with the other person in the room, and leave
white space so there is somewhere to write.

Paper has three properties a file does not. Nobody defends a sketch, so it gets
corrected rather than approved. Two people can draw on it at once. And nothing
about it suggests the design is finished.

Move to Inkscape when the argument has stopped moving. `ux-kit/` exists for that
moment and not before it.

## 7. Anti-patterns

**A long document at 6pt.** If it cannot be read standing up at arm's length, it
is not a one pager. Type size is the test.

**A page nobody can be wrong about.** General descriptions of intent cannot be
contradicted, so they never get corrected, so they drift from the build silently.
Specific numbers and drawn relationships can be wrong, which is what makes them
useful.

**The living document.** A page revised forever loses its history and its edges.
Date it, draw the next one when it is wrong, keep the old one.

**Prose where a picture was needed.** A paragraph describing how three things
connect is a diagram that has not been drawn yet.

**One page per game.** The unit is the system, not the product. One project
accumulated over a hundred of these, from early concept through production.

## 8. Where the long documents fit

We keep them, and they are not design documents. `docs/USER_MANUAL.md` at 4,600
lines is a reference, nobody reads it end to end, and that is correct because its
job is to be searched.

**A reference is looked up. A design document is communicated.** Confusing the
two produces a reference nobody can act on and a design document nobody can
search.

---

## Sources

Slides from the one-page design talk, reviewed 2026-09-17 as screenshots supplied
for this purpose. Supporting published material:

- [Video: One-page designs](https://www.gamedeveloper.com/design/video-one-page-designs)
- ['The goal of design is to efficiently communicate ideas'](https://www.gamedeveloper.com/design/-the-goal-of-design-is-to-efficiently-communicate-ideas-)
- [GDC Vault: One-Page Designs](https://gdcvault.com/play/1012356/One-Page)
- [GDC Vault: Simulating a City, One Page at a Time](https://gdcvault.com/play/1017708/Simulating-a-City-One-Page)
- [One Page Design Philosophy](https://www.youtube.com/watch?v=E9_wLks1kAg)
