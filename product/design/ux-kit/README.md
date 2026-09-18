# UX kit: how you design a screen

This is the answer to "I need a way to create UX myself." It is built for
Inkscape, because that is what is on the machine, and it is built so that
designing a screen is **copy, paste, retype** rather than draw from scratch.

## The four minute version

1. Open `TEMPLATE-screen.svg`. **Save As** under the screen's name into
   `product/design/screens/`.
2. Answer the five questions at the bottom of the page. Do this before drawing.
   It takes a few minutes and it decides most of the layout.
3. Open `STENCIL.svg` in a second window. Select a part, Ctrl+C.
4. Back in your screen, Ctrl+Alt+V (**Paste In Place**) or Ctrl+V. Move it,
   double-click the text, retype it.
5. Add numbered callouts on the Callouts layer explaining anything non-obvious.
6. Save. Drop it on the Milanote board if it is still being argued about.

That is the whole loop. No new tool to learn, no font to install, no build step.

## Why the questions come first

From `_docs_internal/CREATIVE_MODE_UI.md`, which settled the method for this
project:

> The bottleneck is deciding, not drawing. Every UI iteration written in C++ costs
> a five minute build. Every iteration in a mockup costs seconds.

And the jump that matters:

> **Verbs before layout.** "In creative mode someone should be able to block out a
> level and press play without ever opening a settings window." One sentence like
> that constrains the design more than a picture does, and it is the part that
> cannot be guessed.

So the template puts the questions on the page, in ink, where they cannot be
skipped. Question five is on its own in red because it is the one everybody
skips: **how does the person know it worked?** Software that never answers that
feels broken even when it is working.

## The eight files

| File | Holds | Reach for it when |
|---|---|---|
| `STENCIL.svg` | Interface parts: buttons, panels, the rail, the dial, touch, accessibility | You are drawing what the software looks like |
| `STENCIL-page.svg` | Page devices: numbered callout rings, arrow chains, sidebars, tables, page furniture | You are building a page that explains something |
| `SYMBOLS.svg` | Verdict marks, depth bullets, input devices, engine nouns, the reach strip | Always. These go in everything |
| `TEMPLATE-screen.svg` | The blank page with the five questions on it | Starting a new screen |
| `SYMBOLS-VARIANTS-A.svg` | Verdicts and input devices, five approaches each | Choosing a mark |
| `SYMBOLS-VARIANTS-B.svg` | Engine nouns and structural marks, five each | Choosing a mark |
| `SYMBOLS-VARIANTS-C.svg` | Three whole design languages: woodcut, screened, seal | Choosing a SET |
| `SYMBOLS-VARIANTS-D.svg` | Four more per row, including four more gamepads | Still choosing |

**Picking from the sheets.** Every row is numbered the same across A, C and D, so
a choice is named rather than described: `1A`, `6K`, `9C`. Sheet C is the one to
decide first, because its columns are languages rather than single marks, and the
language settles most of the others.

Two cells are flagged and should not be chosen: `2D` and `2E` fail non-text
contrast at 1.79:1 against a 3:1 requirement, and `7C` is a thumb gesture, the
highest-risk pictogram class there is. See `../SYMBOL_STANDARDS.md`.

`../MANUAL_STUDY.md` and `../STUDY_IMAGERY_AND_ICONOGRAPHY.md` are where the
devices in these files came from.

## What is in the interface stencil

Five layers, toggled in Inkscape's layer panel so you can hide what you are not
using.

| Layer | Parts |
|---|---|
| A Controls | Button in four states, checkbox, slider with a value readout, dropdown, inspector field row |
| B Panels | Dockable panel with title bar and a selected row, tab strip, callout panel, modal, and an **empty state** |
| C Editor surfaces | Creative rail with its three bands, radial dial with one sector lit, viewport frame, menu bar, player figure, wind arrows, isometric ledge block |
| D Touch | Phone in landscape, move-stick zone, action button, controls hint strip |
| D Accessibility | Gaze dwell ring, switch scan focus, numbered callout with leader line, sticky note |

Two of those are opinions rather than parts, and they are in the stencil so they
get copied by accident:

- **The empty state is a component.** It says NO SAVES YET and names the path it
  searched. A blank panel cannot be told apart from a screen that failed to draw,
  which is a mistake this project has already shipped once.
- **The scan focus and gaze ring exist at stencil level**, so a screen gets
  checked against a switch and a gaze pointer while it is being drawn, instead of
  being retrofitted after someone complains.

## House rules for drawing

From `product/design/HOUSE_STYLE.md`. The stencil already obeys them, so copying
keeps you right.

- **Hard shadows, never soft.** The shadow is a second rectangle offset by 6px,
  not a blur filter. Inkscape stays fast and the file stays diffable.
- **3px ink border on every box.** This is what holds a loud palette together.
- **Text stays text.** Never convert a label to a path. The moment you do, nobody
  but you can correct the drawing.
- **Green works, gold will surprise you, red is missing.** Colour is a verdict,
  not decoration. A reader should get the verdict before they read the labels.

## Fonts

The stencil asks for Impact and Arial, which are already on Windows, so it opens
correctly with nothing installed. Anton and Archivo are the web equivalents and
are only used by the HTML prototypes.

If you want the exact web faces in Inkscape, install Anton and Archivo from
Google Fonts and change the `font-family` on a copied part. Nothing breaks if you
do not.

## Where the files go

```
product/design/
  ux-kit/
    STENCIL.svg          interface parts. Copy from it, never draw in it
    STENCIL-page.svg     page devices. Same rule
    SYMBOLS.svg          the chosen marks
    SYMBOLS-VARIANTS-*   the pick sheets, A B C D
    TEMPLATE-screen.svg  the blank page. Save As, never edit in place
  screens/               your screens, one file per screen
  diagrams/              explanatory diagrams, not interface mockups
```

Screens and diagrams are kept apart on purpose. A screen shows what something
looks like. A diagram shows how something works. Mixing them produces a picture
that does neither.

## When a screen is settled

It stops being exploration and becomes a spec. Then:

1. Set the status in the title block to `settled`.
2. Build it as an HTML prototype if it needs to be clicked, copying the tokens
   from `product/prototypes/product-manual/index.html`.
3. The agreed mockup **is** the spec. Implement against it rather than against a
   description of it.

## Extending the stencil

Add the part to `STENCIL.svg`, in the right lettered section, as a group with an
`inkscape:label`. Keep it on the same grid as its neighbours.

A part belongs in the stencil once it has been drawn twice. Drawing it a third
time by hand is how two screens end up disagreeing about what a slider looks
like.
