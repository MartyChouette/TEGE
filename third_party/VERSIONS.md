# Vendored third-party versions

These libraries are vendored in-tree so a fresh clone builds with no extra
steps. The CMakeLists.txt in each directory is OURS (an Enjin build wrapper),
not upstream's.

| Library | Upstream | Pinned version |
|---|---|---|
| imgui | https://github.com/ocornut/imgui | 60d7fb207eeb46d6363dd4bde10b35991bae0ce7 (docking-era master, 2026-02) |
| angelscript | https://www.angelcode.com/angelscript/ | SDK vendored 2026 (see source/as_config.h ANGELSCRIPT_VERSION) |
| nanosvg | https://github.com/memononen/nanosvg | vendored 2026 |
| imguizmo | https://github.com/CedricGuillemet/ImGuizmo | vendored 2026 |

## Updating imgui

imgui carries a REQUIRED local patch: `patches/imgui-mrt-colorattachmentcount.patch`
(the Vulkan backend must set colorAttachmentCount for the swapchain MRT pass, or
every ImGui draw violates VUID-07609). After replacing imgui with a newer
version, re-apply the patch and verify ImGuiLayer's
IMGUI_IMPL_VULKAN_HAS_COLOR_ATTACHMENT_COUNT guard still detects it.

The original working clone's .git was parked as `imgui/.git-local` (ignored) so
the exact checkout history is preserved locally without git treating the
directory as an embedded repo.
