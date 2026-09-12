# Visual Novel & Dating Sim

You, and two people worth impressing. One scene, and everything in it is data.

## What is here

| | |
|---|---|
| `scene.enjin` | Two busts, a dialogue box, three answers, an affinity meter per character |
| `data/schemas/conversation.enjschema` | The shape of a scene's script |
| `data/schemas/reaction.enjschema` | The shape of a tag table |
| `data/conversations/first_evening.enjdata` | The demo: 10 beats, 2 questions |
| `data/reactions/vn_default.enjdata` | What `warm` / `neutral` / `cool` / `tease` are worth |

The scripts live in `scripts/enjin_api/` and arrive with every project:
`VNScene.as` (the beat player), `PortraitRig.as` (one bust from swappable
layers), `Reactions.as` (the tag table).

## Writing a scene

Open the **Data Assets** panel, pick the `Conversation` schema, make a record.
The beat fields are **parallel columns**: `beatWho[3]`, `beatEmote[3]` and
`beatLine[3]` all describe beat 3. Beats are separated by `|`, options within a
choice by `;`, so **no authored line may contain either character.**

`beatChoice` is empty for a beat with no question, or the index of a choice
group. A group has `choiceWho` (whose meter moves), `choiceText`, `choiceTags`
and `choiceReply`.

Point the scene's `VNScene.conversation` at your record and press play. No build
script, no recompile.

## The tag seam

An option carries a bare tag. `VNScene` never learns what a tag means; the
Reaction asset does, answering with a face and a number. Change what `tease`
costs by editing four characters in a data record, in the editor, with the game
running next door.

That indirection is the whole reason this template is worth starting from. The
same runtime drives a dating sim, an interrogation, a negotiation, or a
temperament game, because the only thing that differs is a table.

## Art

38 SVG layers in `assets/art/vn`, mounted with `displayGraphic`, which
tessellates them to real triangles: crisp at any scale, unlit, alpha blended,
paint order preserved. `build_vn_art.py` in the Ink Ribbon project draws them if
you want to regenerate.

**Every layer is a full-face document on one 200x260 viewBox**, with only its own
piece drawn and the rest transparent. So every layer mounts at the identical
transform and alignment lives in the art, not in a table of offsets in the scene.
Redraw a brow, keep the viewBox, and it cannot land out of place.

Naming: `base_head`, `brow_*`, `eye_*`, `mouth_*`, `blush_*`, `fx_*`, each with
the rig's `layerPrefix` in front (`L_` and `R_` here). **Two rigs in one scene
must have different prefixes** — lookup is scene-global, so without them the
second bust wears the first one's face. A layer with no entity is skipped rather
than erroring, so partial art runs.

Both busts share every ink layer and differ only in `base_head`, which is why 38
files dress two characters.

### Dimming

Whoever is not speaking is covered by `dim_<who>.svg`, **their own silhouette**
in near-black at 0.6 opacity. A rectangle over the bust leaves a visible box, and
on an emissive material a blend washes the face grey instead of darkening it.
The silhouette is deliberately two barely-overlapping shapes: stacking the five
real head parts would compound the alpha and read as blotches.

### Tessellator limits (v1)

Solid fills only — a gradient collapses to its first stop. No even-odd holes — a
donut fills in. Strokes are miterless, so anything that should read as a line is
drawn as a filled shape here.

## Knobs on VNScene

| Property | |
|---|---|
| `conversation` | the Conversation record to play |
| `revealRate` | characters per second |
| `resetOnStart` | 1 for a demo, 0 to let affinity accumulate between scenes |
| `autoBeat` / `autoPick` | plays itself, for a capture with no hand on the keyboard |

Affinity persists under `vn.affinity.<castId>` in the meta store, so a second
conversation continues the relationship rather than restarting it.
