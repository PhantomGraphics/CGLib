#pragma once

#include "CGLib/VkAppBase/VkAppBase.h"
#include "CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "CGLib/VkAppBase/ScenarioRunner/IScenarioHost.h"
#include "CGLib/VkAppBase/ScenarioRunner/ScenarioBrowserPanel.h"
#include "CGLib/VulkanGraphics/VulkanSPVResolver.h"

#include "CGLib/GltfRenderer/Renderer/GltfSceneRenderer.h"
#include "CGLib/GltfRenderer/IBL/GltfEnvironmentCubemap.h"

#include "World.h"
#include "AnimationPanel.h"
#include "CommandDispatcher.h"

#include <chrono>
#include <string>

namespace Phantom::Animation {

// PMX(+VMD) skeletal animation viewer. Rendering is delegated entirely to
// Phantom::Gltf::GltfSceneRenderer (the same renderer every other glTF-consuming app in this
// repo uses) -- there is no MMD-specific rendering code left here (see
// internal design notes Phase 8: MmdMeshRenderer/GpuSkinnedRenderer/
// BoneWireRenderer/SkinnedMeshRenderer were all removed from this app; the underlying
// Phantom::Animation::AnimationRenderer library they lived in has no other consumer left).
class AnimationViewApp : public ::VKG::VkAppBase, public ::IScenarioHost {
public:
    AnimationViewApp(int w, int h, const std::string& title);

    bool loadScenario(const std::string& jsonPath) override;
    void setExitOnScenarioComplete(bool v) override { exitOnComplete_ = v; }
    int  getExitCode() const               { return exitCode_; }

    // IScenarioHost (drives ScenarioBrowserPanel)
    bool   isScenarioActive()   const override { return runner_.isActive();   }
    bool   scenarioHasFailed()  const override { return runner_.hasFailed();  }
    const std::string& scenarioFailMessage() const override { return runner_.failMessage(); }
    size_t scenarioStepCount()  const override { return runner_.stepCount();  }

    // Real HDRI loading (2026-09-15): AnimationView was originally "a plain model/motion viewer,
    // not a lighting testbed" (see onInit()'s comment) and left IBL/shadow shaders unloaded. It
    // now optionally supports the same real-environment IBL every other glTF-consuming app does,
    // off by default, via the shared Phantom::Gltf::GltfEnvironmentCubemap (see that class's
    // header comment). Mirrors GltfViewer::App's methods of the same name.
    void setUseIBL(bool v) { sceneRenderer_.setUseIBL(v); }
    bool getUseIBL() const { return sceneRenderer_.getUseIBL(); }
    bool loadEnvironmentHDR(const std::string& path);
    void clearEnvironmentHDR();
    bool hasEnvironmentHDR() const { return envCubemap_.isRealHDR(); }

protected:
    void onInit()                          override;
    void onSwapChainCreated()              override;
    void onUpdate(uint32_t frameIndex)     override;
    void onImGui()                         override;
    void onCleanup()                       override;

private:
    World                    world_;
    Phantom::Gltf::GltfSceneRenderer  sceneRenderer_;
    AnimationPanel                    panel_;
    CommandDispatcher        dispatcher_;
    ScenarioRunner                    runner_;
    ScenarioBrowserPanel              scenarioBrowser_;

    // Environment cubemap: flat placeholder sky tint by default, or a real equirectangular .hdr
    // panorama once loaded. Shared with GltfViewer/Universe (2026-09-15, GltfEnvironmentCubemap.h's
    // comment) rather than a duplicate of the same boilerplate.
    Phantom::Gltf::GltfEnvironmentCubemap envCubemap_;
    // shaders/equirect_to_cube.{vert,frag}, loaded once in onInit() and copied into each
    // loadEnvironmentHDR() call (GltfIBLPrecomputer::computeEnvironmentCube() consumes its shader
    // vectors by value/move).
    std::vector<uint32_t> equirectVertSpv_;
    std::vector<uint32_t> equirectFragSpv_;

    std::chrono::steady_clock::time_point lastFrameTime_;

    bool exitOnComplete_ = true;
    int  exitCode_       = 0;

    void setupWindowCallbacks();
    void tickAnimation(float dt);
    void tryLoadModel();

    // Model/motion load tracking
    std::string currentModelPath_;
    std::string currentVMDPath_;
};

} // namespace Phantom::Animation
