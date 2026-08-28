#include "CGLib/Animation/AnimationRenderer/GpuSkinnedRenderer.h"

#include "CGLib/VulkanGraphics/VulkanContext.h"
#include "CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <algorithm>
#include <array>

namespace Phantom::Animation {

void GpuSkinnedRenderer::setMesh(SkinnedMesh mesh)
{
    mesh_      = std::move(mesh);
    meshDirty_ = true;
}

void GpuSkinnedRenderer::updatePose(const std::vector<glm::mat4>& skinMatrices,
                                     const glm::mat4& mvp)
{
    skinMatrices_ = skinMatrices;
    mvp_          = mvp;
}

void GpuSkinnedRenderer::onInit(::VKG::VulkanContext& ctx,
                                  const ::VKG::VulkanCommandPool& pool,
                                 VkRenderPass renderPass,
                                 uint32_t framesInFlight)
{
    ctx_            = &ctx;
    pool_           = &pool;
    device_         = ctx.getDevice();
    framesInFlight_ = framesInFlight;

    // Descriptor layout: binding 0 = CameraUBO, binding 1 = BoneUBO
    VkDescriptorSetLayoutBinding cameraBinding{};
    cameraBinding.binding         = 0;
    cameraBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBinding.descriptorCount = 1;
    cameraBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding boneBinding{};
    boneBinding.binding         = 1;
    boneBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    boneBinding.descriptorCount = 1;
    boneBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    descriptorSetLayout_.create(device_, { cameraBinding, boneBinding });

    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight * 2 },
    };
    descriptorPool_.create(device_, poolSizes, framesInFlight);

    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, descriptorSetLayout_.get());
    descriptorSets_ = descriptorPool_.allocateSets(device_, layouts);

    cameraUBOs_.resize(framesInFlight);
    boneUBOs_.resize(framesInFlight);
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        cameraUBOs_[i].createMapped(ctx, sizeof(CameraUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        boneUBOs_[i].createMapped(ctx, sizeof(BoneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        VkDescriptorBufferInfo cameraInfo{ cameraUBOs_[i].getBuffer(), 0, sizeof(CameraUBO) };
        VkDescriptorBufferInfo boneInfo  { boneUBOs_[i].getBuffer(),   0, sizeof(BoneUBO) };

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = descriptorSets_[i];
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo     = &cameraInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = descriptorSets_[i];
        writes[1].dstBinding      = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].pBufferInfo     = &boneInfo;

        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    // Pipeline: single interleaved vertex binding matching SkinVertex layout
    // location 0: position   vec3   offset 0
    // location 1: normal     vec3   offset 12
    // location 2: texCoord   vec2   offset 24
    // location 3: color      vec4   offset 32
    // location 4: boneIds    ivec4  offset 48
    // location 5: boneWeights vec4  offset 64
    std::vector<VkVertexInputBindingDescription> bindings = {
        { 0, sizeof(SkinVertex), VK_VERTEX_INPUT_RATE_VERTEX },
    };
    std::vector<VkVertexInputAttributeDescription> attrs = {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(SkinVertex, position) },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(SkinVertex, normal) },
        { 2, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(SkinVertex, texCoord) },
        { 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SkinVertex, color) },
        { 4, 0, VK_FORMAT_R32G32B32A32_SINT,   offsetof(SkinVertex, boneIndices) },
        { 5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SkinVertex, boneWeights) },
    };

    ::VKG::PipelineConfig pCfg{};
    pCfg.vertSpv             = shaders_.vertSpv;
    pCfg.fragSpv             = shaders_.fragSpv;
    pCfg.bindingDescs        = bindings;
    pCfg.attrDescs           = attrs;
    pCfg.topology            = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pCfg.descriptorSetLayout = descriptorSetLayout_.get();
    pCfg.cullMode            = VK_CULL_MODE_NONE;
    pCfg.depthTest           = true;
    pCfg.depthWrite          = true;

    pipeline_.create(ctx, renderPass, pCfg);

    if (!mesh_.vertices.empty())
        uploadMesh();
}

void GpuSkinnedRenderer::uploadMesh()
{
    if (mesh_.vertices.empty() || !ctx_ || !pool_) return;

    vertexBuffer_.destroy(device_);
    indexBuffer_.destroy(device_);

    vertexBuffer_.create(*ctx_, *pool_,
        mesh_.vertices.size() * sizeof(SkinVertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        mesh_.vertices.data());

    if (!mesh_.indices.empty()) {
        indexBuffer_.create(*ctx_, *pool_,
            mesh_.indices.size() * sizeof(uint32_t),
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            mesh_.indices.data());
    }

    indexCount_ = static_cast<uint32_t>(mesh_.indices.size());
    meshDirty_  = false;
}

void GpuSkinnedRenderer::onUpdate(uint32_t frameIndex)
{
    if (meshDirty_) {
        vkDeviceWaitIdle(device_);
        uploadMesh();
    }

    if (frameIndex >= static_cast<uint32_t>(cameraUBOs_.size())) return;

    CameraUBO camData{ mvp_ };
    cameraUBOs_[frameIndex].write(&camData, sizeof(camData));

    BoneUBO boneData{};
    const int n = std::min(static_cast<int>(skinMatrices_.size()), static_cast<int>(kMaxBones));
    for (int i = 0; i < n; ++i)
        boneData.bones[i] = skinMatrices_[i];
    for (int i = n; i < static_cast<int>(kMaxBones); ++i)
        boneData.bones[i] = glm::mat4{1.f};
    boneUBOs_[frameIndex].write(&boneData, sizeof(boneData));
}

void GpuSkinnedRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!visible_ || indexCount_ == 0) return;
    if (!vertexBuffer_.isValid() || !indexBuffer_.isValid()) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer     vbuf   = vertexBuffer_.getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

    vkCmdBindIndexBuffer(cmd, indexBuffer_.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1,
                            &descriptorSets_[frameIndex], 0, nullptr);

    vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
}

void GpuSkinnedRenderer::onCleanup(VkDevice device)
{
    for (auto& b : cameraUBOs_) b.destroy(device);
    for (auto& b : boneUBOs_)   b.destroy(device);
    cameraUBOs_.clear();
    boneUBOs_.clear();

    vertexBuffer_.destroy(device);
    indexBuffer_.destroy(device);

    pipeline_.destroy(device);
    descriptorPool_.destroy(device);
    descriptorSetLayout_.destroy(device);

    indexCount_ = 0;
}

} // namespace Phantom::Animation
