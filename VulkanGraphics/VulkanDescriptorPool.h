#pragma once

#include <vulkan/vulkan.h>

#include <cstdio>
#include <vector>

namespace Phantom::VKG {

class VulkanDescriptorSetLayout {
public:
    VulkanDescriptorSetLayout() = default;
    VulkanDescriptorSetLayout(const VulkanDescriptorSetLayout&) = delete;
    VulkanDescriptorSetLayout& operator=(const VulkanDescriptorSetLayout&) = delete;
    ~VulkanDescriptorSetLayout() = default;

    bool create(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings) {
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(bindings.size());
        ci.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &ci, nullptr, &layout_) != VK_SUCCESS) {
            std::fprintf(stderr, "[VKG] Failed to create descriptor set layout\n");
            return false;
        }
        return true;
    }

    void destroy(VkDevice device) {
        if (!layout_) return;
        vkDestroyDescriptorSetLayout(device, layout_, nullptr);
        layout_ = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout get() const { return layout_; }
    bool isValid() const { return layout_ != VK_NULL_HANDLE; }

private:
    VkDescriptorSetLayout layout_ = VK_NULL_HANDLE;
};

class VulkanDescriptorPool {
public:
    VulkanDescriptorPool() = default;
    VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
    ~VulkanDescriptorPool() = default;

    bool create(VkDevice device,
                const std::vector<VkDescriptorPoolSize>& poolSizes,
                uint32_t maxSets,
                VkDescriptorPoolCreateFlags flags = 0)
    {
        VkDescriptorPoolCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        ci.flags = flags;
        ci.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        ci.pPoolSizes = poolSizes.data();
        ci.maxSets = maxSets;

        if (vkCreateDescriptorPool(device, &ci, nullptr, &pool_) != VK_SUCCESS) {
            std::fprintf(stderr, "[VKG] Failed to create descriptor pool\n");
            return false;
        }
        return true;
    }

    std::vector<VkDescriptorSet> allocateSets(
        VkDevice device,
        const std::vector<VkDescriptorSetLayout>& layouts) const
    {
        std::vector<VkDescriptorSet> sets(layouts.size());

        VkDescriptorSetAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool = pool_;
        ai.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        ai.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(device, &ai, sets.data()) != VK_SUCCESS) {
            std::fprintf(stderr, "[VKG] Failed to allocate descriptor sets\n");
            return {};
        }

        return sets;
    }

    void destroy(VkDevice device) {
        if (!pool_) return;
        vkDestroyDescriptorPool(device, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }

    VkDescriptorPool get() const { return pool_; }
    bool isValid() const { return pool_ != VK_NULL_HANDLE; }

private:
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
};

} // namespace VKG

namespace VKG {
using namespace Phantom::VKG;
}
