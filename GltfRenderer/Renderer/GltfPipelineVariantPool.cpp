#include "GltfPipelineVariantPool.h"

#include <functional>

namespace Phantom::Gltf {

size_t GltfPipelineVariantPool::KeyHash::operator()(const Key& k) const
{
    size_t h = std::hash<uint64_t>{}(k.fragSpvHash);
    h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(k.cullMode)) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(k.blendEnable) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= std::hash<bool>{}(k.depthWrite)  + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

uint64_t GltfPipelineVariantPool::hashSpirv(const uint32_t* words, size_t wordCount)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(words);
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < wordCount * sizeof(uint32_t); ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

Phantom::VKG::VulkanPipeline* GltfPipelineVariantPool::find(const Key& key) const
{
    auto it = index_.find(key);
    return it != index_.end() ? it->second : nullptr;
}

Phantom::VKG::VulkanPipeline* GltfPipelineVariantPool::add(
    const Key& key, std::unique_ptr<Phantom::VKG::VulkanPipeline> pipeline, VkDevice device)
{
    if (!pipeline) return nullptr;
    if (auto* existing = find(key)) {
        pipeline->destroy(device);
        return existing;
    }
    Phantom::VKG::VulkanPipeline* raw = pipeline.get();
    pool_.push_back(std::move(pipeline));
    index_.emplace(key, raw);
    return raw;
}

void GltfPipelineVariantPool::destroyAll(VkDevice device)
{
    for (auto& p : pool_) p->destroy(device);
    pool_.clear();
    index_.clear();
}

} // namespace Phantom::Gltf
