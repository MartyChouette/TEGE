# Serialization Audit — Beta 0.8 (2026-02-18)

**Status:** 16 of 17 findings fixed. 1 accepted (style).

## SceneSerializer — Fixed

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| SER-C1 | CRIT | Unbounded vector deser (indices, heightmap, dialogueLines, keys) | Caps: 10M indices, 10K lines/keys, grid-bounded heightmap |
| SER-C2 | CRIT | DialogueBox color arrays without `.get<f32>()` | Added explicit type conversion |
| SER-H1 | HIGH | collisionMask vector unbounded | Bounded to `width * height` |
| SER-H2 | HIGH | tags/keys unbounded | Tags 1K, keys 10K |
| SER-H3 | HIGH | Animation keyframe arrays unbounded | 100K per channel, 1K tracks |
| SER-H4 | HIGH | Skeleton bones array unbounded | 1K bones cap |
| SER-H5 | HIGH | Grid size overflow unchecked | Product cap `4096*4096` |
| SER-M1 | MED | customData pair no bounds check | `is_array() && size() >= 2` |
| SER-M2-5 | MED | Enum fields (ditherPattern, crtMask, dofAperture, stippleColor) unclamped | `std::min(v, 2u)` |
| SER-M6 | MED | String fields no length cap | `SafeStr()` helper with tiered caps: PATH 4KB, NAME 1KB, TEXT 64KB, LARGE 1MB. Applied to 50+ high-risk fields (file paths, notes, scripts, dialogue, quests, UI callbacks) |
| SER-M7 | MED | UICanvas element/childId arrays unbounded | Capped: 10K elements, 10K childIds, 1K options |
| SER-M8 | MED | Bone indices not bounds-checked | Capped to 255 |

## SceneSerializer — Not a Bug

| ID | Description | Why |
|----|-------------|-----|
| SER-L1 | Path traversal via `lexically_normal` bypassable | All 3 serializer uses have post-normalization `..` checks — adequate |

## Accepted by Design

| ID | Sev | Description | Rationale |
|----|-----|-------------|-----------|
| SER-L2 | LOW | Inconsistent validation patterns | Style, not security. No functional impact |
