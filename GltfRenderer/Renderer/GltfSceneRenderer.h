#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "GltfMesh.h"
#include "GltfMaterial.h"
#include "CameraUBO.h"
#include "LightManager.h"
#include "../Gltf/GltfDocument.h"
#include "../IBL/GltfIBLPrecomputer.h"
#include "../../../CGLib/VulkanGraphics/VulkanBuffer.h"
#include "../../../CGLib/VulkanGraphics/VulkanDescriptorPool.h"
#include "../../../CGLib/VulkanGraphics/VulkanPipeline.h"
#include "../../../CGLib/VulkanGraphics/VulkanSampler.h"
#include "../../../CGLib/VkAppBase/IVkSubRenderer.h"

#include <memory>
#include <vector>
#include <array>

namespace Phantom::VKG {
    class VulkanContext;
    class VulkanCommandPool;
}

namespace Phantom::Gltf
{

    struct RtCameraParams {
        glm::vec3 eye = { 0.f, 0.f, 3.f };
        glm::vec3 target = { 0.f, 0.f, 0.f };
        glm::vec3 up = { 0.f, 1.f, 0.f };
        float     fovDeg = 60.f;
    };

    class GltfSceneRenderer : public ::VKG::IVkSubRenderer {
    public:
        struct Shaders {
            std::vector<uint32_t> vertSpv;
            std::vector<uint32_t> fragSpv;
            std::vector<uint32_t> skyboxVertSpv;       // Phase 3: skybox
            std::vector<uint32_t> skyboxFragSpv;
            GltfIBLPrecomputer::Shaders ibl;           // Phase 3: IBL (empty = skip)
            std::vector<uint32_t> shadowVertSpv;       // Phase C: depth-only shadow-caster pass
            std::vector<uint32_t> shadowFragSpv;       // (either empty = shadow casting disabled)
        };

        GltfSceneRenderer() = default;
        GltfSceneRenderer(const GltfSceneRenderer&) = delete;
        GltfSceneRenderer& operator=(const GltfSceneRenderer&) = delete;

        // --- Setup (call before onInit) ---
        // onInit() copies vertSpv/fragSpv when building the main pipeline (and its
        // pipelineDoubleSided_ variant) -- shaders_ is left intact afterward, so a caller that
        // re-runs onCleanup()+onInit() to hot-reload (rather than the lighter loadDocument(),
        // which does not touch the pipeline) does not strictly need to call setShaders() again.
        // Still call it for a real shader hot-reload (new SPIR-V bytes).
        void setShaders(Shaders s) { shaders_ = std::move(s); }

        // Mark every primitive of this document as runtime-deformable: keep the CPU vertex
        // mirror so updateMorphedGeometry() works even without morph targets. Call before
        // onInit()/loadDocument(). Used by PhysicsView's soft-body renderer (docs/todo/
        // PLAN_physicsview_gltf_rendering.md Phase 3).
        void setDynamic(bool v) { dynamic_ = v; }

        // Face-culling mode of the main PBR pipeline. Defaults to VK_CULL_MODE_BACK_BIT
        // (unchanged for every existing caller). PhysicsView's soft bodies keep the default
        // and instead double-wind + view-flip the normal in the fragment shader; this hook
        // exists for callers that would rather disable culling. Call before onInit().
        void setCullMode(VkCullModeFlags m) { cullMode_ = m; }

        // --- IVkSubRenderer ---
        void onInit(Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool,
            VkRenderPass rp, uint32_t framesInFlight) override;
        void onUpdate(uint32_t frameIndex) override;
        void onRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
        void onCleanup(VkDevice device) override;

        // --- Document / extent setup (call before onInit) ---
        void setDocument(const GltfDocument& doc) { doc_ = &doc; }
        void setExtent(VkExtent2D ext) { extent_ = ext; }
        VkExtent2D getExtent() const { return extent_; }

        // --- Dynamic document loading (call after onInit for hot-reload) ---
        void loadDocument(const GltfDocument& doc);

