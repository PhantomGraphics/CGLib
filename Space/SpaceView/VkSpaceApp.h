#pragma once

#include "../../CGLib/VkAppBase/VkAppBase.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/IScenarioHost.h"
#include "../../CGLib/VkAppBase/ScenarioRunner/ScenarioBrowserPanel.h"
#include "../../../CGLib/VulkanGraphics/VulkanSPVResolver.h"
#include "Renderer.h"
#include "CommandDispatcher.h"
#include "World.h"
#include "SpaceMenuPanel.h"

#include <string>

namespace VKSpace {

class VkSpaceApp : public ::VKG::VkAppBase, public ::IScenarioHost {
public:
    VkSpaceApp(int w, int h, const std::string& title);

    World&           getWorld()    { return world_; }
    Renderer& getRenderer() { return renderer_; }

    bool loadScenario(const std::string& jsonPath) override;
    void setExitOnScenarioComplete(bool v) override { exitOnComplete_ = v; }
    int  getExitCode() const               { return exitCode_; }

    // IScenarioHost (drives ScenarioBrowserPanel)
    bool   isScenarioActive()   const override { return runner_.isActive();   }
    bool   scenarioHasFailed()  const override { return runner_.hasFailed();  }
    const std::string& scenarioFailMessage() const override { return runner_.failMessage(); }
    size_t scenarioStepCount()  const override { return runner_.stepCount();  }

protected:
    void onInit()             override;
    void onSwapChainCreated() override;
    void onUpdate(uint32_t frameIndex) override;
    void onImGui()            override;
    void onCleanup()          override;

private:
    World                    world_;
    Renderer          renderer_;
    SpaceMenuPanel           menuPanel_;
    CommandDispatcher dispatcher_;
    ScenarioRunner           runner_;
    ScenarioBrowserPanel     scenarioBrowser_;

    bool exitOnComplete_ = true;
    int  exitCode_       = 0;

    void setupWindowCallbacks();
};

} // namespace VKSpace
