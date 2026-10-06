#pragma once

#include "../../../CGLib/VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
#include "../../../CGLib/VkAppBase/ScenarioRunner/UiCommand.h"
#include "../../../CGLib/VkAppBase/ScenarioRunner/CommandQueue.h"
#include "SpaceMenuPanel.h"
#include "Renderer.h"
#include "World.h"

#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace VKSpace {

class CommandDispatcher : public IScenarioDispatcher {
public:
    void setWorld(World* w)          { world_     = w; }
    void setMenuPanel(SpaceMenuPanel* m) { menuPanel_ = m; }
    void setRenderer(Renderer* r) { renderer_  = r; }

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

    World*           world_     = nullptr;
    SpaceMenuPanel*  menuPanel_ = nullptr;
    Renderer* renderer_  = nullptr;

    CommandQueue queue_;
};

} // namespace VKSpace
