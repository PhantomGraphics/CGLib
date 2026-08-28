#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "CameraUBO.h"
#include "../Gltf/GltfDocument.h"
#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanSampler.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

namespace Phantom::VKG { class VulkanContext; class VulkanCommandPool; }

namespace Phantom::Gltf
{

    // Holds GPU resources for a single glTF material
    class GltfGpuMaterial {
    public:
        static constexpr uint32_t TEXTURE_SLOT_COUNT = 5;

        // Placeholder 1x1 white texture (shared fallback)
        static void createFallback(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
            VkImage& outImage, VkDeviceMemory& outMemory,
            VkImageView& outView);

        // Returns false if descriptor set allocation failed (UBOs/textures are still created;
        // the returned descriptor sets are simply left empty in that case).
        bool build(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
            const GltfDocument& doc,
            const GltfMaterial& gltfMat,
            VkDescriptorSetLayout layout,
            VkDescriptorPool      descPool,
            VkImageView  fallbackView,
            VkSampler    fallbackSampler);

        void destroy(VkDevice device);

        VkDescriptorSet descriptorSet(uint32_t frameIndex) const { return descriptorSets_[frameIndex]; }
        bool            doubleSided()                       const { return doubleSided_; }

    private:
        static constexpr int MAX_FRAMES = 2;

        std::array<Phantom::VKG::VulkanBuffer, MAX_FRAMES>  ubos_;

        std::array<VkImage, TEXTURE_SLOT_COUNT> texImages_{};
        std::array<VkDeviceMemory, TEXTURE_SLOT_COUNT> texMemory_{};
        std::array<VkImageView, TEXTURE_SLOT_COUNT> texViews_{};
        std::array<Phantom::VKG::VulkanSampler, TEXTURE_SLOT_COUNT> samplers_;

        std::vector<VkDescriptorSet> descriptorSets_;
        bool doubleSided_ = false;

        VkImageView fallbackView_ = VK_NULL_HANDLE;
        VkSampler   fallbackSampler_ = VK_NULL_HANDLE;

        bool uploadTexture(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
            const GltfDocument& doc,
            const GltfTextureInfo& texInfo,
            uint32_t slotIndex,
            VkImageView& outView,
            Phantom::VKG::VulkanSampler& outSampler);
    };

}