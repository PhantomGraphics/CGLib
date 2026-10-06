#include "GltfSceneRenderer.h"
#include "GltfMaterial.h"
#include "../Gltf/GltfAnimationEvaluator.h"
#include "GltfPrimitiveData.h"
#include "../Gltf/GltfNodeTransform.h"

#include "../../../CGLib/VulkanGraphics/VulkanContext.h"
#include "../../../CGLib/VulkanGraphics/VulkanCommandPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanImage.h"
#include "../../../CGLib/VulkanGraphics/VulkanDebugUtils.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vk_mem_alloc.h>
#include <fstream>
#include <limits>

using namespace Phantom::Gltf;

// ============================================================
//  Camera helpers
// ============================================================

glm::vec3 GltfSceneRenderer::cameraPosition() const {
    float x = camDist_ * std::sin(camTheta_) * std::cos(camPhi_);
    float y = camDist_ * std::cos(camTheta_);
    float z = camDist_ * std::sin(camTheta_) * std::sin(camPhi_);
    return camTarget_ + glm::vec3(x, y, z);
}

RtCameraParams GltfSceneRenderer::getCameraParams() const {
    return { cameraPosition(), camTarget_, {0.f, 1.f, 0.f}, fovDeg_ };
}

void GltfSceneRenderer::handleMouseButton(bool pressed) {
    isDragging_ = pressed;
}

void GltfSceneRenderer::handleMouseMove(double x, double y) {
    if (isDragging_) {
        float dx = static_cast<float>(x - lastX_) * 0.005f;
        float dy = static_cast<float>(y - lastY_) * 0.005f;
        camPhi_   -= dx;
        camTheta_  = std::max(0.05f, std::min(3.09f, camTheta_ + dy));
    }
    lastX_ = x;
    lastY_ = y;
}

void GltfSceneRenderer::handleScroll(double dy) {
    camDist_ = std::max(0.1f, camDist_ - static_cast<float>(dy) * camDist_ * 0.1f);
}

// ============================================================
//  Fallback cube (1x1 white) for IBL bindings when useIBL=0
// ============================================================

void GltfSceneRenderer::createFallbackCube(const Phantom::VKG::VulkanContext& ctx,
                                            const Phantom::VKG::VulkanCommandPool& pool)
{
    // Six 1x1 white faces. On failure VulkanImage releases everything and leaves the handles null,
    // so destroyFallbackCube() stays a no-op and the descriptor writer sees a null view.
    uint8_t faces[6 * 4];
    std::memset(faces, 255, sizeof(faces));
    if (!Phantom::VKG::VulkanImage::createCubeFromFacesRGBA8(ctx, pool, faces, 1,
            fallbackCubeImage_, fallbackCubeMem_, fallbackCubeView_))
        std::fprintf(stderr, "[GltfSceneRenderer] fallback IBL cube creation failed\n");
}

void GltfSceneRenderer::destroyFallbackCube(VkDevice device) {
    if (fallbackCubeView_)  { vkDestroyImageView(device, fallbackCubeView_, nullptr);  fallbackCubeView_  = VK_NULL_HANDLE; }
    if (fallbackCubeImage_) { vkDestroyImage(device, fallbackCubeImage_, nullptr);     fallbackCubeImage_ = VK_NULL_HANDLE; }
    if (fallbackCubeMem_)   { vkFreeMemory(device, fallbackCubeMem_, nullptr);         fallbackCubeMem_   = VK_NULL_HANDLE; }
}

// ============================================================
//  Global descriptor sets: layouts/pools live in GltfGlobalDescriptors; this resolves which
//  image goes into each slot (real IBL / shadow vs fallback) for every frame in flight.
// ============================================================

