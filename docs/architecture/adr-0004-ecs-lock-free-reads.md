# ADR-0004: Lock-Free ECS Reads (Single-Writer / Fork-Join Model)

## Status

Accepted and implemented (2026-08-13). `World::GetComponent` (both overloads) and
`World::HasComponent` no longer acquire `m_Mutex`. Structural mutation
(`CreateEntity`/`DestroyEntity`/`DestroyEntityImmediate`/`AddComponent`/`RemoveComponent`/
`Clear`/`FlushPendingDestructions`) keeps the existing `recursive_mutex` and, in debug
builds, asserts it runs on the owner thread. A fork-join concurrency test was added
(`Tests/Unit/ECS/TestECS.cpp` -> `ComponentThreadSafety.ConcurrentReadsAreConsistentAndRaceFree`).

## Date

2026-08-13

## Context

`GetComponent<T>()` and `HasComponent<T>()` took a `std::lock_guard<std::recursive_mutex>`
on every call. Those two methods are the ECS read hot path: systems call them thousands of
times per frame, almost all from a single thread. The lock was added as the ECS-C1 audit
fix (AUDIT_2026_05_20) to close a data race with concurrent `AddComponent`/`RemoveComponent`.
The whole-engine review (2026-07-17) flagged the per-call lock as the deepest single perf
cut in the ECS.

The lock guards two real races:

1. **The `m_ComponentStorages` map.** A `find` racing a first-time `GetOrCreateStorage`
   insert (which can rehash) is UB.
2. **A storage's dense vectors.** `Get`/`Has` reading `m_Components`/`m_Entities`/`m_Sparse`
   while a concurrent `Add`/`Remove` reallocates them is a torn read / use-after-free.

Both races require a **write concurrent with a read**. The question was whether that
concurrency ever actually happens.

### What the threading audit found

Two subagent sweeps over Engine/Core/Editor/Player established:

- **Structural mutation is owner-thread-only.** No worker thread anywhere calls
  `Add`/`Remove`/`Create`/`Destroy`. Every mutation site is on the main thread.
- **Every parallel region is strict fork-join.** There are exactly two thread-pool regions
  that touch the World, both in `RenderSystem.cpp`:
  - Animation pose sampling (`~4331`): workers touch only their own animator state, zero
    World access. Main thread blocks at `for (auto& f : futures) f.get()`.
  - Shadow command recording (`~10508`): workers DO read components
    (`GetComponent<Animator/Transform/Parent>` via `ComputeWorldMatrix`), but the main
    thread blocks at the join for the region's whole duration. The region even pre-warms
    the map-mutating `GetOrCreateRenderData` on the main thread before forking, with the
    comment "not safe from the worker threads", and reads transforms lock-free through a
    cached storage pointer. The authors already designed around this exact invariant.

So during a parallel read region the only writer (the main thread) is parked at the join.
Reads inside those regions never overlap a write. The per-call read lock was guarding a
race that cannot occur under the current architecture.

### Why the recursion is load-bearing (and rules out shared_mutex)

The mutex is a `recursive_mutex` because six write/Lock paths re-enter a locking method:
`Hierarchy::SetParent` (holds `World::Lock`, then calls ~13 World methods),
`AddComponent`/`RemoveComponent` -> system `OnEntityAdded`/`OnEntityRemoved` -> `HasComponent`,
`DestroyEntityInternal` (called from two locked contexts, calls `Has`/`Get`/`Remove`), and
`FindEntityByName` -> `RebuildNameCache` -> `GetComponent`. A blanket
`recursive_mutex -> shared_mutex` swap would deadlock on these.

`shared_mutex` would also not fix the measured cost. The problem is single-threaded per-call
overhead in hot loops; a `shared_lock` still does two atomics per call. It only buys
reader-reader concurrency, which is not the bottleneck.

## Decision

Single-writer / fork-join model:

1. **`GetComponent` (both) and `HasComponent` drop the lock entirely.** They become plain
   lock-free reads. This removes the thousands-of-locks-per-frame overhead directly.
2. **The write path keeps the `recursive_mutex` unchanged.** None of the six recursion
   chains change, so there is no deadlock risk and no restructuring. The write lock now only
   ever serializes the owner thread with itself (recursion); it protects nothing
   cross-thread, but it is cheap on the rare structural-change path and keeps the diff
   minimal.
