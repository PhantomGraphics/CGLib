#pragma once

#include "CGLib/VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
#include "World.h"

#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace Phantom::Animation {

class AnimationViewApp;

class CommandDispatcher : public IScenarioDispatcher {
public:
    void setWorld(World* w) { world_ = w; }
    // Non-owning; only needed for the LoadEnvironmentHDR/ClearEnvironmentHDR/GetHasEnvironmentHDR/
    // SetUseIBL/GetUseIBL commands (real-HDRI loading lives on the app, alongside envCubemap_ --
    // see AnimationViewApp::loadEnvironmentHDR()'s comment).
    void setApp(AnimationViewApp* app) { app_ = app; }

    void processQueue();

    void dispatch(const std::string& command) override;
    std::vector<std::string> collectResponses() override;

private:
    std::string route(const std::string& cmd);

    World* world_ = nullptr;
    AnimationViewApp* app_ = nullptr;

    std::mutex              mutex_;
    std::queue<std::string> inputQueue_;
    std::queue<std::string> outputQueue_;
};

} // namespace Phantom::Animation
