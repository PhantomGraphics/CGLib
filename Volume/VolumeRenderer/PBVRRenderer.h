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
#include <string>
#include <algorithm>
#include <atomic>
#include <future>
#include <memory>
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
    void markDirty() { dirty_ = true; shadowContentDirty_ = true; }
    void syncCamera(float azimuth, float elevation, float distance);
    void setCameraDistance(float d) { distance_ = std::max(0.01f, d); }
    void setCameraFovY(float degrees) { fovYDegrees_ = std::clamp(degrees, 5.0f, 120.0f); }
    void setCameraTarget(const glm::vec3& target) { cameraTarget_ = target; if (isCpuScatteringActive()) dirty_ = true; }
    // The view direction is baked into the CPU scattering colours, so a new
    // viewpoint needs a new solve.
    void setCameraAngles(float azimuthDeg, float elevationDeg) {
        azimuth_ = azimuthDeg;
        elevation_ = elevationDeg;
        if (isCpuScatteringActive()) dirty_ = true;
    }
    // Host-app camera (e.g. Universe, which owns orbit/asset/character cameras): draw with
    // this view/projection instead of the built-in orbit camera above. It never requests a
    // solve by itself -- the host decides when the view-dependent scattering colours are
    // re-evaluated, via setScatteringEye().
    void setExternalCamera(const glm::mat4& view, const glm::mat4& proj) {
        externalCamera_ = true;
        externalView_ = view;
        externalProj_ = proj;
    }
    void clearExternalCamera() { externalCamera_ = false; hasScatteringEye_ = false; }
    void setScatteringEye(const glm::vec3& eye) {
        scatteringEye_ = eye;
        hasScatteringEye_ = true;
        if (isCpuScatteringActive()) dirty_ = true;
    }
    // Linear particle colours (exposure * radiance) instead of the soft LDR curve, for hosts
    // that render into an HDR target and tone-map once afterwards.
    void setLinearColorOutput(bool b) { if (linearColorOutput_ != b) { linearColorOutput_ = b; dirty_ = true; } }
    bool isLinearColorOutput() const { return linearColorOutput_; }
    // Written to the UBO's colorScale (draw-time only, no re-solve); only shaders that
    // declare it use it.
    void setColorScale(float s) { colorScale_ = std::max(0.0f, s); }
    // 0 gathers from every particle; otherwise ISM-style random subsets.
    void setProbeSourceBudget(int n) { probeSourceBudget_ = std::max(0, n); dirty_ = true; }
    void setScatteringSHDegree(int n) { scatteringSHDegree_ = std::clamp(n, 0, 2); dirty_ = true; }
    int getProbeSourceBudget() const { return probeSourceBudget_; }
    int getScatteringSHDegree() const { return scatteringSHDegree_; }
    float getLastScatteringSolveMs() const { return lastScatteringSolveMs_; }
    // Linear per-particle radiance of the last CPU scattering solve:
    // uint64 count, then count * (x y z r g b) float32, little-endian.
    bool dumpParticleRadiance(const std::string& path) const;
    void setShaders(Shaders shaders) { shaders_ = std::move(shaders); }
    // Build CPU particles and solve the scattering on a worker thread instead
    // of inside onUpdate(). Off by default (VolumeView scenarios expect the
    // result on the next frame). While a build runs the previous particles
    // stay on screen; volumes handed out by the data source must stay alive
    // until isRegenerating() is false (see waitForRegeneration()).
    void setAsyncCpuRegeneration(bool b) { asyncCpu_ = b; }
    bool isRegenerating() const { return pending_.valid(); }
    // True while a regeneration is requested but not yet applied (dirty or running).
    bool hasPendingWork() const { return dirty_ || pending_.valid(); }
    // Blocks until a running build has finished and applies its result.
    void waitForRegeneration();
    void setEnabled(bool e) { enabled_ = e; }

    TransferFunction& getTransferFunction() { return tf_; }
    void setDensityScale(float s);
    void setParticleSize(float s);
    void setRepeatCount(int n);
    void setUseGPU(bool b);
    void setMaxParticlesPerVoxel(int n);
    void setMultipleScatteringEnabled(bool b) { multipleScatteringEnabled_ = b; dirty_ = true; shadowContentDirty_ = true; }
    void setScatteringOrders(int n) { scatteringOrders_ = std::clamp(n, 0, 8); dirty_ = true; shadowContentDirty_ = true; }
    void setProbeCount(int n) { probeCount_ = std::max(1, n); dirty_ = true; shadowContentDirty_ = true; }
    void setProbeRadius(float r) { probeRadius_ = std::max(1.0e-4f, r); dirty_ = true; shadowContentDirty_ = true; }
    void setPhaseG(float g) { phaseG_ = std::clamp(g, -0.99f, 0.99f); dirty_ = true; shadowContentDirty_ = true; }
    void setScatteringAlbedo(float a) { scatteringAlbedo_ = std::clamp(a, 0.0f, 1.0f); dirty_ = true; shadowContentDirty_ = true; }
    // Radiance is physical (sun irradiance 1); exposure maps it to the LDR swapchain.
    void setScatteringExposure(float e) { scatteringExposure_ = std::max(0.0f, e); dirty_ = true; }
    // Irradiance (linear RGB) of the directional light in the CPU scattering path; the light's
    // direction is setLightDir(). The default matches the light colour constant of the GPU /
    // non-scattering shader. A strong light is simply a large value.
    void setLightIrradiance(const glm::vec3& e) { lightIrradiance_ = glm::max(e, glm::vec3(0.0f)); dirty_ = true; }
    glm::vec3 getLightIrradiance() const { return lightIrradiance_; }
    // Environment light of the CPU scattering path: mean radiance arriving from the upper and
    // lower hemispheres (e.g. an HDRI's sky and ground halves). Each particle in-scatters it
    // (the phase function averaged to isotropic) attenuated by the medium's own transmittance
    // along a few up / down directions, and it seeds the higher orders like the directional
    // light does. Zero (the default) disables it.
    void setEnvironmentRadiance(const glm::vec3& upper, const glm::vec3& lower) {
        envUpper_ = glm::max(upper, glm::vec3(0.0f));
        envLower_ = glm::max(lower, glm::vec3(0.0f));
        dirty_ = true;
    }
    glm::vec3 getEnvironmentUpperRadiance() const { return envUpper_; }
    glm::vec3 getEnvironmentLowerRadiance() const { return envLower_; }
    float getDensityScale() const { return densityScale_; }
    float getParticleSize() const { return particleSize_; }
    int getRepeatCount() const { return repeatCount_; }
    bool isGPUMode() const { return useGPU_; }
    int  getMaxParticlesPerVoxel() const { return maxParticlesPerVoxel_; }
    bool isMultipleScatteringEnabled() const { return multipleScatteringEnabled_; }
    int getScatteringOrders() const { return scatteringOrders_; }
    int getProbeCount() const { return probeCount_; }
    float getProbeRadius() const { return probeRadius_; }
    float getPhaseG() const { return phaseG_; }
    float getScatteringAlbedo() const { return scatteringAlbedo_; }
    float getScatteringExposure() const { return scatteringExposure_; }
    // Read-only diagnostics of the last CPU scattering solve (linear radiance
    // luminance averaged over particles), for scenario assertions.
    float getMeanScatteredRadiance() const { return meanScatteredRadiance_; }
    float getMeanIndirectRadiance() const { return meanIndirectRadiance_; }
    float getMeanSunTransmittance() const { return meanSunTransmittance_; }
    float getMeanEnvironmentRadiance() const { return meanEnvironmentRadiance_; }
    size_t getParticleCount() const {
        return useGPU_ ? static_cast<size_t>(gpuVertexCount_) : particleSet_.count();
    }

    // Self-shadow (experimental, Opacity Shadow Map). See internal design notes.
    void setLightDir(float azimuthDeg, float elevationDeg);
    void setShadowEnabled(bool b) { shadowEnabled_ = b; if (isCpuScatteringActive()) dirty_ = true; }
    void setExtinction(float sigma) { sigma_ = std::max(0.0f, sigma); if (isCpuScatteringActive()) dirty_ = true; }
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

    // The opacity shadow map for other renderers to sample (e.g. the medium's shadow on meshes):
    // layer i holds the particle count accumulated from the light up to light-space NDC depth
    // (i+1)/layers, so exp(-getExtinction() * count) is the expected transmittance of the
    // stochastic particle medium. Only sample it while isShadowMapReady() (the image has no
    // defined contents before its first deposit). The view changes whenever the map is recreated;
    // getShadowMapGeneration() changes with it so callers know to rewrite their descriptors.
    bool        isShadowMapReady()       const { return shadowDeposited_ && shadowMapPass_.isValid(); }
    uint64_t    getShadowMapGeneration() const { return shadowMapGeneration_; }
    VkImageView getShadowMapView()       const { return shadowMapPass_.getArrayView(); }
    VkSampler   getShadowMapSampler()    const { return shadowMapPass_.getSampler(); }
    glm::mat4   getShadowMapLightVP()    const { return shadowMapPass_.getLightVP(); } // of the deposited contents
    uint32_t    getShadowMapLayerCount() const { return shadowMapPass_.getLayerCount(); }

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
    // Everything the CPU particle build reads, captured on the render thread
    // so the build itself is a pure function that can run on a worker.
    struct CpuBuildInput {
        std::vector<const SparseVolumef*> volumes; // non-owning
        TransferFunction tf;
        float densityScale = 1.0f;
        int repeatCount = 1;
        bool scattering = false;
        int orders = 0;
        int probeCount = 256;
        float kernelRadius = 1.0f;
        float phaseG = 0.0f;
        float albedo = 1.0f;
        float exposure = 1.0f;
        int sourceBudget = 0;
        int shDegree = 1;
        bool shadow = false;
        float sigma = 1.0f;
        uint32_t shadowMapSize = 512;
        glm::vec3 towardsLight{0.0f, 1.0f, 0.0f};
        Phantom::Math::Box3df lightBounds = Phantom::Math::Box3df::createDegeneratedBox();
        glm::vec3 eye{0.0f};
        bool linearColor = false;
        glm::vec3 lightIrradiance{1.0f, 0.95f, 0.85f};
        glm::vec3 envUpper{0.0f};
        glm::vec3 envLower{0.0f};
        // Set by the render thread when newer settings arrive; the build then
        // stops early and its result is discarded.
        std::shared_ptr<std::atomic<bool>> cancel;
    };
    struct CpuBuildResult {
        ParticleSet particles;
        std::vector<PBVRVertex> vertices;
        std::vector<glm::vec3> radiance;
        float meanScattered = 0.0f;
        float meanIndirect = 0.0f;
        float meanSunTransmittance = 1.0f;
        float meanEnvironment = 0.0f;
        float solveMs = 0.0f;
        bool cancelled = false;
    };
    static CpuBuildResult buildCpuParticles(const CpuBuildInput& input);
    static void applyMultipleScattering(const CpuBuildInput& input, CpuBuildResult& result);
    CpuBuildInput makeCpuBuildInput() const;
    void applyCpuBuildResult(CpuBuildResult result);
    void updateLightBounds();

    glm::mat4 computeMVP() const;
    glm::vec3 computeEye() const;
    void regenerateParticles();
    bool isCpuScatteringActive() const;
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
    bool multipleScatteringEnabled_ = false;
    int probeCount_ = 256;
    int scatteringOrders_ = 2;
    float probeRadius_ = 2.5f;
    float phaseG_ = 0.85f;
    float scatteringAlbedo_ = 0.8f;
    float scatteringExposure_ = 8.0f;
    float meanScatteredRadiance_ = 0.0f;
    float meanIndirectRadiance_ = 0.0f;
    float meanSunTransmittance_ = 1.0f;
    float meanEnvironmentRadiance_ = 0.0f;
    glm::vec3 lightIrradiance_{1.0f, 0.95f, 0.85f}; // sunColor in pbvr_render.frag
    glm::vec3 envUpper_{0.0f};
    glm::vec3 envLower_{0.0f};
    int probeSourceBudget_ = 4096;
    int scatteringSHDegree_ = 1;
    float lastScatteringSolveMs_ = 0.0f;
    glm::vec3 cameraTarget_{0.0f};
    float fovYDegrees_ = 45.0f;
    std::vector<glm::vec3> particleRadiance_;
    float azimuth_ = 0.0f;
    float elevation_ = 30.0f;
    float distance_ = 50.0f;
    bool externalCamera_ = false;
    glm::mat4 externalView_{1.0f};
    glm::mat4 externalProj_{1.0f};
    bool hasScatteringEye_ = false;
    glm::vec3 scatteringEye_{0.0f};
    bool linearColorOutput_ = false;
    float colorScale_ = 1.0f;

    // Self-shadow (experimental).
    float    lightAzimuth_   = 45.0f;
    float    lightElevation_ = 60.0f;
    bool     shadowEnabled_  = false;
    float    sigma_          = 1.0f;
    int      shadowLayers_   = 8;
    bool     shadowDeposited_     = false; // see isShadowMapReady()
    uint64_t shadowMapGeneration_ = 0;
    uint32_t shadowMapSize_  = 512;
    bool     shadowDirty_    = true; // OpacityShadowMapPass needs (re)creation (layers/size changed)
    // The shadow image is persistent. Re-record the expensive particle deposit only when
    // its contents or the light-space projection changes, following GSView's sceneDirty_ path.
    bool     shadowContentDirty_ = true;
    bool     depositPipelineCreated_ = false;
    Phantom::Math::Box3df lightBounds_ = Phantom::Math::Box3df::createDegeneratedBox();

    OpacityShadowMapPass  shadowMapPass_;
    ::VKG::VulkanPipeline depositPipeline_;

    TransferFunction tf_;
    ParticleSet particleSet_;

    Shaders shaders_;
    std::vector<PBVRVertex> vertices_;
    PBVRPipeline pipeline_;
    ::VKG::VulkanBuffer vertexBuffer_;

    VolumeComputePBVR computePBVR_;
    uint32_t gpuVertexCount_ = 0;

    bool asyncCpu_ = false;
    std::future<CpuBuildResult> pending_;
    std::shared_ptr<std::atomic<bool>> buildCancel_;
};

} // namespace PBVR
