#pragma once

#include "../../VkAppBase/VkAppBase.h"
#include "../../VkAppBase/ScenarioRunner/ScenarioRunner.h"
#include "../../VkAppBase/ScenarioRunner/IScenarioHost.h"
#include "../../VkAppBase/ScenarioRunner/ScenarioBrowserPanel.h"
#include "SparseVolumeRenderer.h"
#include "DenseVolumeRenderer.h"
#include "VectorFieldRenderer.h"
#include "MeshOverlayRenderer.h"
#include "MenuPanel.h"
#include "SceneListPanel.h"
#include "World.h"
#include "CommandDispatcher.h"
#include "../VolumeRenderer/PBVRRenderer.h"
#include "../../../CGLib/VulkanGraphics/VulkanSPVResolver.h"

#include <string>

namespace VkVolumeView {

class VolumeViewApp : public ::VKG::VkAppBase, public ::IScenarioHost {
public:
    VolumeViewApp();

    bool loadScenario(const std::string& jsonPath) override;
    void setExitOnScenarioComplete(bool v) override { exitOnComplete_ = v; }
    int  getExitCode() const               { return exitCode_; }

    // IScenarioHost (drives ScenarioBrowserPanel)
    bool   isScenarioActive()   const override { return runner_.isActive();   }
    bool   scenarioHasFailed()  const override { return runner_.hasFailed();  }
    const std::string& scenarioFailMessage() const override { return runner_.failMessage(); }
    size_t scenarioStepCount()  const override { return runner_.stepCount();  }

protected:
    void onInit()                        override;
    void onSwapChainCreated()            override;
    void onUpdate(uint32_t frameIndex)   override;
    void onPreRender(VkCommandBuffer cmd, uint32_t frameIndex) override;
    void onImGui()                       override;
    void onCleanup()                     override;

private:
    World world_;
    int         activeSceneId_ = -1;
    int         activeDenseSceneId_ = -1;

    SparseVolumeRenderer    pointRenderer_;
    DenseVolumeRenderer     denseRenderer_;
    VectorFieldRenderer     lineRenderer_;
    Phantom::Volume::PBVRRenderer      pbvrRenderer_;
    MeshOverlayRenderer     meshRenderer_;
    SceneListPanel      sceneListPanel_;
    MenuPanel           menuPanel_;
    CommandDispatcher dispatcher_;
    ScenarioRunner            runner_;
    ScenarioBrowserPanel      scenarioBrowser_;

    bool exitOnComplete_ = true;
    int  exitCode_       = 0;

    void notifyAll();
    void setupCallbacks();
};

} // namespace VkVolumeView