        // --- Object animation (node TRS clips; unskinned meshes) ---
        // Opt-in: with no clip set (the default) the renderer behaves exactly as before -- node
        // transforms stay baked at their build-time pose. setAnimationClip(i) picks
        // doc.animations[i]; onUpdate() then re-bakes each primitive under an animated node with
        // that node's world matrix at setAnimationTime()'s seconds (CPU transform + vertex
        // re-upload, reusing the morph/dynamic path -- no shader or pipeline change). Skinned
        // meshes are unaffected (their pose comes from updateSkinMatrices()). Pass -1 to stop
        // and restore the rest pose. animationCount()/animationDuration() report what's loaded.
        void  setAnimationClip(int clipIndex);
        void  setAnimationTime(float seconds);
        int   animationCount() const;
        float animationDuration(int clipIndex) const;
        int   activeAnimationClip() const { return animClip_; }

        // --- External camera override ---
        void setCamera(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& eye);
        void clearCameraOverride() { useExternalCamera_ = false; }

        // --- Per-instance world transform ---
        // GlobalUBO::model defaults to identity (node transforms are baked into world space at
        // setDocument()/loadDocument() time -- see that struct's comment). Callers that keep a
        // single GltfDocument fixed at the origin and instead move a whole GltfSceneRenderer
        // instance around (e.g. Universe's UniverseGltfRenderer, one instance per glTF-sourced
        // entity) can override that identity here instead.
        void setModelMatrix(const glm::mat4& m) { modelMatrix_ = m; }

        // Hides this instance without tearing down its GPU resources (unlike clearDocumentResources()
        // via loadDocument(), which frees them). Defaults to visible so every existing caller that
        // never touches this keeps rendering unconditionally as before.
        void setVisible(bool v) { visible_ = v; }
        bool isVisible() const  { return visible_; }

        // --- GPU skinning ---
        // Per-frame joint matrices for skinned primitives (see GltfSkin/JOINTS_0/WEIGHTS_0 in
        // GltfTypes.h and BoneUBO in CameraUBO.h). Index i is position i within the relevant
        // GltfSkin::joints array, not a node index -- callers combine each joint's current global
        // transform with that joint's inverseBindMatrices entry before passing it in here (see
        // Phantom::Animation::Animator::getSkinMatrices() for the existing pattern this mirrors).
        // Unskinned documents/entities never need to call this: every Vertex defaults to
        // jointIndices=(0,0,0,0)/jointWeights=(1,0,0,0), and entries beyond what was supplied here
        // (or the whole array, if never called) default to identity in onUpdate().
        void updateSkinMatrices(std::vector<glm::mat4> skinMatrices) { skinMatrices_ = std::move(skinMatrices); }

        // --- CPU morph target blending (Phase 7) ---
        // Rewrites the position attribute of the primitive at doc.meshes[meshIndex].primitives[primIndex]
        // and re-uploads its whole vertex buffer -- see GltfGpuMesh::updatePositions()'s comment for
        // why this is a full re-upload rather than a partial write. Pass the result of
        // Phantom::Gltf::applyMorphs() (GltfMorphApply.h), evaluated from
        // GltfAnimationEvaluator::evaluateMorphWeights()'s per-frame weights. No-op (false) if no
        // built primitive matches meshIndex/primIndex (e.g. it has no POSITION accessor, or the
        // document hasn't been loaded through onInit()/loadDocument() yet).
        // nodeIndex restricts updates to one instance; -1 updates all instances of the mesh.
        bool updateMorphedPositions(int meshIndex, int primIndex, const std::vector<glm::vec3>& positions, int nodeIndex = -1);

        // Position + CPU-recomputed normal update for a deforming primitive (setDynamic(true)
        // or morph targets). Same semantics as updateMorphedPositions() otherwise.
        bool updateMorphedGeometry(int meshIndex, int primIndex,
                                   const std::vector<glm::vec3>& positions,
                                   const std::vector<glm::vec3>& normals, int nodeIndex = -1);

        // --- Camera input handlers ---
        void handleMouseButton(bool pressed);
        void handleMouseMove(double x, double y);
        void handleScroll(double dy);

        // --- Panel access: raw pointers for ImGui sliders ---
        float* camDistPtr() { return &camDist_; }
        glm::vec3* camTargetPtr() { return &camTarget_; }

