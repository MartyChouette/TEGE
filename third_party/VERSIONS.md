# Vendored third-party versions

These libraries are vendored in-tree so a fresh clone builds with no extra
steps. The CMakeLists.txt in each directory is OURS (an Enjin build wrapper),
not upstream's.

| Library | Upstream | Pinned version |
|---|---|---|
| imgui | https://github.com/ocornut/imgui | v1.92.9b-docking (b48d1afbe8ee8b238e2961dc363a949dd7304e23) |
| angelscript | https://www.angelcode.com/angelscript/ | SDK vendored 2026 (see source/as_config.h ANGELSCRIPT_VERSION) |
| nanosvg | https://github.com/memononen/nanosvg | vendored 2026 |
| imguizmo | https://github.com/CedricGuillemet/ImGuizmo | 1.10 (b796ac3b861afc6e91ca74e4611effd9c9527367), `src/ImGuizmo.{h,cpp}` only |
| effekseer | https://github.com/effekseer/Effekseer | tag 1807 = 1.80.7 (b87d1a2e3ff3731ba25cad34f8d8abeae382b21f). `Dev/Cpp/{Effekseer, EffekseerRendererCommon, EffekseerRendererLLGI, EffekseerRendererVulkan, EffekseerRendererWebGPU}`, three folders of `EffekseerMaterialCompiler`, `3rdParty/stb_effekseer` |
| LLGI (inside effekseer/3rdParty) | https://github.com/altseed/LLGI | 8c476bdea911d99dc307655be3cad1cc2b8d5a1e, the submodule commit of Effekseer 1807. `src/` base files plus `Vulkan`, `WebGPU`, `Utils` |

## Updating imgui

imgui carries a REQUIRED local patch: `patches/imgui-mrt-colorattachmentcount.patch`
(the Vulkan backend must set colorAttachmentCount for the swapchain MRT pass, or
every ImGui draw violates VUID-07609). After replacing imgui with a newer
version, re-apply the patch and verify ImGuiLayer's
IMGUI_IMPL_VULKAN_HAS_COLOR_ATTACHMENT_COUNT guard still detects it.

The original working clone's .git was parked as `imgui/.git-local` (ignored) so
the exact checkout history is preserved locally without git treating the
directory as an embedded repo.

## Updating imguizmo

imguizmo carries two local fixes: `patches/imguizmo-rect-and-clip.patch` (a typo in
SetRect, and the gizmo clipped to its panel). Upstream 1.10 has neither. Copy the
two files out of upstream `src/` and re-apply the patch.

## Updating effekseer

The folders under `effekseer/` keep upstream's `Dev/Cpp` layout, because the sources
include each other by relative path (`../../Effekseer/...`, `../3rdParty/LLGI/src/...`).
Upstream's CMakeLists.txt files are deleted; `effekseer/CMakeLists.txt` is ours and
builds one backend per platform. LLGI carries one local patch,
`patches/llgi-mrt-colorwritemask.patch`: re-apply it after replacing LLGI.
