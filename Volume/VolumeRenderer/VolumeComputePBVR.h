#pragma once

#include "TransferFunction.h"
#include "PBVRPipeline.h"

#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanComputePipeline.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

namespace Phantom::VKG {
class VulkanContext;
class VulkanCommandPool;
}

namespace Phantom::Volume {

// One active voxel entry serialized from SparseVolumef for GPU upload.
// Layout must match the GLSL struct GpuVoxelEntry in volume_pbvr_gen.comp.
struct GpuVoxelEntry {
    float x, y, z;
    float value;
};

class VolumeComputePBVR {
public:
    void create(const ::VKG::VulkanContext& ctx);
    void destroy(VkDevice device);

    void setDensityScale(float s)       { densityScale_ = s; }
    void setMaxParticlesPerVoxel(int n) { maxPPV_ = static_cast<uint32_t>(n); }

    // GPU particle generation.
    // voxels:   flattened active voxels from SparseVolumef::forEachActive()
    // voxelSize: cell size (same for all voxels in the list)
    // tfLut:    TransferFunction::getLUT() — 256 TFSample {r,g,b,a} entries
    // Blocks until GPU is idle (beginSingleTimeCommands / endSingleTimeCommands).
    void dispatch(const ::VKG::VulkanContext& ctx,
                  const ::VKG::VulkanCommandPool& pool,
                  const std::vector<GpuVoxelEntry>& voxels,
                  float voxelSize,
                  const std::vector<TFSample>& tfLut);

    VkBuffer getVertexBuffer()  const { return outputBuf_.getBuffer(); }
    uint32_t getVertexCount()   const { return totalCount_; }
    bool     isValid()          const { return pipeline_.isValid(); }

private:
    struct PushConstants {
        uint32_t numVoxels;
        uint32_t maxParticlesPerVoxel;
        float    densityScale;
        float    voxelSize;
    };

    ::VKG::VulkanBuffer           voxelBuf_;   // SSBO: GpuVoxelEntry[] (binding 0)
    ::VKG::VulkanBuffer           tfBuf_;      // SSBO: vec4[256]       (binding 1)
    ::VKG::VulkanBuffer           outputBuf_;  // SSBO + VBO: PBVRVertex[] (binding 2)
    ::VKG::VulkanDescriptorSetLayout dsl_;
    ::VKG::VulkanDescriptorPool   descPool_;
    VkDescriptorSet             descSet_  = VK_NULL_HANDLE;
    ::VKG::VulkanComputePipeline  pipeline_;

    uint32_t totalCount_       = 0;
    uint32_t cachedVoxelCount_ = 0;
    uint32_t maxPPV_           = 4;
    float    densityScale_     = 1.0f;

    void rebuildVoxelBuf(const ::VKG::VulkanContext&, const ::VKG::VulkanCommandPool&,
                         const std::vector<GpuVoxelEntry>&);
    void rebuildTfBuf(const ::VKG::VulkanContext&, const ::VKG::VulkanCommandPool&,
                      const std::vector<TFSample>&);
    void rebuildOutputBuf(const ::VKG::VulkanContext&, const ::VKG::VulkanCommandPool&,
                          uint32_t numVoxels);
    void updateDescSet(VkDevice);
};

} // namespace PBVR
