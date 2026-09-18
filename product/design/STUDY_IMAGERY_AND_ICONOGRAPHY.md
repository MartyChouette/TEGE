# Study: imagery and iconography

Read 2026-09-17. All 27 one-page design slides and eight scanned manuals.

The companion study `STUDY_MANUALS_AND_ONEPAGERS.md` covers layout, structure and
the argument. This one covers the pictures: what they are made of, how literal
they are, what the marks mean, and how colour and line are used to carry
information rather than to decorate.

Devices and principles only, described rather than reproduced.

---

## 1. The depiction ladder

The single most transferable observation. Across both corpora, images sit at one
of seven levels of abstraction, and **the level is chosen by what the reader has
to do with the picture**, not by what was easiest to produce.

| Level | Looks like | Chosen when the reader must |
|---|---|---|
| 1. Capture | A screenshot, framed | Recognise the actual screen in front of them |
| 2. Rendered asset | A posed 3D model on white | Identify a specific character or object |
| 3. Painted illustration | Full artwork, unframed, bleeding into the column | Feel the tone. Almost never used to instruct |
| 4. Flat sprite | A small clean vector creature | Compare many things at once |
| 5. Isometric block | Simple solids in three flat tones | Understand shape and connection of space |
| 6. Schematic | Boxes, arrows, labels | Follow a mechanism or a flow |
| 7. Token | A coloured bead, a letter, a dot | Read a pattern across dozens of instances |

Two rules fall out of it.

**Abstraction rises as count rises.** One ability gets a painted panel. Twenty
creatures get flat sprites. Two hundred trait combinations get coloured beads. As
soon as the reader's job becomes comparing rather than recognising, detail stops
helping and starts competing.

**Never mix two levels inside one drawing.** Where a page carries both, they are
separated into zones: a rendered portrait in a box beside a schematic, never a
rendered portrait standing inside the schematic. Mixing them makes the reader ask
which parts are literal.

---

## 2. Token systems

This is where the real iconography lives, and it is the part our own set is
closest to needing.

### The two-letter actor pair

The attack taxonomy sheet reduces every actor to one of two letters: one glyph
for the source of an effect, another for the thing it lands on. Nothing else. No
character, no weapon, no arena.

It works because the sheet is about **shapes of delivery**, and any more detail
would imply the delivery only applies to that character. The abstraction is the
content, not a saving.

The transferable rule: **when defining a vocabulary, strip the actors to tokens,
or readers will read the example as the definition.**

### The coloured bead, and the bead string

The strongest notation in the whole set. A single small filled circle carries one
categorical value by hue. Then a **row of four beads** encodes a sequence, and the
sequence names an outcome.

That is a complete, compact, language-free notation invented for one game, and it
does three things at once: it shows the value, the order, and the result, in a
mark small enough to sit inline in a sentence or repeat forty times around the
edge of a diagram.

It also degrades badly in greyscale, which is the standing warning for us.

### The node as sphere, stem and vector

One diagram draws each element as a small sphere on a short stem with a direction
line. Three attributes on one mark: identity by colour, position by placement,
and tendency by the vector. A legend triad in the centre defines the axes.

Worth noting as an idea we have no equivalent for: **a mark that carries a
direction as well as a value.**

### Numbered circles

Used everywhere, in two distinct jobs that the set keeps carefully apart. As a
**callout index** tying a spot on a drawing to a line in a legend. And as a
**sequence step** in a numbered explainer. The same shape, and the reader is
never confused, because a callout number always sits at the end of a leader line
and a step number always sits at the start of a paragraph.

### Depth bullets

From the manuals. A coloured square whose hue encodes hierarchy level, repeated
without variation for fifty pages. Navy square for a chapter, blue for a section,
gold for a step. The reader never learns this consciously and uses it constantly.

### Affect markers

The training flow chart puts a small thumb-up and thumb-down disc on two of its
five outcome boxes. Only two of five are marked, which is the interesting part:
the markers are not labels, they are **emphasis on the extremes** of an ordered
row.

---

## 3. Colour as identity

Colour is used as a carried identity rather than as styling, and it is carried
*consistently across every representation of the same thing on the page*.

A faction gets a hue. That hue then appears on: the faction's emblem, its zone in
the triangle, the border of its vocabulary sidebar, its label text, its units in
the roster, and its nodes in the lattice. Six different depictions, one hue,
never varied.

That is what makes a dense page navigable. A reader who has learned one faction's
colour can find everything about it without reading a word.

**The ordered row.** The consequence row runs red, orange, neutral, light green,
green across five options. The order is the information; the hues are just the
ordering made visible. This is a scale, not a set of categories, and the set
never confuses the two.

**Where it fails.** The bead notation, the node families and the lattice all put
their entire meaning in hue. In greyscale they collapse completely. Our own
finding applies directly: green and red in our palette measure 1.01:1 against
each other, so a bead notation in our colours would be unreadable the moment it
was printed. If we adopt beads we must adopt textured beads.

---

## 4. Matrix iconography

The interaction matrices carry four devices worth taking whole.

**An icon beside every axis label.** Each row and column header has a small
pictorial mark next to its word. The matrix becomes readable by someone who does
not know the vocabulary yet, and scannable by someone who does.

