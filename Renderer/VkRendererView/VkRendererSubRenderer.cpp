#include "VkRendererSubRenderer.h"

#include "../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../CGLib/VulkanGraphics/VulkanImage.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdio>
#include <cstring>

namespace VKRenderer {

namespace {

void transitionToTransferDst(VkCommandBuffer cmd, VkImage image) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);
}

void transitionToShaderRead(VkCommandBuffer cmd, VkImage image) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);
}

} // namespace

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
    case Mode::Point: {
        if (!pointRenderer_) break;
        Phantom::VKG::VkPointRenderer::Buffer buffer;
        buffer.positions = samplePointPositions_;
        buffer.colors = samplePointColors_;
        buffer.sizes = samplePointSizes_;
        buffer.projectionMatrix = mvp;
        buffer.modelViewMatrix = glm::mat4(1.0f);
        pointRenderer_->upload(*ctx_, *pool_, buffer);
        break;
    }
    case Mode::Line:
        if (lineRenderer_) {
            lineRenderer_->updateMVP(frameIndex, mvp);
        }
        break;
    case Mode::Triangle: {
        if (!triangleRenderer_) break;
        Phantom::VKG::VkTriangleRenderer::Buffer buffer;
        buffer.positions = sampleTrianglePositions_;
        buffer.colors = sampleTriangleColors_;
        buffer.indices = sampleTriangleIndices_;
        buffer.projectionMatrix = mvp;
        buffer.modelViewMatrix = glm::mat4(1.0f);
        triangleRenderer_->upload(*ctx_, *pool_, buffer);
        break;
    }
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

    Phantom::VKG::VulkanImage::create(*ctx_,
                             2,
                             2,
                             VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_TILING_OPTIMAL,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             texImage_,
                             texMemory_);

    texImageView_ = Phantom::VKG::VulkanImage::createView(device,
                                                 texImage_,
                                                 VK_FORMAT_R8G8B8A8_UNORM,
                                                 VK_IMAGE_ASPECT_COLOR_BIT);

    texSampler_.create(device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, false, 1.0f);

    const std::array<uint8_t, 16> pixels = {
        255, 70, 70, 255,
        70, 255, 70, 255,
        70, 70, 255, 255,
        240, 240, 100, 255,
    };

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = static_cast<VkDeviceSize>(pixels.size());
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        std::fprintf(stderr, "[VkRendererSubRenderer] Failed to create texture staging buffer\n");
        return false;
    }

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReq);

    auto memType = ctx_->findMemoryType(memReq.memoryTypeBits,
                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (!memType) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        std::fprintf(stderr, "[VkRendererSubRenderer] Failed to find suitable memory type for texture staging buffer\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = *memType;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        std::fprintf(stderr, "[VkRendererSubRenderer] Failed to allocate texture staging memory\n");
        return false;
    }

    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

    void* mapped = nullptr;
    vkMapMemory(device, stagingMemory, 0, pixels.size(), 0, &mapped);
    std::memcpy(mapped, pixels.data(), pixels.size());
    vkUnmapMemory(device, stagingMemory);

    VkCommandBuffer cmd = pool_->beginSingleTimeCommands();
    transitionToTransferDst(cmd, texImage_);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {2, 2, 1};

    vkCmdCopyBufferToImage(cmd,
                           stagingBuffer,
                           texImage_,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);

    transitionToShaderRead(cmd, texImage_);
    pool_->endSingleTimeCommands(cmd);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    texReady_ = true;
    return true;
}

glm::mat4 VkRendererSubRenderer::computeMVP() const {
    return proj_ * view_;
}

} // namespace VKRenderer
