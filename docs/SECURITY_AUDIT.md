# Security & Robustness Audit Report

**Date:** 2026-02-10 (initial), **Updated:** 2026-04-12
**Scope:** Full codebase audit covering serialization, networking, scripting, build pipeline, renderer, physics, input, and memory management.

---

## Summary

| Severity | Count | Resolved | Remaining |
|----------|-------|----------|-----------|
| CRITICAL | 7 | 6 | 1 |
| HIGH | 13 | 12 | 1 |
| MEDIUM | 13 | 12 | 1 |
| LOW | 5 | 5 | 0 |
| **Total** | **38** | **35** | **3** |

> C1-C6 resolved in Feb 2026 hardening sprint. C7, H13, M13 added 2026-04-12.
> See also: `docs/AUDIT_2026_04_12.md` for the full April 2026 audit (55 findings across 6 categories).

---

## CRITICAL Findings

### C1. Unvalidated Vector Array Access in Prefab Deserialization
**File:** `Engine/src/Scene/Prefab.cpp` (lines 649-657)
**Risk:** Crash / memory corruption
**Description:** When deserializing `Vector3`/`Vector4` from JSON, the code accesses array indices `[0]`, `[1]`, `[2]`, `[3]` without verifying the array has enough elements. A malformed prefab file with a short array causes out-of-bounds access.
**Fix:** Check `array.size() >= 3` (or 4) before accessing elements. Return default vector on failure.

### C2. Missing Array Type Validation in Prefab Deserialization
**File:** `Engine/src/Scene/Prefab.cpp` (lines 620-625)
**Risk:** Crash from type mismatch
**Description:** JSON values expected to be arrays are accessed without `is_array()` checks. If a scalar or object appears where an array is expected, the JSON library throws or returns undefined behavior.
**Fix:** Add `is_array()` guard before all array access in deserialization.

### C3. Unchecked Skeleton Bone Access in Scene Importer
**File:** `Engine/src/Assets/SceneImporter.cpp` (line 454)
**Risk:** Out-of-bounds read
**Description:** Bone indices from imported models are used to index into the skeleton's bone array without bounds checking. A malformed glTF/FBX file with invalid bone indices could read out of bounds.
**Fix:** Validate `boneIndex < skeleton.bones.size()` before access.

### C4. Vulkan Device Lost Deadlock
**File:** `Engine/src/Renderer/Vulkan/VulkanRenderer.cpp` (lines 350-362)
**Risk:** Application hang (unrecoverable)
**Description:** When `vkAcquireNextImageKHR` returns `VK_ERROR_DEVICE_LOST`, the fence may be reset but never signaled. Subsequent frames wait on this fence indefinitely, causing a permanent hang.
**Fix:** On `VK_ERROR_DEVICE_LOST`, skip fence wait, log fatal error, and trigger graceful shutdown or device re-creation.

### C5. Missing Rate Limiting / DoS in Networking
**File:** `Engine/src/Networking/NetworkSystem.cpp` (lines 362-376)
**Risk:** Denial of Service
**Description:** The UDP receive loop processes all incoming packets without rate limiting per sender. A malicious client can flood the host with packets, consuming all CPU time in packet processing and starving the game loop.
**Fix:** Implement per-IP packet rate limiting (e.g., max 100 packets/sec per sender). Drop excess packets with a warning.

### C6. Unauthenticated RPC Execution
**File:** `Engine/src/Networking/NetworkSystem.cpp` (lines 786-817)
**Risk:** Remote code execution / game state manipulation
**Description:** RPC messages are dispatched to registered handlers based on the RPC name string without verifying the sender is an authenticated, connected client. Any UDP source can invoke RPCs.
**Fix:** Verify sender is in the connected clients list and has completed the handshake before dispatching RPCs. Add an RPC permission system (client-callable vs server-only).

---

## HIGH Findings

### H1. Unchecked Mesh Vertex Arrays in Prefab
**File:** `Engine/src/Scene/Prefab.cpp` (lines 285-305)
**Risk:** Out-of-bounds access
**Description:** Vertex attribute arrays (normals, UVs, colors, tangents) are iterated without verifying they match the position array length. Mismatched arrays cause OOB reads.
**Fix:** Validate all attribute arrays match position count, or clamp iteration to `min(positions.size(), normals.size(), ...)`.

