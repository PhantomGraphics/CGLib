#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "CGLib/VkAppBase/IVkSubRenderer.h"
#include "CGLib/Animation/Animation/SkinnedMesh.h"
#include "CGLib/Animation/Animation/MmdMaterial.h"
#include "CGLib/VulkanGraphics/VulkanBuffer.h"
#include "CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "CGLib/VulkanGraphics/VulkanPipeline.h"
#include "VulkanTextureHelper.h"

#include <string>
#include <vector>

namespace Phantom::Animation {

// Multi-material GPU-skinned renderer for MMD (PMX) models.
// Replaces GpuSkinnedRenderer when submesh/texture data is available.
class MmdMeshRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
    };

    static constexpr uint32_t kMaxBones = 256;

    void setShaders(Shaders s)  { shaders_ = std::move(s); }
    void setVisible(bool v)     { visible_ = v; }
    bool isVisible() const      { return visible_; }

    void setMesh(SkinnedMesh mesh);
    void setSubMeshes(std::vector<MmdSubMesh> subMeshes,
                      const std::vector<std::string>& texturePaths,
                      const std::string& modelDir);

    void updatePose(const std::vector<glm::mat4>& skinMatrices,
                    const glm::mat4& mvp,
                    const glm::mat4& view,
                    const glm::mat4& proj,
                    const glm::vec3& eye);

    // IVkSubRenderer
    void onInit(::VKG::VulkanContext& ctx,
                const ::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass,
                uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

private:
    struct CameraUBO {
        glm::mat4 mvp;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec4 eye;  // eye.xyz = camera position
    };
    struct BoneUBO {
        glm::mat4 bones[kMaxBones];
    };
    // std140 layout: 4×vec4 + float + uint + 2×float padding
    struct MaterialUBO {
        glm::vec4 diffuse;    // rgba
        glm::vec4 specular;   // xyz=specular, w=specularity
        glm::vec4 ambient;    // xyz=ambient, w=pad
        glm::vec4 edgeColor;
        float     edgeSize;
        uint32_t  sphereMode;
        float     _pad[2];
    };

    Shaders  shaders_;
    bool     visible_      = true;
    bool     meshDirty_    = false;
    bool     subMeshDirty_ = false;

    const ::VKG::VulkanContext*     ctx_            = nullptr;
    const ::VKG::VulkanCommandPool* pool_           = nullptr;
    VkDevice                      device_         = VK_NULL_HANDLE;
    VkRenderPass                  renderPass_     = VK_NULL_HANDLE;
    uint32_t                      framesInFlight_ = 2;

    ::VKG::VulkanDescriptorSetLayout               descriptorSetLayout_;
    ::VKG::VulkanDescriptorPool                    descriptorPool_;
    // [subMeshIdx][frameIdx]
    std::vector<std::vector<VkDescriptorSet>>    perSubMeshSets_;

    ::VKG::VulkanPipeline            pipeline_;

    ::VKG::VulkanBuffer              vertexBuffer_;
    ::VKG::VulkanBuffer              indexBuffer_;
    uint32_t                       totalIndexCount_ = 0;

    std::vector<::VKG::VulkanBuffer>              cameraUBOs_;  // [frameIdx]
    std::vector<::VKG::VulkanBuffer>              boneUBOs_;    // [frameIdx]
    std::vector<std::vector<::VKG::VulkanBuffer>> materialUBOs_; // [subMeshIdx][frameIdx]

    VulkanTextureHelper           texHelper_;
    std::vector<const GpuTexture*> subMeshTextures_; // [subMeshIdx], points into texHelper_

    SkinnedMesh              mesh_;
    std::vector<MmdSubMesh>  subMeshes_;
    std::vector<std::string> texturePaths_;
    std::string              modelDir_;

    std::vector<glm::mat4> skinMatrices_;
    glm::mat4              mvp_  {1.f};
    glm::mat4              view_ {1.f};
    glm::mat4              proj_ {1.f};
    glm::vec3              eye_  {0.f};

    void uploadMesh();
    void rebuildDescriptors();
    void destroyDescriptors();
};

} // namespace Phantom::Animation
