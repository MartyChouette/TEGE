# Test Fixtures

Binary asset files that can't be generated at runtime (so the golden tests in
`Tests/Unit/Assets/TestGoldenVerification.cpp` carry them as fixtures).

## `humanrig.fbx` (expected — not yet committed)

Drives `GoldenFBX.RiggedMeshImports`. The test **skips** until this file exists,
then asserts the full FBX skinned-mesh import path: mesh, skeleton, skinning,
and animation.

Requirements — the export must contain an actual **skinned mesh**, not just an
armature. `AssimpLoader::Load` rejects mesh-less FBX (Assimp marks them
`INCOMPLETE`), so a rig-only or animation-only export will not load.

Blender export recipe:
1. A mesh (a cube is fine) with an **armature of ≥2 bones**, skinned to it
   (parent with automatic weights).
2. One short **animation action** (e.g. rotate a bone over ~30 frames).
3. `File → Export → FBX` with **Armature + Mesh** selected and **Bake Animation**
   on. Save as `Tests/Fixtures/humanrig.fbx`.

Keep it small (a few hundred KB max) — it lives in the repo.
