#include "GltfGlobalDescriptors.h"

namespace Phantom::Gltf {

namespace {

constexpr VkDescriptorType   kUbo     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
constexpr VkDescriptorType   kSampler = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
constexpr VkShaderStageFlags kFrag    = VK_SHADER_STAGE_FRAGMENT_BIT;
constexpr VkShaderStageFlags kVert    = VK_SHADER_STAGE_VERTEX_BIT;

VkDescriptorSetLayoutBinding binding(uint32_t index, VkDescriptorType type, VkShaderStageFlags stages)
{
    VkDescriptorSetLayoutBinding b{};
    b.binding         = index;
    b.descriptorType  = type;
    b.descriptorCount = 1;
    b.stageFlags      = stages;
    return b;
}

VkWriteDescriptorSet baseWrite(VkDescriptorSet set, uint32_t index, VkDescriptorType type)
{
    VkWriteDescriptorSet w{};
    w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet          = set;
    w.dstBinding      = index;
    w.descriptorType  = type;
    w.descriptorCount = 1;
    return w;
}

} // namespace

std::vector<VkDescriptorSetLayoutBinding> GltfGlobalDescriptors::globalBindings()
{
    return {
        binding(0, kUbo,     kVert | kFrag),
        binding(1, kSampler, kFrag),
        binding(2, kSampler, kFrag),
        binding(3, kSampler, kFrag),
        binding(4, kSampler, kFrag),
        binding(5, kUbo,     kVert),
        binding(6, kUbo,     kFrag), // LightManager::LightBufferGpu
        binding(7, kSampler, kFrag), // volume shadow
    };
}

std::vector<VkDescriptorSetLayoutBinding> GltfGlobalDescriptors::materialBindings()
{
    return {
        binding(0, kUbo,     kFrag),
        binding(1, kSampler, kFrag),
        binding(2, kSampler, kFrag),
        binding(3, kSampler, kFrag),
        binding(4, kSampler, kFrag),
        binding(5, kSampler, kFrag),
    };
}

bool GltfGlobalDescriptors::create(VkDevice device, uint32_t frameCount)
{
    destroy(device);
    frameCount_ = frameCount;

    if (!globalLayout_.create(device, globalBindings()) ||
        !materialLayout_.create(device, materialBindings())) {
        destroy(device);
        return false;
    }

    // frameCount sets: 3 UBOs (GlobalUBO + BoneUBO + LightBufferGpu) + 5 combined image samplers
    // (irradiance/prefiltered/brdfLUT/shadowMap/volumeShadow) each.
    const std::vector<VkDescriptorPoolSize> sizes = {
        {kUbo,     frameCount * 3},
        {kSampler, frameCount * 5},
    };
    if (!globalPool_.create(device, sizes, frameCount)) {
        destroy(device);
        return false;
    }

    globalSets_ = globalPool_.allocateSets(device,
        std::vector<VkDescriptorSetLayout>(frameCount, globalLayout_.get()));
    if (globalSets_.size() != frameCount) {
        destroy(device);
        return false;
    }
    return true;
}

void GltfGlobalDescriptors::destroy(VkDevice device)
{
    destroyMaterialPool(device);
    globalSets_.clear(); // freed together with the pool
    globalPool_.destroy(device);
    globalLayout_.destroy(device);
    materialLayout_.destroy(device);
    frameCount_ = 0;
}

bool GltfGlobalDescriptors::createMaterialPool(VkDevice device, uint32_t materialCount)
{
    const uint32_t totalSets = materialCount * frameCount_;
    const std::vector<VkDescriptorPoolSize> sizes = {
        {kUbo,     1 * totalSets}, // MaterialUBO
        {kSampler, 5 * totalSets}, // 5 textures
    };
    return materialPool_.create(device, sizes, totalSets);
}

void GltfGlobalDescriptors::destroyMaterialPool(VkDevice device)
{
    materialPool_.destroy(device);
}

void GltfGlobalDescriptors::updateFrame(VkDevice device, uint32_t frame, const FrameInputs& in) const
{
    const VkDescriptorSet set = globalSets_[frame];

    const VkDescriptorBufferInfo bufInfo[3] = {
        {in.globalUbo.buffer, 0, in.globalUbo.range},
        {in.boneUbo.buffer,   0, in.boneUbo.range},
        {in.lightUbo.buffer,  0, in.lightUbo.range},
    };
    auto img = [](const ImageBinding& b) { return VkDescriptorImageInfo{b.sampler, b.view, b.layout}; };
    const VkDescriptorImageInfo imgInfo[5] = {
        img(in.irradiance), img(in.prefiltered), img(in.brdfLut), img(in.shadow), img(in.volumeShadow),
    };

    std::vector<VkWriteDescriptorSet> writes;
    auto addBuffer = [&](uint32_t index, const VkDescriptorBufferInfo& info) {
        auto w = baseWrite(set, index, kUbo);
        w.pBufferInfo = &info;
        writes.push_back(w);
    };
    auto addImage = [&](uint32_t index, const VkDescriptorImageInfo& info) {
        auto w = baseWrite(set, index, kSampler);
        w.pImageInfo = &info;
        writes.push_back(w);
    };

    addBuffer(0, bufInfo[0]);
    addImage(1, imgInfo[0]);
    addImage(2, imgInfo[1]);
    addImage(3, imgInfo[2]);
    addImage(4, imgInfo[3]);
    if (in.volumeShadow.view != VK_NULL_HANDLE) addImage(7, imgInfo[4]);
    addBuffer(5, bufInfo[1]);
    addBuffer(6, bufInfo[2]);

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace Phantom::Gltf