        // --- Camera query ---
        RtCameraParams getCameraParams() const;

        // --- Environment / light control (Phase 4 wires these up) ---
        void setEnvironment(VkImageView envView, VkSampler envSampler);
        void setLight(const glm::vec4& pos, const glm::vec4& color);
        void setUseIBL(bool v) { useIBL_ = v ? 1 : 0; }
        int  getUseIBL() const { return useIBL_; }

        // Phase 4B tone mapping: multiplies color before gltf.frag's Reinhard tonemap (1.0 =
        // unchanged from before this existed). GlobalUBO::exposure was appended at the very end
        // of the struct specifically so this is safe to add without shifting any other
        // consumer's field offsets -- see CameraUBO.h's comment. A consumer whose own gltf.frag
        // copy doesn't declare `exposure` (every one but Universe's, as of Phase 4B) simply never
        // reads the extra tail bytes; this setter is a no-op for it either way.
        void  setExposure(float v) { exposure_ = v; }
        float getExposure() const { return exposure_; }

        // --- Multi-light (KHR_lights_punctual; Phase 4B) ---
        // Replaces every previously-set punctual light with `lights` (empty clears them). When
        // non-empty, gltf.frag shades with these instead of the single lightPos_/lightColor_ pair
        // above (up to LightManager::kMaxLights=8; extras beyond that are dropped). Call any time
        // after onInit() -- takes effect on the next onUpdate(). Every caller that never calls
        // this keeps the single-light behavior unchanged (Universe's own lights() plumbing is the
        // first real consumer; see Rendering/GltfRenderer.cpp).
        void setPunctualLights(std::vector<LightEntry> lights);
        int  punctualLightCount() const { return lightManager_.count(); }

        // --- Shadow mapping (Phase C) ---
        // Call once after onInit(), against a ShadowMapPass's render pass; no-op if
        // Shaders::shadowVertSpv/shadowFragSpv were left empty.
        void createShadowPipeline(VkRenderPass shadowRenderPass);
        bool hasShadowPipeline() const { return shadowPipeline_.getPipeline() != VK_NULL_HANDLE; }

        // Draws every primitive's position-only geometry through the shadow-caster
        // pipeline using the same per-instance model matrix as the PBR pass. Must be
        // called between a ShadowMapPass's begin()/end(). No-op if
        // createShadowPipeline() was never called or found no shadow shaders.
        void renderShadowCasters(VkCommandBuffer cmd, const glm::mat4& lightVP);

        // Binds the shadow depth map sampled by the main PBR pass and enables shadowing;
        // pass the same lightVP used for renderShadowCasters().
        void setShadowMap(VkImageView shadowView, VkSampler shadowSampler, const glm::mat4& lightVP);
        void clearShadowMap(); // disables shadowing, reverts to the fallback "always lit" texture

        // Update only the light view-projection the main PBR pass samples the shadow
        // map with (no descriptor write, unlike setShadowMap()). For callers that
        // animate the light direction per frame and re-render the shadow caster
        // pass with a matching lightVP -- safe to call every frame while frames are
        // in flight. No-op-safe before setShadowMap() (the matrix just feeds the
        // per-frame UBO in onUpdate()).
        void setShadowLightVP(const glm::mat4& lightVP) { shadowVP_ = lightVP; }
        void setShadowParams(float bias, float strength) { shadowBias_ = bias; shadowStrength_ = strength; }

        // --- Stats ---
        int                 primitiveCount() const { return static_cast<int>(primitives_.size()); }
        const GltfDocument* document()       const { return doc_; }

    private:
        static constexpr int MAX_FRAMES = 2;

        Shaders             shaders_;
        bool                ready_ = false;
        bool                visible_ = true;
        bool                dynamic_ = false;
        VkCullModeFlags     cullMode_ = VK_CULL_MODE_BACK_BIT;
        const GltfDocument* doc_ = nullptr;
        VkExtent2D          extent_ = { 1280, 720 };

