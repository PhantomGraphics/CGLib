#pragma once

#include <vulkan/vulkan.h>

#include <array>

namespace VKG {

// Command-recording pieces of VkAppBase::drawFrame that depend only on the swapchain extent, so
// the pure parts can be unit-tested without a GPU.

// Clear values for the swapchain render pass: attachment 0 colour, attachment 1 depth/stencil.
std::array<VkClearValue, 2> swapchainClearValues();

// Viewport / scissor covering the whole extent (depth range 0..1).
VkViewport fullViewport(VkExtent2D extent);
VkRect2D   fullScissor(VkExtent2D extent);

// Begins `renderPass` on `framebuffer` (inline contents), then sets the dynamic viewport and
// scissor to the full extent so renderers can rely on both being set.
void beginSwapchainRenderPass(VkCommandBuffer cmd, VkRenderPass renderPass,
                              VkFramebuffer framebuffer, VkExtent2D extent);

} // namespace VKG
