#include "VolumeComputePBVR.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanSPVResolver.h"

#include <cstddef>

// Verify C++ struct layouts match the GLSL std430 structs in volume_pbvr_gen.comp.
static_assert(sizeof(Phantom::Volume::GpuVoxelEntry) == 16,
    "GpuVoxelEntry layout changed — update volume_pbvr_gen.comp accordingly");
static_assert(sizeof(Phantom::Volume::TFSample) == 16,
    "TFSample layout changed — update volume_pbvr_gen.comp accordingly");
static_assert(sizeof(Phantom::Volume::PBVRVertex) == 28,
    "PBVRVertex layout changed — update volume_pbvr_gen.comp accordingly");
static_assert(offsetof(Phantom::Volume::PBVRVertex, color) == 12,
    "PBVRVertex color offset changed — update volume_pbvr_gen.comp accordingly");

namespace Phantom::Volume {

void VolumeComputePBVR::create(const ::VKG::VulkanContext& ctx)
{
    VkDevice dev = ctx.getDevice();

    // Descriptor set layout: binding 0 = voxel SSBO, 1 = TF LUT SSBO, 2 = output SSBO
    VkDescriptorSetLayoutBinding b0{};
    b0.binding         = 0;
    b0.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutBinding b1 = b0;
    b1.binding = 1;

    VkDescriptorSetLayoutBinding b2 = b0;
    b2.binding = 2;

    dsl_.create(dev, {b0, b1, b2});

    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3};
    descPool_.create(dev, {ps}, 1);
    auto sets = descPool_.allocateSets(dev, {dsl_.get()});
    descSet_ = sets.front();

    ::VKG::ComputePipelineConfig cfg{};
    cfg.compSpv             = ::VKG::loadSPVRepo("shaders/volume_pbvr_gen.comp.spv");
    cfg.descriptorSetLayout = dsl_.get();
    cfg.pushConstantRange   = {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants)};
    pipeline_.create(ctx, cfg);
}

void VolumeComputePBVR::destroy(VkDevice device)
{
    pipeline_.destroy(device);
    descPool_.destroy(device);
    dsl_.destroy(device);
    descSet_ = VK_NULL_HANDLE;
    outputBuf_.destroy(device);
    tfBuf_.destroy(device);
    voxelBuf_.destroy(device);
    totalCount_       = 0;
    cachedVoxelCount_ = 0;
}

void VolumeComputePBVR::dispatch(const ::VKG::VulkanContext& ctx,
                                    const ::VKG::VulkanCommandPool& pool,
                                    const std::vector<GpuVoxelEntry>& voxels,
                                    float voxelSize,
                                    const std::vector<TFSample>& tfLut)
{
    if (voxels.empty()) {
        totalCount_ = 0;
        return;
    }

    const uint32_t numVoxels = static_cast<uint32_t>(voxels.size());

    // Rebuild voxel SSBO when voxel count changes
    if (numVoxels != cachedVoxelCount_) {
        rebuildVoxelBuf(ctx, pool, voxels);
        cachedVoxelCount_ = numVoxels;
    }

    // Always update TF LUT (small buffer, low cost)
    rebuildTfBuf(ctx, pool, tfLut);

    // Rebuild output buffer when capacity is insufficient
    const VkDeviceSize neededBytes =
        static_cast<VkDeviceSize>(numVoxels) * maxPPV_ * sizeof(PBVRVertex);
    if (!outputBuf_.isValid() || outputBuf_.getSize() < neededBytes) {
        rebuildOutputBuf(ctx, pool, numVoxels);
    }

    totalCount_ = numVoxels * maxPPV_;

    PushConstants pc{numVoxels, maxPPV_, densityScale_, voxelSize};

    VkCommandBuffer cmd = pool.beginSingleTimeCommands();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_.getPipeline());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipeline_.getLayout(), 0, 1, &descSet_, 0, nullptr);
    vkCmdPushConstants(cmd, pipeline_.getLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);

    const uint32_t groups = (numVoxels + 63u) / 64u;
    vkCmdDispatch(cmd, groups, 1, 1);

    // Barrier: compute SSBO write -> vertex attribute read
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 1, &barrier, 0, nullptr, 0, nullptr);

    pool.endSingleTimeCommands(cmd);
}

void VolumeComputePBVR::rebuildVoxelBuf(const ::VKG::VulkanContext& ctx,
                                            const ::VKG::VulkanCommandPool& pool,
                                           const std::vector<GpuVoxelEntry>& voxels)
{
    voxelBuf_.destroy(ctx.getDevice());
    const VkDeviceSize bytes =
        static_cast<VkDeviceSize>(voxels.size()) * sizeof(GpuVoxelEntry);
    voxelBuf_.create(ctx, pool, bytes,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     voxels.data());
    updateDescSet(ctx.getDevice());
}

void VolumeComputePBVR::rebuildTfBuf(const ::VKG::VulkanContext& ctx,
                                         const ::VKG::VulkanCommandPool& pool,
                                        const std::vector<TFSample>& tfLut)
{
    tfBuf_.destroy(ctx.getDevice());
    const VkDeviceSize bytes =
        static_cast<VkDeviceSize>(tfLut.size()) * sizeof(TFSample);
    tfBuf_.create(ctx, pool, bytes,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                  tfLut.data());
    updateDescSet(ctx.getDevice());
}

void VolumeComputePBVR::rebuildOutputBuf(const ::VKG::VulkanContext& ctx,
                                             const ::VKG::VulkanCommandPool& pool,
                                            uint32_t numVoxels)
{
    outputBuf_.destroy(ctx.getDevice());
    const VkDeviceSize bytes =
        static_cast<VkDeviceSize>(numVoxels) * maxPPV_ * sizeof(PBVRVertex);
    outputBuf_.create(ctx, pool, bytes,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      nullptr);
    updateDescSet(ctx.getDevice());
}

void VolumeComputePBVR::updateDescSet(VkDevice device)
{
    if (!voxelBuf_.isValid() || !tfBuf_.isValid() || !outputBuf_.isValid()) return;

    VkDescriptorBufferInfo voxelInfo{voxelBuf_.getBuffer(),  0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo tfInfo{tfBuf_.getBuffer(),        0, VK_WHOLE_SIZE};
    VkDescriptorBufferInfo outInfo{outputBuf_.getBuffer(),   0, VK_WHOLE_SIZE};

    VkWriteDescriptorSet writes[3]{};
    writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet          = descSet_;
    writes[0].dstBinding      = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[0].pBufferInfo     = &voxelInfo;

    writes[1]             = writes[0];
    writes[1].dstBinding  = 1;
    writes[1].pBufferInfo = &tfInfo;

    writes[2]             = writes[0];
    writes[2].dstBinding  = 2;
    writes[2].pBufferInfo = &outInfo;

    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
}

} // namespace PBVR