        // Vulkan context cached for hot-reload (set in onInit)
        const Phantom::VKG::VulkanContext* ctx_ = nullptr;
        const Phantom::VKG::VulkanCommandPool* pool_ = nullptr;

        // External camera override
        bool      useExternalCamera_ = false;
        glm::mat4 extView_ = glm::mat4(1.f);
        glm::mat4 extProj_ = glm::mat4(1.f);
        glm::vec3 extEye_ = {};

        // Per-instance world transform (see setModelMatrix()).
        glm::mat4 modelMatrix_ = glm::mat4(1.f);

        // GPU skinning (see updateSkinMatrices()).
        std::vector<glm::mat4> skinMatrices_;

        // Camera state (spherical coordinates)
        float     camTheta_ = 0.6f;
        float     camPhi_ = 0.4f;
        float     camDist_ = 3.0f;
        float     fovDeg_ = 45.f; // single source of truth: shared by rasterized projection and getCameraParams()
        glm::vec3 camTarget_{ 0.f, 0.f, 0.f };
        double    lastX_ = 0.0;
        double    lastY_ = 0.0;
        bool      isDragging_ = false;

        // --- set=0: Global per-frame resources (document-independent) ---
        std::array<Phantom::VKG::VulkanBuffer, MAX_FRAMES> globalUbos_;
        std::array<Phantom::VKG::VulkanBuffer, MAX_FRAMES> boneUbos_;
        std::array<Phantom::VKG::VulkanBuffer, MAX_FRAMES> lightUbos_; // binding 6, see setPunctualLights()
        Phantom::VKG::VulkanDescriptorSetLayout globalSetLayout_;
        VkDescriptorPool               globalDescPool_ = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet>   globalDescSets_;

        // --- set=1: Per-material resources (document-dependent) ---
        Phantom::VKG::VulkanDescriptorSetLayout materialSetLayout_;
        VkDescriptorPool               descriptorPool_ = VK_NULL_HANDLE;

        // Light state written to GlobalUBO each frame
        glm::vec4 lightPos_ = { 1.f, 1.f, 1.f, 0.f };  // w=0: directional
        glm::vec4 lightColor_ = { 1.f, 1.f, 1.f, 3.f };  // w=intensity

        // Multi-light (see setPunctualLights()). Empty by default -- gltf.frag falls back to
        // lightPos_/lightColor_ above when punctualLightCount() == 0.
        LightManager lightManager_;
        int       useIBL_ = 0;
        float     exposure_ = 1.0f; // see setExposure()

        // Environment cubemap (set externally by GltfViewerApp)
        VkImageView envView_ = VK_NULL_HANDLE;
        VkSampler   envSampler_ = VK_NULL_HANDLE;

        // 4 pipeline variants over 2 independent axes, selected per-primitive in onRender():
        //   - cull:  pipeline_/pipelineBlend_ (cullMode_, usually back-face) vs
        //            pipelineDoubleSided_/pipelineBlendDoubleSided_ (VK_CULL_MODE_NONE) --
        //            see GltfGpuMaterial::doubleSided().
        //   - blend: pipeline_/pipelineDoubleSided_ (opaque: depthWrite on, blend off) vs
        //            pipelineBlend_/pipelineBlendDoubleSided_ (alpha BLEND: depthWrite off,
        //            src-alpha/one-minus-src-alpha blend) -- see GltfGpuMaterial::isBlend().
        // alpha MASK draws through the opaque pair (the fragment shader discards below cutoff
        // instead, since a MASK'd surface is still either fully opaque or invisible per-fragment).
        Phantom::VKG::VulkanPipeline pipeline_;
        Phantom::VKG::VulkanPipeline pipelineDoubleSided_;
        Phantom::VKG::VulkanPipeline pipelineBlend_;
        Phantom::VKG::VulkanPipeline pipelineBlendDoubleSided_;
        // True if any material in the current document has alphaMode=Blend -- lets onRender()
        // skip the blend-sorting pass entirely (the common case) instead of allocating/sorting an
        // always-empty list. Set in buildDocumentResources(), cleared in clearDocumentResources().
        bool hasBlendMaterials_ = false;
        // Eye position onRender() sorts alpha-BLEND primitives back-to-front against: mirrors the
        // same useExternalCamera_ branch onUpdate() uses to fill GlobalUBO::camPos.
        glm::vec3 currentEyePosition() const { return useExternalCamera_ ? extEye_ : cameraPosition(); }

