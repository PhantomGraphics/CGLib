#pragma once
#include "../../CGLib/VkAppBase/VkAppBase.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/IScenarioHost.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioBrowserPanel.h"
#include "../GltfRenderer/Gltf/GltfDocument.h"
#include "../GltfRenderer/Gltf/GltfLightsCameras.h"
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "../GltfRenderer/IBL/GltfEnvironmentCubemap.h"
#include "CommandDispatcher.h"
#include "ControlPanel.h"
#include "SceneGraphPanel.h"
#include "ViewPanel.h"
#include "VrmViewState.h"
#include <vulkan/vulkan.h>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Phantom::Gltf {

    class App : public ::VKG::VkAppBase, public ::IScenarioHost {
    public:
        explicit App(const std::filesystem::path& gltfPath);

        bool loadScenario(const std::string& jsonPath) override;
        void setExitOnScenarioComplete(bool v) override { exitOnComplete_ = v; }
        int  getExitCode() const { return exitCode_; }

        // IScenarioHost (drives ScenarioBrowserPanel)
        bool   isScenarioActive()   const override { return runner_.isActive();   }
        bool   scenarioHasFailed()  const override { return runner_.hasFailed();  }
        const std::string& scenarioFailMessage() const override { return runner_.failMessage(); }
        size_t scenarioStepCount()  const override { return runner_.stepCount();  }

    protected:
        void onInit()                      override;
        void onUpdate(uint32_t frameIndex) override;
        void onSwapChainCreated()          override;
        void onCleanup()                   override;
        void onImGui()                      override;

    public:
        // VRM-specific metadata for the currently loaded document. Kept separate from `doc_` --
        // which stays the single source of truth GltfSceneRenderer renders -- because none of
        // this has a home in generic glTF vocabulary. GltfViewerPanel reads this to draw the VRM
        // section; GltfCommandDispatcher reads/drives it for scenario tests.
        const VrmViewState& vrmState() const { return vrm_; }

        // Sets expressionWeights_[index] and re-blends every morph target it affects. No-op
        // (does nothing, does not crash) if index is out of range or the renderer hasn't built
        // its GPU primitives yet (see GltfSceneRenderer::updateMorphedPositions()'s own no-op
        // convention for a document not yet loaded through onInit()).
        void setVrmExpressionWeight(int index, float weight);

        // Real HDRI loading (2026-09-15): replaces envCubemap_'s flat placeholder tint with a
        // real environment cube baked from an equirectangular .hdr panorama
        // (GltfEnvironmentCubemap::loadFromHDR(), shared with Universe -- see that class's
        // header comment). Returns false (leaving the current environment, real or placeholder,
        // untouched) if the file can't be read or the GPU conversion fails.
        bool loadEnvironmentHDR(const std::string& path);
        // Reverts to the flat placeholder tint.
        void clearEnvironmentHDR();
        bool hasEnvironmentHDR() const { return envCubemap_.isRealHDR(); }
        const std::string& environmentHDRPath() const { return envCubemap_.hdrPath(); }

        // .phmat shader graph material override (Phase 4C, first vertical slice --
        // docs/todo/PLAN_blender_universe_authoring_loop.md): parses/validates/compiles the
        // node graph at path (Phantom::Gltf::Phmat::loadPhmatMaterial(), CGLib/GltfRenderer/
        // Phmat/PhmatCompiler.h) and, on success, replaces materialIndex's fragment shader with
        // it (GltfSceneRenderer::setMaterialShaderOverride()). Returns false and leaves whatever
        // pipeline materialIndex already had (shared default, or a previous override) untouched
        // on any failure -- outError, if given, carries the parse/validate/glslc diagnostic.
        bool loadPhmatMaterial(int materialIndex, const std::string& path, std::string* outError = nullptr);
        // Reverts materialIndex to the shared default pipeline.
        void clearPhmatMaterial(int materialIndex);
        bool hasPhmatOverride(int materialIndex) const { return renderer_.hasMaterialShaderOverride(materialIndex); }
        // Distinct VkPipeline objects behind every active/previously-applied .phmat override --
        // see GltfSceneRenderer::materialPipelineVariantCount()'s comment (Phase 4C item 5).
        int phmatPipelineVariantCount() const { return renderer_.materialPipelineVariantCount(); }

        // Phase 4C item 5 ("hot reload"): opt-in, off by default (same convention as every other
        // opt-in feature in this file). While enabled, checkPhmatHotReload() (called from
        // onUpdate()) periodically stat()s every active override's .phmat file and the .phshader
        // files it references (PhmatLoadResult::dependencyPaths); on a change to any of them, the
        // same loadPhmatMaterial() path a manual "Load .phmat..." click uses re-runs automatically.
        // A reload that fails behaves exactly like a manual failed reload (old pipeline kept,
        // error surfaced) -- see loadPhmatMaterial()'s own comment.
        bool phmatHotReloadEnabled() const { return phmatHotReloadEnabled_; }
        void setPhmatHotReloadEnabled(bool v) { phmatHotReloadEnabled_ = v; }

        // "Camera-as-scene-component" (2026-09-16): if the loaded document has a camera attached
        // to any scene node, lets the viewer actually look through it instead of the free orbit
        // camera GltfSceneRenderer maintains internally -- unlike Universe's equivalent
        // (Renderer::applyAssetCamera(), which wins by default the moment an asset ships one),
        // this is opt-in and off by default, same convention as RayTracer's
        // SetUseAssetCamera/GetHasAssetCamera (RayTracerApp::extractFirstCamera()): a general
        // inspection tool shouldn't yank the camera out from under a user who just wants to
        // freely orbit a newly loaded asset. Only the first camera instance found (depth-first
        // scene traversal, same policy as Universe/RayTracer) is used.
        bool hasAssetCamera() const { return hasAssetCamera_; }
        bool useAssetCamera() const { return useAssetCamera_; }
        // No-op if hasAssetCamera() is false. `true` (re-)computes the projection from the
        // stored camera against the current viewport aspect, so a later resize (onSwapChainCreated())
        // re-pushing this keeps an aspectRatio-less camera correct -- same hazard Universe's
        // review R3 fixed (docs/todo/PLAN_blender_universe_authoring_loop.md).
        void setUseAssetCamera(bool use);

    private:
        GltfDocument             doc_;
        VrmViewState              vrm_;
        GltfSceneRenderer        renderer_;
        ControlPanel          panel_;
        SceneGraphPanel       sceneGraphPanel_;
        ViewPanel             viewPanel_;
        CommandDispatcher    dispatcher_;
        ScenarioRunner           runner_;
        //ScenarioConsole          console_;
        ScenarioBrowserPanel     scenarioBrowser_;
        int selectedNode_ = -1;

        std::optional<std::filesystem::path> pendingPath_;

        bool        screenshotPending_ = false;
        std::string screenshotPendingPath_;

        bool exitOnComplete_ = true;
        int  exitCode_ = 0;

        // Environment cubemap: flat placeholder sky tint by default, or a real equirectangular
        // .hdr panorama once loaded. Shared with Universe (2026-09-15, GltfEnvironmentCubemap.h's
        // comment) rather than this app's own duplicate of the same boilerplate.
        GltfEnvironmentCubemap envCubemap_;
        // shaders/equirect_to_cube.{vert,frag}, loaded once in applyShaders() and copied into
        // each loadEnvironmentHDR() call (GltfIBLPrecomputer::computeEnvironmentCube() consumes
        // its shader vectors by value/move).
        std::vector<uint32_t> equirectVertSpv_;
        std::vector<uint32_t> equirectFragSpv_;

        // Phase 4C item 5 ("hot reload") -- see phmatHotReloadEnabled()'s comment. One entry per
        // materialIndex currently holding a successfully-applied .phmat override; absent =
        // nothing to watch for that index (matches renderer_.hasMaterialShaderOverride()).
        struct PhmatWatchEntry {
            std::string phmatPath;
            std::vector<std::string> dependencyPaths; // this load's own PhmatLoadResult::dependencyPaths
            std::unordered_map<std::string, std::filesystem::file_time_type> lastWriteTimes;
        };
        std::unordered_map<int, PhmatWatchEntry> phmatWatches_;
        bool phmatHotReloadEnabled_ = false;
        int  phmatHotReloadFrameCounter_ = 0; // throttle -- see checkPhmatHotReload()'s comment
        // Called every frame from onUpdate(). No-op unless phmatHotReloadEnabled_ and at least one
        // watch entry exists.
        void checkPhmatHotReload();
        // Re-stats an existing watch entry's own dependencyPaths (does not change which paths are
        // watched) -- called after a failed (re)load so checkPhmatHotReload() doesn't see the same
        // "changed" state forever and retry every tick; see loadPhmatMaterial()'s comment.
        void refreshPhmatWatchMTimes(int materialIndex);

        // "Camera-as-scene-component" state -- see setUseAssetCamera()'s comment above.
        // Captured once per loadFile() from doc_'s first camera instance (collectGltfLightsAndCameras()),
        // kept around (rather than only pushed straight into renderer_'s override) so
        // setUseAssetCamera(true) and a later resize can both recompute the projection without
        // needing to re-walk doc_ (doc_ itself stays valid for as long as this App does, unlike
        // Universe's Renderer, which only sees a LoadAsset-scoped GltfDocument reference).
        bool                          hasAssetCamera_ = false;
        bool                          useAssetCamera_ = false;
        Phantom::Gltf::GltfCameraInstance assetCameraInst_;

        // (Re-)pushes assetCameraInst_'s view/proj/eye into renderer_ at the current viewport
        // aspect. No-op if hasAssetCamera_ is false.
        void pushAssetCameraOverride();
        // Re-derives hasAssetCamera_/assetCameraInst_ from doc_ and resets useAssetCamera_ to
        // false. Called from onInit() (startup document) and loadFile() (hot-reload) alike.
        void captureAssetCamera();

        void applyShaders();
        void setupCallbacks();
        // Main menu bar (File/View) drawn on top of VkAppBase::onImGui()'s sub-renderer/panel
        // pass. Kept separate from onImGui() itself only to keep that override short.
        void drawMainMenuBar();
        bool loadFile(const std::filesystem::path& path);
        // Points the orbit camera at doc_'s AABB center and sets a distance proportional to its
        // radius, so real-world-scale small assets (e.g. Corset/Avocado, ~0.1-0.2m) aren't left
        // as an invisible speck at the fixed default camDist=3.0 calibrated for ~1m props. No-op
        // if doc_ has no POSITION-bearing primitive (see GltfAabb::valid).
        void frameCameraToDocument();

        // Shared by the constructor (Vulkan not yet initialized) and loadFile() (hot-reload):
        // dispatches on path's extension -- ".vrm" goes through VrmReader, everything else
        // through the existing loadAsGltfDocument() free function (.obj/.stl/plain glTF).
        // Returns false (outDoc/outVrm left untouched) on any load failure.
        static bool loadDocumentForPath(const std::filesystem::path& path,
                                         GltfDocument& outDoc, VrmViewState& outVrm);

        // Computes bind-pose (no animation) joint matrices for doc_.skins[0] and pushes them to
        // renderer_ -- without this, GltfSceneRenderer defaults every joint to identity, which
        // renders any multi-joint skinned mesh (VRM avatars always are) fragmented/disjointed.
        // Safe to call before renderer_.onInit() (updateSkinMatrices() is a trivial setter).
        // No-op if doc_ has no skins. See internal design notes §6.2.
        void applySkinBindPose();

        // Recomputes and re-uploads every morph-targeted primitive's blended positions from
        // vrm_.expressions x vrm_.expressionWeights. Called from setVrmExpressionWeight(), not
        // every frame -- CPU blend + full vertex buffer re-upload is too costly to do
        // unconditionally (see GltfMorphApply.h / GltfSceneRenderer::updateMorphedPositions()).
        void applyVrmExpressionWeights();
    };

}
