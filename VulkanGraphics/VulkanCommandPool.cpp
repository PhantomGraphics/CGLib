#include "VulkanCommandPool.h"
#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

bool VulkanCommandPool::init(VulkanContext* ctx, VkSurfaceKHR surface) {
    // Re-creating a live object releases the previous handles first (no leak).
    destroy();
    ctx_ = ctx;
    auto indices = ctx_->findQueueFamilies(ctx_->getPhysicalDevice(), surface);
    if (!indices.graphicsFamily) {
        std::fprintf(stderr, "[VKG] VulkanCommandPool: no graphics queue family\n");
        return false;
    }

    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = indices.graphicsFamily.value();

    VKG_CHECK(vkCreateCommandPool(ctx_->getDevice(), &ci, nullptr, &pool_),
              "Failed to create command pool", false);
    return true;
}

void VulkanCommandPool::destroy() {
    if (pool_ && ctx_) {
        vkDestroyCommandPool(ctx_->getDevice(), pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
}

std::vector<VkCommandBuffer> VulkanCommandPool::allocateCommandBuffers(uint32_t count) const {
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = pool_;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = count;

    std::vector<VkCommandBuffer> bufs(count);
    if (vkAllocateCommandBuffers(ctx_->getDevice(), &ai, bufs.data()) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to allocate command buffers\n");
        return {};
    }
    return bufs;
}

void VulkanCommandPool::freeCommandBuffers(std::vector<VkCommandBuffer>& bufs) const {
    if (!bufs.empty()) {
        vkFreeCommandBuffers(ctx_->getDevice(), pool_,
                             (uint32_t)bufs.size(), bufs.data());
        bufs.clear();
    }
}

VkCommandBuffer VulkanCommandPool::beginSingleTimeCommands() const {
    if (!ctx_ || !pool_ || !ctx_->getDevice()) return VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = pool_;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(ctx_->getDevice(), &ai, &cmd) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] VulkanCommandPool: failed to allocate a single-time command buffer\n");
        return VK_NULL_HANDLE; // callers that ignore this crash on a null handle instead of using garbage
    }

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to begin single-time command buffer\n");
        vkFreeCommandBuffers(ctx_->getDevice(), pool_, 1, &cmd);
        return VK_NULL_HANDLE;
    }
    return cmd;
}

bool VulkanCommandPool::endSingleTimeCommands(VkCommandBuffer cmd) const {
    if (cmd == VK_NULL_HANDLE) return false;
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to end single-time command buffer\n");
        vkFreeCommandBuffers(ctx_->getDevice(), pool_, 1, &cmd);
        return false;
    }

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    if (vkQueueSubmit(ctx_->getGraphicsQueue(), 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to submit single-time command buffer\n");
        vkFreeCommandBuffers(ctx_->getDevice(), pool_, 1, &cmd);
        return false;
    }
    const VkResult result = vkQueueWaitIdle(ctx_->getGraphicsQueue());
    if (result != VK_SUCCESS)
        std::fprintf(stderr, "[VKG] Failed to wait for single-time commands\n");
    vkFreeCommandBuffers(ctx_->getDevice(), pool_, 1, &cmd);
    return result == VK_SUCCESS;
}

} // namespace VKG
