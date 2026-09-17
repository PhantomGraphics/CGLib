#pragma once

#include <vulkan/vulkan.h>

namespace Phantom::VKG::DebugUtils {

// Thin wrappers over VK_EXT_debug_utils (vkCmdBeginDebugUtilsLabelEXT / vkCmdEndDebugUtilsLabelEXT /
// vkCmdInsertDebugUtilsLabelEXT / vkSetDebugUtilsObjectNameEXT), so a renderer can label a draw
// region or name a Vulkan object (for RenderDoc/Nsight/PIX capture) without every call site
// checking whether the extension is actually enabled first. VulkanContext.cpp only appends
// VK_EXT_DEBUG_UTILS_EXTENSION_NAME when validation is on, which is the same "debug-tooling-only"
// gate this file's callers should expect -- every function here is a silent no-op (the entry point
// is simply never resolved) when the instance extension is unavailable, so call sites never need
// their own #ifdef/feature check.
//
// Entry points are resolved via vkGetInstanceProcAddr() once per distinct VkInstance and cached
// (this codebase creates at most one VkInstance per process, so this is not meant as a general
// multi-instance cache, just enough to avoid re-resolving on every call).

void beginLabel(VkInstance instance, VkCommandBuffer cmd, const char* name,
                 float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f);
void endLabel(VkInstance instance, VkCommandBuffer cmd);
void insertLabel(VkInstance instance, VkCommandBuffer cmd, const char* name,
                  float r = 1.f, float g = 1.f, float b = 1.f, float a = 1.f);

// objectHandle is the raw Vulkan handle (e.g. reinterpret_cast<uint64_t>(vkPipeline) or a
// non-dispatchable handle's value) cast to uint64_t -- VkDebugUtilsObjectNameInfoEXT's own shape.
void setObjectName(VkInstance instance, VkDevice device, VkObjectType objectType,
                    uint64_t objectHandle, const char* name);

} // namespace Phantom::VKG::DebugUtils
