# Study: game manuals and one-page designs, and what we take from both

Read 2026-09-17. Two corpora, studied together on purpose.

**Corpus A, eight scanned manuals.** EarthBound's 118 page pack-in guide, A Link
to the Past, We Love Katamari, Super Mario Bros 3, StarTropics, Donkey Kong
Country, Super Mario World, Pokemon Red.

**Corpus B, all 27 slides from the one-page design talk**, showing real working
design pages dated between 2004 and 2009.

Devices and principles only. Nothing here is traced or copied; the point is the
mechanism.

---

## 1. The claim this study is making

These look like two different disciplines. A manual is a finished artefact
printed for a stranger. A one-pager is a working document drawn for a colleague
who will build the thing.

They are the same discipline pointed at two different readers, and they converge
on the same five solutions:

1. **One dominant picture per page.** Not illustrations scattered through prose.
   One subject you can see from across a room.
2. **Labels in place, on leader lines.** Nothing explained in a paragraph that
   could be marked on the drawing.
3. **A fixed small vocabulary**, repeated without variation, so a reader learns it
   once. Coloured bullets for depth in the manuals, fixed sidebar headings in the
   design pages.
4. **The page is the unit.** Not the chapter, not the wiki, not the section.
5. **A date on it.**

The convergence is the finding. Two groups of people, twelve years apart, one
writing for players and one for developers, arriving at the same layout, is
strong evidence that the layout is doing real work rather than following fashion.

---

## 2. What the manuals do

Covered in more depth in `MANUAL_STUDY.md`. The devices that survive contact with
our material:

**Depth is carried by a coloured bullet, not by type size.** A three level
hierarchy where a navy numbered square means chapter, a blue square means section
and a gold square means step. Open the book anywhere and you know your depth
without reading.

**A frame means it is real.** Screenshots get a border. Explanatory drawings float
unframed. A reader learns the rule in two pages and never confuses the two again.

**The label count picks the device.** Up to three labels, arrows onto the picture.
Four to six, labels outside with leaders pointing in. Seven or more, a numbered
ring down both sides with a numbered legend below.

**Humour lives in the data, not in a caption.** The funniest page in the
EarthBound guide is a reference table of default names. The humour is in the rows.

**A margin figure with no job.** Small creatures in the Katamari margins explain
nothing and are not the player. They react. That is the entire cuteness
mechanism: somebody in the margin having a feeling about what you are reading.

---

## 3. What the one-pagers do

### The anatomy

One slide gives the template explicitly, and every example follows it: title and
date top left, one dominant **main illustration**, **callouts on leader lines**
into it, a **detail inset** with its own notes, a **sidebar** of bullets, a
**description** beneath, and whitespace labelled on the template as a feature
rather than as leftover space.

### The archetypes

Fourteen recurring page types. Pick one rather than inventing a layout.

| Archetype | What it shows |
|---|---|
| Screen flow | Every screen small, every transition arrowed. One example carries an entire login and account system including error states |
| World structure | An exploded stack of levels with an annotation band down the side |
| Annotated map | A map with a roster box per area on leader lines |
| Master map with satellites | A whole town, with every building's interior plan arrayed around the edge |
| Level sheet | One level, numbered locations, spawn list per location, objectives down the right |
| Interior plan | A floorplan with an inset showing a step sequence to reach something hidden |
| Taxonomy sheet | A grid of variants, each with a small diagram and its own parameter list |
| Ability spec | One ability, described, with a three panel sequence and its UI |
| Storyboard grid | The same frame repeated across a grid, banded by phase |
| Module map | Every part of the product and how they connect, click rules alongside |
| Constraint triangle | Three opposed attributes with archetypes placed by what they trade away |
| Radial progression fan | Concentric tiers branching outward to terminal classes, with a legend |
| Map plus timeline | A map with a gameplay-minutes ruler along the bottom |
| Numbered explainer | Numbered steps that build a notation up piece by piece, stamped Proposal |

**Map plus timeline** is the most interesting page in the set. It puts space and
pacing on the same sheet, so "where the player is" and "how long they have been
playing" can be read against each other. Most projects keep those in separate
documents and never compare them. Our worked version is
`diagrams/onepager-kettle-hollow-timeline.svg`.

### The devices worth stealing

**Everything is dated.** Across five years of examples, without exception.

**NOT TO SCALE is stated when true.** A drawing that looks measured and is not
will be measured by somebody.

**The vocabulary sidebar.** Factions defined not by stats but by a short list
under fixed headings: traits, materials, effects, sounds, reference. Fixed
headings make three factions comparable at a glance, and it briefs an artist or a
sound designer far better than a paragraph.

**Empty cells are marked, not left blank.** The class and faction matrix puts an
explicit mark in every combination that does not exist. An explicit no reads
differently from an omission, and only one of them proves somebody considered it.

**Graded consequence rows.** Five player responses laid out on a spectrum, each
with its numeric effect stated in identical form underneath. The ordering carries
as much information as the numbers.

