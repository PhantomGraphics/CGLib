#include "VulkanPipelineCache.h"
#include "VulkanContext.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace Phantom::VKG {

std::string VulkanPipelineCache::filePath(const std::string& directory)
{
    return (std::filesystem::path(directory) / "vulkan_pipeline_cache.bin").string();
}

// A cache blob saved on a different GPU/driver is, per spec, safe to feed back into
// vkCreatePipelineCache() (the implementation must discard incompatible entries), but checking the
// header first avoids even attempting that with a blob vkGetPipelineCacheData() would never have
// produced for this device. A mismatched/corrupt cache is a plain miss, not an error.
bool VulkanPipelineCache::headerMatches(const std::vector<char>& data, const VkPhysicalDeviceProperties& props)
{
    if (data.size() < 32) return false;
    uint32_t headerSize = 0, headerVersion = 0, vendorID = 0, deviceID = 0;
    std::memcpy(&headerSize,    data.data() + 0,  sizeof(uint32_t));
    std::memcpy(&headerVersion, data.data() + 4,  sizeof(uint32_t));
    std::memcpy(&vendorID,      data.data() + 8,  sizeof(uint32_t));
    std::memcpy(&deviceID,      data.data() + 12, sizeof(uint32_t));
    if (headerVersion != VK_PIPELINE_CACHE_HEADER_VERSION_ONE) return false;
    if (vendorID != props.vendorID || deviceID != props.deviceID) return false;
    if (headerSize < 32 || data.size() < headerSize) return false;
    return std::memcmp(data.data() + 16, props.pipelineCacheUUID, VK_UUID_SIZE) == 0;
}

bool VulkanPipelineCache::create(const VulkanContext& ctx, const std::string& directory)
{
    // Re-creating a live object releases the previous cache first (no leak).
    destroy(ctx.getDevice());
    directory_ = directory;

    std::vector<char> initialData;
    if (!directory_.empty()) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(ctx.getPhysicalDevice(), &props);
        std::ifstream in(filePath(directory_), std::ios::binary | std::ios::ate);
        if (in) {
            const std::streamsize size = in.tellg();
            if (size > 0) {
                initialData.resize(static_cast<size_t>(size));
                in.seekg(0);
                in.read(initialData.data(), size);
                if (!in || !headerMatches(initialData, props)) initialData.clear();
            }
        }
    }

    VkPipelineCacheCreateInfo ci{};
    ci.sType           = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    ci.initialDataSize = initialData.size();
    ci.pInitialData    = initialData.empty() ? nullptr : initialData.data();
    if (vkCreatePipelineCache(ctx.getDevice(), &ci, nullptr, &cache_) != VK_SUCCESS) {
        std::fprintf(stderr, "[VKG] Failed to create pipeline cache (continuing without)\n");
        cache_ = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

void VulkanPipelineCache::destroy(VkDevice device)
{
    if (cache_ == VK_NULL_HANDLE) return;
    if (!directory_.empty()) {
        size_t dataSize = 0;
        vkGetPipelineCacheData(device, cache_, &dataSize, nullptr);
        if (dataSize > 0) {
            std::vector<char> data(dataSize);
            if (vkGetPipelineCacheData(device, cache_, &dataSize, data.data()) == VK_SUCCESS) {
                std::error_code ec;
                std::filesystem::create_directories(directory_, ec);
                std::ofstream out(filePath(directory_), std::ios::binary | std::ios::trunc);
                if (out) out.write(data.data(), static_cast<std::streamsize>(dataSize));
            }
        }
    }
    vkDestroyPipelineCache(device, cache_, nullptr);
    cache_ = VK_NULL_HANDLE;
}

} // namespace Phantom::VKG
