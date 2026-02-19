# ECS Audit — Beta 0.8 (2026-02-18)

**Status:** All 5 findings fixed. Tests expanded 13 → 40.

## Findings

| ID | Sev | Description | File | Fix |
|----|-----|-------------|------|-----|
| ECS-C1 | CRIT | `IsValid()` O(N) linear scan via `std::find` | Entity.cpp | `unordered_set<Entity>` for O(1) |
| ECS-C2 | CRIT | `DestroyEntity()` O(N) find + O(N) erase | Entity.cpp | O(1) set erase + swap-and-pop |
| ECS-H1 | HIGH | `GetComponent<T>()` silently allocated storage on read path | World.h | `GetStorageMut<T>()` helper |
| ECS-M1 | MED | `FlushPendingDestructions()` TOCTOU race | World.cpp | Lock before empty check |
| ECS-L1 | LOW | EventBus ID wraparound returns 0 (reserved) | EntityEventBus.cpp | Skip-zero guard |

## Tests Added

40 tests in `TestECS.cpp`: invalid entity safety, double-destroy, deferred destruction visibility, entity recycling, mass create/destroy (1000 entities), swap-and-pop integrity, hierarchy cleanup, EventBus (send/receive/remove/deferred/iterator/clear), StringIntern.
