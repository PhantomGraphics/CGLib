#pragma once

#include "../../../CGLib/VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
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

private:
    std::string route(const std::string& cmd);

    World*           world_     = nullptr;
    SpaceMenuPanel*  menuPanel_ = nullptr;
    Renderer* renderer_  = nullptr;

    std::mutex              mutex_;
    std::queue<std::string> inputQueue_;
    std::queue<std::string> outputQueue_;
};

} // namespace VKSpace
