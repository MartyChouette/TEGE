# Symbol standards: inclusive and international

Checked 2026-09-17 against the public symbol-design literature, because a symbol
set is the one design decision that is expensive to change later. Every icon
lands in documents, in the editor, and eventually in a shipped game's interface,
and by then it is everywhere.

## The bodies of work this draws on

**ISO 7001** (public information symbols) and **ISO 7000 / IEC 60417** (equipment
symbols) are the registries. More useful to us than the registries themselves is
**ISO 9186**, which is the *method*: comprehension testing and judgement testing,
with a pass threshold. A symbol is not good because a designer likes it. It is
good because people who have never seen it say what it means.

**The AIGA and US DOT symbol signs** (1974 and 1979, public domain) are the
canonical proof that a small set built on one geometric logic outperforms a large
set of individually clever marks.

**Otl Aicher's Munich 1972 pictograms** are the argument for a construction grid:
every mark built from the same angles, the same stroke weight, the same
terminals, so the set reads as one voice.

**Isotype**, Neurath and Arntz, gives the counting rule that matters for our
reach strip: **to show more, repeat the unit; never scale it.** A bigger symbol
means a different thing, not more of the same thing.

**Dreyfuss, Symbol Sourcebook** (1972) is the cross-cultural survey, and its
lesson is mostly cautionary: very few marks are actually universal.

**Mijksenaar, Open Here** is on instructional design specifically, which is what
a manual is.

**WCAG 2.2** supplies the numbers: 1.4.1 never use colour alone, 1.4.11 non-text
contrast at least 3:1, 2.5.8 target size at least 24 by 24.

---

## What the set already gets right

Not by luck in every case, but worth writing down so it does not get undone.

**Redundant coding.** The three verdicts differ by shape before they differ by
colour: circle for works, triangle for surprise, square for missing. A reader
with any form of colour blindness, or a black and white printout, still gets the
verdict. This also happens to line up with the safety-sign convention where a
triangle carries warning and a rectangle carries information, so the shapes are
not arbitrary.

**Repeat, do not scale.** The reach strip is seven equal chips. A device is
present or it is not. Nothing grows to mean more.

**One construction grid.** Every mark is drawn in a 50px box with a 3px stroke,
4px for the single emphasised element, square terminals, no gradients, no detail
finer than the stroke. That is what stops a set of twenty marks looking like
twenty decisions.

**Symbol plus label on first use.** The stencils label every mark. Symbols alone
rarely clear a comprehension threshold, and pretending otherwise is how a legend
becomes mandatory reading.

---

## Three problems this check found

### P1. Gold fails non-text contrast, and two variants rely on it

Measured against the stock colour `#FDFBF4`:

| Colour | Contrast | WCAG 1.4.11 needs 3:1 |
|---|---|---|
| ink `#16161A` | 17.43:1 | pass |
| navy `#12233D` | 15.21:1 | pass |
| blue `#0B5FCE` | 5.73:1 | pass |
| red `#D62828` | 4.84:1 | pass |
| green `#17804A` | 4.80:1 | pass |
| **gold `#F2B417`** | **1.79:1** | **fail** |

Gold is the caution colour, so this matters more than it would for a decorative
hue. It is unreadable as a line on paper.

It is fine as a **fill**: ink content on gold measures 9.72:1, the best pairing
in the whole palette. The ink keyline around every shape is what has been
carrying it.

**Rule.** Gold never appears as the only stroke, and never as a line on the
stock. It is a fill, always with an ink keyline and ink content on top.

**Consequence.** In `SYMBOLS-VARIANTS-A.svg`, **2D and 2E fail** and should not be
chosen. Both are gold-stroked on white with no ink keyline. They are the two that
look cleanest at full size, which is exactly how this mistake normally ships.

### P2. The reach strip is not internationalizable, and that is my error

The chips read `M K G T S E P`. Those are the initials of *English* words. In any
other language they carry nothing, and worse, they look like they should.

This is the failure the Dreyfuss survey is entirely about: a mark that feels
universal to its author because the author speaks the language it was built from.

**The fix, in three parts:**

1. **Position is the primary carrier.** The seven devices always appear in the
   same order, so the third chip is the gamepad in every document, in every
   language, forever. Position is language-free and it is what Isotype relies on.