void GltfSceneRenderer::updateGlobalDescriptorSets(VkDevice device) {
    if (descriptors_.frameCount() == 0) return; // descriptor setup failed (logged in onInit)
    // Resolve which cube/2D view to bind for IBL slots. Real IBL (iblResult_, see recomputeIBL())
    // wins when available; otherwise fall back to sampling the raw environment cubemap directly
    // for both irradiance and prefiltered (a flat approximation -- no convolution/prefiltering)
    // and a white 2D fallback for the BRDF LUT, exactly as before real IBL existed.
    VkImageView cubeView = (envView_ != VK_NULL_HANDLE) ? envView_ : fallbackCubeView_;
    VkSampler   cubeSamp = (envSampler_ != VK_NULL_HANDLE) ? envSampler_ : fallbackSampler_.get();
    const bool  hasIBL   = iblResult_.isValid();
    VkImageView irrView  = hasIBL ? iblResult_.irradianceView    : cubeView;
    VkSampler   irrSamp  = hasIBL ? iblResult_.irradianceSampler : cubeSamp;
    VkImageView preView  = hasIBL ? iblResult_.prefilterView     : cubeView;
    VkSampler   preSamp  = hasIBL ? iblResult_.prefilterSampler  : cubeSamp;
    VkImageView lutView  = hasIBL ? iblResult_.brdfLUTView       : fallbackView_;
    VkSampler   lutSamp  = hasIBL ? iblResult_.brdfLUTSampler    : fallbackSampler_.get();

    // binding 4: shadowMap (fallback white 2D -- samples as depth=1.0 "far", i.e. never occluded).
    // The real shadow depth view sits in DEPTH_STENCIL_READ_ONLY_OPTIMAL (see VulkanOffscreen's
    // depth attachment finalLayout); the color fallback sits in the usual SHADER_READ_ONLY_OPTIMAL
    // -- the two views need different declared layouts.
    const bool hasRealShadow = (shadowView_ != VK_NULL_HANDLE);

    // binding 7: volume shadow (opacity shadow map array, or the zero-density fallback)
    const bool hasVolumeShadow = volumeShadowView_ != VK_NULL_HANDLE;

    for (int f = 0; f < MAX_FRAMES; ++f) {
        GltfGlobalDescriptors::FrameInputs in;
        in.globalUbo   = { globalUbos_[f].get(), sizeof(GlobalUBO) };
        in.boneUbo     = { boneUbos_[f].get(),   sizeof(BoneUBO) };
        in.lightUbo    = { lightUbos_[f].get(),  sizeof(LightManager::LightBufferGpu) };
        in.irradiance  = { irrSamp, irrView,   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        in.prefiltered = { preSamp, preView,   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        in.brdfLut     = { lutSamp, lutView,   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        in.shadow      = { hasRealShadow ? shadowSampler_ : fallbackSampler_.get(),
                           hasRealShadow ? shadowView_    : fallbackView_,
                           hasRealShadow ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                         : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        in.volumeShadow = { hasVolumeShadow ? volumeShadowSampler_ : fallbackSampler_.get(),
                            hasVolumeShadow ? volumeShadowView_    : zeroArrayView_,
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        descriptors_.updateFrame(device, static_cast<uint32_t>(f), in);
    }
}

// ============================================================
//  Node traversal
// ============================================================

void GltfSceneRenderer::traverseNode(const GltfDocument& doc, int nodeIndex,
                                      const glm::mat4& parentTransform,
                                      const Phantom::VKG::VulkanContext& ctx,
                                      const Phantom::VKG::VulkanCommandPool& pool)
{
    const auto& node  = doc.nodes[nodeIndex];
    glm::mat4   world = parentTransform * nodeLocalMatrix(node);

    if (node.meshIndex >= 0 && node.meshIndex < (int)doc.meshes.size()) {
        const auto& mesh = doc.meshes[node.meshIndex];
        // Per glTF spec, a skinned mesh's world position comes entirely from its joint matrices
        // (supplied per-frame via updateSkinMatrices()) -- the mesh-holding node's own transform
        // is ignored, not composed with it. Baking `world` here as well would double-apply it.
        const bool skinned = (node.skin >= 0);
        const glm::mat4& bakeTransform = skinned ? glm::mat4(1.f) : world;

        // Object animation (this document has clips and the mesh isn't skinned): keep a CPU
        // mirror plus the accessor-space positions/normals so onUpdate() can re-bake the whole
        // primitive with its node's animated world matrix each frame.
        const bool objectAnimatable = !skinned && !doc.animations.empty();

        for (int primIdx = 0; primIdx < static_cast<int>(mesh.primitives.size()); ++primIdx) {
            const auto& prim = mesh.primitives[primIdx];
            if (prim.positionAccessor < 0) continue;
            auto entry = std::make_unique<PrimitiveEntry>();
            entry->materialIndex = prim.materialIndex;
            entry->meshIndex     = node.meshIndex;
            entry->primIndex     = primIdx;
            entry->nodeIndex     = nodeIndex;
            entry->restWorld     = bakeTransform;
            entry->mesh.setKeepCpuVertices(dynamic_ || objectAnimatable);

            // AABB center in accessor space -- used only for alpha-BLEND back-to-front sort
            // ordering (onRender()), cheap enough to compute for every primitive unconditionally
            // rather than only when this document turns out to have a BLEND material.
            entry->localCenter = accessorAabbCenter(doc, prim.positionAccessor);

            if (objectAnimatable)
                readPrimitiveGeometry(doc, prim, entry->localPos, entry->localNrm);

            if (entry->mesh.build(ctx, pool, doc, prim, bakeTransform))
                primitives_.push_back(std::move(entry));
        }
    }

    for (int child : node.children)
        traverseNode(doc, child, world, ctx, pool);
}

// ============================================================
//  Object animation (node TRS)  — CPU re-bake per frame
// ============================================================

void GltfSceneRenderer::setAnimationClip(int clipIndex)
{
    objAnim_.setClip(clipIndex);
}

void GltfSceneRenderer::setAnimationTime(float seconds)
{
    objAnim_.setTime(seconds);
}

int GltfSceneRenderer::animationCount() const
{
    return objAnim_.clipCount();
}

float GltfSceneRenderer::animationDuration(int clipIndex) const
{
    return objAnim_.duration(clipIndex);
}

void GltfSceneRenderer::applyObjectAnimation()
{
    if (!ready_ || !ctx_ || !pool_ || !doc_ || !objAnim_.consumeDirty()) return;

    // Clip disabled: restore every animated primitive to its rest-pose bake once.
    if (objAnim_.clip() < 0) {
        for (auto& e : primitives_) {
            if (e->nodeIndex < 0 || e->localPos.empty()) continue;
            e->mesh.setBakeTransform(e->restWorld);
            e->mesh.updatePositionsAndNormals(*ctx_, *pool_, e->localPos, e->localNrm);
        }
        return;
    }

    const std::vector<glm::mat4> globals = objAnim_.evaluateGlobals();

    for (auto& e : primitives_) {
        if (e->nodeIndex < 0 || e->localPos.empty()) continue;
        if (e->nodeIndex >= (int)globals.size()) continue;
        if (!objAnim_.isNodeAnimated(e->nodeIndex)) continue; // static node -- leave its build-time bake
        e->mesh.setBakeTransform(globals[e->nodeIndex]);
        e->mesh.updatePositionsAndNormals(*ctx_, *pool_, e->localPos, e->localNrm);
    }
}

// ============================================================
//  IVkSubRenderer::onInit
// ============================================================

void GltfSceneRenderer::onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
                                VkRenderPass renderPass, uint32_t /*framesInFlight*/)
{
    ctx_  = &ctx;
    pool_ = &pool;
    renderPass_ = renderPass; // cached for setMaterialShaderOverride(), called after onInit()
    VkDevice device = ctx.getDevice();
    materialPipelineCache_.create(*ctx_, materialPipelineCacheDir_); // non-fatal on failure. Phase 4C item 5 ("pipeline cache") -- see setMaterialShaderCacheDir()

    // Global UBOs (document-independent)
    for (int f = 0; f < MAX_FRAMES; ++f) {
        globalUbos_[f].createMapped(ctx, sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        boneUbos_[f].createMapped(ctx, sizeof(BoneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        lightUbos_[f].createMapped(ctx, sizeof(LightManager::LightBufferGpu), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        lightManager_.uploadUBO(lightUbos_[f]); // seed with the empty buffer (count=0) before the first onUpdate()
    }


    // Shared fallback resources
    GltfGpuMaterial::createFallback(ctx, pool, fallbackImage_, fallbackMemory_, fallbackView_);
    fallbackSampler_.create(device);
    createFallbackCube(ctx, pool);
    Phantom::VKG::VulkanImage::createZeroArrayTexture(ctx, pool, VK_FORMAT_R32_SFLOAT,
                                                      zeroArrayImage_, zeroArrayMemory_, zeroArrayView_);

    // Descriptor layouts + global pool + sets (the fallback IBL textures are written just below)
    if (!descriptors_.create(device, static_cast<uint32_t>(MAX_FRAMES)))
        fprintf(stderr, "[GltfSceneRenderer] global descriptor setup failed; rendering may be incomplete\n");

    // Real IBL, if setEnvironment() was already called (the common pattern -- see its comment):
    // ctx_/pool_ weren't valid yet at that point, so the actual precompute was deferred to here.
    recomputeIBL();
    updateGlobalDescriptorSets(device);

    // Graphics pipeline with 2 descriptor set layouts. Config is reused (not moved-from) below
    // to also build the other 3 variants -- VulkanPipeline::create() takes it by const&.
    Phantom::VKG::PipelineConfig cfg;
    cfg.vertSpv = shaders_.vertSpv;
    cfg.fragSpv = shaders_.fragSpv;
    {
        auto bd = GltfGpuMesh::Vertex::getBindingDescription();
        cfg.bindingDescs = {bd};
        cfg.attrDescs    = GltfGpuMesh::Vertex::getAttributeDescriptions();
    }
    cfg.descriptorSetLayouts = { descriptors_.globalLayout(), descriptors_.materialLayout() };
    // Phase 2 item 5 後半 ("共有GPU asset化"): a vertex-stage push constant carrying the
    // per-draw model matrix, on every main-pass pipeline variant below AND setMaterialShaderOverride()'s
    // pipelines (identical range everywhere -- Vulkan keeps pushed values live across a
    // vkCmdBindPipeline switch only when the new pipeline's layout declares a *compatible* range at
    // the same offset, see onRender()'s single vkCmdPushConstants call before its pipeline-switching
    // draw loop). Declaring this range is harmless for a caller whose own gltf.vert copy still reads
    // GlobalUBO::model instead (Vulkan does not require a shader to consume every declared push
    // constant range) -- see renderInstances()'s header comment for which callers actually need it.
    cfg.pushConstantRanges  = { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) } };
    cfg.blendEnable          = false;
    cfg.depthWrite           = true;
    cfg.cullMode             = cullMode_;
    pipeline_.create(ctx, renderPass, cfg);

    cfg.cullMode = VK_CULL_MODE_NONE;
    pipelineDoubleSided_.create(ctx, renderPass, cfg);

    // Alpha BLEND variants: standard src-alpha/one-minus-src-alpha blend, depth test on but depth
    // write off (so overlapping BLEND surfaces don't occlude each other by depth alone -- draw
    // order does that instead, see onRender()'s back-to-front sort).
    cfg.blendEnable = true;
    cfg.depthWrite  = false;
    cfg.cullMode    = cullMode_;
    pipelineBlend_.create(ctx, renderPass, cfg);

    cfg.cullMode = VK_CULL_MODE_NONE;
    pipelineBlendDoubleSided_.create(ctx, renderPass, cfg);

    // Skybox (see setUseSkybox()): only if the caller populated both shaders.
    if (!shaders_.skyboxVertSpv.empty() && !shaders_.skyboxFragSpv.empty()) {
        Phantom::VKG::VkSkyBoxRenderer::Config skyCfg;
        skyCfg.vertSpv = shaders_.skyboxVertSpv;
        skyCfg.fragSpv = shaders_.skyboxFragSpv;
        skybox_.emplace(std::move(skyCfg));
        skybox_->create(ctx, pool, renderPass, MAX_FRAMES);
        // setEnvironment() may have already run before onInit() (the common call order -- see
        // its own comment); bind whatever cubemap it stored so the skybox isn't blank until the
        // next setEnvironment() call.
        if (envView_ != VK_NULL_HANDLE)
            skybox_->setCubeMap(device, envView_, envSampler_);
    }

    // Document-specific resources: only if a document was already set.
    if (doc_) buildDocumentResources();
}

// ============================================================
//  Shadow mapping (Phase C)
// ============================================================

void GltfSceneRenderer::createShadowPipeline(VkRenderPass shadowRenderPass)
{
    if (!ctx_ || shaders_.shadowVertSpv.empty() || shaders_.shadowFragSpv.empty())
        return;

    // A second call (e.g. Phase 2 item 5 後半's shared renderer: Universe::GltfRenderer::
    // enableShadowCasting() calls this once per Instance sharing this same GltfSceneRenderer,
    // not once per underlying object) must not silently orphan the previous VkPipeline/
    // VkPipelineLayout -- shadowPipeline_.create() below would otherwise just overwrite the
    // handles, leaking both (caught via VK_LAYER_KHRONOS_validation's "leaked objects" report at
    // vkDestroyDevice()). Idempotent either way: destroy() on a never-created VulkanPipeline is a
    // no-op (its handles start VK_NULL_HANDLE).
    shadowPipeline_.destroy(ctx_->getDevice());

    Phantom::VKG::PipelineConfig cfg;
    cfg.vertSpv = shaders_.shadowVertSpv;
    cfg.fragSpv = shaders_.shadowFragSpv;

    auto bd = GltfGpuMesh::Vertex::getBindingDescription();
    cfg.bindingDescs = { bd };
    // Depth-only: only the position attribute (location 0) is consumed; the interleaved
    // normal/uv/tangent bytes in the same vertex buffer are simply not declared here.
    auto allAttrs = GltfGpuMesh::Vertex::getAttributeDescriptions();
    cfg.attrDescs = { allAttrs[0] };

    cfg.cullMode    = VK_CULL_MODE_NONE; // avoid peter-panning on thin/back-facing casters
    cfg.blendEnable = false;
    cfg.pushConstantRanges = { VkPushConstantRange{
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) * 2 } };

    shadowPipeline_.create(*ctx_, shadowRenderPass, cfg);
}

void GltfSceneRenderer::renderShadowCasters(VkCommandBuffer cmd, const glm::mat4& lightVP)
{
    if (!ready_ || shadowPipeline_.getPipeline() == VK_NULL_HANDLE || primitives_.empty())
        return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_.getPipeline());
    renderShadowCastersWithModel(cmd, lightVP, modelMatrix_);
}

void GltfSceneRenderer::renderShadowCasterInstances(VkCommandBuffer cmd, const glm::mat4& lightVP,
                                                      const std::vector<glm::mat4>& modelMatrices)
{
    if (!ready_ || shadowPipeline_.getPipeline() == VK_NULL_HANDLE || primitives_.empty() || modelMatrices.empty())
        return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline_.getPipeline());
    for (const glm::mat4& model : modelMatrices)
        renderShadowCastersWithModel(cmd, lightVP, model);
}

void GltfSceneRenderer::renderShadowCastersWithModel(VkCommandBuffer cmd, const glm::mat4& lightVP, const glm::mat4& model)
{
    const std::array<glm::mat4, 2> push{ lightVP, model };
    vkCmdPushConstants(cmd, shadowPipeline_.getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(push), push.data());

    for (auto& entry : primitives_) {
        if (nodeFilter_ >= 0 && entry->nodeIndex != nodeFilter_) continue;
        VkBuffer     vbuf   = entry->mesh.vertexBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

        if (entry->mesh.hasIndices()) {
            vkCmdBindIndexBuffer(cmd, entry->mesh.indexBuffer(), 0, entry->mesh.indexType());
            vkCmdDrawIndexed(cmd, entry->mesh.indexCount(), 1, 0, 0, 0);
        } else {
            vkCmdDraw(cmd, entry->mesh.vertexCount(), 1, 0, 0);
        }
    }
}

void GltfSceneRenderer::setShadowMap(VkImageView shadowView, VkSampler shadowSampler, const glm::mat4& lightVP)
{
    shadowView_    = shadowView;
    shadowSampler_ = shadowSampler;
    shadowVP_      = lightVP;
    shadowEnabled_ = 1;
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::setVolumeShadowMap(VkImageView arrayView, VkSampler sampler)
{
    volumeShadowView_    = arrayView;
    volumeShadowSampler_ = sampler;
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::clearVolumeShadowMap()
{
    volumeShadowView_    = VK_NULL_HANDLE;
    volumeShadowSampler_ = VK_NULL_HANDLE;
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::clearShadowMap()
{
    shadowView_    = VK_NULL_HANDLE;
    shadowSampler_ = VK_NULL_HANDLE;
    shadowEnabled_ = 0;
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
}

void GltfSceneRenderer::buildDocumentResources()
{
    VkDevice device = ctx_->getDevice();

    uint32_t matCount = doc_->materials.empty() ? 1u : static_cast<uint32_t>(doc_->materials.size());
    if (!descriptors_.createMaterialPool(device, matCount))
        fprintf(stderr, "[GltfSceneRenderer] material descriptor pool creation failed; materials may not render\n");

    if (doc_->materials.empty()) {
        GltfMaterial gltfMat;
        auto mat = std::make_unique<GltfGpuMaterial>();
        if (!mat->build(*ctx_, *pool_, *doc_, gltfMat,
                   descriptors_.materialLayout(), descriptors_.materialPool(),
                   fallbackView_, fallbackSampler_.get()))
            fprintf(stderr, "[GltfSceneRenderer] failed to build default material\n");
        materials_.push_back(std::move(mat));
    } else {
        for (const auto& gltfMat : doc_->materials) {
            auto mat = std::make_unique<GltfGpuMaterial>();
            if (!mat->build(*ctx_, *pool_, *doc_, gltfMat,
                       descriptors_.materialLayout(), descriptors_.materialPool(),
                       fallbackView_, fallbackSampler_.get()))
                fprintf(stderr, "[GltfSceneRenderer] failed to build material '%s'\n", gltfMat.name.c_str());
            materials_.push_back(std::move(mat));
        }
    }

    hasBlendMaterials_ = false;
    for (const auto& mat : materials_) {
        if (mat->isBlend()) { hasBlendMaterials_ = true; break; }
    }

    int sceneIdx = doc_->defaultScene;
    if (sceneIdx < 0 || sceneIdx >= (int)doc_->scenes.size()) sceneIdx = 0;
    if (!doc_->scenes.empty()) {
        for (int rootNode : doc_->scenes[sceneIdx].nodes)
            traverseNode(*doc_, rootNode, glm::mat4(1.f), *ctx_, *pool_);
    }

    ready_ = true;
}

void GltfSceneRenderer::clearDocumentResources()
{
    if (!ctx_) return;
    VkDevice device = ctx_->getDevice();

    for (auto& entry : primitives_) entry->mesh.destroy(device);
    primitives_.clear();

    for (auto& mat : materials_) mat->destroy(device);
    materials_.clear();

    // materialIndex keys no longer correspond to the same materials once materials_ is rebuilt
    // by the next buildDocumentResources() -- an override left in place could silently apply to
    // an unrelated material. Only the (non-owning) mapping is dropped -- the pooled VkPipeline
    // objects themselves survive for potential reuse by the next document (see
    // materialPipelineVariants_'s comment) and are only destroyed at onCleanup().
    materialPipelineOverrides_.clear();

    descriptors_.destroyMaterialPool(device);
    hasBlendMaterials_ = false;
    ready_ = false;
}

void GltfSceneRenderer::loadDocument(const GltfDocument& doc)
{
    if (ctx_) {
        vkDeviceWaitIdle(ctx_->getDevice());
        clearDocumentResources();
    }
    doc_ = &doc;
    // Node/clip indices from the previous document don't carry over.
    objAnim_.reset(doc_);
    if (ctx_) buildDocumentResources();
}

bool GltfSceneRenderer::updateMorphedPositions(int meshIndex, int primIndex, const std::vector<glm::vec3>& positions, int nodeIndex)
{
    if (!ready_ || !ctx_ || !pool_) return false;
    applyObjectAnimation(); // Update bake matrices before uploading local-space morph geometry.
    bool updated = false;
    for (auto& entry : primitives_) {
        if (entry->meshIndex != meshIndex || entry->primIndex != primIndex ||
            (nodeIndex >= 0 && entry->nodeIndex != nodeIndex)) continue;
        if (!entry->mesh.updatePositions(*ctx_, *pool_, positions)) return false;
        // Preserve deformation when a later object-animation update re-bakes this entry.
        if (!entry->localPos.empty()) entry->localPos = positions;
        updated = true;
    }
    return updated;
}

bool GltfSceneRenderer::updateMorphedGeometry(int meshIndex, int primIndex,
                                              const std::vector<glm::vec3>& positions,
                                              const std::vector<glm::vec3>& normals, int nodeIndex)
{
    if (!ready_ || !ctx_ || !pool_) return false;
    applyObjectAnimation();
    bool updated = false;
    for (auto& entry : primitives_) {
        if (entry->meshIndex != meshIndex || entry->primIndex != primIndex ||
            (nodeIndex >= 0 && entry->nodeIndex != nodeIndex)) continue;
        if (!entry->mesh.updatePositionsAndNormals(*ctx_, *pool_, positions, normals)) return false;
        if (!entry->localPos.empty()) {
            entry->localPos = positions;
            entry->localNrm = normals;
        }
        updated = true;
    }
    return updated;
}

void GltfSceneRenderer::setCamera(const glm::mat4& view, const glm::mat4& proj,
                                   const glm::vec3& eye)
{
    extView_ = view;
    extProj_ = proj;
    extEye_  = eye;
    useExternalCamera_ = true;
}

void GltfSceneRenderer::setEnvironment(VkImageView envView, VkSampler envSampler)
{
    envView_    = envView;
    envSampler_ = envSampler;
    recomputeIBL();
    if (ctx_) updateGlobalDescriptorSets(ctx_->getDevice());
    // Skybox shows the same environment IBL samples from -- keep both in sync on every change
    // (initial load, HDRI swap, and ClearEnvironmentHDR's revert to the placeholder alike).
    if (skybox_) skybox_->setCubeMap(ctx_->getDevice(), envView, envSampler);
}

void GltfSceneRenderer::recomputeIBL()
{
    if (!ctx_) return; // deferred: onInit() calls this again once ctx_/pool_ exist

    if (iblResult_.isValid())
        iblPrecomputer_.destroy(ctx_->getDevice(), iblResult_);

    if (envView_ == VK_NULL_HANDLE) return; // no environment set (yet) -- nothing to precompute
    if (auto result = iblPrecomputer_.compute(*ctx_, *pool_, envView_, envSampler_, shaders_.ibl))
        iblResult_ = *result;
    // else: Shaders::ibl was empty or a pass failed -- iblResult_ stays invalid,
    // updateGlobalDescriptorSets() falls back to sampling envView_ directly (old behavior).
}

void GltfSceneRenderer::setLight(const glm::vec4& pos, const glm::vec4& color)
{
    lightPos_   = pos;
    lightColor_ = color;
}

void GltfSceneRenderer::setPunctualLights(std::vector<LightEntry> lights)
{
    lightManager_ = LightManager{}; // clear (LightManager has no bulk-clear method of its own)
    for (const auto& l : lights) {
        if (lightManager_.addLight(l) < 0) {
            std::fprintf(stderr, "[GltfSceneRenderer] setPunctualLights: dropping light(s) beyond "
                                  "LightManager::kMaxLights=%d\n", LightManager::kMaxLights);
            break;
        }
    }
    // Written to lightUbos_ on the next onUpdate(); onInit() already seeded an empty buffer for
    // any frame that renders before that (e.g. the very first frame after a fresh onInit()).
}

// ============================================================
//  IVkSubRenderer::onUpdate  — compute MVP and write GlobalUBO
// ============================================================

void GltfSceneRenderer::onUpdate(uint32_t frameIndex) {
    if (!ready_) return;

    applyObjectAnimation(); // no-op unless a clip is set and the time/clip changed

    GlobalUBO cam{};
    cam.model          = modelMatrix_;
    cam.lightVP        = shadowVP_;
    cam.lightPos       = lightPos_;
    cam.lightColor     = lightColor_;
    cam.useIBL         = useIBL_;
    cam.shadowEnabled  = shadowEnabled_;
    cam.shadowBias     = shadowBias_;
    cam.shadowStrength = shadowStrength_;
    cam.exposure       = exposure_;
    cam.volumeShadowVP = volumeShadowVP_;
    cam.volumeShadowParams = volumeShadowView_ != VK_NULL_HANDLE ? volumeShadowParams_ : glm::vec4(0.f);

    if (useExternalCamera_) {
        cam.view   = extView_;
        cam.proj   = extProj_;
        cam.camPos = glm::vec4(extEye_, 1.f);
    } else {
        glm::vec3 eye = cameraPosition();
        float aspect = (extent_.height > 0)
            ? static_cast<float>(extent_.width) / static_cast<float>(extent_.height)
            : 1.f;
        cam.view   = glm::lookAt(eye, camTarget_, glm::vec3(0.f, 1.f, 0.f));
        cam.proj   = glm::perspective(glm::radians(fovDeg_), aspect, 0.001f, 1000.f);
        cam.proj[1][1] *= -1.f; // Vulkan Y flip
        cam.camPos = glm::vec4(eye, 1.f);
    }

    globalUbos_[frameIndex].write(&cam, sizeof(GlobalUBO));

    if (skybox_ && useSkybox_) {
        // Same view/proj as the main pass, but with translation stripped from the view matrix
        // (a skybox only ever rotates with the camera, never translates -- VkSkyBoxRenderer::
        // Buffer's own doc comment) -- same recipe as VkRendererView's SkyBox mode.
        Phantom::VKG::VkSkyBoxRenderer::Buffer skyBuf;
        skyBuf.projectionMatrix = cam.proj;
        glm::mat4 viewNoTranslation = cam.view;
        viewNoTranslation[3] = glm::vec4(0.f, 0.f, 0.f, cam.view[3].w);
        skyBuf.viewMatrix = viewNoTranslation;
        skyBuf.exposure = cam.exposure; // same linear-HDR scale as the shaded geometry
        skybox_->upload(skyBuf, frameIndex);
    }

    // BoneUBO: entries beyond skinMatrices_'s size (including the whole array, if
    // updateSkinMatrices() was never called) default to identity -- see BoneUBO's comment.
    BoneUBO bones;
    const size_t suppliedCount = std::min(skinMatrices_.size(), static_cast<size_t>(kMaxGltfBones));
    for (size_t i = 0; i < suppliedCount; ++i)
        bones.bones[i] = skinMatrices_[i];
    for (size_t i = suppliedCount; i < kMaxGltfBones; ++i)
        bones.bones[i] = glm::mat4(1.f);
    boneUbos_[frameIndex].write(&bones, sizeof(BoneUBO));

    lightManager_.uploadUBO(lightUbos_[frameIndex]);

    if (!scalarField_.empty()) {
        auto& buffer = scalarBuffers_[frameIndex];
        const VkDeviceSize bytes = scalarField_.size() * sizeof(glm::vec4);
        if (!buffer.isValid() || buffer.getSize() < bytes) {
            // VkAppBase has waited for this frame slot's fence before onUpdate.
            buffer.destroy(ctx_->getDevice());
            if (!buffer.createMapped(*ctx_, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)) {
                fprintf(stderr, "[GltfSceneRenderer] Scalar field allocation failed\n");
                ready_ = false;
                return;
            }
            descriptors_.updateScalarField(ctx_->getDevice(), frameIndex, {buffer.get(), bytes});
        }
        buffer.write(scalarField_.data(), bytes);
        vmaFlushAllocation(ctx_->getAllocator(), buffer.getVmaAllocation(), 0, bytes);
    }
}

// ============================================================
//  Per-material shader graph override (Phase 4C, ".phmat")
// ============================================================

bool GltfSceneRenderer::setMaterialShaderOverride(int materialIndex, const std::vector<uint32_t>& fragSpv, std::string* outError)
{
    if (!ctx_ || renderPass_ == VK_NULL_HANDLE) {
        if (outError) *outError = "setMaterialShaderOverride() called before onInit()";
        return false;
    }
    if (materialIndex < 0 || materialIndex >= static_cast<int>(materials_.size())) {
        if (outError) *outError = "material index out of range";
        return false;
    }
    if (fragSpv.empty()) {
        if (outError) *outError = "empty fragment SPIR-V";
        return false;
    }

    const GltfGpuMaterial& mat = *materials_[materialIndex];

    // Mirrors which of the 4 shared pipeline variants this material would otherwise have drawn
    // through -- a .phmat graph replaces the fragment math, not the alpha-mode/culling policy.
    GltfPipelineVariantPool::Key key;
    key.fragSpvHash = GltfPipelineVariantPool::hashSpirv(fragSpv);
    key.cullMode    = mat.doubleSided() ? VK_CULL_MODE_NONE : cullMode_;
    key.blendEnable = mat.isBlend();
    key.depthWrite  = !mat.isBlend();

    // Phase 4C item 5 ("shader variant"): identical compiled SPIR-V + fixed-function state
    // already has a pipeline -- reuse it instead of building a redundant one.
    Phantom::VKG::VulkanPipeline* variant = materialPipelineVariants_.find(key);
    if (!variant) {
        Phantom::VKG::PipelineConfig cfg;
        cfg.vertSpv = shaders_.vertSpv; // gltf.vert is unchanged -- only the fragment stage differs
        cfg.fragSpv = fragSpv;
        {
            auto bd = GltfGpuMesh::Vertex::getBindingDescription();
            cfg.bindingDescs = { bd };
            cfg.attrDescs    = GltfGpuMesh::Vertex::getAttributeDescriptions();
        }
        cfg.descriptorSetLayouts = { descriptors_.globalLayout(), descriptors_.materialLayout() };
        // Must match pipeline_/pipelineDoubleSided_/pipelineBlend_/pipelineBlendDoubleSided_'s
        // range exactly (see onInit()'s comment) -- onRender()/renderInstances() push the model
        // matrix once via pipeline_.getLayout() and rely on every pipeline they might bind
        // afterward (this override included) declaring a compatible range at the same offset.
        cfg.pushConstantRanges = { VkPushConstantRange{ VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) } };
        cfg.cullMode        = key.cullMode;
        cfg.blendEnable     = key.blendEnable;
        cfg.depthWrite      = key.depthWrite;
        cfg.pipelineCache   = materialPipelineCache_.get(); // Phase 4C item 5 ("pipeline cache")

        auto newPipeline = std::make_unique<Phantom::VKG::VulkanPipeline>();
        if (!newPipeline->create(*ctx_, renderPass_, cfg)) {
            if (outError) *outError = "pipeline creation failed (see stderr for the glslc/Vulkan validation log)";
            return false; // any previous pipeline for this material (shared, or an earlier override) is untouched
        }

        // Phase 4C item 5 ("GPU marker"): name the pipeline for RenderDoc/Nsight/PIX capture --
        // no-op if VK_EXT_debug_utils isn't enabled (see VulkanDebugUtils.h's comment).
        char hashHex[17];
        std::snprintf(hashHex, sizeof(hashHex), "%016llx", static_cast<unsigned long long>(key.fragSpvHash));
        Phantom::VKG::DebugUtils::setObjectName(ctx_->getInstance(), ctx_->getDevice(),
            VK_OBJECT_TYPE_PIPELINE, reinterpret_cast<uint64_t>(newPipeline->getPipeline()),
            (std::string("phmat:") + hashHex).c_str());

        variant = materialPipelineVariants_.add(key, std::move(newPipeline), ctx_->getDevice());
    }

    materialPipelineOverrides_[materialIndex] = variant; // non-owning; overwrites any previous entry
    return true;
}

void GltfSceneRenderer::clearMaterialShaderOverride(int materialIndex)
{
    // Only drops materialIndex's mapping -- the pooled VkPipeline itself may still be referenced
    // by another material/document (see materialPipelineVariants_'s comment) and is never
    // destroyed here.
    materialPipelineOverrides_.erase(materialIndex);
}

bool GltfSceneRenderer::hasMaterialShaderOverride(int materialIndex) const
{
    return materialPipelineOverrides_.count(materialIndex) != 0;
}

// ============================================================
//  IVkSubRenderer::onRender
// ============================================================

void GltfSceneRenderer::onRender(VkCommandBuffer cmd, uint32_t frameIndex) {
    if (!ready_ || !visible_) return;
    if (primitives_.empty()) {
        // No glTF geometry loaded (or none visible) -- still draw the skybox alone if enabled,
        // rather than bailing out like every primitive-drawing path below does.
        if (useSkybox_ && skybox_ && skybox_->isValid()) skybox_->render(cmd, frameIndex);
        return;
    }

    // Bind global descriptor set (set=0) once for all primitives. All 4 pipeline variants (and
    // any .phmat override pipeline) share the same descriptor set layouts + push constant range
    // (see onInit()), so pipeline_'s layout works here regardless of which pipeline ends up bound
    // first in renderPrimitivesWithModel()'s draw loop.
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1, descriptors_.globalSet(frameIndex), 0, nullptr);
    renderPrimitivesWithModel(cmd, frameIndex, modelMatrix_);

    // Skybox last: VkSkyBoxRenderer's pipeline writes depth=1.0 (max) with depthWrite off and a
    // LEQUAL compare, so drawing it after every opaque/blend primitive lets the depth test reject
    // it wherever real geometry already covered a pixel -- standard "skybox last" optimization,
    // not required for correctness (either order composites the same way).
    if (useSkybox_ && skybox_ && skybox_->isValid()) skybox_->render(cmd, frameIndex);
}

void GltfSceneRenderer::renderInstances(VkCommandBuffer cmd, uint32_t frameIndex,
                                         const std::vector<glm::mat4>& modelMatrices) {
    if (!ready_ || !visible_ || primitives_.empty() || modelMatrices.empty()) return;

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_.getLayout(), 0, 1, descriptors_.globalSet(frameIndex), 0, nullptr);
    for (const glm::mat4& model : modelMatrices)
        renderPrimitivesWithModel(cmd, frameIndex, model);

    if (useSkybox_ && skybox_ && skybox_->isValid()) skybox_->render(cmd, frameIndex); // once, not per instance
}

void GltfSceneRenderer::renderPrimitivesWithModel(VkCommandBuffer cmd, uint32_t frameIndex, const glm::mat4& model) {
    // Recorded into the command buffer verbatim (unlike GlobalUBO::model, a single host-visible
    // value every draw recorded against this frame's descriptor set reads at *execution* time --
    // see setModelMatrix()'s comment) -- vkCmdPushConstants is what lets renderInstances() draw
    // the same GPU mesh/pipeline resources more than once per frame, each with its own model,
    // without one instance's transform silently winning over another's the way writing
    // GlobalUBO::model twice before a single submit would (renderShadowCasters() already relies
    // on this same property for lightVP/model below).
    vkCmdPushConstants(cmd, pipeline_.getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

    VkPipeline boundPipeline = VK_NULL_HANDLE; // force the first draw to bind explicitly

    auto materialIndexFor = [&](PrimitiveEntry* entry) -> int {
        return (entry->materialIndex >= 0 && entry->materialIndex < (int)materials_.size())
             ? entry->materialIndex : 0;
    };
    // A material with a successful setMaterialShaderOverride() draws through its own pipeline
    // instead of one of the 4 shared variants below (see that method's comment).
    auto overridePipelineFor = [&](int matIdx) -> Phantom::VKG::VulkanPipeline* {
        auto it = materialPipelineOverrides_.find(matIdx);
        return it != materialPipelineOverrides_.end() ? it->second : nullptr;
    };

    auto draw = [&](PrimitiveEntry* entry, GltfGpuMaterial* mat, Phantom::VKG::VulkanPipeline& matPipeline, bool isPhmatOverride) {
        if (matPipeline.getPipeline() != boundPipeline) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, matPipeline.getPipeline());
            boundPipeline = matPipeline.getPipeline();
        }

        // Bind per-material descriptor set (set=1)
        VkDescriptorSet ds = mat->descriptorSet(frameIndex);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                matPipeline.getLayout(), 1, 1, &ds, 0, nullptr);

        VkBuffer     vbuf   = entry->mesh.vertexBuffer();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vbuf, &offset);

        // Phase 4C item 5 ("GPU marker"): label draws going through a .phmat override pipeline so
        // they stand out in a RenderDoc/Nsight/PIX capture -- no-op if VK_EXT_debug_utils isn't
        // enabled (see VulkanDebugUtils.h's comment).
        if (isPhmatOverride)
            Phantom::VKG::DebugUtils::beginLabel(ctx_->getInstance(), cmd, "phmat override", 0.8f, 0.2f, 0.8f, 1.f);

        if (entry->mesh.hasIndices()) {
            vkCmdBindIndexBuffer(cmd, entry->mesh.indexBuffer(), 0, entry->mesh.indexType());
            vkCmdDrawIndexed(cmd, entry->mesh.indexCount(), 1, 0, 0, 0);
        } else {
            vkCmdDraw(cmd, entry->mesh.vertexCount(), 1, 0, 0);
        }

        if (isPhmatOverride)
            Phantom::VKG::DebugUtils::endLabel(ctx_->getInstance(), cmd);
    };

    // Pass 1: opaque + alpha MASK, depth write on, in build (traversal) order. alpha-BLEND
    // primitives are set aside for pass 2 instead of drawn here.
    std::vector<PrimitiveEntry*> blendEntries;
    for (auto& entryPtr : primitives_) {
        PrimitiveEntry* entry = entryPtr.get();
        if (nodeFilter_ >= 0 && entry->nodeIndex != nodeFilter_) continue;
        int matIdx = materialIndexFor(entry);
        GltfGpuMaterial* mat = materials_[matIdx].get();
        if (hasBlendMaterials_ && mat->isBlend()) {
            blendEntries.push_back(entry);
            continue;
        }
        if (Phantom::VKG::VulkanPipeline* ov = overridePipelineFor(matIdx))
            draw(entry, mat, *ov, true);
        else
            draw(entry, mat, mat->doubleSided() ? pipelineDoubleSided_ : pipeline_, false);
    }

    // Pass 2: alpha BLEND, depth write off, back-to-front (painter's algorithm) so overlapping
    // BLEND surfaces composite correctly against each other, not just against the opaque pass.
    // Sort key is each primitive's rest-pose AABB center transformed by its build-time world
    // matrix -- exact for static geometry, an approximation for an object-animated or skinned
    // BLEND primitive (rare in practice; re-sorting every frame from live transforms would need
    // per-primitive current-world tracking that object animation/skinning don't expose today).
    if (!blendEntries.empty()) {
        sortFarthestFirst(blendEntries, model, currentEyePosition());
        for (PrimitiveEntry* entry : blendEntries) {
            int matIdx = materialIndexFor(entry);
            GltfGpuMaterial* mat = materials_[matIdx].get();
            if (Phantom::VKG::VulkanPipeline* ov = overridePipelineFor(matIdx))
                draw(entry, mat, *ov, true);
            else
                draw(entry, mat, mat->doubleSided() ? pipelineBlendDoubleSided_ : pipelineBlend_, false);
        }
    }
}

