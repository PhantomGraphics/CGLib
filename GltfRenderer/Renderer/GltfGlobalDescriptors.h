#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "../../VulkanGraphics/VulkanDescriptorPool.h"

namespace Phantom::Gltf {

    // Descriptor layouts, pools and per-frame global sets of GltfSceneRenderer, split out so the
    // binding table and the pool/set lifetime live in one place.
    //
    //  set=0 (global, one set per frame in flight, document-independent):
    //    0 GlobalUBO (vert+frag)   1 irradiance cube   2 prefiltered cube   3 BRDF LUT
    //    4 shadow map              5 BoneUBO (vert)    6 LightBufferGpu     7 volume shadow array
    //    8 optional scalar-field SSBO (vert+frag)
    //  set=1 (per material, document-dependent pool):
    //    0 MaterialUBO             1-5 five textures
    //
    // Which image goes into each slot (real IBL vs fallback, real shadow vs fallback, ...) is
    // decided by the renderer and passed in as already-resolved FrameInputs.
    class GltfGlobalDescriptors {
    public:
        struct ImageBinding {
            VkSampler     sampler = VK_NULL_HANDLE;
            VkImageView   view    = VK_NULL_HANDLE;
            VkImageLayout layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        };
        struct BufferBinding {
            VkBuffer     buffer = VK_NULL_HANDLE;
            VkDeviceSize range  = 0;
        };
        struct FrameInputs {
            BufferBinding globalUbo, boneUbo, lightUbo;
            ImageBinding  irradiance, prefiltered, brdfLut, shadow;
            ImageBinding  volumeShadow; // view == VK_NULL_HANDLE leaves binding 7 unwritten
        };

        static std::vector<VkDescriptorSetLayoutBinding> globalBindings();
        static std::vector<VkDescriptorSetLayoutBinding> materialBindings();

        // Creates both layouts, the global pool and `frameCount` global sets. On failure nothing
        // stays allocated and false is returned. Calling it again on a live object re-creates.
        bool create(VkDevice device, uint32_t frameCount);
        void destroy(VkDevice device);

        // Per-document material pool: `materialCount * frameCount` sets of the material layout.
        bool createMaterialPool(VkDevice device, uint32_t materialCount);
        void destroyMaterialPool(VkDevice device);

        void updateFrame(VkDevice device, uint32_t frame, const FrameInputs& in) const;
        void updateScalarField(VkDevice device, uint32_t frame, BufferBinding buffer) const;

        VkDescriptorSetLayout  globalLayout() const   { return globalLayout_.get(); }
        VkDescriptorSetLayout  materialLayout() const { return materialLayout_.get(); }
        VkDescriptorPool       materialPool() const   { return materialPool_.get(); }
        const VkDescriptorSet* globalSet(uint32_t frame) const { return &globalSets_[frame]; }
        uint32_t               frameCount() const { return frameCount_; }

    private:
        Phantom::VKG::VulkanDescriptorSetLayout globalLayout_;
        Phantom::VKG::VulkanDescriptorSetLayout materialLayout_;
        Phantom::VKG::VulkanDescriptorPool      globalPool_;
        Phantom::VKG::VulkanDescriptorPool      materialPool_;
        std::vector<VkDescriptorSet>            globalSets_;
        uint32_t                                frameCount_ = 0;
    };

} // namespace Phantom::Gltf
