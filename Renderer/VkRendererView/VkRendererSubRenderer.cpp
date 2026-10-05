#include "VkRendererSubRenderer.h"

#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanImage.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <cstring>

namespace VKRenderer {

void VkRendererSubRenderer::onInit(Phantom::VKG::VulkanContext& ctx,
                                   const Phantom::VKG::VulkanCommandPool& pool,
                                   VkRenderPass renderPass,
                                   uint32_t framesInFlight) {
    ctx_ = &ctx;
    pool_ = &pool;
    renderPass_ = renderPass;
    framesInFlight_ = framesInFlight;

    Phantom::VKG::VkPointRenderer::Config pointCfg;
    pointCfg.vertSpv = std::move(shaders_.pointVert);
    pointCfg.fragSpv = std::move(shaders_.pointFrag);
    pointRenderer_.emplace(std::move(pointCfg));
    pointRenderer_->create(ctx, pool, renderPass, framesInFlight);

    Phantom::VKG::VkLineRenderer::Config lineCfg;
    lineCfg.vertSpv = std::move(shaders_.lineVert);
    lineCfg.fragSpv = std::move(shaders_.lineFrag);
    lineRenderer_.emplace(std::move(lineCfg));
    lineRenderer_->create(ctx, pool, renderPass, framesInFlight);

    Phantom::VKG::VkTriangleRenderer::Config triCfg;
    triCfg.vertSpv = std::move(shaders_.triVert);
    triCfg.fragSpv = std::move(shaders_.triFrag);
    triangleRenderer_.emplace(std::move(triCfg));
    triangleRenderer_->create(ctx, pool, renderPass, framesInFlight);

    Phantom::VKG::VkTexRenderer::Config texCfg;
    texCfg.vertSpv = std::move(shaders_.texVert);
    texCfg.fragSpv = std::move(shaders_.texFrag);
    texRenderer_.emplace(std::move(texCfg));
    texRenderer_->create(ctx, pool, renderPass, framesInFlight);

    Phantom::VKG::VkSkyBoxRenderer::Config skyCfg;
    skyCfg.vertSpv = std::move(shaders_.skyboxVert);
    skyCfg.fragSpv = std::move(shaders_.skyboxFrag);
    skyBoxRenderer_.emplace(std::move(skyCfg));
    skyBoxRenderer_->create(ctx, pool, renderPass, framesInFlight);

    uploadSamplePoint();
    uploadSampleLine();
    uploadSampleTriangle();
    // Geometry is static: upload it once here. onUpdate() only rewrites the per-frame MVP, because
    // upload() recreates the vertex buffers, which an earlier frame in flight may still be using.
    uploadSampleGeometry();
    if (!createSampleTexture())
        std::fprintf(stderr, "[VkRendererSubRenderer] createSampleTexture failed; sample texture disabled\n");

    if (hasExternalCubeMap_) {
        skyBoxRenderer_->setCubeMap(ctx.getDevice(), externalCubeMapView_, externalCubeMapSampler_);
        skyBoxReady_ = true;
    } else {
        fallbackCubeMap_.createDummy(ctx, pool);
        skyBoxRenderer_->setCubeMap(ctx.getDevice(),
                                    fallbackCubeMap_.getImageView(),
                                    fallbackCubeMap_.getSampler());
        skyBoxReady_ = true;
    }
}

void VkRendererSubRenderer::onUpdate(uint32_t frameIndex) {
    if (!ctx_ || !pool_) return;

    const glm::mat4 mvp = computeMVP();

    switch (activeMode_) {
    case Mode::Point:
        if (pointRenderer_) pointRenderer_->updateMVP(frameIndex, mvp);
        break;
    case Mode::Line:
        if (lineRenderer_) {
            lineRenderer_->updateMVP(frameIndex, mvp);
        }
        break;
    case Mode::Triangle:
        if (triangleRenderer_) triangleRenderer_->updateMVP(frameIndex, mvp);
        break;
    case Mode::Tex:
        if (texRenderer_ && texReady_) {
            texRenderer_->setTexture(ctx_->getDevice(), texImageView_, texSampler_.get(), frameIndex);
        }
        break;
    case Mode::SkyBox:
        if (skyBoxRenderer_ && skyBoxReady_) {
            Phantom::VKG::VkSkyBoxRenderer::Buffer buffer;
            buffer.projectionMatrix = proj_;
            glm::mat4 viewNoTranslation = view_;
            viewNoTranslation[3] = glm::vec4(0.f, 0.f, 0.f, view_[3].w);
            buffer.viewMatrix = viewNoTranslation;
            skyBoxRenderer_->upload(buffer, frameIndex);
        }
        break;
    }
}

void VkRendererSubRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    switch (activeMode_) {
    case Mode::Point:
        if (pointRenderer_ && pointRenderer_->isValid()) {
            pointRenderer_->render(cmd, frameIndex);
        }
        break;
    case Mode::Line:
        if (lineRenderer_ && lineRenderer_->isValid()) {
            lineRenderer_->render(cmd, frameIndex);
        }
        break;
    case Mode::Triangle:
        if (triangleRenderer_ && triangleRenderer_->isValid()) {
            triangleRenderer_->render(cmd, frameIndex);
        }
        break;
    case Mode::Tex:
        if (texRenderer_ && texRenderer_->isValid()) {
            texRenderer_->render(cmd, frameIndex);
        }
        break;
    case Mode::SkyBox:
        if (skyBoxRenderer_ && skyBoxRenderer_->isValid()) {
            skyBoxRenderer_->render(cmd, frameIndex);
        }
        break;
    }
}

