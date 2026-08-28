#pragma once

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <vector>

namespace Phantom {
    namespace Volume {

        struct PBVRVertex {
            glm::vec3 pos;
            glm::vec4 color;
        };

        class PBVRPipeline {
        public:
            // Field order matches the std140 layout of the UBO block in pbvr_render.vert/.frag:
            // mat4 (16-byte aligned) boundaries land on multiples of 16 as long as scalar runs
            // between them stay a multiple of 4 floats. Keep that invariant when extending this.
            struct UBO {
                glm::mat4 mvp;
                float particleSize = 4.0f;
                float _pad0 = 0.0f;
                float _pad1 = 0.0f;
                float _pad2 = 0.0f;
                // Self-shadow (experimental, opt-in via enableShadowSampler). Unused/zero-initialised
                // when the shadow sampler is not enabled (e.g. GSView, which only ever sets mvp/particleSize).
                glm::mat4 lightVP      = glm::mat4(1.0f);
                float     sigma        = 1.0f;
                float     layerCount   = 8.0f;
                float     shadowEnabled = 0.0f;
                float     _pad3        = 0.0f;
            };

            // enableAlphaBlend:    opts into standard alpha blending instead of the default opaque
            //                      point-sprite draw (GSView keeps the default: false).
            // enableShadowSampler: adds a binding=1 combined-image-sampler (sampler2DArray) for the
            //                      Opacity Shadow Map. Caller must call updateShadowMap() for every
            //                      frame in flight before the first draw once enabled (see
            //                      PBVRRenderer::onInit()) -- Vulkan validation requires every
            //                      declared binding to be written at least once before use.
            void create(const ::VKG::VulkanContext& ctx, VkRenderPass renderPass, uint32_t framesInFlight,
                std::vector<uint32_t> vertSpv, std::vector<uint32_t> fragSpv,
                bool enableAlphaBlend = false, bool enableShadowSampler = false);
            void destroy(VkDevice device);
            void updateUBO(uint32_t frameIndex, const UBO& ubo);
            void updateShadowMap(uint32_t frameIndex, VkImageView arrayView, VkSampler sampler);

            VkPipeline getPipeline() const { return pipeline_.getPipeline(); }
            VkPipelineLayout getLayout() const { return pipeline_.getLayout(); }
            VkDescriptorSet getDescriptorSet(uint32_t frameIndex) const { return sets_[frameIndex]; }

        private:
            VkDevice device_ = VK_NULL_HANDLE;
            bool shadowSamplerEnabled_ = false;
            ::VKG::VulkanDescriptorSetLayout dsl_;
            ::VKG::VulkanDescriptorPool pool_;
            std::vector<VkDescriptorSet> sets_;
            ::VKG::VulkanPipeline pipeline_;
            std::vector<::VKG::VulkanBuffer> ubos_;
        };

    }
} // namespace Phantom::Volume
