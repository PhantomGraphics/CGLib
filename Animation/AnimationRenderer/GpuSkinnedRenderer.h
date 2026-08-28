#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "CGLib/Animation/Animation/SkinnedMesh.h"
#include "CGLib/VulkanGraphics/VulkanBuffer.h"
#include "CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "CGLib/VulkanGraphics/VulkanPipeline.h"

#include <vector>

namespace Phantom::Animation {

// GPU-skinned renderer: bone matrices sent via UBO; skinning done in vertex shader.
// CPU skinning is bypassed -- raw SkinVertex data is uploaded once per mesh change.
class GpuSkinnedRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    static constexpr uint32_t kMaxBones = 256;

    void setShaders(Shaders s)   { shaders_ = std::move(s); }
    void setVisible(bool v)      { visible_ = v; }
    bool isVisible() const       { return visible_; }

    // Supply the mesh (bind pose). Re-uploads vertex/index buffers on next onUpdate().
    void setMesh(SkinnedMesh mesh);

    // Call every frame: bone matrices are written to BoneUBO each frame.
    void updatePose(const std::vector<glm::mat4>& skinMatrices,
                    const glm::mat4& mvp);

    // IVkSubRenderer
    void onInit(::VKG::VulkanContext& ctx,
                const ::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass,
                uint32_t framesInFlight) override;

    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    struct CameraUBO { glm::mat4 mvp; };
    struct BoneUBO   { glm::mat4 bones[kMaxBones]; };

    Shaders  shaders_;
    bool     visible_   = true;
    bool     meshDirty_ = false;

    const ::VKG::VulkanContext*     ctx_            = nullptr;
    const ::VKG::VulkanCommandPool* pool_           = nullptr;
    VkDevice                      device_         = VK_NULL_HANDLE;
    uint32_t                      framesInFlight_ = 2;
    uint32_t                      indexCount_     = 0;

    ::VKG::VulkanDescriptorSetLayout descriptorSetLayout_;
    ::VKG::VulkanDescriptorPool      descriptorPool_;
    std::vector<VkDescriptorSet>   descriptorSets_;
    ::VKG::VulkanPipeline            pipeline_;

    ::VKG::VulkanBuffer              vertexBuffer_;
    ::VKG::VulkanBuffer              indexBuffer_;
    std::vector<::VKG::VulkanBuffer> cameraUBOs_;
    std::vector<::VKG::VulkanBuffer> boneUBOs_;

    SkinnedMesh            mesh_;
    std::vector<glm::mat4> skinMatrices_;
    glm::mat4              mvp_{1.f};

    void uploadMesh();
};

} // namespace Phantom::Animation
