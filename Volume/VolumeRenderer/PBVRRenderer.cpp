#include "PBVRRenderer.h"
#include "ParticleProbeScattering.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <future>
#include <cmath>
#include <vector>

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
    shadowContentDirty_ = true;
}

void PBVRRenderer::setParticleSize(const float s) {
    particleSize_ = std::clamp(s, 1.0f, 20.0f);
}

void PBVRRenderer::setRepeatCount(const int n) {
    repeatCount_ = std::max(1, n);
    dirty_ = true;
    shadowContentDirty_ = true;
}

void PBVRRenderer::setUseGPU(const bool b) {
    useGPU_ = b;
    dirty_ = true;
    shadowContentDirty_ = true;
}

void PBVRRenderer::setMaxParticlesPerVoxel(const int n) {
    maxParticlesPerVoxel_ = std::max(1, n);
    dirty_ = true;
    shadowContentDirty_ = true;
}

void PBVRRenderer::setLightDir(const float azimuthDeg, const float elevationDeg) {
    if (lightAzimuth_ == azimuthDeg && lightElevation_ == elevationDeg) return;
    lightAzimuth_ = azimuthDeg;
    lightElevation_ = std::clamp(elevationDeg, -89.0f, 89.0f);
    shadowContentDirty_ = true;
    // The CPU scattering bakes T_sun and the phase angle into the particles.
    if (isCpuScatteringActive()) dirty_ = true;
}

void PBVRRenderer::setShadowLayers(const int n) {
    const int clamped = std::clamp(n, 2, 32);
    if (clamped != shadowLayers_) {
        shadowDirty_ = true;
        shadowContentDirty_ = true;
    }
    shadowLayers_ = clamped;
}

void PBVRRenderer::setShadowMapSize(const uint32_t size) {
    const uint32_t clamped = std::max<uint32_t>(64, size);
    if (clamped != shadowMapSize_) {
        shadowDirty_ = true;
        shadowContentDirty_ = true;
    }
    shadowMapSize_ = clamped;
}

