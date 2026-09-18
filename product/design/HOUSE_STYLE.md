# House style: 1993 strategy guide

**This governs every note, document, diagram and prototype we make, not just the
manual.** Decided 2026-09-17.

## Why a style rule at all

The reason is not decoration, and it is worth writing down because it is the part
that gets forgotten first.

We spend most of our time staring at audit tables, gap lists, backlog rows and
enum dumps. That material is inert. Read enough of it in plain grey Markdown and
you stop being a person making a game engine and start being a person maintaining
a spreadsheet. The energy of the thing goes missing from the documents about the
thing.

A game engine's documents should look like they came from the industry the engine
is for. So: **a 1993 strategy guide.** Four-colour on coated stock, loud banners,
boxed tips, hard drop shadows, numbered walkthrough panels.

Two things it buys us. **Tone**, because the project reads as what it is instead
of as enterprise software. And **signal**, because a guide from that era had a
visual vocabulary for exactly the distinctions we make constantly: this works,
this will surprise you, this is missing. Those were tip boxes and warning boxes
long before they were callout components.

## Colour

Four-colour process. Signals are semantic and carry meaning on their own; nothing
is coloured to look lively.

| Token | Hex | Means |
|---|---|---|
| `--stock` | `#FDFBF4` | Coated paper. The reading ground |
| `--stock-2` | `#F0EBDD` | Tint block behind secondary content |
| `--ink` | `#16161A` | Text, and every border |
| `--ink-2` | `#5A5A63` | Captions and deck copy |
| `--navy` | `#12233D` | Masthead, section banners, code blocks |
| `--red` | `#D62828` | **GAP.** The thing that is missing |
| `--gold` | `#F2B417` | **CAUTION.** Present, and will surprise you |
| `--green` | `#17804A` | **GO.** Verified, works as described |
| `--blue` | `#0B5FCE` | Navigation and links only |

Red, gold and green are the same values in the diagrams as on the page, so a
diagram lifted out of a document still means what it meant inside it. If you add
a fourth signal you are probably making a distinction the reader does not need.

## Type

| Role | Face | Fallback | Notes |
|---|---|---|---|
| Display | Anton | Arial Narrow, Impact | Banners, panel headers, step titles. Uppercase, skewed -7 degrees on banners. Never below 15px |
| Body | Archivo | Helvetica Neue, Arial | 400 for text, 800 for emphasis. Italic exists and is used sparingly |
| Utility | JetBrains Mono | Consolas | Every engine noun a reader will type or search for literally |

For LibreOffice documents the display face falls back to **Impact** and the body
to **Arial**, because those are installed on the machine and Anton is not. The
document keeps the shape of the style even where it cannot keep the exact face.

The mono face earns its place more here than in most documents. This project is
full of things a reader will type or grep for. `GravityZoneMode::Directional`,
`.enjinproject`, `Ctrl+P`. Set one of those in the body face and it stops being
quotable.

## The devices

**Hard shadows, never soft.** `6px 6px 0 var(--ink)`, no blur, no alpha. Print
cannot blur, and the moment a shadow gets soft the whole thing reads as a modern
web page wearing a costume.

**Every box has a 3px ink border.** That is the single most characteristic mark
of the era, and it is what holds a loud palette together.

**Section banners are skewed navy bars** with the heading knocked out in white.
Red for a walkthrough, gold for findings, navy for everything else.

**Callout panels are one object.** A coloured header strip with an uppercase
label, a white body, an ink border, a hard shadow. The strip colour and the label
are the only things that change between TRY IT, WATCH OUT and THE ENGINE OWES YOU
ONE. One varying attribute is what lets a reader tell them apart in peripheral
vision while flipping.

**Walkthrough steps get a knockout numeral** in a red square against the title
bar. Numbering is only allowed where the content genuinely is a sequence. A gap
list is not a sequence, so gaps get an ID badge instead.

**Tables are boxed and zebra-striped**, with an ink header row and knockout
display type.

