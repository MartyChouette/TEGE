# Renderer Audit — Beta 0.8 (2026-02-18)

**Status:** All 16 findings fixed.

## VulkanContext.cpp (6 fixes)

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| VK-C1 | CRIT | `vkEnumeratePhysicalDevices` unchecked | VkResult check + early return |
| VK-C2 | CRIT | `vkEnumerateInstanceLayerProperties` unchecked | VkResult check |
| VK-C3 | CRIT | `vkEnumerateDeviceExtensionProperties` unchecked | VkResult check |
| VK-H1 | HIGH | `vkGetPhysicalDeviceSurfaceSupportKHR` unchecked | VkResult check, skip on fail |
| VK-H2 | HIGH | `vkGetDeviceQueue` null handle not validated | Null check, fail if null |
| VK-H3 | HIGH | `vkDeviceWaitIdle` in Shutdown unchecked | Check and log |

## VulkanBuffer.cpp (7 fixes)

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| BUF-H1 | HIGH | No null context/device check in `Create()` | Guard at entry |
| BUF-H2 | HIGH | Zero-size buffer accepted | Reject with error |
| BUF-H3 | HIGH | Resource leak on `AllocateMemory` failure | Destroy buffer on fail |
| BUF-H4 | HIGH | Resource leak on `vkBindBufferMemory` failure | Full cleanup on fail |
| BUF-H5 | HIGH | `Destroy()` crashes if context null | Null guard + state reset |
| BUF-M1 | MED | `UploadData` integer overflow `size + offset` | Overflow-safe bounds check |
| BUF-M2 | MED | `Map()` double-maps persistent buffer | Return persistent first |

## VulkanSwapchain.cpp (3 fixes)

| ID | Sev | Description | Fix |
|----|-----|-------------|-----|
| SC-M1 | MED | `vkBindImageMemory` depth buffer unchecked | VkResult + cleanup on fail |
| SC-M2 | MED | `vkGetSwapchainImagesKHR` unchecked | VkResult check |
| SC-M3 | MED | `Recreate()` void — errors not propagated | Changed to `bool` |