3. **Enforce the invariant in all builds.** The World captures its constructing thread as
   the owner (overridable via `AdoptOwnerThread()` if a loader hands the World to another
   thread). Every structural mutation runs `AssertOwnerThread()`, which checks the calling
   thread in ALL build configs — mutation is the rare, non-hot path, so a thread-id compare
   there is free relative to a frame. A debug build hard-stops at the offending call
   (`ENJIN_ASSERT`); a release build logs a loud error once and continues, so even a shipped
   build surfaces a broken invariant instead of silently racing the lock-free readers. The
   reads stay unchecked and lock-free.

Reads are left unchecked, on purpose. Since writes are owner-only (guarded) and parallel
reads are fork-join (owner parked), a read can only race a write if a write happens off the
owner thread, and that write trips the mutation guard first. Guarding the write side is
sufficient, and it keeps the read path free of even a branch.

## Alternatives considered

1. **`shared_mutex` (the audit's off-hand suggestion).** Rejected: would deadlock on the
   recursion paths, and does not remove the single-threaded per-call cost that was actually
   measured.
2. **Keep the read lock, hoist `GetComponentStorage` in hot loops only.** This is the
   existing escape hatch (already used by the physics sync and the shadow worker). It
   removes calls, not the lock, and only where hand-applied. Good as a follow-up (see
   Stage B) but leaves the default read path locked.
3. **Atomic per-storage generation / seqlock reads.** Real lock-free-with-writers scheme,
   but it solves a concurrency that does not exist here and adds cost to every read. Not
   worth it under fork-join.
4. **Do nothing.** Leaves the deepest ECS perf cut on the table.

## Consequences

- Positive: removes the ECS read hot-path lock; no API change; no deadlock risk; the write
  path and its recursion are untouched; the invariant is now explicit and debug-enforced
  instead of implicit.
- Risk: safety rests entirely on the fork-join invariant holding forever. If someone later
  adds off-thread component mutation, the mutation guard catches it in every build (debug
  aborts, release logs a loud error once), so the failure is loud rather than a silent race.
  The residual risk is only the window between the off-thread write and the reader observing
  it on that one run; the logged error points straight at the bug. The ADR, the class-level
  comment in `World.h`, and a CLAUDE.md rule document the contract; new parallel work that
  mutates components must either stay on the owner thread or redesign this model (not silence
  the guard).
- `IsValid`/`IsPendingDestruction`/`FindEntityByName` keep their locks. They touch the
  pending-destruction set and the name cache (which `FindEntityByName` mutates via
  `RebuildNameCache`), not the component storages, and they are not the measured hot path.
- The `GetEntitiesWithComponent`/`GetEntitiesWithComponents` family was already lock-free
  and is unchanged; it is consistent with this model (dense-array reads, safe under
  fork-join).

## Stage B (follow-up, not part of this ADR)

Convert the hottest per-entity `GetComponent` loops to hoist `GetComponentStorage<T>()`
once and iterate the dense arrays (`GetEntities`/`GetByIndex`). This removes the calls, not
just the locks, and improves cache locality. It is incremental and per-system, and is not
required to bank the Stage A win.

## Test plan

- `ComponentThreadSafety.ConcurrentReadsAreConsistentAndRaceFree` (added): the owner thread
  populates two storages, then parks while 8 worker threads do 50 passes of lock-free
  `Get`/`Has` over 2000 entities (including the storage-miss path). Asserts no missing
  reads, no torn values, and that the owner can resume mutation after the join. This is the
  fork-join usage the model depends on; run it under a thread sanitizer to confirm the read
  path is race-free for the intended pattern.
- The full ECS suite (`TestECS`, 42/42) must stay green.

## References

- `Engine/include/Enjin/ECS/World.h` -> class-level thread-safety note, `GetComponent`/
  `HasComponent`, `AssertOwnerThread`, `AdoptOwnerThread`, `m_OwnerThreadId`.
- `Engine/src/ECS/World.cpp` -> owner capture in the constructor, `AssertOwnerThread` on the
  mutation entry points.
- `Engine/src/ECS/Systems/RenderSystem.cpp` -> the two fork-join regions (`~4331` animation
  sample, `~10508` shadow record) this model relies on.
- AUDIT_2026_05_20 (ECS-C1, the lock this replaces) and the 2026-07-17 whole-engine review
  (per-call lock flagged as the deepest ECS cut).
