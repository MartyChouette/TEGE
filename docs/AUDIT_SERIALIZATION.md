# Serialization Audit — Beta 0.8 (2026-02-18)

**Status:** 14 of 17 findings fixed. 3 deferred (LOW risk).

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
| SER-M8 | MED | Bone indices not bounds-checked | Capped to 255 |

## SceneSerializer — Deferred

| ID | Sev | Description | Reason |
|----|-----|-------------|--------|
| SER-M6 | MED | String fields no length cap | nlohmann caps at available memory |
| SER-M7 | MED | UIElement recursive deser no depth limit | Low attack surface |
| SER-L1 | LOW | Path traversal via `lexically_normal` bypassable | Needs broader fix |
| SER-L2 | LOW | Inconsistent validation patterns | Style, not security |