## Diagrams

Same palette, same weights. 3px strokes, 4px on the emphasised element, flat
colour fills with knockout text, no gradients and no rounded corners.

Plain SVG with named Inkscape layers and live `<text>`. Text stays text: the
moment a label becomes a path, the diagram stops being correctable by anyone but
the person who drew it.

Green fills the element that works, red outlines and dashes the element that does
not, and a red cross sits on the connection that is missing. A diagram should be
readable as a verdict before it is read as a mechanism.

### Draw the world, not only the call graph

A boxes-and-arrows diagram is right for a data path. It is wrong for anything a
player would recognise, and most of what we document is the second kind.

When the subject is a level, a control scheme or a thing somebody does, draw the
thing. The era's guides did this constantly and it is why they are still readable
thirty years later.

- **Isometric cutaways** for level geometry. Three flat tones per block, light on
  top, mid on the front, dark on the side, 3px ink outline. Parallelograms, not
  perspective.
- **A player figure** stands in the scene to give it scale and a point of view.
  There is one in the stencil. Reuse it rather than redrawing.
- **Numbered callout circles with leader lines**, red for a problem, blue for an
  explanation, green for a reward. The number is only meaningful if the callouts
  are read in order, so number left to right along the path the player takes.
- **Motion is drawn.** Wind is a wavy line with an arrowhead, a jump is a dashed
  arc, a gap gets a dimension line with end ticks and a label in a box.
- **Verdict badges** sit over the drawing, not beside it. A red plate reading
  HOVER: NOT AUTHORABLE with the gap ID on it is the fastest way to say that
  everything in the picture works except one thing.

`design/diagrams/hover-gap-level.svg` is the reference for this. Copy it.

## Four rules taken from the study

From `MANUAL_STUDY.md`, which read eight scanned manuals to get them.

**Depth is carried by a coloured bullet, not by type size.** Navy numbered square
for a chapter, blue square for a section, gold square for a step. A long heading
at one level and a short one at the next look alike when only size separates
them, and a reader who opens the page in the middle is then lost.

**A frame means it is real.** Screenshots, captures and interface mockups get a
3px border. Explanatory drawings float unframed. A reader learns the rule in two
pages and never confuses a diagram for a screenshot again.

**The number of labels picks the callout device.** Up to three, draw arrows onto
the picture. Four to six, put labels outside the frame with leaders pointing in.
Seven or more, use a numbered ring down both sides with a numbered legend below.
Below three a legend is ceremony; above six, arrows on the image are spaghetti.

**Every page carries a running head and a page number.** Section name at the top
outer corner, page number in a chip at the foot. Cross-references go inline and
parenthetical, right where the reader is, not gathered at the end.

## Single theme, on purpose

No dark mode. A printed guide has one ground, and every colour here is painted
explicitly so a page holds up wherever it lands. This is a choice rather than an
omission, and it only applies to documents. It says nothing about the editor,
which has its own theming problem and a real sRGB trap underneath it.

## What this deliberately is not

Cream stock with a serif and a terracotta accent. Near-black with one acid pop.
Hairline rules and dense broadsheet columns. A gradient hero. Inter or Space
Grotesk. Emoji as section markers. The same rounded card with an accent bar
repeated down the page.

Those are the looks that arrive when nobody chose one. We chose one.

## Applying it

- **HTML prototypes** copy the token block and the device classes from
  `product/prototypes/product-manual/index.html`. That page is the reference
  implementation.
- **LibreOffice documents** get it from
  `product/build/reference.odt` via `make-docs.sh`. Do not hand-style an ODT.
- **Diagrams** copy an existing file in `design/diagrams/` rather than starting
  from a blank Inkscape canvas.
- **Plain Markdown notes** keep the vocabulary even without the colour: the three
  callout labels, the gap IDs, the numbered walkthrough steps. A note written
  this way converts to the full treatment without being rewritten.