        // Shadow mapping (Phase C)
        Phantom::VKG::VulkanPipeline shadowPipeline_; // depth-only, push-constant lightVP only
        VkImageView shadowView_     = VK_NULL_HANDLE;
        VkSampler   shadowSampler_  = VK_NULL_HANDLE;
        glm::mat4   shadowVP_       = glm::mat4(1.f);
        int         shadowEnabled_  = 0;
        float       shadowBias_     = 0.0025f;
        float       shadowStrength_ = 1.0f;

        // Geometry per primitive
        struct PrimitiveEntry {
            GltfGpuMesh mesh;
            int         materialIndex = -1;
            int         meshIndex = -1; // doc.meshes[] index this primitive came from (see updateMorphedPositions())
            int         primIndex = -1; // index within that mesh's primitives[]
            int         nodeIndex = -1; // doc.nodes[] index (object animation)
            glm::mat4   restWorld{ 1.f }; // build-time bake transform (rest pose)
            glm::vec3   localCenter{ 0.f }; // accessor-space AABB center, for alpha-BLEND back-to-front
                                             // sorting in onRender() (restWorld * localCenter is only
                                             // exact at the rest pose -- an approximation under object
                                             // animation/skinning, see onRender()'s sort comment)
            std::vector<glm::vec3> localPos; // accessor-space positions, only kept for object animation
            std::vector<glm::vec3> localNrm;
        };
        std::vector<std::unique_ptr<PrimitiveEntry>> primitives_;

        // Object animation state (see setAnimationClip()).
        int               animClip_  = -1;
        float             animTime_  = 0.f;
        bool              animDirty_ = false;   // clip/time changed since the last applyObjectAnimation()
        std::vector<uint8_t> animatedNode_;     // per node: 1 if the active clip moves it (or an ancestor)
        void applyObjectAnimation();
        void markSubtreeAnimated(int nodeIndex);

        // Materials
        std::vector<std::unique_ptr<GltfGpuMaterial>> materials_;

        // Shared fallback 2D texture (1x1 white) — also used as brdfLUT fallback
        VkImage        fallbackImage_ = VK_NULL_HANDLE;
        VkDeviceMemory fallbackMemory_ = VK_NULL_HANDLE;
        VkImageView    fallbackView_ = VK_NULL_HANDLE;
        Phantom::VKG::VulkanSampler fallbackSampler_;

        // Fallback cube image (1x1 white) for IBL bindings when useIBL=0
        VkImage        fallbackCubeImage_ = VK_NULL_HANDLE;
        VkDeviceMemory fallbackCubeMem_ = VK_NULL_HANDLE;
        VkImageView    fallbackCubeView_ = VK_NULL_HANDLE;

        glm::vec3 cameraPosition() const;

        void traverseNode(const GltfDocument& doc, int nodeIndex,
            const glm::mat4& parentTransform,
            const Phantom::VKG::VulkanContext& ctx,
            const Phantom::VKG::VulkanCommandPool& pool);

        glm::mat4 nodeLocalTransform(const GltfNode& node) const;

        // Descriptor layout helpers (document-independent, called once in onInit)
        void createGlobalSetLayout(VkDevice device);
        void createMaterialSetLayout(VkDevice device);
        bool createGlobalDescPool(VkDevice device);
        bool createGlobalDescriptorSets(VkDevice device);
        void updateGlobalDescriptorSets(VkDevice device);

        // Material pool (document-dependent, called in buildDocumentResources)
        bool createDescriptorPool(VkDevice device, uint32_t materialCount);

        // Fallback resource helpers
        void createFallbackCube(const Phantom::VKG::VulkanContext& ctx, const Phantom::VKG::VulkanCommandPool& pool);
        void destroyFallbackCube(VkDevice device);

        // Document-lifecycle helpers
        void buildDocumentResources();
        void clearDocumentResources();
    };

}
