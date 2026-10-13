#pragma once

#include "../../CGLib/VkAppBase/VkAppBase.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/IScenarioHost.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioBrowserPanel.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ViewShell.h"
#include "CGLib/VulkanGraphics/VulkanSPVResolver.h"
#include "Renderer.h"
#include "CommandDispatcher.h"
#include "MeshWorld.h"
#include "MeshMenuPanel.h"

#include <string>

namespace MeshView {

class VkMeshEditApp : public ::VKG::VkAppBase, public ::IScenarioHost {
public:
    VkMeshEditApp(int w, int h, const std::string& title);

    MeshWorld& getWorld() { return world_; }
    Renderer& getRenderer() { return renderer_; }

    bool loadScenario(const std::string& jsonPath) override;
    void setExitOnScenarioComplete(bool v) override { exitOnComplete_ = v; }
    int getExitCode() const { return exitCode_; }

    // IScenarioHost (drives ScenarioBrowserPanel)
    bool isScenarioActive() const override { return runner_.isActive(); }
    bool scenarioHasFailed() const override { return runner_.hasFailed(); }
    const std::string& scenarioFailMessage() const override { return runner_.failMessage(); }
    size_t scenarioStepCount() const override { return runner_.stepCount(); }

protected:
    void onInit() override;
    void onSwapChainCreated() override;
    void onUpdate(uint32_t frameIndex) override;
    void onImGui() override;
    void onImGuiReady() override;
    void onCleanup() override;

private:
    MeshWorld world_;
    Renderer renderer_;
    MeshMenuPanel menuPanel_;
    ViewShell shell_;   // Command / Outliner windows
    CommandDispatcher dispatcher_;
    ScenarioRunner runner_;
    ScenarioBrowserPanel scenarioBrowser_;

    bool exitOnComplete_ = true;
    int exitCode_ = 0;

    // A left click that did not turn into a drag selects the element under the cursor (edit mode).
    double lastX_ = 0., lastY_ = 0., pressX_ = 0., pressY_ = 0.;
    bool pressed_ = false;

    void setupWindowCallbacks();
};

}  // namespace MeshView
