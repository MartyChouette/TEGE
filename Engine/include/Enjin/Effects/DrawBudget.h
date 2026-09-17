#pragma once
// Sharing a fixed instance budget between things that all want to draw.
//
// Every renderer in the engine that batches many sources into one instance
// buffer has to answer the same two questions, and the two that had answered
// them independently had both got them wrong in the same two ways:
//
//   Who gets how much?  Taking it first-come means one saturated source fills
//   the buffer and everything after it draws NOTHING, which reads as content
//   that was never placed rather than content that was dropped.
//
//   What happens over budget?  Stopping partway through a scan removes a
//   CONTIGUOUS piece in scan order, which for a z-outermost 3D scan is a flat
//   slab through the middle of the subject. Taking every stride-th item
//   instead loses density and keeps shape, which is almost always the one a
//   viewer forgives.
//
// Measured case that prompted this (Tests/Integration/FluidBench.cpp): one
// 48^3 smoke volume wanted 40,118 cells against a 16,384 cap shared with every
// other volume in the scene.
#include "Enjin/Platform/Platform.h"
#include "Enjin/Platform/Types.h"

namespace Enjin {
namespace Effects {

// An equal slice of a shared budget, never first-come. Never returns zero, so
// two hundred campfires each draw something rather than the first few taking
// everything. A zero consumer count gets the whole budget, so a caller need
// not special-case an empty scene.
ENJIN_API usize DrawBudgetShare(usize budget, usize consumers);

// Take every Nth item to fit `wanted` into `budget`. Returns 1 when it already
// fits, so the caller can apply the stride unconditionally.
ENJIN_API usize DrawStride(usize wanted, usize budget);

// Scale to apply to every emitter's spawn rate when a scene asks for more
// simultaneous particles than the pool can hold. Returns 1 when it fits.
//
// The GPU particle pool is a RING: a spawn overwrites the oldest slot,
// wherever it came from. So an emitter's real cost is not its spawn rate but
// its steady-state population, rate * lifetime, and when the scene's total
// exceeds the ring, the fastest emitter cycles it and overwrites every OTHER
// emitter's particles before they have lived out their lifetime. A quiet
// six-second smoke plume gets cut short by someone else's sparks, and nothing
// about the smoke emitter says why.
//
// Proportional rather than an equal share: an emitter asking for a hundred
// particles should not be forced up to a share of thousands, nor cut to
// nothing because a neighbour is greedy. Everyone loses the same fraction.
ENJIN_API f32 PoolDemandScale(f64 demand, usize capacity);

} // namespace Effects
} // namespace Enjin
