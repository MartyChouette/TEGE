# Study: what 1990s and 2000s game manuals actually do

Read 2026-09-17 from eight scanned manuals. This is the analysis that
`HOUSE_STYLE.md` and the stencils are built on. Devices only. Nothing here is
traced or copied; the point is the mechanism, not the artwork.

Sources studied closely: the EarthBound Player's Guide (1995, 118pp), the A Link
to the Past manual (1992, 49pp), the We Love Katamari manual (2005, 52pp). Also
on hand: Super Mario Bros 3, StarTropics, Donkey Kong Country, Super Mario World,
Pokemon Red.

A correction worth recording, because it changes how the EarthBound book should
be read: **that guide was the pack-in.** The US release shipped in an oversized
box with the cartridge and all 118 pages together, which is much of why it cost
what it did. Nintendo Power guides were normally sold separately; that one was
not. So it is not an outlier "guide" against seven "manuals". It is a pack-in
manual that happened to be 118 pages long, and it shows what a pack-in can be
when nobody decides it has to be thin.

---

## 1. Flow

### Depth is encoded in colour, not in size

The Link to the Past manual runs a strict three-level hierarchy, and the level is
carried by a **coloured square bullet** rather than by type size:

| Level | Mark | Treatment |
|---|---|---|
| Chapter | numbered | Red display heading on a tinted band, `3. Using the Controller` |
| Section | blue square | Heading with a rule under it |
| Step | gold square | Heading knocked out of a dark navy plate |

The effect is that you can open the book at any page and know your depth without
reading a word. Size alone cannot do that, because a long heading at level two
and a short one at level three end up looking similar.

**Take it.** Our documents have exactly this problem and currently solve it with
type size alone.

### The running head is doing real work

Every page carries the chapter name in small type at the top outer corner, and
the page number in a dark chip at the bottom. A reader who flips into the middle
is never lost. Our HTML has a sticky contents rail, which is the same idea, but
the printed documents have nothing.

### A manual is a network and says so

Cross-references are constant and parenthetical: `(See page 26.)` at the end of
the sentence, not in a footnote, not at the end of the section. The manual admits
it is not a linear read. That is worth copying literally, because our material is
far more cross-linked than a game's is.

### Boxed versus floating is a rule, not a whim

In the Link to the Past manual, **screenshots get a border and illustrations do
not**. Illustrations bleed into the text column, unframed, alternating sides.

That is a clean distinction and it is worth adopting exactly: **a frame means
this is a real thing you will see on screen. No frame means this is a drawing
that explains.** A reader learns the rule in two pages and then never mistakes a
diagram for a screenshot again.

### An opening spread can be pure tone

We Love Katamari opens with two pages of verse set in hand lettering, running
across an illustrated landscape with no grid at all, no instruction, and no
information. Then page six snaps into instructional mode.

It works because it buys patience. By the time the reader hits the controls, they
have already agreed to the book's mood. Worth stealing for a cover spread, and
worth stealing only there; a second tone page would read as padding.

---

## 2. Data layout

Three ways to label a picture, and the choice is governed by count:

| Labels | Device | Where seen |
|---|---|---|
| Up to 3 | Curved arrows drawn straight onto the image, labels placed wherever there is room | Katamari |
| 4 to 6 | Labels outside the frame, straight leader lines pointing in | EarthBound, game screen page |
| 7 or more | Numbered ring down both sides, leaders in, numbered legend in two columns below | EarthBound, status screen page |

That is a real rule and it resolves an argument before it starts. Below three
labels a legend is pointless ceremony. Above six, arrows on the image turn into
spaghetti.

Two more devices worth having:

- **Arrow-chained sequence.** Screens in a row joined by a chunky circular arrow.
  No numbers. Use it when each step follows automatically from the last. Numbers
  imply the reader must do something; the arrow implies the machine does it.
- **Per-column coloured table headers.** Each column gets its own hue rather than
  one header colour for the whole table. On a long chart the eye tracks a column
  down without a ruler.

The Katamari diagrams also do something structurally odd and effective: the
screenshot is clipped to a **wobbly organic shape** rather than a rectangle, and
the numbered marks run across a two-page spread rather than resetting per page.
The clipping is what keeps a dense instructional page from feeling like a
spreadsheet.

---

## 3. Jokes and cuteness

The humour is not decoration laid over the information. It is structural, and it
comes in identifiable forms:

