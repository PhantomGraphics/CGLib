#include "VulkanBuffer.h"
#include "VulkanContext.h"
#include "VulkanCommandPool.h"
#include "detail/VkCheckInternal.h"

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

#include <cstring>

namespace Phantom::VKG {

bool VulkanBuffer::create(const VulkanContext& ctx, const VulkanCommandPool& pool,
                           VkDeviceSize size, VkBufferUsageFlags usage,
                           const void* initialData)
{
    allocator_ = ctx.getAllocator();
    size_      = size;

    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = initialData ? (usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT) : usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VKG_CHECK(vmaCreateBuffer(allocator_, &bi, &allocCI, &buffer_, &alloc_, nullptr),
              "Failed to create buffer", false);

    if (initialData) {
        // ステージングバッファ経由でデバイスローカルバッファにコピー
        VkBuffer         stagingBuf;
        VmaAllocation    stagingAlloc;

        VkBufferCreateInfo stagingBI{};
        stagingBI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBI.size        = size;
        stagingBI.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo stagingAllocCI{};
        stagingAllocCI.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

        if (vmaCreateBuffer(allocator_, &stagingBI, &stagingAllocCI,
                            &stagingBuf, &stagingAlloc, nullptr) != VK_SUCCESS) {
            std::fprintf(stderr, "[VKG] Failed to create staging buffer\n");
            vmaDestroyBuffer(allocator_, buffer_, alloc_);
            buffer_ = VK_NULL_HANDLE;
            alloc_  = nullptr;
            return false;
        }

        void* data;
        vmaMapMemory(allocator_, stagingAlloc, &data);
        memcpy(data, initialData, static_cast<size_t>(size));
        vmaUnmapMemory(allocator_, stagingAlloc);

        copyBuffer(ctx, pool, stagingBuf, buffer_, size);

        vmaDestroyBuffer(allocator_, stagingBuf, stagingAlloc);
    }
    return true;
}

bool VulkanBuffer::createMapped(const VulkanContext& ctx,
                                 VkDeviceSize size, VkBufferUsageFlags usage)
{
    allocator_ = ctx.getAllocator();
    size_      = size;

    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_AUTO;
    allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo{};
    VKG_CHECK(vmaCreateBuffer(allocator_, &bi, &allocCI, &buffer_, &alloc_, &allocInfo),
              "Failed to create mapped buffer", false);

    mapped_ = allocInfo.pMappedData;
    return true;
}

bool VulkanBuffer::upload(const VulkanContext& ctx, const VulkanCommandPool& pool,
                           const void* data, VkDeviceSize size)
{
    VkBuffer      stagingBuf;
    VmaAllocation stagingAlloc;

    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_AUTO;
    allocCI.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    VKG_CHECK(vmaCreateBuffer(allocator_, &bi, &allocCI, &stagingBuf, &stagingAlloc, nullptr),
              "Failed to create staging buffer for upload", false);

    void* mapped;
    vmaMapMemory(allocator_, stagingAlloc, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vmaUnmapMemory(allocator_, stagingAlloc);

    copyBuffer(ctx, pool, stagingBuf, buffer_, size);

    vmaDestroyBuffer(allocator_, stagingBuf, stagingAlloc);
    return true;
}

void VulkanBuffer::write(const void* data, VkDeviceSize size) {
    if (mapped_) memcpy(mapped_, data, static_cast<size_t>(size));
}

void VulkanBuffer::destroy(VkDevice /*device*/) {
    mapped_ = nullptr;
    if (buffer_ != VK_NULL_HANDLE && allocator_ != nullptr) {
        vmaDestroyBuffer(allocator_, buffer_, alloc_);
        buffer_    = VK_NULL_HANDLE;
        alloc_     = nullptr;
        allocator_ = nullptr;
    }
    size_ = 0;
}

// ============================================================
//  髱咏噪繝ｦ繝ｼ繝・ぅ繝ｪ繝・ぅ
// ============================================================

void VulkanBuffer::copyBuffer(const VulkanContext& ctx, const VulkanCommandPool& pool,
                               VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    auto cmd = pool.beginSingleTimeCommands();
    VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    pool.endSingleTimeCommands(cmd);
}

} // namespace VKG