### H2. Path Traversal in Texture Paths
**File:** `Engine/src/Scene/SceneSerializer.cpp` (lines 297-307)
**Risk:** Reading arbitrary files from disk
**Description:** Texture paths loaded from scene files are used directly in file I/O without sanitizing `..` sequences. A crafted scene file could reference `../../../../etc/passwd` or similar.
**Fix:** Normalize paths with `std::filesystem::lexically_normal()` and verify the resolved path stays within the project directory.

### H3. Unbounded Entity Count in Scene Loading
**File:** `Engine/src/Scene/SceneSerializer.cpp` (line 5162)
**Risk:** Memory exhaustion
**Description:** The entity count from a scene file is used to drive a creation loop without any upper bound. A crafted scene claiming millions of entities causes unbounded memory allocation.
**Fix:** Cap entity count to a reasonable maximum (e.g., 100,000) and reject files exceeding the limit.

### H4. Unbounded Vertex Data in Scene Loading
**File:** `Engine/src/Scene/SceneSerializer.cpp` (lines 336-362)
**Risk:** Memory exhaustion
**Description:** Vertex and index arrays from scene files are loaded without size limits. A crafted scene with enormous vertex counts causes unbounded allocation.
**Fix:** Cap total vertex/index count per mesh and per scene.

### H5. Entity Ownership Spoofing
**File:** `Engine/src/Networking/NetworkSystem.cpp` (lines 733-767)
**Risk:** Unauthorized entity manipulation
**Description:** Entity state updates include a sender ID, but the ownership check only compares this self-reported ID against the entity's owner. A client can claim to be any sender ID and modify entities they don't own.
**Fix:** Derive sender identity from the source socket address (validated during connection handshake), not from the packet payload.

### H6. Sender ID Spoofing / No Address Verification
**File:** `Engine/src/Networking/NetworkSystem.cpp` (lines 378-408)
**Risk:** Identity impersonation
**Description:** The sender ID is extracted from the packet header without verifying it matches the source IP:port. Any client can impersonate another.
**Fix:** Map client IDs to their authenticated socket addresses during handshake. Verify all subsequent packets match the registered address.

### H7. No Connection Authentication
**File:** `Engine/src/Networking/NetworkSystem.cpp` (lines 465-536)
**Risk:** Unauthorized access to game sessions
**Description:** The connection handshake accepts any client without authentication. No password, token, or lobby code is required to join a game.
**Fix:** Add optional session password/code support. Implement a challenge-response handshake to prevent replay attacks.

### H8. Script Include Path Traversal
**File:** `Engine/src/Scripting/ScriptEngine.cpp` (lines 1068-1126)
**Risk:** Reading arbitrary files via script includes
**Description:** AngelScript `#include` directives are resolved with `lexically_normal()` but not restricted to the scripts directory. A script can include files outside the project.
**Fix:** Resolve the canonical path and verify it starts with the project's script directory prefix. Reject paths that escape the sandbox.
**Note:** This is a known issue documented in CLAUDE.md.

### H9. AngelScript Instruction Limit Enforcement
**File:** `Engine/src/Scripting/ScriptEngine.cpp`
**Risk:** Partial DoS via CPU exhaustion
**Description:** The 1M instruction limit is set via `SetLineCallback()`, which only fires at line boundaries. A single complex expression or built-in function call could exceed the limit between callbacks.
**Fix:** Consider using `SetTimeoutCallback()` as a secondary safeguard, or reduce the instruction limit. Document that built-in calls are not counted.

### H10. Uninitialized Current Image Index
**File:** `Engine/src/Renderer/Vulkan/VulkanRenderer.cpp` (lines 273-322)
**Risk:** Using stale/invalid swapchain image
**Description:** If `vkAcquireNextImageKHR` fails (timeout, suboptimal), the current image index may retain its previous value. Subsequent rendering commands use this stale index.
**Fix:** Initialize `currentImageIndex` to `UINT32_MAX` before acquire. Check for valid index before proceeding with rendering.

