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
#include "../../../CGLib/Renderer/VkRenderer/VkSkyBoxRenderer.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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

        // --- Multi-instance draw (Phase 2 item 5 後半 of PLAN_blender_universe_authoring_loop.md:
        // "共有GPU asset化", completion condition "pipeline数がentity数に比例しない") ---
        // Draws this document's mesh/material/pipeline resources once per entry in
        // `modelMatrices`, each with its own model matrix delivered through the vertex push
        // constant onInit() now reserves on every main-pass pipeline (see the shadow pass'
        // renderShadowCasters(), which already used this exact technique) -- NOT through
        // GlobalUBO::model, which is a single per-frame value shared by every draw recorded
        // against it and would race across instances the way onRender()'s own doc comment on
        // setModelMatrix() warns about. onRender() itself now also pushes modelMatrix_ this same
        // way (see its .cpp definition), so calling this with a one-element vector containing
        // modelMatrix_ renders identically to onRender().
        //
        // Scope of this first slice: caller's responsibility to only call this for a document
        // with no active object/skeletal animation and no morph targets (animation/morph state --
        // animClip_/animTime_/skinMatrices_/the CPU-rebaked vertex buffers -- is still a single,
        // non-per-instance value on this object; sharing those across instances with independent
        // playback is the separate "(1) GPU/UBO駆動" prerequisite the plan calls out as unstarted
        // follow-up work). Ignores visible_ == false the same way onRender() does not (callers
        // filter which entities/matrices to pass in). No-op if primitives_ is empty, !ready_, or
        // modelMatrices is empty. Draws the skybox (if enabled) exactly once, not once per instance
        // -- it is a background, not per-instance geometry.
        void renderInstances(VkCommandBuffer cmd, uint32_t frameIndex, const std::vector<glm::mat4>& modelMatrices);

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

        // Restricts this renderer to primitives authored by the given glTF node. An empty
        // filter restores the normal whole-document draw. The document and GPU buffers remain
        // shared; this is only a draw-time selection used by Universe's sidecar entity import.
        void setNodeFilter(int nodeIndex) { nodeFilter_ = nodeIndex; }
        int  nodeFilter() const { return nodeFilter_; }

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
        // envView must be a CUBE image view. If Shaders::ibl (irradiance/prefilter/brdf vert+frag,
        // set via setShaders() before onInit()) was populated, this also (re-)computes real IBL
        // textures from it (Phantom::Gltf::GltfIBLPrecomputer -- irradiance convolution, a
        // roughness-mip prefiltered environment, and the split-sum BRDF LUT) for
        // updateGlobalDescriptorSets() to bind instead of sampling envView directly. Safe to call
        // before onInit() (the common pattern: envView/envSampler are stored either way, but the
        // actual precompute needs ctx_/pool_ and is deferred to onInit() in that case) and safe to
        // call again later with a different environment (the previous IBL result is destroyed
        // first). A caller that never sets Shaders::ibl keeps exactly the old behavior (envView
        // sampled directly for irradiance/prefiltered, BRDF LUT stays a flat fallback).
        void setEnvironment(VkImageView envView, VkSampler envSampler);
        void setLight(const glm::vec4& pos, const glm::vec4& color);
        void setUseIBL(bool v) { useIBL_ = v ? 1 : 0; }
        int  getUseIBL() const { return useIBL_; }

        // --- Skybox (Phase 4B, background display of the environment cubemap) ---
        // Draws the same cubemap setEnvironment() bound (IBL and the skybox always show the same
        // environment -- no separate "skybox-only" texture slot) as a background behind all
        // opaque/blend primitives, using Phantom::VKG::VkSkyBoxRenderer (already used by
        // VkRendererView/PhysicsView's SSFR skybox mode; reused as-is rather than reimplemented).
        // No-op unless Shaders::skyboxVertSpv/skyboxFragSpv were populated before onInit() --
        // every existing caller that leaves them empty (the common case before this feature)
        // keeps rendering exactly as before. Default OFF even when the shaders are present, so a
        // caller that already loads them (GltfViewer, RayTracerView -- previously unused, dead
        // fields) does not suddenly grow a visible skybox without opting in.
        void setUseSkybox(bool v) { useSkybox_ = v; }
        bool getUseSkybox() const { return useSkybox_; }
        bool hasSkyboxPipeline() const { return skybox_.has_value(); }

        // --- Per-material shader graph override (Phase 4C, ".phmat") ---
        // Replaces the fragment shader used to draw materialIndex's primitives with fragSpv
        // (typically Phantom::Gltf::Phmat::loadPhmatMaterial()'s output -- see Phmat/
        // PhmatCompiler.h), keeping gltf.vert and this material's own doubleSided()/isBlend()
        // (still read from its glTF alphaMode/doubleSided, unaffected by the graph) exactly as
        // before. Must be called after onInit() (needs the render pass + descriptor set layouts)
        // and after a document is loaded (materialIndex must already exist in materials_).
        // Returns false and leaves any existing pipeline for this material untouched -- the
        // shared default or a previous successful override -- if pipeline creation fails
        // (outError, if given, explains why); this is the "compile失敗時は旧pipelineを維持する"
        // contract from the plan (Phase 4C item 4). Safe to call repeatedly (e.g. re-applying an
        // edited .phmat): the previous override for the same index is destroyed only once the
        // new one has successfully been created.
        bool setMaterialShaderOverride(int materialIndex, const std::vector<uint32_t>& fragSpv, std::string* outError = nullptr);
        // Reverts materialIndex to the shared default pipeline (pipeline_/pipelineBlend_/...).
        // No-op if it had no override. Does not destroy the underlying VkPipeline -- see
        // materialPipelineVariantPool_'s comment on why override pipelines are pooled and only
        // ever destroyed at onCleanup()/clearDocumentResources().
        void clearMaterialShaderOverride(int materialIndex);
        bool hasMaterialShaderOverride(int materialIndex) const;

        // --- Phase 4C item 5: pipeline cache persistence for .phmat overrides ---
        // Opt-in directory a persistent VkPipelineCache blob for setMaterialShaderOverride()'s
        // pipelines is loaded from (if present) at onInit() and written back to at onCleanup() --
        // lets the driver skip re-doing identical compilation work across app runs, on top of
        // PhmatCompiler.h's own SPIR-V-level cache (which only skips the glslc invocation, not the
        // driver's own SPIR-V->native-ISA compile). Must be called before onInit(); empty (the
        // default) means "keep an in-process-only VkPipelineCache" -- variant reuse (see
        // materialPipelineVariantPool_) still applies within a single run either way, only the
        // cross-run disk persistence is gated by this.
        void setMaterialShaderCacheDir(const std::string& dir) { materialPipelineCacheDir_ = dir; }
        // Number of distinct VkPipeline objects backing every setMaterialShaderOverride() call so
        // far (materials sharing the same compiled SPIR-V + doubleSided/blend state reuse one
        // entry -- see setMaterialShaderOverride()'s comment). Exposed for tests/introspection.
        int materialPipelineVariantCount() const { return static_cast<int>(materialPipelineVariantPool_.size()); }

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

        // Phase 2 item 5 後半 counterpart of renderInstances(): casts once per entry in
        // `modelMatrices` instead of once at modelMatrix_ -- lets every member of a shared
        // renderer group (Universe::GltfRenderer::findShareableRenderer()) cast into the scene
        // shadow map at its OWN transform, not just the first one (see that class's
        // renderShadowCasters() for the limitation this replaces). Same no-op guards as the
        // single-matrix overload; empty modelMatrices is also a no-op.
        void renderShadowCasterInstances(VkCommandBuffer cmd, const glm::mat4& lightVP,
                                          const std::vector<glm::mat4>& modelMatrices);

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
        VkRenderPass renderPass_ = VK_NULL_HANDLE; // cached for setMaterialShaderOverride(), called after onInit()

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

        // Environment cubemap (set externally, see setEnvironment())
        VkImageView envView_ = VK_NULL_HANDLE;
        VkSampler   envSampler_ = VK_NULL_HANDLE;

        // Real IBL (irradiance/prefiltered-env/BRDF LUT), computed from envView_/envSampler_ by
        // recomputeIBL() whenever setEnvironment() is called (or onInit(), for a setEnvironment()
        // that ran before ctx_/pool_ existed -- the common call order, see setEnvironment()'s
        // comment). No-op (iblResult_ stays invalid) unless the caller populated Shaders::ibl --
        // updateGlobalDescriptorSets() falls back to sampling envView_ directly for irradiance/
        // prefiltered (a flat approximation) and a white 2D fallback for the BRDF LUT when it is.
        GltfIBLPrecomputer         iblPrecomputer_;
        GltfIBLPrecomputer::Result iblResult_;

        void recomputeIBL();

        // Skybox (see setUseSkybox()). Constructed in onInit() only if Shaders::skyboxVertSpv/
        // skyboxFragSpv are non-empty; std::optional so a caller that never sets those shaders
        // pays no extra Vulkan resource cost (matches the shadow pipeline's shaders_.shadowVertSpv-
        // gated pattern above, just with an owned sub-object instead of a lazily-created pipeline).
        std::optional<Phantom::VKG::VkSkyBoxRenderer> skybox_;
        bool useSkybox_ = false;

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
        int nodeFilter_ = -1;

        // Object animation state (see setAnimationClip()).
        int               animClip_  = -1;
        float             animTime_  = 0.f;
        bool              animDirty_ = false;   // clip/time changed since the last applyObjectAnimation()
        std::vector<uint8_t> animatedNode_;     // per node: 1 if the active clip moves it (or an ancestor)
        void applyObjectAnimation();
        void markSubtreeAnimated(int nodeIndex);

        // Materials
        std::vector<std::unique_ptr<GltfGpuMaterial>> materials_;

        // Per-material shader graph override pipelines (see setMaterialShaderOverride()). Keyed
        // by index into materials_/doc_->materials; absent = draw through the shared
        // pipeline_/pipelineBlend_/... variants as before. Non-owning -- points into
        // materialPipelineVariantPool_ below, which is what actually owns every override
        // VkPipeline.
        std::unordered_map<int, Phantom::VKG::VulkanPipeline*> materialPipelineOverrides_;

        // Phase 4C item 5 ("shader variant"): more than one material -- in this document, or
        // (since a pipeline only depends on the render pass/shader/vertex layout, none of which
        // are per-document) a document loaded earlier in this same GltfSceneRenderer's lifetime --
        // can end up wanting the exact same compiled fragment SPIR-V plus the same
        // doubleSided()/isBlend()-derived fixed-function state (the common case: the same .phmat
        // file applied more than once). Rather than let setMaterialShaderOverride() build a
        // redundant VkPipeline for each such call, every override pipeline actually created is
        // pooled here and looked up by MaterialPipelineVariantKey first; clearMaterialShaderOverride()
        // and clearDocumentResources() only ever drop materialPipelineOverrides_' non-owning
        // entries, never a pool entry itself (a variant might still be referenced by another
        // material/document) -- pool entries are only destroyed at onCleanup(), the same
        // "shared, never destroyed individually" lifetime the 4 shared pipeline_/pipelineBlend_/...
        // variants above already have.
        struct MaterialPipelineVariantKey {
            uint64_t        fragSpvHash = 0; // FNV-1a 64 over the raw SPIR-V words -- a cache key, not a security boundary
            VkCullModeFlags cullMode    = VK_CULL_MODE_BACK_BIT;
            bool            blendEnable = false;
            bool            depthWrite  = true;
            bool operator==(const MaterialPipelineVariantKey& o) const {
                return fragSpvHash == o.fragSpvHash && cullMode == o.cullMode &&
                       blendEnable == o.blendEnable && depthWrite == o.depthWrite;
            }
        };
        struct MaterialPipelineVariantKeyHash {
            size_t operator()(const MaterialPipelineVariantKey& k) const {
                size_t h = std::hash<uint64_t>{}(k.fragSpvHash);
                h ^= std::hash<uint32_t>{}(static_cast<uint32_t>(k.cullMode)) + 0x9e3779b9u + (h << 6) + (h >> 2);
                h ^= std::hash<bool>{}(k.blendEnable) + 0x9e3779b9u + (h << 6) + (h >> 2);
                h ^= std::hash<bool>{}(k.depthWrite)  + 0x9e3779b9u + (h << 6) + (h >> 2);
                return h;
            }
        };
        std::vector<std::unique_ptr<Phantom::VKG::VulkanPipeline>> materialPipelineVariantPool_;
        std::unordered_map<MaterialPipelineVariantKey, Phantom::VKG::VulkanPipeline*, MaterialPipelineVariantKeyHash>
            materialPipelineVariantIndex_;

        // Phase 4C item 5 ("pipeline cache"): a real VkPipelineCache passed to every override
        // pipeline's vkCreateGraphicsPipelines() call (see PipelineConfig::pipelineCache) -- always
        // created in onInit() (in-process reuse is free/always-on), optionally persisted to disk
        // under materialPipelineCacheDir_ at onCleanup() if that was set via
        // setMaterialShaderCacheDir() before onInit(). Distinct from PhmatCompiler.h's own SPIR-V
        // disk cache: that one skips invoking glslc at all on a hit, this one only helps the
        // driver's own SPIR-V -> native-ISA compile step, which still runs once per distinct
        // shader/state combination even on a glslc cache hit.
        VkPipelineCache materialPipelineCache_ = VK_NULL_HANDLE;
        std::string      materialPipelineCacheDir_; // see setMaterialShaderCacheDir()
        void createMaterialPipelineCache(VkDevice device);
        void destroyMaterialPipelineCache(VkDevice device); // persists to disk first if materialPipelineCacheDir_ is set

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

        // Shared body of onRender()/renderInstances(): binds set=0 once (caller's job -- both
        // public entry points do it before their first call here), pushes `model` as the vertex
        // push constant, then draws every primitive through it exactly like onRender() always
        // has. Does NOT touch the skybox -- callers draw it themselves, once, after their own loop.
        void renderPrimitivesWithModel(VkCommandBuffer cmd, uint32_t frameIndex, const glm::mat4& model);

        // Shared body of renderShadowCasters()/renderShadowCasterInstances(): pushes {lightVP,
        // model} and draws every primitive's position-only geometry through shadowPipeline_.
        void renderShadowCastersWithModel(VkCommandBuffer cmd, const glm::mat4& lightVP, const glm::mat4& model);

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