void PBVRRenderer::setTransferFunctionPreset(const int preset) {
    // Presets replace the whole curve. Merging left e.g. the rainbow preset's
    // green point at 0.5 inside the OpenVDB preset (green mid-density voxels).
    tf_.clearPoints();
    if (preset == 1) {
        // Cloud preset: dense white-ish core, fading to transparent at the SDF band edge.
        // The default rainbow preset clamps to alpha=0 at scalar=0, which leaves an SDF
        // sphere's interior invisible -- self-shadowing needs visible interior density.
        tf_.setPoint(0.0f, 0.9f, 0.9f, 0.95f, 0.6f);
        tf_.setPoint(0.5f, 0.9f, 0.9f, 0.95f, 0.3f);
        tf_.setPoint(1.0f, 0.9f, 0.9f, 0.95f, 0.0f);
    } else if (preset == 2) {
        // OpenVDB density preset. Unlike the SDF cloud preset, density grids
        // are non-negative and may already be normalized above one. Treat any
        // positive value as participating media instead of making the upper
        // end transparent.
        tf_.setPoint(0.0f, 0.92f, 0.94f, 1.0f, 0.0f);
        tf_.setPoint(0.0001f, 0.92f, 0.94f, 1.0f, 0.85f);
        tf_.setPoint(1.0f, 0.92f, 0.94f, 1.0f, 1.0f);
    } else if (preset == 3) {
        // Linear density: opacity (and so the PBVR particle count) is
        // proportional to the density, so the particle medium has an
        // extinction proportional to it -- needed to match a renderer that
        // uses sigma_t = scale * density (e.g. the WDAS cloud Mitsuba scene).
        tf_.setPoint(0.0f, 1.0f, 1.0f, 1.0f, 0.0f);
        tf_.setPoint(1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    } else {
        tf_.setPoint(0.0f, 0.0f, 0.0f, 1.0f, 0.0f);
        tf_.setPoint(0.5f, 0.0f, 1.0f, 0.0f, 0.5f);
        tf_.setPoint(1.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    }
    tf_.buildLUT();
    dirty_ = true;
    shadowContentDirty_ = true;
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
    shadowContentDirty_ = true;
}

void PBVRRenderer::onUpdate(uint32_t frameIndex) {
    if (!ctx_ || !pool_) {
        return;
    }

    if (pending_.valid() &&
        pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        CpuBuildResult result = pending_.get();
        if (!result.cancelled)
            applyCpuBuildResult(std::move(result));
    }

    if (dirty_) {
        if (asyncCpu_ && !useGPU_ && dataSource_) {
            // One build at a time. Newer settings cancel a running build (its
            // result would be stale anyway); the next build starts as soon as
            // the cancelled one has returned.
            if (pending_.valid()) {
                if (buildCancel_)
                    buildCancel_->store(true);
            } else {
                updateLightBounds();
                CpuBuildInput input = makeCpuBuildInput();
                buildCancel_ = std::make_shared<std::atomic<bool>>(false);
                input.cancel = buildCancel_;
                pending_ = std::async(std::launch::async, &PBVRRenderer::buildCpuParticles, std::move(input));
                dirty_ = false;
            }
        } else {
            regenerateParticles();
            dirty_ = false;
        }
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
    // With CPU multiple scattering T_sun is already inside the particle colour
    // (direct term only); shadowing the whole colour again would also darken
    // the indirect light that is supposed to fill the shadows.
    ubo.shadowEnabled = (shadowEnabled_ && !isCpuScatteringActive()) ? 1.0f : 0.0f;
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
    if (!shadowContentDirty_ && !shadowDirty_) return;

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
        shadowContentDirty_ = true;
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
    shadowContentDirty_ = false;
}

void PBVRRenderer::waitForRegeneration() {
    if (!pending_.valid())
        return;
    CpuBuildResult result = pending_.get();
    if (result.cancelled)
        dirty_ = true; // settings changed while it ran: rebuild with the new ones
    else
        applyCpuBuildResult(std::move(result));
}

void PBVRRenderer::onCleanup(VkDevice device) {
    if (pending_.valid())
        pending_.wait(); // the worker reads the data source's volumes
    pending_ = {};
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

glm::vec3 PBVRRenderer::computeEye() const {
    const float az = glm::radians(azimuth_);
    const float el = glm::radians(elevation_);
    return cameraTarget_ + distance_ * glm::vec3(
        std::cos(el) * std::sin(az),
        std::sin(el),
        std::cos(el) * std::cos(az));
}

glm::mat4 PBVRRenderer::computeMVP() const {
    const glm::mat4 view = glm::lookAt(computeEye(), cameraTarget_, glm::vec3(0.0f, 1.0f, 0.0f));

    const float aspect = (extent_.height > 0)
        ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
        : 1.0f;

    glm::mat4 proj = glm::perspective(glm::radians(fovYDegrees_), aspect,
                                      std::max(0.01f, distance_ * 1.0e-3f), std::max(1000.0f, distance_ * 10.0f));
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

void PBVRRenderer::updateLightBounds() {
    lightBounds_ = Phantom::Math::Box3df::createDegeneratedBox();
    bool haveBounds = false;
    for (const auto& entry : dataSource_->getPBVREntries()) {
        if (!entry.visible || !entry.volume) continue;
        const auto box = entry.volume->getBoundingBox();
        if (!haveBounds) { lightBounds_ = box; haveBounds = true; }
        else            { lightBounds_.add(box); }
    }
}

void PBVRRenderer::regenerateParticles() {
    if (!dataSource_ || !ctx_ || !pool_) {
        return;
    }
    // New particles (and, with CPU scattering, new colours) need a new deposit.
    shadowContentDirty_ = true;
    updateLightBounds();

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

    applyCpuBuildResult(buildCpuParticles(makeCpuBuildInput()));
}

PBVRRenderer::CpuBuildInput PBVRRenderer::makeCpuBuildInput() const {
    CpuBuildInput input;
    if (dataSource_) {
        for (const auto& entry : dataSource_->getPBVREntries()) {
            if (entry.visible && entry.volume)
                input.volumes.push_back(entry.volume);
        }
    }
    input.tf = tf_;
    input.densityScale = densityScale_;
    input.repeatCount = repeatCount_;
    input.scattering = multipleScatteringEnabled_;
    input.orders = scatteringOrders_;
    input.probeCount = probeCount_;
    input.kernelRadius = probeRadius_;
    input.phaseG = phaseG_;
    input.albedo = scatteringAlbedo_;
    input.exposure = scatteringExposure_;
    input.sourceBudget = probeSourceBudget_;
    input.shDegree = scatteringSHDegree_;
    input.shadow = shadowEnabled_;
    input.sigma = sigma_;
    input.shadowMapSize = shadowMapSize_;
    input.towardsLight = computeLightDir();
    input.lightBounds = lightBounds_;
    input.eye = computeEye();
    return input;
}

void PBVRRenderer::applyCpuBuildResult(CpuBuildResult result) {
    particleSet_ = std::move(result.particles);
    vertices_ = std::move(result.vertices);
    particleRadiance_ = std::move(result.radiance);
    meanScatteredRadiance_ = result.meanScattered;
    meanIndirectRadiance_ = result.meanIndirect;
    meanSunTransmittance_ = result.meanSunTransmittance;
    lastScatteringSolveMs_ = result.solveMs;
    gpuVertexCount_ = 0;
    shadowContentDirty_ = true;
    if (!ctx_ || !pool_)
        return;

    // A frame still in flight may be drawing from the old buffer.
    vkDeviceWaitIdle(ctx_->getDevice());
    vertexBuffer_.destroy(ctx_->getDevice());
    if (vertices_.empty()) {
        return;
    }

    vertexBuffer_.create(*ctx_, *pool_,
                         sizeof(PBVRVertex) * vertices_.size(),
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                         vertices_.data());
}

PBVRRenderer::CpuBuildResult PBVRRenderer::buildCpuParticles(const CpuBuildInput& input) {
    CpuBuildResult result;
    ParticleGenerator generator;
    generator.setTransferFunction(&input.tf);
    generator.setDensityScale(input.densityScale);
    // Every regeneration reproduces the same realization, so changing a
    // scattering or view parameter does not also resample the medium.
    generator.reseed(42U);

    for (const SparseVolumef* volume : input.volumes) {
        for (int r = 0; r < input.repeatCount; ++r) {
            ParticleSet generated = generator.generate(*volume);
            result.vertices.reserve(result.vertices.size() + generated.particles.size());
            result.particles.particles.reserve(result.particles.particles.size() + generated.particles.size());

            for (const auto& p : generated.particles) {
                result.particles.particles.push_back(p);
                result.vertices.push_back(PBVRVertex{p.pos, glm::vec4(p.color, 1.0f)});
            }
        }
    }

    const std::atomic<bool>* cancel = input.cancel.get();
    if (ParticleProbeScattering::cancelled(cancel)) {
        result.cancelled = true;
        return result;
    }
    applyMultipleScattering(input, result);
    result.cancelled = ParticleProbeScattering::cancelled(cancel);
    return result;
}

bool PBVRRenderer::isCpuScatteringActive() const {
    return multipleScatteringEnabled_ && !useGPU_;
}

void PBVRRenderer::applyMultipleScattering(const CpuBuildInput& input, CpuBuildResult& result) {
    const auto& particles = result.particles.particles;
    if (!input.scattering || particles.empty() || input.orders < 0) {
        return;
    }

    const std::size_t count = particles.size();
    const auto solveStart = std::chrono::steady_clock::now();
    std::vector<glm::vec3> positions;
    positions.reserve(count);
    for (const auto& particle : particles)
        positions.push_back(particle.pos);

    // Model the particles as the medium the opacity shadow map already sees:
    // every particle deposits sigma into a 3-pixel point sprite (a disc of
    // 1.5 shadow texels), i.e. an opaque-with-probability disc of that radius
    // and opacity 1 - exp(-sigma). Probe maps and T_sun then agree with the
    // GPU shadow in expectation, and SetPBVRExtinction drives both.
    const glm::vec3 extent = input.lightBounds.getLength();
    const float lightHalfSize = std::max(0.5f * glm::length(extent), 1.0f) * 1.2f;
    const float shadowTexel = 2.0f * lightHalfSize / static_cast<float>(std::max(1U, input.shadowMapSize));
    ProbeScatteringSettings settings;
    settings.particleRadius = 1.5f * shadowTexel;
    settings.kernelRadius = input.kernelRadius;
    settings.phaseG = input.phaseG;
    settings.degree = input.shDegree;
    // ISM-style subsets keep the CPU reference interactive on real clouds
    // (O(probes * subset) instead of O(probes * particles)).
    settings.maxSourcesPerProbe = static_cast<std::size_t>(input.sourceBudget);
    const std::vector<float> opacity(count, 1.0f - std::exp(-input.sigma));
    const std::vector<float> albedo(count, input.albedo);

    const glm::vec3 towardsLight = input.towardsLight;
    const glm::vec3 lightPropagation = -towardsLight;
    const glm::vec3 sunIrradiance(1.0f, 0.95f, 0.85f); // sunColor in pbvr_render.frag
    const std::vector<float> sunTransmittance = input.shadow
        ? ParticleProbeScattering::computeDirectionalTransmittance(
              positions, opacity, settings.particleRadius, towardsLight)
        : std::vector<float>(count, 1.0f);

    // Single scattering (plan Sec. 3.2 step 1). It is kept exact for the final
    // view-dependent evaluation; only its SH projection (a delta lobe
    // convolved with HG, i.e. g^l * Y_lm(lightPropagation)) seeds the higher
    // orders, where the distribution is already broad.
    const int coefficientCount = (settings.degree + 1) * (settings.degree + 1);
    std::vector<glm::vec3> singleScatteringWeight(count);
    std::vector<Phantom::Math::SHRGB> direct(count);
    for (std::size_t i = 0; i < count; ++i) {
        // The transfer-function colour tints the scattering albedo.
        singleScatteringWeight[i] = particles[i].color * input.albedo *
            sunIrradiance * sunTransmittance[i];
        direct[i].degree = settings.degree;
        for (int coefficient = 0; coefficient < coefficientCount; ++coefficient) {
            const int band = coefficient == 0 ? 0 : (coefficient < 4 ? 1 : 2);
            const float bandScale = std::pow(input.phaseG, static_cast<float>(band));
            direct[i].coefficients[static_cast<std::size_t>(coefficient)] = singleScatteringWeight[i] *
                (bandScale * Phantom::Math::sphericalHarmonicBasis(coefficient, lightPropagation));
        }
    }

    const auto probes = ParticleProbeScattering::selectUniform(
        positions, static_cast<std::size_t>(input.probeCount), 0x50425652U);
    const auto total = ParticleProbeScattering::solve(
        positions, direct, probes, albedo, opacity, settings, input.orders, input.cancel.get());
    if (total.size() != count || result.vertices.size() != count)
        return;

    constexpr float pi = 3.14159265358979323846f;
    const float g = input.phaseG;
    const glm::vec3 luminanceWeights(0.2126f, 0.7152f, 0.0722f);
    double radianceSum = 0.0;
    double indirectSum = 0.0;
    double transmittanceSum = 0.0;
    result.radiance.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const glm::vec3 toEye = input.eye - positions[i];
        const float eyeDistance = glm::length(toEye);
        const glm::vec3 viewDirection = eyeDistance > 1.0e-6f ? toEye / eyeDistance : glm::vec3(0.0f, 0.0f, 1.0f);

        const float cosTheta = glm::dot(lightPropagation, viewDirection);
        const float phase = (1.0f - g * g) /
            (4.0f * pi * std::pow(std::max(1.0e-6f, 1.0f + g * g - 2.0f * g * cosTheta), 1.5f));
        Phantom::Math::SHRGB indirect = total[i];
        for (int coefficient = 0; coefficient < coefficientCount; ++coefficient)
            indirect.coefficients[static_cast<std::size_t>(coefficient)] -=
                direct[i].coefficients[static_cast<std::size_t>(coefficient)];
        const glm::vec3 indirectRadiance = Phantom::Math::evaluate(indirect, viewDirection, true);
        const glm::vec3 radiance = singleScatteringWeight[i] * phase + indirectRadiance;
        radianceSum += glm::dot(radiance, luminanceWeights);
        indirectSum += glm::dot(indirectRadiance, luminanceWeights);
        transmittanceSum += sunTransmittance[i];
        result.radiance.push_back(radiance);

        // The swapchain is LDR. A soft exposure curve keeps the ratios of the
        // HDR result instead of hard-clipping the forward-scattering peak.
        result.vertices[i].color = glm::vec4(glm::vec3(1.0f) - glm::exp(-input.exposure * radiance),
                                             result.vertices[i].color.a);
    }
    result.meanScattered = static_cast<float>(radianceSum / static_cast<double>(count));
    result.meanIndirect = static_cast<float>(indirectSum / static_cast<double>(count));
    result.meanSunTransmittance = static_cast<float>(transmittanceSum / static_cast<double>(count));
    result.solveMs = static_cast<float>(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - solveStart).count());
    std::fprintf(stderr, "[PBVR] multiple scattering: %zu particles, %d probes, %d orders, %.0f ms\n",
                 count, input.probeCount, input.orders, result.solveMs);
}

bool PBVRRenderer::dumpParticleRadiance(const std::string& path) const {
    if (particleRadiance_.empty() || particleRadiance_.size() != particleSet_.particles.size())
        return false;
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;
    const std::uint64_t count = particleRadiance_.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (std::size_t i = 0; file && i < particleRadiance_.size(); ++i) {
        const glm::vec3& p = particleSet_.particles[i].pos;
        const glm::vec3& l = particleRadiance_[i];
        const float record[6] = {p.x, p.y, p.z, l.x, l.y, l.z};
        file.write(reinterpret_cast<const char*>(record), sizeof(record));
    }
    return static_cast<bool>(file);
}

} // namespace PBVR
