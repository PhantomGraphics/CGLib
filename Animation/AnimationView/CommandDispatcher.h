#pragma once

#include "CGLib/VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
#include "CGLib/VkAppBase/ScenarioRunner/UiCommand.h"
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
    std::vector<CommandInfo> commandCatalog() const override;

    // GUI operations take the same queue and handlers as typed commands; the
    // response is discarded (see UiCommand.h).
    void submitUi(const std::string& cmd) { dispatch(markUiCommand(cmd)); }

private:
    std::string route(const std::string& cmd);
    std::string cmdCheckCommandCatalog();

    World* world_ = nullptr;
    AnimationViewApp* app_ = nullptr;

    std::mutex              mutex_;
    std::queue<std::string> inputQueue_;
    std::queue<std::string> outputQueue_;
};

} // namespace Phantom::Animation