// ============================================================
//  IVkSubRenderer::onCleanup
// ============================================================

void GltfSceneRenderer::onCleanup(VkDevice device) {
    if (!ctx_) return;
    ready_ = false;

    pipeline_.destroy(device);
    pipelineDoubleSided_.destroy(device);
    pipelineBlend_.destroy(device);
    pipelineBlendDoubleSided_.destroy(device);
    shadowPipeline_.destroy(device);
    if (skybox_) { skybox_->destroy(device); skybox_.reset(); }

    for (auto& entry : primitives_) entry->mesh.destroy(device);
    primitives_.clear();

    for (auto& mat : materials_) mat->destroy(device);
    materials_.clear();

    materialPipelineOverrides_.clear();
    materialPipelineVariants_.destroyAll(device);
    materialPipelineCache_.destroy(device); // persists to disk first if materialPipelineCacheDir_ is set

    // Descriptor pools (material = document-dependent, global = not) and set layouts
    descriptors_.destroy(device);

    // Real IBL (if any was computed)
    if (iblResult_.isValid()) iblPrecomputer_.destroy(device, iblResult_);

    // Fallback resources
    if (zeroArrayView_)   vkDestroyImageView(device, zeroArrayView_, nullptr);
    if (zeroArrayImage_)  vkDestroyImage(device, zeroArrayImage_, nullptr);
    if (zeroArrayMemory_) vkFreeMemory(device, zeroArrayMemory_, nullptr);
    zeroArrayView_   = VK_NULL_HANDLE;
    zeroArrayImage_  = VK_NULL_HANDLE;
    zeroArrayMemory_ = VK_NULL_HANDLE;
    destroyFallbackCube(device);
    if (fallbackSampler_.isValid()) fallbackSampler_.destroy(device);
    if (fallbackView_)   vkDestroyImageView(device, fallbackView_, nullptr);
    if (fallbackImage_)  vkDestroyImage(device, fallbackImage_, nullptr);
    if (fallbackMemory_) vkFreeMemory(device, fallbackMemory_, nullptr);
    fallbackView_   = VK_NULL_HANDLE;
    fallbackImage_  = VK_NULL_HANDLE;
    fallbackMemory_ = VK_NULL_HANDLE;

    for (int f = 0; f < MAX_FRAMES; ++f) {
        globalUbos_[f].destroy(device);
        boneUbos_[f].destroy(device);
        scalarBuffers_[f].destroy(device);
        lightUbos_[f].destroy(device);
    }

    ctx_ = nullptr;
}