### H11. Null Vertex Shader Pointer
**File:** `Engine/src/Renderer/Vulkan/VulkanPipeline.cpp` (lines 213-236)
**Risk:** Null dereference crash
**Description:** Pipeline creation uses a vertex shader module pointer without null-checking. If shader compilation or loading fails silently, this causes a crash.
**Fix:** Validate both vertex and fragment shader modules are non-null before creating the pipeline. Return an error result on failure.

### H12. Weak XOR Obfuscation in Asset Packs
**File:** `Engine/src/Build/AssetPacker.cpp` (lines 170-176)
**Risk:** Trivial asset extraction
**Description:** Asset pack files use XOR obfuscation with a known key (`enjin_default_pack_key_2025`). This is trivially broken with known-plaintext attacks since many packed files have predictable headers (JSON, PNG, etc.).
**Fix:** For commercial releases, replace XOR with AES-256-GCM authenticated encryption. Allow per-project encryption keys.
**Note:** Already documented as a known limitation in CLAUDE.md.

---

## MEDIUM Findings

### M1. Type Safety in Index Deserialization
**File:** `Engine/src/Scene/SceneSerializer.cpp` (line 362)
**Risk:** Data truncation
**Description:** Index values are loaded as `int` but stored as `u32`. Negative values in malformed files wrap to large unsigned values.
**Fix:** Validate indices are non-negative before casting.

### M2. Missing `contains()` Checks in JSON Iteration
**File:** `Engine/src/Scene/SceneSerializer.cpp`, `Engine/src/Scene/Prefab.cpp` (various)
**Risk:** Crash on malformed JSON
**Description:** Several deserialization paths access JSON keys without `.contains()` checks, relying on the JSON library's default behavior which may throw.
**Fix:** Add `.contains()` or `.value()` with defaults for all optional JSON fields.

### M3. Symlink Following in Asset Loading
**File:** `Engine/src/Build/AssetReader.cpp`
**Risk:** Reading files outside pack directory
**Description:** File paths resolved during asset loading may follow symlinks, potentially reading files outside the intended project directory.
**Fix:** Use `std::filesystem::is_symlink()` checks or resolve canonical paths before loading.

### M4. Missing Bounds Check on Collision Pair Indices
**File:** `Engine/src/Physics/PhysicsWorld.cpp`
**Risk:** Out-of-bounds access
**Description:** Collision pair entity indices are used to look up collider data without verifying the entities still exist or have valid components.
**Fix:** Validate entity existence and component presence before accessing collision pair data.

### M5. Unbounded Mouse Delta Values
**File:** `Engine/src/Core/Input.cpp`
**Risk:** Camera teleportation / UI glitch
**Description:** Raw mouse delta values are passed directly to camera/gizmo systems without clamping. A large delta (from alt-tab, cursor warp, or input spike) can cause extreme camera movement.
**Fix:** Clamp mouse delta to a reasonable maximum (e.g., 500 pixels per frame).

### M6. Null Context Pointer in VulkanBuffer
**File:** `Engine/src/Renderer/Vulkan/VulkanBuffer.cpp`
**Risk:** Null dereference
**Description:** Some buffer operations access the Vulkan context pointer without null-checking in error paths.
**Fix:** Add null-checks for context pointer before Vulkan API calls.

### M7. Double-Free Risk in PoolAllocator
**File:** `Engine/src/Memory/Memory.cpp`
**Risk:** Memory corruption
**Description:** The pool allocator's free list doesn't detect double-frees. Freeing the same block twice corrupts the free list, leading to allocating the same memory twice.
**Fix:** Add a debug-mode double-free detector (e.g., canary value in freed blocks, or track allocated blocks in a set).

### M8. No Replay Protection in Networking
**File:** `Engine/src/Networking/NetworkSystem.cpp`
**Risk:** Replay attacks
**Description:** Network packets don't include sequence numbers or timestamps for replay detection. Captured packets can be replayed to duplicate actions.
**Fix:** Add monotonically increasing sequence numbers per connection. Reject packets with sequence numbers below the last received.

