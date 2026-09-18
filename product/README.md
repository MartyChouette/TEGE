# product/

TEGE documented as a product rather than as a codebase.

`docs/` answers "what does this function do". This folder answers "what is it
like to be the person using this, and does it hold up". Three people, three
journeys, one manual written in their company.

Nothing here replaces `docs/USER_MANUAL.md` (4,600 lines of component
reference). This sits above it. The manual is the thing you hand someone; the
reference is the thing they look up afterwards.

## The three players

The manual is written by walking three specific people through the engine, in
character, rather than by listing features.

| | Who | What the journey has to prove |
|---|---|---|
| **One** | Has shipped games before | That the fast path is actually fast: scripting API within reach, a debugger worth using, and a broken texture or animation fixable in the engine instead of round-tripping to another tool |
| **Two** | Has never opened a game engine | That Creative Mode to a playable level to a link a friend taps on a phone is one continuous path, with no command line and no settings window |
| **Three** | Drives everything with a sip-and-puff switch and a gaze tracker | That every operation is reachable from a dial and from the menus, and that nothing important hides behind a gesture their input cannot make |

Player Two's journey is written in full. One and Three are framed, anchored to
verified engine facts, and not yet written.

## Layout

```
product/
  ENGINE_FACTS.md      every claim the manual is allowed to make, with sources
  manual/              the manual, in Markdown source
  design/
    HOUSE_STYLE.md     the 1993 strategy-guide look. Governs everything we make
    ONE_PAGE_DESIGN.md how we write a design document: one page, drawn, on a wall
    GAMEPLAY_CHARTS.md the seven charts worth having, and the rules for drawing them
    SYMBOL_STANDARDS.md inclusive and international symbol rules, with the numbers
    MANUAL_STUDY.md    what 1990s manuals actually do, read from eight of them
    STUDY_MANUALS_AND_ONEPAGERS.md  both corpora together, and the argument
    STUDY_IMAGERY_AND_ICONOGRAPHY.md  the pictures: depiction levels, tokens, colour, line
    ux-kit/            Inkscape stencil + screen template. Start here to design
    screens/           your UX screens, one file per screen
    diagrams/          editable SVG, authored to open cleanly in Inkscape
      iconography-study.svg            the imagery study, drawn
      manual-contents-mockup.svg       the manual's chapters and sections
      onepager-how-to-make-a-gdd.svg   the method, as a flow chart
      onepager-kettle-hollow-timeline.svg   worked example: a location over time
      onepager-move-set.svg            worked example: a move and attack taxonomy
      onepager-windmill-yard-events.svg     worked example: sequenced vs free events
      creative-gesture / gravity-zone-gap / dial-reach / hover-gap-level
    MILANOTE.md        paste-ready board structure for the UX board
  prototypes/
    product-manual/    the clickable prototype, with its own README
  build/
    make-docs.sh       Markdown to .odt via pandoc
    reference.odt      house style for LibreOffice output
  out/                 generated documents (not tracked)
```

## Everything here is local

These documents are not published anywhere. No artifact links, no hosting. Open
the HTML from disk and the ODT in LibreOffice.

## Everything here looks like a strategy guide

`design/HOUSE_STYLE.md` is the project's visual direction, and it is not limited
to this folder. Notes, prototypes and diagrams all get it.

The short version: we spend our days reading audit tables and gap lists, and that
material is inert. Read enough of it in plain grey Markdown and the energy of the
thing goes missing from the documents about the thing. A game engine's documents
should look like they came from the industry the engine is for.

It also earns its place as signal. A guide from that era already had a visual
vocabulary for the three distinctions we make constantly: this works, this will
surprise you, this is missing.

## The rule this folder runs on

> No claim about the engine goes into a manual, a diagram or a prototype unless
> it is verified in `ENGINE_FACTS.md`, with the source it was read from.

A manual that guesses is worse than no manual, because a person trusts it and
then loses an afternoon. When something cannot be verified it goes in the gaps
section as a gap, not into the prose as a maybe.

## Why prototypes and not only Markdown

`_docs_internal/CREATIVE_MODE_UI.md` already settled this for the editor, and the
same reasoning applies to documentation:

> The bottleneck is deciding, not drawing. The design gets settled somewhere
> cheap and only then written in C++. The agreed mockup becomes the spec.

So the interface work in this folder is done as a clickable prototype and as
editable vector diagrams, not as a description of a picture. A description of a
picture cannot be reacted to.

## Generating the documents

Source of truth is Markdown, so the text diffs in git. LibreOffice is the reading
and editing format.

```bash
bash product/build/make-docs.sh
```

Writes `product/out/TEGE-Manual.odt`. Needs pandoc on PATH; LibreOffice opens the
result directly. Re-run it after editing anything in `manual/`.

## Editing the diagrams

`design/diagrams/*.svg` are plain SVG with named Inkscape layers and live `<text>`
elements, not outlined paths. Open, edit, save, and the diff stays readable. Keep
text as text: the moment a label becomes a path, the diagram stops being
correctable by anyone but the person who drew it.
