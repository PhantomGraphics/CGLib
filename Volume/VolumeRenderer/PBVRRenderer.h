#pragma once

#include "IPBVRDataSource.h"
#include "ParticleGenerator.h"
#include "TransferFunction.h"
#include "PBVRPipeline.h"
#include "VolumeComputePBVR.h"
#include "OpacityShadowMapPass.h"

#include "../../VkAppBase/IVkSubRenderer.h"
#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/Math/Box3d.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Phantom::Volume {

class PBVRRenderer : public ::VKG::IVkSubRenderer {
public:
    struct Shaders {
        std::vector<uint32_t> vertSpv;
        std::vector<uint32_t> fragSpv;
        // Self-shadow deposit pass (Phase 3). Left empty to opt out of the shadow feature
        // entirely (renderShadowDeposit() then no-ops even if setShadowEnabled(true) is called).
        std::vector<uint32_t> depositVertSpv;
        std::vector<uint32_t> depositFragSpv;
    };

    void setDataSource(IPBVRDataSource* src) { dataSource_ = src; }
    void setExtent(VkExtent2D ext) { extent_ = ext; }
    void markDirty() { dirty_ = true; }
    void syncCamera(float azimuth, float elevation, float distance);
    void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }
    void setEnabled(bool e) { enabled_ = e; }

    TransferFunction& getTransferFunction() { return tf_; }
    void setDensityScale(float s);
    void setParticleSize(float s);
    void setRepeatCount(int n);
    void setUseGPU(bool b);
    void setMaxParticlesPerVoxel(int n);
    float getDensityScale() const { return densityScale_; }
    float getParticleSize() const { return particleSize_; }
    int getRepeatCount() const { return repeatCount_; }
    bool isGPUMode() const { return useGPU_; }
    int  getMaxParticlesPerVoxel() const { return maxParticlesPerVoxel_; }
    size_t getParticleCount() const {
        return useGPU_ ? static_cast<size_t>(gpuVertexCount_) : particleSet_.count();
    }

    // Self-shadow (experimental, Opacity Shadow Map). See docs/idea/pbvr.md.
    void setLightDir(float azimuthDeg, float elevationDeg);
    void setShadowEnabled(bool b) { shadowEnabled_ = b; }
    void setExtinction(float sigma) { sigma_ = std::max(0.0f, sigma); }
    void setShadowLayers(int n);
    void setShadowMapSize(uint32_t size);
    void setTransferFunctionPreset(int preset);

    float    getLightAzimuth()    const { return lightAzimuth_; }
    float    getLightElevation()  const { return lightElevation_; }
    bool     isShadowEnabled()    const { return shadowEnabled_; }
    float    getExtinction()      const { return sigma_; }
    int      getShadowLayers()    const { return shadowLayers_; }
    uint32_t getShadowMapSize()   const { return shadowMapSize_; }
    glm::vec3 computeLightDir()   const;

    void onInit(::VKG::VulkanContext& ctx, const ::VKG::VulkanCommandPool& pool,
                VkRenderPass renderPass, uint32_t framesInFlight) override;
    void onUpdate(uint32_t frameIndex) override;
    void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onCleanup(VkDevice device) override;
    void onImGui() override;

    // Records the light-space opacity-accumulation passes (Phase 3). Must be called from
    // VkAppBase::onPreRender(), before the swap-chain render pass begins -- IVkSubRenderer's
    // onRender() runs inside that render pass and cannot nest another vkCmdBeginRenderPass.
    // No-ops unless both setShadowEnabled(true) was called and deposit shaders were provided.
    void renderShadowDeposit(VkCommandBuffer cmd);

private:
    glm::mat4 computeMVP() const;
    void regenerateParticles();
    glm::mat4 computeLightView() const;
    glm::mat4 computeLightProj() const;
    bool getActiveBuffer(VkBuffer& vbuf, uint32_t& vtxCount) const;

    IPBVRDataSource* dataSource_ = nullptr;
    const ::VKG::VulkanContext* ctx_ = nullptr;
    const ::VKG::VulkanCommandPool* pool_ = nullptr;
    VkExtent2D extent_{1280, 720};
    uint32_t framesInFlight_ = 2;

    bool dirty_ = true;
    bool enabled_ = false;
    float densityScale_ = 1.0f;
    float particleSize_ = 4.0f;
    int repeatCount_ = 1;
    bool useGPU_ = false;
    int maxParticlesPerVoxel_ = 4;
    float azimuth_ = 0.0f;
    float elevation_ = 30.0f;
    float distance_ = 50.0f;

    // Self-shadow (experimental).
    float    lightAzimuth_   = 45.0f;
    float    lightElevation_ = 60.0f;
    bool     shadowEnabled_  = false;
    float    sigma_          = 1.0f;
    int      shadowLayers_   = 8;
    uint32_t shadowMapSize_  = 512;
    bool     shadowDirty_    = true; // OpacityShadowMapPass needs (re)creation (layers/size changed)
    bool     depositPipelineCreated_ = false;
    Phantom::Math::Box3df lightBounds_ = Phantom::Math::Box3df::createDegeneratedBox();

    OpacityShadowMapPass  shadowMapPass_;
    ::VKG::VulkanPipeline depositPipeline_;

    TransferFunction tf_;
    ParticleGenerator generator_;
    ParticleSet particleSet_;

    Shaders shaders_;
    std::vector<PBVRVertex> vertices_;
    PBVRPipeline pipeline_;
    ::VKG::VulkanBuffer vertexBuffer_;

    VolumeComputePBVR computePBVR_;
    uint32_t gpuVertexCount_ = 0;
};

} // namespace PBVR
