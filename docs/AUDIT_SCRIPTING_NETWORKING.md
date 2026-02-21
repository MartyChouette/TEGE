# Audit: Scripting & Networking (Beta 0.8)

Date: 2026-02-21
Scope: `Engine/src/Scripting/`, `Engine/src/Networking/`, `Engine/include/Enjin/Networking/`

## Scripting Findings

| ID | Severity | File | Issue | Status |
|----|----------|------|-------|--------|
| SC-1 | MEDIUM | ScriptBindings*.cpp (29 files) | `AS_CHECK` used `assert()` — stripped in Release, registration failures silent | **Fixed** — replaced with `ENJIN_LOG_ERROR` |
| SC-2 | MEDIUM | ScriptBindings.cpp:42 | `ValidateScriptAssetPath` had no length cap on path parameter | **Fixed** — added 1024-char cap |
| SC-3 | LOW | ScriptEngine.cpp:158 | Module name from `stem()` only — collision if same filename in different dirs | **Fixed** — uses `parent_stem` format |
| SC-4 | — | ScriptEngine.cpp:1143 | Include depth limit | No fix needed — already correct (RAII guard, depth 16) |

## Networking Findings

| ID | Severity | File | Issue | Status |
|----|----------|------|-------|--------|
| NET-1 | HIGH | NetworkSystem.cpp:247 | RPC lookup via FNV-1a u32 hash — no collision detection, silent overwrite | **Fixed** — reject registration if hash exists with different name |
| NET-2 | MEDIUM | NetworkSystem.cpp:1173 | Host forwards RPCs without re-signing | **Already handled** — `SendPacket` calls `AuthenticateOutgoing` |
| NET-3 | MEDIUM | NetworkSystem.cpp:1108 | No rate limiting on ownership requests | **Fixed** — 500ms per-player cooldown via `lastOwnershipRequestTime` |
| NET-4 | MEDIUM | HTTPClient.cpp:48 | URL parser silently falls back to port 443 on invalid port | **Fixed** — added `ENJIN_LOG_WARN` before fallback |
| NET-5 | LOW | NetworkSystem.cpp:984 | Entity snapshot cap 1024 — generous for typical MTU | **Fixed** — reduced to 256 |
| NET-6 | — | NetworkSystem.cpp:1608 | Session key gen | No fix needed — already validates not-all-zeros |
| NET-7 | — | NetworkSystem.cpp:958 | Lobby playerCount u8 truncation | No fix needed — already capped to 255 |

## Files Changed

| File | Changes |
|------|---------|
| `Engine/src/Scripting/ScriptBindings.cpp` | SC-1 (macro), SC-2 (path length cap) |
| `Engine/src/Scripting/ScriptBindings_*.cpp` (27 files) | SC-1 (macro replacement) |
| `Engine/src/Scripting/FlashAPIShim.cpp` | SC-1 (macro replacement) |
| `Engine/src/Scripting/ScriptEngine.cpp` | SC-3 (module name collision avoidance) |
| `Engine/src/Networking/NetworkSystem.cpp` | NET-1 (RPC collision), NET-3 (ownership rate limit), NET-5 (snapshot cap) |
| `Engine/src/Networking/HTTPClient.cpp` | NET-4 (URL parser warning) |
| `Engine/include/Enjin/Networking/NetworkTypes.h` | NET-3 (`lastOwnershipRequestTime` field) |

## Verification

- Build: Release config compiles clean (0 errors, 0 warnings)
- Tests: 50/50 CTest targets pass (700+ cases)
