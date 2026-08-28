#pragma once

#include "../../VkAppBase/IVkSubRenderer.h"
#include "../VkRenderer/VkLineRenderer.h"
#include "../VkRenderer/VkPointRenderer.h"
#include "../VkRenderer/VkSkyBoxRenderer.h"
#include "../VkRenderer/VkTexRenderer.h"
#include "../VkRenderer/VkTriangleRenderer.h"
#include "../../../CGLib/VulkanGraphics/VulkanCubeMap.h"
#include "../../../CGLib/VulkanGraphics/VulkanSampler.h"

#include <glm/glm.hpp>

#include <optional>
#include <vector>

namespace VKRenderer {

class VkRendererSubRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> pointVert, pointFrag;
        std::vector<uint32_t> lineVert, lineFrag;
        std::vector<uint32_t> triVert, triFrag;
        std::vector<uint32_t> texVert, texFrag;
        std::vector<uint32_t> skyboxVert, skyboxFrag;
    };
    void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }

    VkRendererSubRenderer() = default;

    void setExtent(VkExtent2D ext) { extent_ = ext; }

    enum class Mode {
        Point = 0,
        Line,
        Triangle,
        Tex,
        SkyBox,
    };

    void setMode(Mode m) { activeMode_ = m; }
    Mode getMode() const { return activeMode_; }

    void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;

    void setCamera(const glm::mat4& proj, const glm::mat4& view) {
        proj_ = proj;
        view_ = view;
    }

    void setCubeMap(VkDevice device, VkImageView view, VkSampler sampler);

private:
    Shaders shaders_;
    VkExtent2D extent_ = {1280, 720};
    Mode activeMode_ = Mode::Point;

    glm::mat4 proj_{1.0f};
    glm::mat4 view_{1.0f};

    const Phantom::VKG::VulkanContext* ctx_ = nullptr;
    const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;
    uint32_t framesInFlight_ = 2;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;

    std::optional<Phantom::VKG::VkPointRenderer> pointRenderer_;
    std::optional<Phantom::VKG::VkLineRenderer> lineRenderer_;
    std::optional<Phantom::VKG::VkTriangleRenderer> triangleRenderer_;
    std::optional<Phantom::VKG::VkTexRenderer> texRenderer_;
    std::optional<Phantom::VKG::VkSkyBoxRenderer> skyBoxRenderer_;

    // Sample static geometry reused every frame.
    std::vector<float> samplePointPositions_;
    std::vector<float> samplePointColors_;
    std::vector<float> samplePointSizes_;

    std::vector<float> sampleLinePositions_;
    std::vector<float> sampleLineColors_;
    std::vector<uint32_t> sampleLineIndices_;

    std::vector<float> sampleTrianglePositions_;
    std::vector<float> sampleTriangleColors_;
    std::vector<uint32_t> sampleTriangleIndices_;

    VkImage texImage_ = VK_NULL_HANDLE;
    VkDeviceMemory texMemory_ = VK_NULL_HANDLE;
    VkImageView texImageView_ = VK_NULL_HANDLE;
    Phantom::VKG::VulkanSampler texSampler_;
    bool texReady_ = false;

    Phantom::VKG::VulkanCubeMap fallbackCubeMap_;
    bool skyBoxReady_ = false;
    bool hasExternalCubeMap_ = false;
    VkImageView externalCubeMapView_ = VK_NULL_HANDLE;
    VkSampler externalCubeMapSampler_ = VK_NULL_HANDLE;

    void uploadSamplePoint();
    void uploadSampleLine();
    void uploadSampleTriangle();
    // Returns false (and logs to stderr) on Vulkan resource creation failure;
    // texReady_ stays false so the texture is simply skipped when rendering.
    bool createSampleTexture();

    glm::mat4 computeMVP() const;
};

} // namespace VKRenderer
