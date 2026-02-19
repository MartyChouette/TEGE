# Physics & Audio Audit — Beta 0.8 (2026-02-18)

**Status:** All 8 findings fixed.

## JoltBackend.cpp (4 fixes)

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| JLT-C1 | CRIT | `createConstraint` uses result of `Create()` without null check | Added `if (!c) return nullptr` |
| JLT-H1 | HIGH | Hinge joint zero-axis produces NaN via `Normalized()` | Default to Y-axis if near-zero |
| JLT-H2 | HIGH | Slider joint zero-axis same issue | Default to X-axis if near-zero |
| JLT-H3 | HIGH | Spring joint negative stiffness/damping | `std::max(value, 0.0f)` clamping |

Also fixed: cone constraint null check in BallSocket, distance joint negative stiffness guard.

## Box2DBackend.cpp (2 fixes)

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| B2D-C1 | CRIT | `ResolveEntity()` calls `b2Shape_GetBody` without shape validity check | Added `b2Shape_IsValid` + `b2Body_IsValid` guards |
| B2D-H1 | HIGH | Stale collision pairs remain after entity destroy | Purge pairs for destroyed entity from `m_ActiveContacts`/`m_ActiveSensorContacts` |

Also fixed: sensor contact carry-forward (was not persisting between frames like contact events).

## SimpleAudio.cpp (2 fixes)

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| AUD-H1 | HIGH | `Play()`/`Play3D()` don't clamp volume/pitch at entry | `Math::Clamp` on volume [0,1] and pitch [0.1,3.0] |
| AUD-H2 | HIGH | 3D `minDist`/`maxDist` not validated | `minDist >= 0.01`, `maxDist > minDist` |

Also fixed: active sound count cap (256) to prevent unbounded growth.
