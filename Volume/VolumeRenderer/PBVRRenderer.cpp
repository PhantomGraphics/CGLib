#include "PBVRRenderer.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cstdio>

namespace Phantom::Volume {

namespace {
// Layout must match the push_constant block in opacity_shadow_deposit.vert.
struct DepositPushConstants {
    glm::mat4 lightVP;
    float     layerFar;
};
} // namespace

void PBVRRenderer::syncCamera(const float azimuth, const float elevation, const float distance) {
    azimuth_ = azimuth;
    elevation_ = elevation;
    distance_ = distance;
}

void PBVRRenderer::setDensityScale(const float s) {
    densityScale_ = std::max(0.0f, s);
    dirty_ = true;
}

void PBVRRenderer::setParticleSize(const float s) {
    particleSize_ = std::clamp(s, 1.0f, 20.0f);
}

void PBVRRenderer::setRepeatCount(const int n) {
    repeatCount_ = std::max(1, n);
    dirty_ = true;
}

void PBVRRenderer::setUseGPU(const bool b) {
    useGPU_ = b;
    dirty_ = true;
}

void PBVRRenderer::setMaxParticlesPerVoxel(const int n) {
    maxParticlesPerVoxel_ = std::max(1, n);
    dirty_ = true;
}

void PBVRRenderer::setLightDir(const float azimuthDeg, const float elevationDeg) {
    lightAzimuth_ = azimuthDeg;
    lightElevation_ = std::clamp(elevationDeg, -89.0f, 89.0f);
}

void PBVRRenderer::setShadowLayers(const int n) {
    const int clamped = std::clamp(n, 2, 32);
    if (clamped != shadowLayers_) shadowDirty_ = true;
    shadowLayers_ = clamped;
}

void PBVRRenderer::setShadowMapSize(const uint32_t size) {
    const uint32_t clamped = std::max<uint32_t>(64, size);
    if (clamped != shadowMapSize_) shadowDirty_ = true;
    shadowMapSize_ = clamped;
}

void PBVRRenderer::setTransferFunctionPreset(const int preset) {
    if (preset == 1) {
        // Cloud preset: dense white-ish core, fading to transparent at the SDF band edge.
        // The default rainbow preset clamps to alpha=0 at scalar=0, which leaves an SDF
        // sphere's interior invisible -- self-shadowing needs visible interior density.
        tf_.setPoint(0.0f, 0.9f, 0.9f, 0.95f, 0.6f);
        tf_.setPoint(0.5f, 0.9f, 0.9f, 0.95f, 0.3f);
        tf_.setPoint(1.0f, 0.9f, 0.9f, 0.95f, 0.0f);
    } else {
        tf_.setPoint(0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        tf_.setPoint(0.5f, 0.0f, 1.0f, 0.0f, 0.5f);
        tf_.setPoint(1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    }
    tf_.buildLUT();
    dirty_ = true;
}

glm::vec3 PBVRRenderer::computeLightDir() const {
    const float az = glm::radians(lightAzimuth_);
    const float el = glm::radians(lightElevation_);
    return glm::normalize(glm::vec3(
        std::cos(el) * std::sin(az),
        std::sin(el),
        std::cos(el) * std::cos(az)));
}

void PBVRRenderer::onInit(::VKG::VulkanContext& ctx, const ::VKG::VulkanCommandPool& pool,
                            VkRenderPass renderPass, uint32_t framesInFlight) {
    ctx_ = &ctx;
    pool_ = &pool;
    framesInFlight_ = framesInFlight;

    setTransferFunctionPreset(0);

    generator_.setTransferFunction(&tf_);
    generator_.setDensityScale(densityScale_);

    // Create the shadow infrastructure eagerly (not lazily on first use) so that binding=1 of
    // the main PBVRPipeline below always has something valid written to it -- pbvr_render.frag
    // unconditionally declares that binding, so the pipeline layout must always include it here
    // (VolumeView's shaders differ from GSView's separate gs_pbvr.vert/.frag in this respect).
    shadowMapPass_.create(ctx, shadowMapSize_, static_cast<uint32_t>(shadowLayers_));
    shadowDirty_ = false;

    ::VKG::PipelineConfig depositCfg{};
    depositCfg.vertSpv = std::move(shaders_.depositVertSpv);
    depositCfg.fragSpv = std::move(shaders_.depositFragSpv);
    depositCfg.bindingDescs = { {0, sizeof(PBVRVertex), VK_VERTEX_INPUT_RATE_VERTEX} };
    depositCfg.attrDescs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(PBVRVertex, pos))},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(PBVRVertex, color))},
    };
    depositCfg.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    depositCfg.cullMode = VK_CULL_MODE_NONE;
    depositCfg.depthTest = false;
    depositCfg.depthWrite = false;
    depositCfg.blendEnable = true;
    depositCfg.additiveBlend = true;

    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcRange.size = sizeof(DepositPushConstants);
    depositCfg.pushConstantRanges = { pcRange };

    depositPipelineCreated_ = depositPipeline_.create(ctx, shadowMapPass_.getRenderPass(), depositCfg);

    pipeline_.create(ctx, renderPass, framesInFlight,
                     std::move(shaders_.vertSpv), std::move(shaders_.fragSpv),
                     /*enableAlphaBlend=*/true, /*enableShadowSampler=*/true);

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        pipeline_.updateShadowMap(i, shadowMapPass_.getArrayView(), shadowMapPass_.getSampler());
    }

    computePBVR_.create(ctx);
    dirty_ = true;
}

