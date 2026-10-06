#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace Phantom::VKG {

class VulkanContext;

/// A VkPipelineCache that can optionally be persisted to `<dir>/vulkan_pipeline_cache.bin`.
///
/// Always usable in-process (reuse across pipelines is free). With a non-empty directory the blob
/// from a previous run is loaded in create() and written back in destroy(). A blob from another
/// GPU/driver, or a corrupt one, is treated as a plain miss, never an error. Creation failure is
/// non-fatal: get() stays VK_NULL_HANDLE, which vkCreate*Pipelines accepts.
///
/// create() on a live object persists and releases the previous cache first.
class VulkanPipelineCache {
public:
    VulkanPipelineCache() = default;
    VulkanPipelineCache(const VulkanPipelineCache&) = delete;
    VulkanPipelineCache& operator=(const VulkanPipelineCache&) = delete;
    ~VulkanPipelineCache() = default;

    /// @param directory Empty = in-memory only.
    /// @return false if vkCreatePipelineCache failed (get() is then VK_NULL_HANDLE).
    bool create(const VulkanContext& ctx, const std::string& directory = {});

    /// Persists to disk first when a directory was given. Safe on a never-created object.
    void destroy(VkDevice device);

    VkPipelineCache get() const { return cache_; }
    bool isValid() const { return cache_ != VK_NULL_HANDLE; }

    /// Standard VkPipelineCacheHeaderVersionOne sanity check against `props`.
    static bool headerMatches(const std::vector<char>& data, const VkPhysicalDeviceProperties& props);

    static std::string filePath(const std::string& directory);

private:
    VkPipelineCache cache_ = VK_NULL_HANDLE;
    std::string     directory_;
};

} // namespace Phantom::VKG
