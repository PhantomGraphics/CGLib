#include "CGLib/Animation/AnimationRenderer/MmdMeshRenderer.h"

#include "CGLib/VulkanGraphics/VulkanContext.h"
#include "CGLib/VulkanGraphics/VulkanCommandPool.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>

namespace Phantom::Animation {

void MmdMeshRenderer::setMesh(SkinnedMesh mesh)
{
    mesh_      = std::move(mesh);
    meshDirty_ = true;
}

void MmdMeshRenderer::setSubMeshes(std::vector<MmdSubMesh> subMeshes,
                                     const std::vector<std::string>& texturePaths,
                                     const std::string& modelDir)
{
    subMeshes_    = std::move(subMeshes);
    texturePaths_ = texturePaths;
    modelDir_     = modelDir;
    subMeshDirty_ = true;
}

void MmdMeshRenderer::updatePose(const std::vector<glm::mat4>& skinMatrices,
                                   const glm::mat4& mvp,
                                   const glm::mat4& view,
                                   const glm::mat4& proj,
                                   const glm::vec3& eye)
{
    skinMatrices_ = skinMatrices;
    mvp_  = mvp;
    view_ = view;
    proj_ = proj;
    eye_  = eye;
}

// ============================================================
//  IVkSubRenderer
// ============================================================

void MmdMeshRenderer::onInit(::VKG::VulkanContext& ctx,
                               const ::VKG::VulkanCommandPool& pool,
                               VkRenderPass renderPass,
                               uint32_t framesInFlight)
{
    ctx_            = &ctx;
    pool_           = &pool;
    device_         = ctx.getDevice();
    renderPass_     = renderPass;
    framesInFlight_ = framesInFlight;

    // Descriptor set layout:
    // binding 0: Camera UBO  (vert)
    // binding 1: Bone UBO    (vert)
    // binding 2: Material UBO (frag)
    // binding 3: Diffuse texture (frag)
    VkDescriptorSetLayoutBinding b0{};
    b0.binding         = 0;
    b0.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b0.descriptorCount = 1;
    b0.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding b1{};
    b1.binding         = 1;
    b1.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b1.descriptorCount = 1;
    b1.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding b2{};
    b2.binding         = 2;
    b2.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    b2.descriptorCount = 1;
    b2.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding b3{};
    b3.binding         = 3;
    b3.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b3.descriptorCount = 1;
    b3.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    descriptorSetLayout_.create(device_, {b0, b1, b2, b3});

    // Shared per-frame camera and bone UBOs
    cameraUBOs_.resize(framesInFlight);
    boneUBOs_.resize(framesInFlight);
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        cameraUBOs_[i].createMapped(ctx, sizeof(CameraUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        boneUBOs_[i].createMapped(ctx, sizeof(BoneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    }

    // Vertex input: same layout as SkinVertex
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

    if (!mesh_.vertices.empty()) uploadMesh();
    if (subMeshDirty_)           rebuildDescriptors();
}

void MmdMeshRenderer::onUpdate(uint32_t frameIndex)
{
    if (subMeshDirty_) {
        vkDeviceWaitIdle(device_);
        uploadMesh();
        rebuildDescriptors();
        subMeshDirty_ = false;
        meshDirty_    = false;
    } else if (meshDirty_) {
        vkDeviceWaitIdle(device_);
        uploadMesh();
        meshDirty_ = false;
    }

    if (frameIndex >= framesInFlight_) return;

    // Update camera UBO
    CameraUBO camData{};
    camData.mvp  = mvp_;
    camData.view = view_;
    camData.proj = proj_;
    camData.eye  = glm::vec4(eye_, 0.f);
    cameraUBOs_[frameIndex].write(&camData, sizeof(camData));

    // Update bone UBO
    BoneUBO boneData{};
    const int nBones = std::min(static_cast<int>(skinMatrices_.size()),
                                static_cast<int>(kMaxBones));
    for (int i = 0; i < nBones; ++i)
        boneData.bones[i] = skinMatrices_[i];
    for (int i = nBones; i < static_cast<int>(kMaxBones); ++i)
        boneData.bones[i] = glm::mat4{1.f};
    boneUBOs_[frameIndex].write(&boneData, sizeof(boneData));
}

void MmdMeshRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex)
{
    if (!visible_ || totalIndexCount_ == 0 || subMeshes_.empty()) return;
    if (!vertexBuffer_.isValid() || !indexBuffer_.isValid()) return;
    if (perSubMeshSets_.empty()) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.getPipeline());

    VkBuffer     vbuf   = vertexBuffer_.getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);
    vkCmdBindIndexBuffer(cmd, indexBuffer_.getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    const uint32_t N = static_cast<uint32_t>(subMeshes_.size());
    for (uint32_t j = 0; j < N; ++j) {
        if (j >= static_cast<uint32_t>(perSubMeshSets_.size())) break;
        if (subMeshes_[j].indexCount == 0) continue;

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_.getLayout(), 0, 1,
                                &perSubMeshSets_[j][frameIndex], 0, nullptr);

        vkCmdDrawIndexed(cmd,
            subMeshes_[j].indexCount,
            1,
            subMeshes_[j].indexOffset,
            0, 0);
    }
}

void MmdMeshRenderer::onCleanup(VkDevice device)
{
    destroyDescriptors();

    for (auto& b : cameraUBOs_) b.destroy();
    cameraUBOs_.clear();
    for (auto& b : boneUBOs_)   b.destroy();
    boneUBOs_.clear();

    vertexBuffer_.destroy();
    indexBuffer_.destroy();
    totalIndexCount_ = 0;

    pipeline_.destroy(device);
    descriptorSetLayout_.destroy(device);
}

// ============================================================
//  Private helpers
// ============================================================

void MmdMeshRenderer::uploadMesh()
{
    if (mesh_.vertices.empty() || !ctx_ || !pool_) return;

    vertexBuffer_.destroy();
    indexBuffer_.destroy();

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
    totalIndexCount_ = static_cast<uint32_t>(mesh_.indices.size());
}

void MmdMeshRenderer::destroyDescriptors()
{
    perSubMeshSets_.clear();
    descriptorPool_.destroy(device_);

    for (auto& frameUBOs : materialUBOs_)
        for (auto& ubo : frameUBOs)
            ubo.destroy();
    materialUBOs_.clear();

    texHelper_.destroyAll(device_);
    subMeshTextures_.clear();
}

void MmdMeshRenderer::rebuildDescriptors()
{
    destroyDescriptors();

    if (subMeshes_.empty()) return;
    if (!ctx_ || !pool_)    return;

    const uint32_t N = static_cast<uint32_t>(subMeshes_.size());
    const uint32_t F = framesInFlight_;

    // Descriptor pool: N×F sets, each with 3 UBOs + 1 combined image sampler
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          N * F * 3 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  N * F     },
    };
    descriptorPool_.create(device_, poolSizes, N * F);

    // Material UBOs: per-submesh × per-frame
    materialUBOs_.resize(N);
    for (uint32_t j = 0; j < N; ++j) {
        materialUBOs_[j].resize(F);
        for (uint32_t i = 0; i < F; ++i) {
            materialUBOs_[j][i].createMapped(*ctx_, sizeof(MaterialUBO),
                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            MaterialUBO uboData{};
            uboData.diffuse    = subMeshes_[j].props.diffuse;
            uboData.specular   = glm::vec4{subMeshes_[j].props.specular,
                                           subMeshes_[j].props.specularity};
            uboData.ambient    = glm::vec4{subMeshes_[j].props.ambient, 0.f};
            uboData.edgeColor  = subMeshes_[j].props.edgeColor;
            uboData.edgeSize   = subMeshes_[j].props.edgeSize;
            uboData.sphereMode = subMeshes_[j].props.sphereMode;
            materialUBOs_[j][i].write(&uboData, sizeof(uboData));
        }
    }

    // Load textures
    texHelper_.createFallback(*ctx_, *pool_);
    subMeshTextures_.resize(N);
    for (uint32_t j = 0; j < N; ++j) {
        const int ti = subMeshes_[j].textureIdx;
        if (ti >= 0 && ti < static_cast<int>(texturePaths_.size())) {
            // Resolve relative path against modelDir_
            std::filesystem::path absPath =
                std::filesystem::path(modelDir_) / texturePaths_[ti];
            subMeshTextures_[j] = &texHelper_.load(*ctx_, *pool_, absPath.string());
        } else {
            subMeshTextures_[j] = &texHelper_.fallback();
        }
    }

    // Allocate descriptor sets: N×F sets all with the same layout
    std::vector<VkDescriptorSetLayout> layouts(N * F, descriptorSetLayout_.get());
    std::vector<VkDescriptorSet> flatSets = descriptorPool_.allocateSets(device_, layouts);

    perSubMeshSets_.resize(N, std::vector<VkDescriptorSet>(F));
    for (uint32_t j = 0; j < N; ++j)
        for (uint32_t i = 0; i < F; ++i)
            perSubMeshSets_[j][i] = flatSets[j * F + i];

    // Write descriptor sets
    for (uint32_t j = 0; j < N; ++j) {
        for (uint32_t i = 0; i < F; ++i) {
            VkDescriptorSet ds = perSubMeshSets_[j][i];

            VkDescriptorBufferInfo camInfo{
                cameraUBOs_[i].getBuffer(), 0, sizeof(CameraUBO) };
            VkDescriptorBufferInfo boneInfo{
                boneUBOs_[i].getBuffer(),   0, sizeof(BoneUBO) };
            VkDescriptorBufferInfo matInfo{
                materialUBOs_[j][i].getBuffer(), 0, sizeof(MaterialUBO) };

            const GpuTexture& tex = *subMeshTextures_[j];
            VkDescriptorImageInfo imgInfo{};
            imgInfo.sampler     = tex.sampler;
            imgInfo.imageView   = tex.view;
            imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            std::array<VkWriteDescriptorSet, 4> writes{};
            writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet = ds; writes[0].dstBinding = 0;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].descriptorCount = 1;
            writes[0].pBufferInfo     = &camInfo;

            writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet = ds; writes[1].dstBinding = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[1].descriptorCount = 1;
            writes[1].pBufferInfo     = &boneInfo;

            writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet = ds; writes[2].dstBinding = 2;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[2].descriptorCount = 1;
            writes[2].pBufferInfo     = &matInfo;

            writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet = ds; writes[3].dstBinding = 3;
            writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[3].descriptorCount = 1;
            writes[3].pImageInfo      = &imgInfo;

            vkUpdateDescriptorSets(device_, 4, writes.data(), 0, nullptr);
        }
    }
}

} // namespace Phantom::Animation