void PBVRRenderer::onUpdate(uint32_t frameIndex) {
    if (!ctx_ || !pool_) {
        return;
    }

    if (dirty_) {
        regenerateParticles();
        dirty_ = false;
    }

    PBVRPipeline::UBO ubo{};
    ubo.mvp = computeMVP();
    ubo.particleSize = particleSize_;
    // Computed the same way renderShadowDeposit() sets shadowMapPass_'s light VP (same pure
    // functions, same member state) so the main pass always samples with the matrix that was
    // actually used to deposit this frame's shadow map, regardless of onUpdate/onPreRender order.
    ubo.lightVP = computeLightProj() * computeLightView();
    ubo.sigma = sigma_;
    ubo.layerCount = static_cast<float>(shadowLayers_);
    ubo.shadowEnabled = shadowEnabled_ ? 1.0f : 0.0f;
    pipeline_.updateUBO(frameIndex, ubo);
}

bool PBVRRenderer::getActiveBuffer(VkBuffer& vbuf, uint32_t& vtxCount) const {
    if (useGPU_) {
        if (!computePBVR_.isValid() || gpuVertexCount_ == 0) return false;
        vbuf     = computePBVR_.getVertexBuffer();
        vtxCount = gpuVertexCount_;
    } else {
        if (vertices_.empty() || !vertexBuffer_.isValid()) return false;
        vbuf     = vertexBuffer_.getBuffer();
        vtxCount = static_cast<uint32_t>(vertices_.size());
    }
    return true;
}

void PBVRRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (!enabled_) return;

    VkBuffer vbuf;
    uint32_t vtxCount;
    if (!getActiveBuffer(vbuf, vtxCount)) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

    const VkDescriptorSet set = pipeline_.getDescriptorSet(frameIndex);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1, &set, 0, nullptr);

    vkCmdDraw(cmd, vtxCount, 1, 0, 0);
}

void PBVRRenderer::renderShadowDeposit(VkCommandBuffer cmd) {
    // Deliberately NOT gated on shadowEnabled_: the main PBVRPipeline's descriptor set is
    // written once (at onInit()/updateShadowMap()) declaring binding=1's image layout as
    // SHADER_READ_ONLY_OPTIMAL, but that layout is only actually reached via this render
    // pass's finalLayout transition. If this were skipped while shadowEnabled_ is false, the
    // very first frame that flips shadowEnabled_ back to true (or simply the first frame PBVR
    // rendering turns on, since onRender() draws unconditionally of shadowEnabled_) would bind
    // a descriptor set whose declared layout doesn't match the image's actual (UNDEFINED)
    // layout -- a Vulkan validation error. shadowEnabled_ instead only gates whether the main
    // pass's shader *uses* the deposited data (via UBO.shadowEnabled, see onUpdate()).
    if (!enabled_ || !ctx_ || !depositPipelineCreated_) return;

    VkBuffer vbuf;
    uint32_t vtxCount;
    if (!getActiveBuffer(vbuf, vtxCount)) return;

    if (shadowDirty_) {
        // Layer count / resolution changed via the UI: recreate at the new size. The wait avoids
        // rewriting descriptor sets (below) or destroying the old image while a still-in-flight
        // command buffer from a previous frame might still reference them (see
        // PBVRPipeline::updateShadowMap()'s comment).
        vkDeviceWaitIdle(ctx_->getDevice());
        shadowMapPass_.destroy(*ctx_);
        if (!shadowMapPass_.create(*ctx_, shadowMapSize_, static_cast<uint32_t>(shadowLayers_))) {
            return; // shadowDirty_ stays true; retried on the next call.
        }
        shadowDirty_ = false;
        for (uint32_t i = 0; i < framesInFlight_; ++i) {
            pipeline_.updateShadowMap(i, shadowMapPass_.getArrayView(), shadowMapPass_.getSampler());
        }
    }
    if (!shadowMapPass_.isValid()) return;

    shadowMapPass_.setLightViewProj(computeLightView(), computeLightProj());

    DepositPushConstants pc{};
    pc.lightVP = shadowMapPass_.getLightVP();

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depositPipeline_.getPipeline());

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

    const uint32_t layerCount = shadowMapPass_.getLayerCount();
    for (uint32_t i = 0; i < layerCount; ++i) {
        pc.layerFar = static_cast<float>(i + 1) / static_cast<float>(layerCount);
        shadowMapPass_.beginLayer(cmd, i);
        vkCmdPushConstants(cmd, depositPipeline_.getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                           0, sizeof(DepositPushConstants), &pc);
        vkCmdDraw(cmd, vtxCount, 1, 0, 0);
        shadowMapPass_.endLayer(cmd);
    }
}

