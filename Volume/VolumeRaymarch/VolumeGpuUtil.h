#pragma once

// Small Vulkan helpers shared by the VolumeRaymarch GPU classes (internal to this module).

#include "../../VulkanGraphics/VulkanContext.h"

#include <vulkan/vulkan.h>

namespace Phantom::Volume::detail {

/** @brief Image layout / access transition (both layouts may be equal for a pure memory barrier). */
inline void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to,
                         VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                         VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkImageMemoryBarrier b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    b.srcAccessMask = srcAccess;
    b.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

/** @brief Buffer / global memory dependency. */
inline void memoryBarrier(VkCommandBuffer cmd, VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                          VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage)
{
    VkMemoryBarrier b{ VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, srcAccess, dstAccess };
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 1, &b, 0, nullptr, 0, nullptr);
}

/** @brief Host-visible, coherent, mapped scratch buffer for staging / readback. */
struct HostBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;

    bool create(const Phantom::VKG::VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage)
    {
        VkDevice device = ctx.getDevice();
        VkBufferCreateInfo bi{};
        bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size = size;
        bi.usage = usage;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &bi, nullptr, &buffer) != VK_SUCCESS) return false;
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device, buffer, &req);
        auto type = ctx.findMemoryType(req.memoryTypeBits,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (!type) return false;
        VkMemoryAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize = req.size;
        ai.memoryTypeIndex = *type;
        if (vkAllocateMemory(device, &ai, nullptr, &memory) != VK_SUCCESS) return false;
        vkBindBufferMemory(device, buffer, memory, 0);
        return vkMapMemory(device, memory, 0, size, 0, &mapped) == VK_SUCCESS;
    }

    void destroy(const Phantom::VKG::VulkanContext& ctx)
    {
        VkDevice device = ctx.getDevice();
        if (mapped) vkUnmapMemory(device, memory);
        if (buffer) vkDestroyBuffer(device, buffer, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
        *this = HostBuffer();
    }
};

} // namespace Phantom::Volume::detail
