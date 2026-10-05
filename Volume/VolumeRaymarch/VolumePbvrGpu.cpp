#include "VolumePbvrGpu.h"

#include "VolumeGpuUtil.h"

#include "../../VulkanGraphics/VulkanCommandPool.h"
#include "../../VulkanGraphics/VulkanContext.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace Phantom::Volume {

using namespace Phantom::VKG;
using detail::imageBarrier;
using detail::memoryBarrier;

// std140 layout; keep in sync with volume_pbvr_common.glsl.
struct VolumePbvrGpu::Params {
    glm::mat4 viewProj;
    glm::vec4 camPos;
    glm::vec4 gridOrigin;
    glm::vec4 gridDims;
    glm::vec4 sunDirIrr;
    glm::vec4 scatter;
    glm::vec4 pbvr;      // pixelAngle, minDiameterPx, maxPerCell, projScalePx
    glm::uvec4 limits;   // capacity
};

namespace {
constexpr VkFormat kEnsembleFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
constexpr VkFormat kAccumFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
constexpr VkShaderStageFlags kAllStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT;

struct GeneratePush { uint32_t seed; };
struct AccumPush { uint32_t first, width, height; };
}

bool VolumePbvrGpu::create(const VulkanContext& ctx, const VulkanCommandPool& pool, const Config& config)
{
    config_ = config;
    VkDevice device = ctx.getDevice();
    const uint32_t frames = std::max(1u, config.framesInFlight);

    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = si.minFilter = VK_FILTER_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(device, &si, nullptr, &nearestSampler_) != VK_SUCCESS) return false;

    // Generate / draw set: params UBO, density, sun transmittance, particles, draw args, stats.
    genLayout_.create(device, {
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, kAllStages, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, kAllStages, nullptr },
        { 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr } });
    accumLayout_.create(device, {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr } });
    compositeLayout_.create(device, {
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr } });

    descriptorPool_.create(device, {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frames },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 * frames + 2 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 * frames },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 } }, frames + 2);
    std::vector<VkDescriptorSetLayout> layouts(frames, genLayout_.get());
    layouts.push_back(accumLayout_.get());
    layouts.push_back(compositeLayout_.get());
    std::vector<VkDescriptorSet> sets = descriptorPool_.allocateSets(device, layouts);
    if (sets.size() != layouts.size()) return false;
    genSets_.assign(sets.begin(), sets.begin() + frames);
    accumSet_ = sets[frames];
    compositeSet_ = sets[frames + 1];

    ubos_.resize(frames);
    for (auto& ubo : ubos_) {
        if (!ubo.createMapped(ctx, sizeof(Params), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)) return false;
    }
    const VkDeviceSize particleBytes = static_cast<VkDeviceSize>(config.particleCapacity) * 32;
    if (!particles_.create(ctx, pool, particleBytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) return false;
    if (!drawArgs_.create(ctx, pool, 32,
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                          VK_BUFFER_USAGE_TRANSFER_DST_BIT)) return false;
    if (!stats_.create(ctx, pool, 16,
                       VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) return false;

    ComputePipelineConfig gc;
    gc.compSpv = config.generateCompSpv;
    gc.descriptorSetLayout = genLayout_.get();
    gc.pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GeneratePush) };
    if (!generatePipeline_.create(ctx, gc)) return false;

    ComputePipelineConfig ac;
    ac.compSpv = config.accumulateCompSpv;
    ac.descriptorSetLayout = accumLayout_.get();
    ac.pushConstantRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(AccumPush) };
    if (!accumulatePipeline_.create(ctx, ac)) return false;
    return true;
}