**Rules in the empty centre.** The trait triangle puts its governing rules inside
the shape, where the eye already is.

**Parameters beside the diagram, never in prose.** So an implementer does not
have to extract numbers from sentences.

---

## 4. Where they genuinely differ

Three differences, and each one is a decision rather than an accident.

**Finish.** Manual pages are typeset and final. Design pages are visibly working
documents with rough edges, and that is deliberate: a clean page invites approval
and a rough one invites correction.

**Density.** Manual pages are paced for reading in sequence. Design pages are
dense because they are consulted while standing, repeatedly, by someone who
already knows most of it.

**Certainty.** A manual describes a finished thing and can speak with confidence.
A design page describes something that does not exist yet and has to be honest
about that.

The design pages do have a device for this, which an earlier draft of this study
missed: one sheet is stamped **Proposal** beside its date, in the corner where the
date usually sits alone. So the idea exists.

What is missing is that the device is **whole-page and binary**. The entire sheet
is a proposal or it is not. Nothing marks one part of a page as built and another
as intended, which is the state most real pages are actually in.

That is where our own contribution goes, and it is a refinement of an existing
idea rather than an invention, which is a better position to be in.

---

## 5. The argument

> **A one-pager and a manual page are the same artifact at two points in its
> life. If we author them in the same layout and vocabulary, then shipping a
> feature becomes largely a matter of changing the verdict marks on a page that
> already exists.**

Today a feature is documented twice by different people at different times, and
the second version is written from scratch because the first one was in a
different form, in a different folder, with a different vocabulary. That is why
manuals lag engines.

Under this argument the sequence is one page, changed three times:

1. **Proposal.** Drawn before it exists. Every part marked missing, in red. The
   page is the design document.
2. **Under construction.** Parts flip to works as they land. The reds that remain
   are the actual todo list and they are spatial, so what is missing is visible
   rather than enumerated.
3. **Shipped.** Mostly works marks, some will-surprise-you marks on the traps.
   The page is now a manual page. It did not get rewritten, it got re-marked.

This resolves the certainty problem neither corpus solved, and it does so with a
device we already have: the verdict marks in `SYMBOL_STANDARDS.md`. It is the one
place where our documents can be better than the material we are learning from,
rather than merely as good.

It also explains why our gap IDs matter more than they appear to. **A gap ID is a
red mark with a name**, which is what lets the same page carry a design intention
and a bug report without ambiguity.

---

## 6. What we adopt

- The seven part anatomy, as the default layout for anything drawn
- The fourteen archetypes as a menu, so nobody invents a layout under time pressure
- Dates on everything, and NOT TO SCALE when true
- The vocabulary sidebar, for any set of things that should be comparable
- Explicit marks in empty matrix cells
- Parameters listed beside diagrams, never in prose
- Coloured bullets for depth, and the frame rule, from the manuals
- One page per system, many pages, redrawn rather than revised

## 7. What we reject, and why

**The undated living document.** Both corpora avoid it and so should we.

**Per-letter multicolour headings.** Charming in the 1995 guide and unusable for
us: a heading that spends every hue on decoration has no colour left to mean
anything, and our whole system depends on colour meaning something.

**Pastel calm for gap reporting.** The Katamari register is beautiful and would
undermine us. A soft pastel page saying a feature is missing reads as not minding
very much.

**Prose density in a design page.** Our instinct under pressure is to write a
paragraph. Every page in corpus B resists that instinct, and where one of our
pages has three paragraphs it has a diagram that was not drawn.

## 8. Open questions

- **Nobody has tested our marks.** Section 5 rests on readers understanding the
  verdict marks without a key. That has not been checked, and the cheap ISO 9186
  style test is in `SYMBOL_STANDARDS.md` waiting to be run.
- **Which of the fourteen archetypes fits an engine rather than a game?** Screen
  flow, module map and taxonomy sheet map over cleanly. Annotated map and level
  sheet do not obviously apply to a tool. The map plus timeline form might apply
  to a first session with the editor, which would be an unusual and useful page.
- **Nothing is left unread.** All 27 slides were reviewed. The last four added one
  archetype, the numbered explainer, and forced one correction: the design pages DO
  carry a certainty device, covered in section 4.

---

## Sources

Screenshots of the one-page design talk and the eight scanned manuals, both
supplied for this purpose, in `Game Design Inspiration/`. Supporting published
material:

- [Video: One-page designs](https://www.gamedeveloper.com/design/video-one-page-designs)
- ['The goal of design is to efficiently communicate ideas'](https://www.gamedeveloper.com/design/-the-goal-of-design-is-to-efficiently-communicate-ideas-)
- [GDC Vault: One-Page Designs](https://gdcvault.com/play/1012356/One-Page)
- [GDC Vault: Simulating a City, One Page at a Time](https://gdcvault.com/play/1017708/Simulating-a-City-One-Page)
