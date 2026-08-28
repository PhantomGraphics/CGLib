#include "VulkanCommandPool.h"
#include "VulkanContext.h"
#include "detail/VkCheckInternal.h"

namespace Phantom::VKG {

bool VulkanCommandPool::init(VulkanContext* ctx, VkSurfaceKHR surface) {
    ctx_ = ctx;
    auto indices = ctx_->findQueueFamilies(ctx_->getPhysicalDevice(), surface);

    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = indices.graphicsFamily.value();

    VKG_CHECK(vkCreateCommandPool(ctx_->getDevice(), &ci, nullptr, &pool_),
              "Failed to create command pool", false);
    return true;
}

void VulkanCommandPool::destroy() {
    if (pool_) {
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
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = pool_;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx_->getDevice(), &ai, &cmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void VulkanCommandPool::endSingleTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    vkQueueSubmit(ctx_->getGraphicsQueue(), 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx_->getGraphicsQueue());
    vkFreeCommandBuffers(ctx_->getDevice(), pool_, 1, &cmd);
}

} // namespace VKG