### M9. Decompression Bomb in Asset Unpacking
**File:** `Engine/src/Build/AssetReader.cpp`
**Risk:** Memory exhaustion
**Description:** If compressed assets are added in the future, there's no ratio limit on decompressed-to-compressed size.
**Fix:** When adding compression, enforce a maximum decompression ratio (e.g., 100:1).

### M10. Missing TLS Certificate Validation
**File:** `Engine/src/Networking/HTTPClient.cpp`
**Risk:** Man-in-the-middle attacks
**Description:** HTTP client connections to external APIs (e.g., a legacy web portal) may not fully validate TLS certificates depending on the backend library configuration.
**Fix:** Ensure TLS certificate validation is enabled. Pin certificates for known APIs if feasible.

### M11. Form Parameter Injection in HTTP Client
**File:** `Engine/src/Networking/HTTPClient.cpp`
**Risk:** API request manipulation
**Description:** Form parameters for HTTP requests may not properly encode special characters, potentially allowing parameter injection.
**Fix:** URL-encode all form parameter values before sending.

### M12. Entry Count Cap in Asset Pack
**File:** `Engine/src/Build/AssetPacker.cpp`
**Risk:** Memory exhaustion
**Description:** The number of entries in an asset pack header is read and used to allocate arrays without an upper bound.
**Fix:** Cap the entry count to a reasonable maximum (e.g., 50,000 files).

---

## LOW Findings

### L1. Resource Leak in BindlessResources
**File:** `Engine/src/Renderer/Vulkan/BindlessResources.cpp`
**Risk:** GPU memory leak
**Description:** In some error paths, allocated descriptor sets or textures may not be properly released.
**Fix:** Use RAII wrappers or ensure all error paths clean up resources.

### L2. Thread-Unsafe Vector Operations
**File:** Various renderer files
**Risk:** Race condition (unlikely in current single-threaded usage)
**Description:** Some shared vectors in the renderer are accessed without synchronization. Currently safe because rendering is single-threaded, but would break if multi-threaded rendering is added.
**Fix:** Add mutex protection or use concurrent containers if multi-threading is planned.

### L3. Integer Overflow in Size Calculations
**File:** `Engine/src/Build/AssetPacker.cpp`
**Risk:** Allocation size overflow
**Description:** Size calculations for packed assets multiply counts by element sizes without overflow checks. Extremely large values could wrap to small allocations.
**Fix:** Use checked arithmetic or validate sizes before multiplication.

### L4. Missing Key Contains Checks in Serialization
**File:** Various serialization files
**Risk:** Crash on minimal/legacy files
**Description:** Some serialization paths assume all keys exist. Loading very old or minimal save files may crash if expected keys are absent.
**Fix:** Use `.value("key", default)` for all non-essential fields.

### L5. Prefab Recursion Depth
**File:** `Engine/src/Scene/Prefab.cpp`
**Risk:** Stack overflow (properly mitigated)
**Description:** Nested prefab instantiation has a recursion guard, but the depth limit should be explicitly documented and configurable.
**Note:** Already properly implemented with recursion guard. Low priority.

---

## Future-Proofing & Extensibility Issues

### Architecture

| Priority | Issue | Description |
|----------|-------|-------------|
| CRITICAL | Vulkan Coupling | Renderer tightly coupled to Vulkan, blocking D3D12/Metal/WebGPU backends. Need RenderBackend abstraction fully implemented. |
| CRITICAL | Component Serialization | Component serialize/deserialize is hardcoded in SceneSerializer. Plugin components can't be serialized without engine modification. Need a component registry with serialize callbacks. |
| CRITICAL | Physics Backend | Physics is directly implemented, not abstracted. Blocks future Jolt/PhysX integration. |
| HIGH | Audio Backend | miniaudio is directly used. Need an audio backend abstraction for FMOD/Wwise support. |
| HIGH | ImGui in Engine Layer | Some ImGui code exists in engine layer (not just editor). Should be editor-only for clean player builds. |
| HIGH | Hardcoded Light Limits | Light array sizes are compile-time constants. Should be configurable per scene/platform. |
| HIGH | No Custom Render Pass Support | Third-party render passes can't be injected into the pipeline. Need a render graph system. |
| HIGH | Descriptor Binding Magic Numbers | Descriptor set bindings use literal integers throughout. Should be named constants or an enum. |