**The diagonal is boxed.** Self against self is outlined as a special case rather
than left to look like an ordinary cell.

**The redundant half is greyed, not deleted.** In a symmetric matrix the lower
triangle is printed faint rather than removed. That shows the symmetry, which is
a real property of the system, instead of hiding it and making the reader wonder
whether cells are missing.

**Empty cells get an explicit mark.** A cell with no interaction carries a mark
saying so. An explicit no reads differently from an omission, and only one of
them proves somebody considered the combination.

That last one is the cheapest quality signal in the entire study.

---

## 5. Map iconography

**Marker vocabulary.** A star for a spawn or a key location, a numbered pin for a
named place, small repeated glyphs for collectibles, and distinct glyphs for
patrolling versus static entities. Fixed across the page and legended once.

**Zone tint for difficulty.** Broad soft washes of colour behind regions, labelled
with words rather than numbers, sitting under the geometry so they never compete
with it.

**Route lines carry state in their style.** Solid for the critical path, dashed
for optional or alternative, arrowed for one-way. One level sheet adds a tiny
legend distinguishing the state of a route before and after an item is collected,
which is a two-line legend encoding a whole progression gate.

**Satellite detail.** A master map with every interior plan arranged around the
edge on hairline leaders. The hairlines are deliberately the faintest lines on
the page so the eye reads the map first and the connections only when it looks
for them.

**The timeline ruler.** One map has a gameplay-minutes scale along the bottom
edge. Space and pacing on one sheet.

---

## 6. Line grammar

Consistent across the corpus and rarely stated, so worth stating:

- **Leader lines are thin, unarrowed, and often bent once.** They connect a label
  to a thing. They never imply flow.
- **Flow arrows are thick and arrowed.** They imply causation or movement.
- **Dashed means conditional, optional, or hidden.** Never used decoratively.
- **A double-headed arrow means a relationship, not a round trip.**
- **Arrow weight tracks importance**, so a primary path is visibly heavier than a
  branch.

The important part is that a reader can tell a label from a flow without reading
either. Our diagrams already follow most of this, but by habit rather than rule,
which means it will drift.

---

## 7. What the manuals add

**The inline glyph.** A control drawn as a small mark inside running prose, every
time it is mentioned. The most transferable device in either corpus, and the
direct ancestor of our reach strip.

**Framed means real.** Screenshots get a border, drawings float unframed.

**The numbered ring.** Numbers ranged down both sides of a capture, leaders in, a
numbered legend in two columns below. The standard solution above six labels.

**Hardware on a tint panel.** The controller diagram sits on a saturated yellow
field so grey plastic separates from white paper. A small production trick with a
real lesson: **when a subject is low contrast, change the ground rather than the
subject.**

**Illustrations bleed, diagrams are contained.** Painted art crosses the column
edge freely; anything instructional is squared up and aligned.

---

## 8. A correction to the companion study

`STUDY_MANUALS_AND_ONEPAGERS.md` claims neither corpus has a device for marking
certainty. That is wrong and the last slide shows it: one sheet is stamped
**Proposal** beside its date, in the corner where the date normally sits alone.

So the device exists. What is missing is that it is **whole-page and binary**. The
entire sheet is a proposal or it is not. Nothing marks one part of a page as
built and another as intended, which is the state most real pages are actually
in.

Our per-part verdict marks are therefore a refinement of an existing idea rather
than an invention, which is a better position to be in. The companion study has
been corrected.

---

## 9. What we take

- **The depiction ladder as an explicit rule.** Write the level on the page brief
  before drawing. Count decides abstraction.
- **Never mix depiction levels inside one drawing.** Zone them.
- **Strip actors to tokens when defining a vocabulary.**
- **One hue per identity, carried across every depiction of it on the page.**
- **Icons beside matrix axis labels**, the boxed diagonal, the greyed redundant
  half, and explicit marks in empty cells.
- **The line grammar**, written down so it stops being habit.
- **Change the ground, not the subject**, when contrast is poor.
- **Hairline leaders for satellite detail** so connections stay secondary.
- **The inline glyph**, which we already committed to via the reach strip.

## 10. What we do not take

**Hue-only notation.** The bead string is the best notation in the set and we
cannot copy it as it stands. Any adoption must be textured beads, per the
screened language on sheet C.

**Rendered 3D assets as document furniture.** They date badly, they imply the
asset is final, and they cost more than they return on a page that is meant to be
redrawn.

**Painted illustration for instruction.** Beautiful in the manuals, and every one
of them is doing tone rather than teaching.

---

## 11. What this reveals about our own set

Three gaps, in order of how much they matter.

**We have no sequence notation.** Nothing in our symbol set encodes an ordered
series the way a bead string does. Our steps are numbered panels, which is
heavyweight. A compact inline sequence mark would serve the touch scheme
fingerprint and the startup flow immediately.

**We have no directional mark.** Every symbol we have carries a value. None
carries a tendency or a direction. The sphere-stem-vector idea is the obvious
starting point if we ever chart trade-offs.

**Our matrix conventions are unwritten.** We have drawn matrices in the gap
analysis without deciding whether empty means considered or unvisited. The
explicit-mark rule settles it and should go in the house style.

One thing we already do better: **per-part verdict marks**. Nothing in either
corpus can say that half a page is built.
