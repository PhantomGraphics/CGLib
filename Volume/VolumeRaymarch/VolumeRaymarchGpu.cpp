#include "VolumeRaymarchGpu.h"

#include "../../VulkanGraphics/VulkanCommandPool.h"
#include "../../VulkanGraphics/VulkanContext.h"

#include <algorithm>
#include <cstdio>

namespace Phantom::Volume {

// std140 layout; keep in sync with volume_raymarch.frag.
struct VolumeRaymarchGpu::Params {
    glm::mat4 invViewProj;
    glm::vec4 camPos;
    glm::vec4 gridOrigin;   // xyz origin, w cellSize
    glm::vec4 gridDims;
    glm::vec4 sunDirIrr;
    glm::vec4 scatter;      // extinction, albedo, phaseG, ambient
    glm::vec4 march;        // stepLength, maxSteps, minTransmittance, tMax
};

namespace {
// Keep in sync with volume_sun_transmittance.comp.
struct SunPush {
    glm::vec4 gridOrigin;
    glm::vec4 gridDims;
    glm::vec4 sunDirExt;
    glm::vec4 step;
};

VkSampler makeBorderSampler(VkDevice device, VkBorderColor border)
{
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    si.borderColor = border;
    si.unnormalizedCoordinates = VK_FALSE;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VkSampler s = VK_NULL_HANDLE;
    return vkCreateSampler(device, &si, nullptr, &s) == VK_SUCCESS ? s : VK_NULL_HANDLE;
}
}

bool VolumeRaymarchGpu::create(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                               const Config& config)
{
    (void)pool;
    VkDevice device = ctx.getDevice();
    const uint32_t frames = std::max(1u, config.framesInFlight);

    zeroBorderSampler_ = makeBorderSampler(device, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK);
    oneBorderSampler_ = makeBorderSampler(device, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);
    if (!zeroBorderSampler_ || !oneBorderSampler_) return false;

    // Compute set: 0 = density sampler, 1 = sun-transmittance storage image.
    sunSetLayout_.create(device, {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr } });
    // Raymarch set: 0 = params UBO, 1 = density, 2 = sun transmittance.
    marchSetLayout_.create(device, {
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } });

    pool_.create(device, {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 + 2 * frames },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frames } }, 1 + frames);
    std::vector<VkDescriptorSetLayout> layouts{ sunSetLayout_.get() };
    layouts.insert(layouts.end(), frames, marchSetLayout_.get());
    std::vector<VkDescriptorSet> sets = pool_.allocateSets(device, layouts);
    if (sets.size() != layouts.size()) return false;
    sunSet_ = sets[0];
    marchSets_.assign(sets.begin() + 1, sets.end());

    ubos_.resize(frames);
    for (auto& ubo : ubos_) {
        if (!ubo.createMapped(ctx, sizeof(Params), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) return false;
    }

    Phantom::VKG::ComputePipelineConfig cc;
    cc.compSpv = config.sunTransmittanceCompSpv;
    cc.descriptorSetLayout = sunSetLayout_.get();
    cc.pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SunPush) };
    if (!sunPipeline_.create(ctx, cc)) return false;

    if (config.renderPass != VK_NULL_HANDLE) {
        Phantom::VKG::PipelineConfig pc{};
        pc.vertSpv = config.fullscreenVertSpv;
        pc.fragSpv = config.raymarchFragSpv;
        pc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pc.descriptorSetLayout = marchSetLayout_.get();
        pc.cullMode = VK_CULL_MODE_NONE;
        pc.depthTest = false;
        pc.depthWrite = false;
        pc.blendEnable = true;
        pc.premultipliedAlphaBlend = true;
        if (!raymarchPipeline_.create(ctx, config.renderPass, pc)) return false;
    }
    return true;
}

void VolumeRaymarchGpu::destroy(const Phantom::VKG::VulkanContext& ctx)
{
    VkDevice device = ctx.getDevice();
    raymarchPipeline_.destroy(device);
    sunPipeline_.destroy(device);
    for (auto& ubo : ubos_) ubo.destroy(device);
    ubos_.clear();
    pool_.destroy(device);
    sunSetLayout_.destroy(device);
    marchSetLayout_.destroy(device);
    density_.destroy(ctx);
    sunT_.destroy(ctx);
    if (zeroBorderSampler_) vkDestroySampler(device, zeroBorderSampler_, nullptr);
    if (oneBorderSampler_) vkDestroySampler(device, oneBorderSampler_, nullptr);
    zeroBorderSampler_ = oneBorderSampler_ = VK_NULL_HANDLE;
    sunSet_ = VK_NULL_HANDLE;
    marchSets_.clear();
}