void PBVRRenderer::onCleanup(VkDevice device) {
    computePBVR_.destroy(device);
    vertexBuffer_.destroy(device);
    pipeline_.destroy(device);
    depositPipeline_.destroy(device);
    if (ctx_) shadowMapPass_.destroy(*ctx_);
    vertices_.clear();
    particleSet_.clear();
    gpuVertexCount_ = 0;
}

void PBVRRenderer::onImGui() {
}

glm::mat4 PBVRRenderer::computeMVP() const {
    const float az = glm::radians(azimuth_);
    const float el = glm::radians(elevation_);

    const glm::vec3 target(0.0f, 0.0f, 0.0f);
    const glm::vec3 eye(
        distance_ * std::cos(el) * std::sin(az),
        distance_ * std::sin(el),
        distance_ * std::cos(el) * std::cos(az));

    const glm::mat4 view = glm::lookAt(eye, target, glm::vec3(0.0f, 1.0f, 0.0f));

    const float aspect = (extent_.height > 0)
        ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
        : 1.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 1000.0f);
    proj[1][1] *= -1.0f;

    return proj * view;
}

glm::mat4 PBVRRenderer::computeLightView() const {
    const glm::vec3 center = lightBounds_.getCenter();
    const glm::vec3 extent = lightBounds_.getLength();
    const float radius = std::max(0.5f * glm::length(extent), 1.0f);
    const glm::vec3 eye = center + computeLightDir() * (radius * 2.0f);
    return glm::lookAt(eye, center, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 PBVRRenderer::computeLightProj() const {
    const glm::vec3 extent = lightBounds_.getLength();
    const float radius = std::max(0.5f * glm::length(extent), 1.0f);
    const float halfSize = radius * 1.2f;
    return glm::ortho(-halfSize, halfSize, -halfSize, halfSize, 0.01f, radius * 4.0f + 1.0f);
}

void PBVRRenderer::regenerateParticles() {
    if (!dataSource_ || !ctx_ || !pool_) {
        return;
    }

    lightBounds_ = Phantom::Math::Box3df::createDegeneratedBox();
    bool haveBounds = false;
    for (const auto& entry : dataSource_->getPBVREntries()) {
        if (!entry.visible || !entry.volume) continue;
        const auto box = entry.volume->getBoundingBox();
        if (!haveBounds) { lightBounds_ = box; haveBounds = true; }
        else            { lightBounds_.add(box); }
    }

    if (useGPU_) {
        // Serialize all visible volumes into a flat voxel list for the GPU
        std::vector<GpuVoxelEntry> allVoxels;
        float voxelSize = 1.0f;

        for (const auto& entry : dataSource_->getPBVREntries()) {
            if (!entry.visible || !entry.volume) continue;
            voxelSize = entry.volume->getVoxelSize();
            entry.volume->forEachActive(
                [&](const Phantom::Volume::Coord&,
                    const Phantom::Math::Vector3df& p, float v) {
                    allVoxels.push_back({p.x, p.y, p.z, v});
                });
        }

        computePBVR_.setDensityScale(densityScale_);
        computePBVR_.setMaxParticlesPerVoxel(maxParticlesPerVoxel_);
        computePBVR_.dispatch(*ctx_, *pool_, allVoxels, voxelSize, tf_.getLUT());
        gpuVertexCount_ = computePBVR_.getVertexCount();
        return;
    }

    // CPU path
    vertices_.clear();
    particleSet_.clear();
    gpuVertexCount_ = 0;

    generator_.setDensityScale(densityScale_);

    for (const auto& entry : dataSource_->getPBVREntries()) {
        if (!entry.visible || !entry.volume) {
            continue;
        }

        for (int r = 0; r < repeatCount_; ++r) {
            ParticleSet generated = generator_.generate(*entry.volume);
            vertices_.reserve(vertices_.size() + generated.particles.size());
            particleSet_.particles.reserve(particleSet_.particles.size() + generated.particles.size());

            for (const auto& p : generated.particles) {
                particleSet_.particles.push_back(p);
                vertices_.push_back(PBVRVertex{p.pos, glm::vec4(p.color, 1.0f)});
            }
        }
    }

    vertexBuffer_.destroy(ctx_->getDevice());
    if (vertices_.empty()) {
        return;
    }

    vertexBuffer_.create(*ctx_, *pool_,
                         sizeof(PBVRVertex) * vertices_.size(),
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vertices_.data());
}

} // namespace PBVR
