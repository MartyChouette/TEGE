# Hardening Sprint Summary (Beta 0.8.5)

**Date:** February 2026
**Duration:** 4-week focused hardening sprint
**Objective:** Resolve all audit findings, fix crash bugs, expand test coverage, and harden security before new platform targets.

## Results

### Week 1: Critical Safety & Security
- **S-C1:** Eliminated `std::system()` command injection in HTML5Exporter.cpp — replaced with `CreateProcess`/`posix_spawn`
- **S-C2:** Added `CollabPermission` (Owner/Editor/Viewer) to collaborative editing — destructive ops require Owner
- **S-C3/C4:** Network packet overflow guards verified (already present)
- **S-H10:** Sync response size cap (64MB) verified
- **S-M2:** Deserialized string cap (64KB) verified
- **S-M6:** Pending remote ops cap (10K) verified
- MeshFactory division-by-zero fixed (subdivisions/divisions clamped to >= 1)
- FeedbackSystem: `CreateBugReport`/`CreateFeedback` now return by ID to avoid dangling references
- ProjectionType enum range check added

### Week 2: Crash Bugs & Robustness
- ShaderGraph: `nodeMap[]` operator replaced with `find()` to prevent silent map mutation
- Prefab: Added `contains()`/`is_array()` guards for JSON access in `LoadFromFile`
- RT Pipeline: Added VkResult checks to 24 unchecked Vulkan API calls across 6 files (RTAmbientOcclusion, RTReflections, RTGlobalIllumination, PathTracer, RTTranslucency, RTCaustics)
- Verified already-resolved: SceneSerializer try/catch, EntityEventBus copy-before-dispatch, Spline size guards, ControllerSystem quaternion normalization, Audio handle wraparound, QuestFlow/VisualScript iterator safety, CoroutineScheduler O(N) fix

### Week 3: Test Coverage Expansion
- **New test files:** TestBuildPipeline.cpp (~33 tests), TestTemplateCreator.cpp (~39 tests), TestScriptBindings.cpp (~49 tests), TestHardeningRegression.cpp (~39 tests)
- **Expanded:** TestNetworking.cpp (+57 tests), TestBehaviorTree.cpp (+23 tests), TestVisualScript.cpp (+15 tests)
- **Total new tests:** ~255
- **New CTest targets:** 4 (TestBuildPipeline, TestTemplateCreator, TestScriptBindings, TestHardeningRegression)

### Week 4: Templates, Polish & Documentation
- Template validation: entity count warnings (0) and caps (10K), file size warnings (>10MB)
- Thumbnail generator: already uses proper box-filter averaging (no change needed)
- Audit documentation updated with resolution status
- This summary document created

## Files Modified

### Engine Source (Week 1-2 fixes)
| File | Change |
|------|--------|
| `Engine/src/Build/HTML5Exporter.cpp` | `std::system` → `CreateProcess`/`posix_spawn` |
| `Engine/include/Enjin/Editor/CollaborativeEditing.h` | Added `CollabPermission` enum to `CollabPeer` |
| `Engine/src/Editor/CollaborativeEditing.cpp` | Permission checks on edit ops |
| `Engine/src/Renderer/MeshFactory.cpp` | Clamped subdivisions/divisions to >= 1 |
| `Engine/include/Enjin/Editor/FeedbackSystem.h` | Changed Create return types to u64 |
| `Engine/src/Editor/FeedbackSystem.cpp` | Create functions return ID |
| `Engine/src/Editor/EditorLayerPanels.cpp` | Updated callers to use ID-based API |
| `Engine/src/Editor/EditorLayerComponents.cpp` | ProjectionType range check |
| `Engine/src/Editor/ShaderGraph.cpp` | `nodeMap[]` → `find()` |
| `Engine/src/Assets/Prefab.cpp` | JSON array validation guards |
| `Engine/src/Renderer/RayTracing/RTAmbientOcclusion.cpp` | VkResult checks |
| `Engine/src/Renderer/RayTracing/RTReflections.cpp` | VkResult checks |
| `Engine/src/Renderer/RayTracing/RTGlobalIllumination.cpp` | VkResult checks |
| `Engine/src/Renderer/RayTracing/PathTracer.cpp` | VkResult checks |
| `Engine/src/Renderer/RayTracing/RTTranslucency.cpp` | VkResult checks |
| `Engine/src/Renderer/RayTracing/RTCaustics.cpp` | VkResult checks |
| `Engine/src/Editor/TemplateCreator.cpp` | Entity count + file size validation |

### Test Files (Week 3)
| File | Tests |
|------|-------|
| `Tests/Unit/TestBuildPipeline.cpp` | ~33 new |
| `Tests/Unit/TestTemplateCreator.cpp` | ~39 new |
| `Tests/Unit/TestScriptBindings.cpp` | ~49 new |
| `Tests/Unit/TestHardeningRegression.cpp` | ~39 new |
| `Tests/Unit/TestNetworking.cpp` | +57 expanded |
| `Tests/Unit/TestBehaviorTree.cpp` | +23 expanded |
| `Tests/Unit/TestVisualScript.cpp` | +15 expanded |
| `Tests/CMakeLists.txt` | 4 new test targets |

## Audit Issue Status
All 22 originally reported audit issues are now resolved or verified as already-fixed.

## Follow-Up Audit (2026-04-12)
A comprehensive 6-dimension audit was conducted in April 2026 covering memory safety, thread safety, Vulkan renderer, security, logic bugs, and performance. 55 findings total, 5 critical/high fixes applied, 7 false positives verified. See `docs/AUDIT_2026_04_12.md` for the full report.