bool VolumeRaymarchGpu::setGrid(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                const ScalarGridDesc& desc)
{
    density_.destroy(ctx);
    sunT_.destroy(ctx);
    desc_ = desc;
    if (!density_.create(ctx, pool, desc.nx, desc.ny, desc.nz) ||
        !sunT_.create(ctx, pool, desc.nx, desc.ny, desc.nz)) {
        density_.destroy(ctx);
        sunT_.destroy(ctx);
        return false;
    }
    updateDescriptors(ctx.getDevice());
    return true;
}

void VolumeRaymarchGpu::updateDescriptors(VkDevice device)
{
    VkDescriptorImageInfo densityInfo{ zeroBorderSampler_, density_.view(), VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo sunStore{ VK_NULL_HANDLE, sunT_.view(), VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo sunSample{ oneBorderSampler_, sunT_.view(), VK_IMAGE_LAYOUT_GENERAL };

    std::vector<VkWriteDescriptorSet> writes;
    auto add = [&](VkDescriptorSet set, uint32_t binding, VkDescriptorType type, const VkDescriptorImageInfo* img,
                   const VkDescriptorBufferInfo* buf) {
        VkWriteDescriptorSet w{};
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = set;
        w.dstBinding = binding;
        w.descriptorCount = 1;
        w.descriptorType = type;
        w.pImageInfo = img;
        w.pBufferInfo = buf;
        writes.push_back(w);
    };
    add(sunSet_, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &densityInfo, nullptr);
    add(sunSet_, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &sunStore, nullptr);
    std::vector<VkDescriptorBufferInfo> bufInfos(marchSets_.size());
    for (size_t i = 0; i < marchSets_.size(); ++i) {
        bufInfos[i] = { ubos_[i].get(), 0, sizeof(Params) };
    }
    for (size_t i = 0; i < marchSets_.size(); ++i) {
        add(marchSets_[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &bufInfos[i]);
        add(marchSets_[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &densityInfo, nullptr);
        add(marchSets_[i], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sunSample, nullptr);
    }
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

bool VolumeRaymarchGpu::uploadDensity(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                      const ScalarGrid3D& density) const
{
    if (!(density.desc() == desc_)) return false;
    return density_.upload(ctx, pool, density.data().data(), density.data().size());
}

void VolumeRaymarchGpu::recordSunTransmittance(VkCommandBuffer cmd, const ScatteringParams& params) const
{
    if (!hasGrid()) return;
    // The density image may just have been written (upload or another compute pass).
    density_.recordWriteToRead(cmd);

    SunPush pc;
    pc.gridOrigin = glm::vec4(desc_.origin, desc_.cellSize);
    pc.gridDims = glm::vec4(static_cast<float>(desc_.nx), static_cast<float>(desc_.ny), static_cast<float>(desc_.nz), 0.0f);
    pc.sunDirExt = glm::vec4(params.sunDirection, params.extinction);
    pc.step = glm::vec4(VolumeScattering::effectiveStep(desc_, params), 0.0f, 0.0f, 0.0f);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sunPipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, sunPipeline_.getLayout(), 0, 1, &sunSet_, 0, nullptr);
    vkCmdPushConstants(cmd, sunPipeline_.getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SunPush), &pc);
    vkCmdDispatch(cmd, (desc_.nx + 3) / 4, (desc_.ny + 3) / 4, (desc_.nz + 3) / 4);
    sunT_.recordWriteToRead(cmd);
}

void VolumeRaymarchGpu::recordRaymarch(VkCommandBuffer cmd, uint32_t frameIndex, const Camera& camera,
                                       const ScatteringParams& params, float tMax)
{
    if (!hasGrid() || !canRaymarch() || marchSets_.empty()) return;
    const uint32_t f = frameIndex % static_cast<uint32_t>(marchSets_.size());

    Params p;
    p.invViewProj = camera.invViewProj;
    p.camPos = glm::vec4(camera.position, 1.0f);
    p.gridOrigin = glm::vec4(desc_.origin, desc_.cellSize);
    p.gridDims = glm::vec4(static_cast<float>(desc_.nx), static_cast<float>(desc_.ny), static_cast<float>(desc_.nz), 0.0f);
    p.sunDirIrr = glm::vec4(params.sunDirection, params.sunIrradiance);
    p.scatter = glm::vec4(params.extinction, params.albedo, params.phaseG, params.ambient);
    p.march = glm::vec4(VolumeScattering::effectiveStep(desc_, params), static_cast<float>(params.maxSteps),
                        params.minTransmittance, tMax);
    ubos_[f].write(&p, sizeof(p));

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, raymarchPipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, raymarchPipeline_.getLayout(), 0, 1,
                            &marchSets_[f], 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

} // namespace Phantom::Volume