void VolumePbvrGpu::destroyTargets(const VulkanContext& ctx)
{
    VkDevice device = ctx.getDevice();
    ensembleTarget_.destroy(ctx);
    pointPipeline_.destroy(device);
    compositePipeline_.destroy(device);
    if (accumView_) vkDestroyImageView(device, accumView_, nullptr);
    if (accumImage_) vkDestroyImage(device, accumImage_, nullptr);
    if (accumMemory_) vkFreeMemory(device, accumMemory_, nullptr);
    accumView_ = VK_NULL_HANDLE;
    accumImage_ = VK_NULL_HANDLE;
    accumMemory_ = VK_NULL_HANDLE;
}

void VolumePbvrGpu::destroy(const VulkanContext& ctx)
{
    VkDevice device = ctx.getDevice();
    destroyTargets(ctx);
    generatePipeline_.destroy(device);
    accumulatePipeline_.destroy(device);
    for (auto& ubo : ubos_) ubo.destroy(device);
    ubos_.clear();
    particles_.destroy(device);
    drawArgs_.destroy(device);
    stats_.destroy(device);
    descriptorPool_.destroy(device);
    genLayout_.destroy(device);
    accumLayout_.destroy(device);
    compositeLayout_.destroy(device);
    if (nearestSampler_) vkDestroySampler(device, nearestSampler_, nullptr);
    nearestSampler_ = VK_NULL_HANDLE;
    genSets_.clear();
}