2. **The glyph rides on the position**, drawn at 24px or larger so it is legible,
   rather than a letter. That also satisfies the WCAG 2.5.8 target size if a
   strip ever becomes interactive.
3. **The letter becomes an optional locale layer**, on its own Inkscape layer,
   swapped or switched off per language. Never the only thing distinguishing two
   chips.

**Closed 2026-09-17.** The strip is redrawn this way. The letters live on the
layer `E2 Locale letters`; switch it off to check the strip still reads.

### P3. Hand and gesture symbols are the highest-risk class

Hand gestures are the most reliably offensive category of pictogram across
cultures, and thumb gestures in particular are obscene in several regions. A
symbol set that ships one has a problem that no amount of local testing at home
will surface.

**Closed 2026-09-17.** `7C` is now a contact mark on a surface. The
fingertip forms (7A, 7E) and the abstract tap forms (7B, 7D) carry the same
meaning with none of the risk, and 7B and 7D are better marks anyway because they
describe the *action* rather than the body part.

The eye symbol for gaze is a milder version of the same issue. It is widely
understood, but it also reads as surveillance. The reticle (9B) and the dwell
ring (9C) describe what the system does rather than what the user's body looks
like, and 9C has the additional virtue of showing dwell, which is the part people
actually need to understand.

---

## Rules added to the house style

**Never colour alone.** Every state differs in shape as well as hue. Check by
converting to greyscale; if two marks become the same mark, one is wrong.

**Knockout follows the fill.** Measured, not guessed:

| Fill | Use this on top | Why |
|---|---|---|
| green `#17804A` | white | white 4.97:1, ink only 3.63:1 |
| red `#D62828` | white | white 5.01:1, ink only 3.60:1 |
| gold `#F2B417` | ink | ink 9.72:1 |

**Mirror directional marks in right-to-left locales, and only those.** Arrows,
chevrons, the see-also mark, the pipeline arrow: these mirror. The verdict marks,
the device glyphs, the engine nouns and anything containing a clock or a
numeral: these do not. Getting this backwards is the classic localisation bug,
and it is worth putting the directional marks on their own Inkscape layer so they
can be flipped as a group.

**Avoid metaphors that are local.** A mailbox, a filing cabinet, a piggy bank and
a thumb all mean different things in different places. Prefer the shape of the
thing the software actually does.

**Test, do not assert.** The ISO 9186 method, run cheaply: show a person the mark
with no label and ask what it means. If fewer than two thirds of a handful of
people get it, the mark is wrong however good it looks. This has not been done
for our set. Nothing in this folder should claim the set is comprehensible until
it has been.

---

## Status

Done on 2026-09-17, in the same pass that wrote this file:

- **P1 closed.** `2D` and `2E` are flagged FAILS 3:1 on the pick sheet, and the
  rule is written into `HOUSE_STYLE.md`: gold is a fill with an ink keyline, never
  a line on the stock.
- **P2 closed.** The reach strip is redrawn positionally. Seven fixed positions in
  a fixed order, the device glyph at 26px, lit or struck. The letters moved to
  their own Inkscape layer, `E2 Locale letters`, which can be switched off to
  check the strip still reads without them.
- **P3 closed.** `7C` is no longer a hand. It is a contact mark on a surface,
  which describes what the system senses rather than what a body looks like.
- **Directional marks are named.** `SYMBOLS.svg` carries a band listing exactly
  what mirrors in a right-to-left locale: `sym-xref`, `sym-system`, `sym-ship`,
  and the order of the reach strip. Everything else stays put.

## What still needs doing

- **Run the comprehension test.** The ISO 9186 method, cheaply: show a person a
  mark with no label and ask what it means. Fewer than two thirds correct means
  the mark is wrong however good it looks. **This has not been done, and nothing
  in this folder should claim the set is comprehensible until it has.**
- **Pick the set.** Four sheets and roughly 200 options exist. Sheet C decides the
  most, because its columns are whole languages rather than single marks.
- **Apply the chosen language to the editor.** 57 ASCII stand-ins are inventoried,
  including the visibility toggle that is the letter `O`, and none are replaced.