void VkRendererSubRenderer::onCleanup(VkDevice device) {
    if (pointRenderer_) {
        pointRenderer_->destroy(device);
        pointRenderer_.reset();
    }
    if (lineRenderer_) {
        lineRenderer_->destroy(device);
        lineRenderer_.reset();
    }
    if (triangleRenderer_) {
        triangleRenderer_->destroy(device);
        triangleRenderer_.reset();
    }
    if (texRenderer_) {
        texRenderer_->destroy(device);
        texRenderer_.reset();
    }
    if (skyBoxRenderer_) {
        skyBoxRenderer_->destroy(device);
        skyBoxRenderer_.reset();
    }

    if (texSampler_.isValid()) {
        texSampler_.destroy(device);
    }
    if (texImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device, texImageView_, nullptr);
        texImageView_ = VK_NULL_HANDLE;
    }
    if (texImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device, texImage_, nullptr);
        texImage_ = VK_NULL_HANDLE;
    }
    if (texMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device, texMemory_, nullptr);
        texMemory_ = VK_NULL_HANDLE;
    }

    fallbackCubeMap_.destroy(device);

    texReady_ = false;
    skyBoxReady_ = false;
}

void VkRendererSubRenderer::setCubeMap(VkDevice device, VkImageView view, VkSampler sampler) {
    hasExternalCubeMap_ = (view != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE);
    externalCubeMapView_ = view;
    externalCubeMapSampler_ = sampler;

    if (skyBoxRenderer_ && hasExternalCubeMap_) {
        skyBoxRenderer_->setCubeMap(device, view, sampler);
        skyBoxReady_ = true;
    }
}

void VkRendererSubRenderer::uploadSampleGeometry() {
    const glm::mat4 mvp = computeMVP();
    if (pointRenderer_) {
        Phantom::VKG::VkPointRenderer::Buffer buffer;
        buffer.positions = samplePointPositions_;
        buffer.colors = samplePointColors_;
        buffer.sizes = samplePointSizes_;
        buffer.projectionMatrix = mvp;
        buffer.modelViewMatrix = glm::mat4(1.0f);
        pointRenderer_->upload(*ctx_, *pool_, buffer);
    }
    if (triangleRenderer_) {
        Phantom::VKG::VkTriangleRenderer::Buffer buffer;
        buffer.positions = sampleTrianglePositions_;
        buffer.colors = sampleTriangleColors_;
        buffer.indices = sampleTriangleIndices_;
        buffer.projectionMatrix = mvp;
        buffer.modelViewMatrix = glm::mat4(1.0f);
        triangleRenderer_->upload(*ctx_, *pool_, buffer);
    }
}

void VkRendererSubRenderer::uploadSamplePoint() {
    samplePointPositions_ = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
    };

    samplePointColors_ = {
        1.f, 0.f, 0.f, 1.f,
        0.f, 1.f, 0.f, 1.f,
        0.f, 0.f, 1.f, 1.f,
        1.f, 1.f, 0.f, 1.f,
        1.f, 0.f, 1.f, 1.f,
        0.f, 1.f, 1.f, 1.f,
        1.f, 0.5f, 0.f, 1.f,
        1.f, 1.f, 1.f, 1.f,
    };

    samplePointSizes_.assign(8, 12.0f);
}

void VkRendererSubRenderer::uploadSampleLine() {
    sampleLinePositions_ = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
    };

    sampleLineColors_ = {
        1.f, 0.9f, 0.2f, 1.f,
        1.f, 0.9f, 0.2f, 1.f,
        1.f, 0.9f, 0.2f, 1.f,
        1.f, 0.9f, 0.2f, 1.f,
        0.2f, 0.9f, 1.f, 1.f,
        0.2f, 0.9f, 1.f, 1.f,
        0.2f, 0.9f, 1.f, 1.f,
        0.2f, 0.9f, 1.f, 1.f,
    };

    sampleLineIndices_ = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7,
    };

    if (!lineRenderer_ || !ctx_ || !pool_) return;

    Phantom::VKG::VkLineRenderer::Buffer buffer;
    buffer.positions = sampleLinePositions_;
    buffer.colors = sampleLineColors_;
    buffer.indices = sampleLineIndices_;
    buffer.projectionMatrix = computeMVP();
    buffer.modelViewMatrix = glm::mat4(1.0f);
    lineRenderer_->upload(*ctx_, *pool_, buffer);
}

void VkRendererSubRenderer::uploadSampleTriangle() {
    sampleTrianglePositions_ = {
         0.0f,  0.6f, 0.0f,
        -0.6f, -0.4f, 0.0f,
         0.6f, -0.4f, 0.0f,
    };

    sampleTriangleColors_ = {
        1.f, 0.2f, 0.2f, 1.f,
        0.2f, 1.f, 0.2f, 1.f,
        0.2f, 0.4f, 1.f, 1.f,
    };

    sampleTriangleIndices_ = {0, 1, 2};
}

bool VkRendererSubRenderer::createSampleTexture() {
    if (!ctx_ || !pool_) return false;

    VkDevice device = ctx_->getDevice();

    const std::array<uint8_t, 16> pixels = {
        255, 70, 70, 255,
        70, 255, 70, 255,
        70, 70, 255, 255,
        240, 240, 100, 255,
    };

    // One call uploads the 2x2 RGBA texture; on failure nothing is left allocated.
    if (!Phantom::VKG::VulkanImage::createFromPixelsRGBA8(*ctx_, *pool_, pixels.data(), 2, 2, false,
                                                          texImage_, texMemory_, texImageView_)) {
        std::fprintf(stderr, "[VkRendererSubRenderer] Failed to create sample texture\n");
        return false;
    }

    texSampler_.create(device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, false, 1.0f);

    texReady_ = true;
    return true;
}

glm::mat4 VkRendererSubRenderer::computeMVP() const {
    return proj_ * view_;
}

} // namespace VKRenderer
