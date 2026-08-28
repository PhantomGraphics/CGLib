#pragma once
#include "../../CGLib/VkAppBase/VkAppBase.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/IScenarioHost.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioBrowserPanel.h"
#include "../GltfRenderer/Gltf/GltfDocument.h"
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "CommandDispatcher.h"
#include "ControlPanel.h"
#include "VrmViewState.h"
#include <vulkan/vulkan.h>
#include <filesystem>
#include <optional>
#include <string>
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

    private:
        GltfDocument             doc_;
        VrmViewState              vrm_;
        GltfSceneRenderer        renderer_;
        ControlPanel          panel_;
        CommandDispatcher    dispatcher_;
        ScenarioRunner           runner_;
        //ScenarioConsole          console_;
        ScenarioBrowserPanel     scenarioBrowser_;

        std::optional<std::filesystem::path> pendingPath_;

        bool        screenshotPending_ = false;
        std::string screenshotPendingPath_;

        bool exitOnComplete_ = true;
        int  exitCode_ = 0;

        // Environment cubemap (solid-color procedural sky)
        VkImage        envImage_ = VK_NULL_HANDLE;
        VkDeviceMemory envMem_ = VK_NULL_HANDLE;
        VkImageView    envView_ = VK_NULL_HANDLE;
        VkSampler      envSampler_ = VK_NULL_HANDLE;

        void applyShaders();
        void createEnvCubemap();
        void destroyEnvCubemap();
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
        // No-op if doc_ has no skins. See docs/issue/CODEBASE_ISSUES.md §6.2.
        void applySkinBindPose();

        // Recomputes and re-uploads every morph-targeted primitive's blended positions from
        // vrm_.expressions x vrm_.expressionWeights. Called from setVrmExpressionWeight(), not
        // every frame -- CPU blend + full vertex buffer re-upload is too costly to do
        // unconditionally (see GltfMorphApply.h / GltfSceneRenderer::updateMorphedPositions()).
        void applyVrmExpressionWeights();
    };

}