1. **Headings that talk to you.** `Hey! The battle swirl is a different color!`
   and `OK Then, Let's Get Rolling!` are headings written as speech. A heading
   that is a sentence somebody says is warmer than a heading that is a noun, and
   it costs nothing.
2. **Answer a question the reader did not ask, in their voice.** Katamari opens
   by asking why you would roll a katamari and answering, immediately, that it is
   fun. It disarms the question rather than pretending nobody has it.
3. **Enthusiasm used as punctuation.** Repeated exclamation marks as a structural
   beat rather than as emphasis on any particular word.
4. **A mascot with no job.** The little creatures in the Katamari margins are
   spectators. They do not explain anything and they are not the player. They
   react. That is the whole cuteness engine: somebody in the margin having a
   feeling about what you are reading.
5. **Speech bubbles from background figures** saying ordinary, enthusiastic,
   slightly banal things.
6. **A joke that lives in the data.** EarthBound's chart of default names is a
   reference table that is also the funniest page in the book. The humour is in
   the rows, not in a caption about the rows.
7. **Wry admission.** Limitations get named with a shrug rather than buried.

### What of this we can actually use

Points 6 and 7 are ours already and we have not noticed. **Our gap list is the
joke that lives in the data**, in exactly the EarthBound sense: a reference table
that is honest to the point of being funny. `THE ENGINE OWES YOU ONE` is already
a heading written as speech, which is point 1.

Point 4 is the one to add. A margin figure that reacts to a finding, with no
explanatory role, would carry the tone without touching the accuracy of anything.
The player figure in the stencil is currently a diagram element; a second,
reacting figure is a different job.

Points 2 and 3 are available and should be used sparingly. Our subject is a
person losing an afternoon to a silent bug, and three exclamation marks next to
that reads as making light of it.

---

## 4. Palettes

Three registers, thirteen years apart, and the differences are not fashion. Each
suits a different job.

**A Link to the Past, 1992. Earnest instructional.**
Pale blue-grey page tint, white content areas, saturated red for chapter
headings, near-black navy plates for step headings, gold and cyan square bullets,
a yellow tint panel behind the hardware diagram so grey plastic pops.
High contrast, few hues, each one doing a job.

**EarthBound, 1995. Friendly and busy.**
Candy pastels. Lilac, mint, coral, butter yellow, sky blue. Per-letter
multicolour headings with soft glow halos behind them. Screenshots sit on tinted
check patterns. Warm and generous, and genuinely harder to read; the headings
sacrifice legibility for charm.

**We Love Katamari, 2005. Calm and odd.**
Desaturated gouache. Periwinkle, sky, cream, grass green, one hot orange.
Enormous white space, hand lettering throughout, illustrations floating free.
Confident enough to leave two thirds of a page empty.

### Which register is right for us

Ours is closest to **Link to the Past**, and that is the correct answer rather
than a compromise.

The job of our document is to say, unambiguously and at a glance, which of three
states a thing is in: it works, it will surprise you, it is missing. That needs
few hues, high contrast, and each colour doing exactly one job. Our navy, red,
gold and green on cream is that register already.

EarthBound's palette would actively fight us, because a per-letter multicolour
heading has no colour left over to mean anything. Katamari's would undermine the
gap reporting, since a calm pastel page saying a feature is missing reads as not
minding very much.

**Borrow, do not adopt.** Take EarthBound's data devices, take Katamari's opening
spread and its margin mascot, keep Link to the Past's colour discipline.

---

## 5. What this changed

- `HOUSE_STYLE.md` gains the depth-bullet system, the frame rule, the label-count
  rule, and running heads.
- `ux-kit/STENCIL-page.svg` gained the numbered ring, the arrow chain, the
  outside-label pointer, the two-branch sidebar, the per-column table and page
  furniture.
- `ux-kit/SYMBOLS.svg` is new, and is the biggest thing this study produced. See
  the next section.

## 6. The symbol argument

The Link to the Past manual draws the control pad as a **small glyph inside
running prose**, mid-sentence, every time it is mentioned. It is a tiny thing and
it is the most transferable device in any of these books, because it means a
reader scanning a page can find every mention of an input without reading.

We have a much sharper need for this than a game does, and it comes from our own
findings. Gap G2 is entirely about **which input devices can reach which
operation**. That question currently takes a paragraph to answer and is therefore
answered nowhere.

A row of input marks, lit or greyed, beside an operation answers it in one glyph
each. Nothing else in our documents can do that, and no engine documentation this
project has produced has ever tried. That is what `SYMBOLS.svg` is for.