bool VolumePbvrGpu::setViewport(const VulkanContext& ctx, const VulkanCommandPool& pool, uint32_t width, uint32_t height)
{
    destroyTargets(ctx);
    width_ = width;
    height_ = height;
    accumulated_ = 0;
    if (width == 0 || height == 0) return false;
    VkDevice device = ctx.getDevice();

    if (!ensembleTarget_.create(ctx, width, height, kEnsembleFormat, config_.depthFormat)) return false;

    VkImageCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = kAccumFormat;
    ci.extent = { width, height, 1 };
    ci.mipLevels = ci.arrayLayers = 1;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &ci, nullptr, &accumImage_) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(device, accumImage_, &req);
    auto type = ctx.findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!type) return false;
    VkMemoryAllocateInfo ai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, req.size, *type };
    if (vkAllocateMemory(device, &ai, nullptr, &accumMemory_) != VK_SUCCESS) return false;
    // On failure the handles stay set; destroyTargets() (next setViewport()/destroy()) releases them.
    if (vkBindImageMemory(device, accumImage_, accumMemory_, 0) != VK_SUCCESS) return false;
    VkImageViewCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image = accumImage_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = kAccumFormat;
    vi.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    if (vkCreateImageView(device, &vi, nullptr, &accumView_) != VK_SUCCESS) return false;

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    imageBarrier(cmd, accumImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0,
                 VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    pool.endSingleTimeCommands(cmd);

    // Pipelines against the (new) ensemble render pass and the caller's composite pass.
    PipelineConfig pc{};
    pc.vertSpv = config_.pointVertSpv;
    pc.fragSpv = config_.pointFragSpv;
    pc.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    pc.descriptorSetLayout = genLayout_.get();
    pc.cullMode = VK_CULL_MODE_NONE;
    pc.depthTest = true;
    pc.depthWrite = true;
    pc.depthCompareOp = VK_COMPARE_OP_LESS;
    pc.blendEnable = false;
    if (!pointPipeline_.create(ctx, ensembleTarget_.getRenderPass(), pc)) return false;

    if (config_.compositeRenderPass != VK_NULL_HANDLE) {
        PipelineConfig cc{};
        cc.vertSpv = config_.fullscreenVertSpv;
        cc.fragSpv = config_.compositeFragSpv;
        cc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        cc.descriptorSetLayout = compositeLayout_.get();
        cc.cullMode = VK_CULL_MODE_NONE;
        cc.depthTest = false;
        cc.depthWrite = false;
        cc.blendEnable = true;
        cc.premultipliedAlphaBlend = true;
        cc.pushConstantRanges.push_back({ VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) });
        if (!compositePipeline_.create(ctx, config_.compositeRenderPass, cc)) return false;
    }

    // Screen-sized descriptors.
    VkDescriptorImageInfo ens{ nearestSampler_, ensembleTarget_.getColorImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo accStore{ VK_NULL_HANDLE, accumView_, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo accSample{ nearestSampler_, accumView_, VK_IMAGE_LAYOUT_GENERAL };
    VkWriteDescriptorSet w[3]{};
    for (auto& x : w) { x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; x.descriptorCount = 1; }
    w[0].dstSet = accumSet_; w[0].dstBinding = 0; w[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[0].pImageInfo = &ens;
    w[1].dstSet = accumSet_; w[1].dstBinding = 1; w[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE; w[1].pImageInfo = &accStore;
    w[2].dstSet = compositeSet_; w[2].dstBinding = 0; w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[2].pImageInfo = &accSample;
    vkUpdateDescriptorSets(device, 3, w, 0, nullptr);
    return true;
}

void VolumePbvrGpu::bindGrid(const VulkanContext& ctx, const VolumeRaymarchGpu& grid)
{
    if (!grid.hasGrid()) return;
    VkDevice device = ctx.getDevice();
    VkDescriptorImageInfo density{ grid.densitySampler(), grid.density().view(), VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo sunT{ grid.transmittanceSampler(), grid.sunTransmittance().view(), VK_IMAGE_LAYOUT_GENERAL };
    std::vector<VkDescriptorBufferInfo> uboInfo(genSets_.size());
    VkDescriptorBufferInfo particlesInfo{ particles_.get(), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo argsInfo{ drawArgs_.get(), 0, VK_WHOLE_SIZE };
    VkDescriptorBufferInfo statsInfo{ stats_.get(), 0, VK_WHOLE_SIZE };
    std::vector<VkWriteDescriptorSet> writes;
    auto add = [&](VkDescriptorSet set, uint32_t binding, VkDescriptorType type, const VkDescriptorImageInfo* img,
                   const VkDescriptorBufferInfo* buf) {
        VkWriteDescriptorSet x{};
        x.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        x.dstSet = set;
        x.dstBinding = binding;
        x.descriptorCount = 1;
        x.descriptorType = type;
        x.pImageInfo = img;
        x.pBufferInfo = buf;
        writes.push_back(x);
    };
    for (size_t f = 0; f < genSets_.size(); ++f) {
        uboInfo[f] = { ubos_[f].get(), 0, sizeof(Params) };
        add(genSets_[f], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uboInfo[f]);
        add(genSets_[f], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &density, nullptr);
        add(genSets_[f], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sunT, nullptr);
        add(genSets_[f], 3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &particlesInfo);
        add(genSets_[f], 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &argsInfo);
        add(genSets_[f], 5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &statsInfo);
    }
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VolumePbvrGpu::recordEnsembles(VkCommandBuffer cmd, uint32_t frameIndex, const VolumeRaymarchGpu& grid,
                                    const Camera& camera, const ScatteringParams& sp, uint32_t count)
{
    if (!isReady() || !grid.hasGrid() || count == 0 || genSets_.empty()) return;
    const uint32_t f = frameIndex % static_cast<uint32_t>(genSets_.size());
    const ScalarGridDesc& d = grid.gridDesc();

    Params p;
    p.viewProj = camera.viewProj;
    p.camPos = glm::vec4(camera.position, 1.0f);
    p.gridOrigin = glm::vec4(d.origin, d.cellSize);
    p.gridDims = glm::vec4(static_cast<float>(d.nx), static_cast<float>(d.ny), static_cast<float>(d.nz), 0.0f);
    p.sunDirIrr = glm::vec4(sp.sunDirection, sp.sunIrradiance);
    p.scatter = glm::vec4(sp.extinction, sp.albedo, sp.phaseG, sp.ambient);
    p.pbvr = glm::vec4(camera.pixelAngle, config_.minDiameterPx, config_.maxPerCell, camera.projScalePx);
    p.limits = glm::uvec4(config_.particleCapacity, 0, 0, 0);
    ubos_[f].write(&p, sizeof(p));

    if (resetStats_) {
        vkCmdFillBuffer(cmd, stats_.get(), 0, 16, 0);
        memoryBarrier(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        resetStats_ = false;
    }

    // The density / sun-transmittance images may have just been written by their own passes.
    memoryBarrier(cmd, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    const uint32_t groupsX = (d.nx + 3) / 4, groupsY = (d.ny + 3) / 4, groupsZ = (d.nz + 3) / 4;
    for (uint32_t e = 0; e < count; ++e) {
        // Draw args {vertexCount = 0, instanceCount = 1, firstVertex = 0, firstInstance = 0, pad...}.
        const uint32_t args[8] = { 0, 1, 0, 0, 0, 0, 0, 0 };
        vkCmdUpdateBuffer(cmd, drawArgs_.get(), 0, sizeof(args), args);
        memoryBarrier(cmd, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        GeneratePush gp{ ++ensembleCounter_ };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, generatePipeline_.getPipeline());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, generatePipeline_.getLayout(), 0, 1, &genSets_[f], 0, nullptr);
        vkCmdPushConstants(cmd, generatePipeline_.getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(gp), &gp);
        vkCmdDispatch(cmd, groupsX, groupsY, groupsZ);

        memoryBarrier(cmd, VK_ACCESS_SHADER_WRITE_BIT,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT);

        ensembleTarget_.beginRenderPass(cmd, { 0.f, 0.f, 0.f, 0.f }, 1.0f);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointPipeline_.getPipeline());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointPipeline_.getLayout(), 0, 1, &genSets_[f], 0, nullptr);
        vkCmdDrawIndirect(cmd, drawArgs_.get(), 0, 1, 16);
        ensembleTarget_.endRenderPass(cmd);

        imageBarrier(cmd, ensembleTarget_.getColorImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                     VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        AccumPush ap{ accumulated_ == 0 ? 1u : 0u, width_, height_ };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, accumulatePipeline_.getPipeline());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, accumulatePipeline_.getLayout(), 0, 1, &accumSet_, 0, nullptr);
        vkCmdPushConstants(cmd, accumulatePipeline_.getLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ap), &ap);
        vkCmdDispatch(cmd, (width_ + 7) / 8, (height_ + 7) / 8, 1);
        ++accumulated_;

        // accum RAW for the next ensemble; ensemble target WAR before it is drawn again.
        memoryBarrier(cmd, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                      VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        memoryBarrier(cmd, 0, 0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT);
    }
    memoryBarrier(cmd, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                  VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

void VolumePbvrGpu::recordComposite(VkCommandBuffer cmd) const
{
    if (!isReady() || accumulated_ == 0 || compositePipeline_.getPipeline() == VK_NULL_HANDLE) return;
    const float invCount = 1.0f / static_cast<float>(accumulated_);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_.getLayout(), 0, 1, &compositeSet_, 0, nullptr);
    vkCmdPushConstants(cmd, compositePipeline_.getLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &invCount);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

bool VolumePbvrGpu::readStats(const VulkanContext& ctx, const VulkanCommandPool& pool, Stats& out) const
{
    detail::HostBuffer host;
    if (!host.create(ctx, 16, VK_BUFFER_USAGE_TRANSFER_DST_BIT)) {
        host.destroy(ctx);
        return false;
    }
    VkCommandBuffer cmd = pool.beginSingleTimeCommands();
    memoryBarrier(cmd, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT);
    VkBufferCopy region{ 0, 0, 16 };
    vkCmdCopyBuffer(cmd, stats_.get(), host.buffer, 1, &region);
    pool.endSingleTimeCommands(cmd);
    const uint32_t* v = static_cast<const uint32_t*>(host.mapped);
    out.overflowed = v[0];
    out.generated = v[1];
    host.destroy(ctx);
    return true;
}

} // namespace Phantom::Volume
