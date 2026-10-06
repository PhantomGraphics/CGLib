#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "../../VulkanGraphics/VulkanPipeline.h"

namespace Phantom::Gltf {

    // Pool of per-material shader-override pipelines (.phmat), keyed by what a pipeline actually
    // depends on: the compiled fragment SPIR-V plus the fixed-function state derived from the
    // material. The same .phmat applied to several materials (or documents) therefore shares one
    // VkPipeline instead of building a redundant one per call.
    //
    // The pool owns every pipeline it was given; pointers returned by find()/add() stay valid
    // until destroyAll() (entries are never removed individually -- another material or a later
    // document may still use them).
    class GltfPipelineVariantPool {
    public:
        struct Key {
            uint64_t        fragSpvHash = 0; // FNV-1a 64 over the SPIR-V words -- a cache key, not a security boundary
            VkCullModeFlags cullMode    = VK_CULL_MODE_BACK_BIT;
            bool            blendEnable = false;
            bool            depthWrite  = true;
            bool operator==(const Key& o) const {
                return fragSpvHash == o.fragSpvHash && cullMode == o.cullMode &&
                       blendEnable == o.blendEnable && depthWrite == o.depthWrite;
            }
        };
        struct KeyHash {
            size_t operator()(const Key& k) const;
        };

        // FNV-1a 64 over the raw bytes of the SPIR-V words.
        static uint64_t hashSpirv(const uint32_t* words, size_t wordCount);
        static uint64_t hashSpirv(const std::vector<uint32_t>& spv) { return hashSpirv(spv.data(), spv.size()); }

        Phantom::VKG::VulkanPipeline* find(const Key& key) const;

        // Takes ownership. If `key` already exists the existing pipeline is kept, the new one is
        // destroyed with `device` and the existing pointer is returned. nullptr input is rejected.
        Phantom::VKG::VulkanPipeline* add(const Key& key, std::unique_ptr<Phantom::VKG::VulkanPipeline> pipeline,
                                          VkDevice device);

        size_t size() const { return pool_.size(); }

        void destroyAll(VkDevice device);

    private:
        std::vector<std::unique_ptr<Phantom::VKG::VulkanPipeline>> pool_;
        std::unordered_map<Key, Phantom::VKG::VulkanPipeline*, KeyHash> index_;
    };

} // namespace Phantom::Gltf
