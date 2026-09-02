#pragma once

#include "CGLib/VkAppBase/VkAppBase.h"
#include "CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "CGLib/VkAppBase/ScenarioRunner/IScenarioHost.h"
#include "CGLib/VkAppBase/ScenarioRunner/ScenarioBrowserPanel.h"
#include "CGLib/VulkanGraphics/VulkanSPVResolver.h"

#include "CGLib/GltfRenderer/Renderer/GltfSceneRenderer.h"

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