### Configuration & Extensibility

| Priority | Issue | Description |
|----------|-------|-------------|
| MEDIUM | VSync Toggle Disabled | VSync toggle in editor UI is disabled due to swapchain sync issues. Should be fixed. |
| MEDIUM | RT Denoiser Not Integrated | SVGF denoiser is implemented but not wired into the compositor pipeline. |
| MEDIUM | Pool Buffer BLAS Missing | Acceleration structure manager doesn't handle object pooling (reused entities). |
| MEDIUM | No Asset Import Plugin API | Third-party asset formats can't be added without engine modification. |
| MEDIUM | Hardcoded Particle Limits | Max 16384 particles per system is a compile-time constant. |
| MEDIUM | No Scene Transition API | Scene loading is immediate. No built-in transition/loading screen system. |
| MEDIUM | Build Pipeline Not Extensible | Build steps are hardcoded. No plugin hooks for custom build steps. |
| MEDIUM | No Telemetry/Analytics Framework | No way to collect performance or usage metrics in shipped games. |

---

## Findings Added 2026-04-12

### C7. Heap Buffer Overflow in cgltf Path Concatenation [OPEN]
**File:** `Engine/include/cgltf.h:1288-1292`
**Risk:** Remote code execution via malicious glTF file
**Description:** `cgltf_combine_paths()` uses `strcpy()` without bounds checking. The buffer is allocated as `strlen(uri) + strlen(gltf_path) + 1` but doesn't account for the prefix bytes from the base path directory. A crafted glTF file with a long texture URI combined with a deep directory path overflows the heap buffer.
**Status:** Open — third-party header. Requires upstream patch or local override with `snprintf`.

### H13. Command Injection via system() in OpenInExplorer [RESOLVED]
**File:** `Engine/src/Editor/EditorLayerProjectHub.cpp:180-191`
**Risk:** Arbitrary code execution on macOS/Linux
**Description:** `std::system("open \"" + folderPath + "\"")` passed user-controlled folder path through a shell. A project folder named `foo"; rm -rf /; #` would execute arbitrary commands.
**Fix Applied:** Replaced with `fork()`/`execlp()` which passes the path as a direct exec argument, bypassing shell interpretation. Windows was already safe (`ShellExecuteA`).
**Date Fixed:** 2026-04-12

### M13. TOCTOU Race in Project Duplication [OPEN]
**File:** `Engine/src/Editor/EditorLayerProjectHub.cpp:205-212`
**Risk:** Symlink attack / arbitrary file write
**Description:** `DuplicateProject` checks `fs::exists(destDir)` then calls `fs::copy()`. Between the check and the copy, an attacker could create `destDir` as a symlink to a system directory. Requires local access and timing.
**Status:** Open — low priority (local-only, requires attacker on same machine).

---

## Recommended Priority Order

1. **Immediate (before any release):** C1-C3 (serialization crashes), C4 (device lost deadlock), H2 (path traversal), H10-H11 (renderer stability) — **ALL RESOLVED** (Feb 2026 hardening sprint)
2. **Before multiplayer release:** C5-C6 (networking DoS/RPC), H5-H7 (authentication/spoofing), M8 (replay protection) — **ALL RESOLVED** (Feb 2026 hardening sprint, HMAC-SHA256 + replay window + rate limiting added)
3. **Before commercial release:** H3-H4 (memory exhaustion), H8-H9 (script sandbox), H12 (encryption upgrade), M10-M11 (HTTP security) — **MOSTLY RESOLVED** (vertex/index/entity caps added)
4. **Open:** C7 (cgltf buffer overflow — third-party), M13 (TOCTOU in project duplication)
5. **Architecture:** Plan backend abstractions for renderer, physics, and audio as major milestone work
