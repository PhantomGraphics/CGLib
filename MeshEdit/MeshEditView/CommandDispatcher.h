#pragma once

#include "CGLib/VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
#include "CGLib/VkAppBase/ScenarioRunner/UiCommand.h"
#include "CGLib/VkAppBase/ScenarioRunner/CommandQueue.h"
#include "MeshWorld.h"

#include <functional>
#include <string>
#include <vector>

namespace Phantom::MeshView {

class Renderer;

// What the viewer's commands need from the window side: the camera and picking view. Kept abstract so the command router can
// be tested (MeshEditTest) with a stand-in and no Vulkan.
struct CameraAccess {
    std::function<void()> reset;
    std::function<void(float theta, float phi, float dist)> set;
    std::function<float()> distance;
    std::function<void(double dy)> scroll;
    std::function<glm::vec3()> eye;
    std::function<Phantom::MeshEdit::MeshPickView()> pickView;
};

// Commands are `Verb` or `Verb:first-argument [key=value ...]`, e.g. `Extrude:distance=0.5`, `Select:0,1,4-6 mode=add`.
// route() is the whole command language; the queue only decides when it runs (the render thread, once per frame).
class CommandDispatcher : public IScenarioDispatcher {
public:
    void setWorld(MeshWorld* w) { world_ = w; }
    void setCamera(CameraAccess camera) { camera_ = std::move(camera); }

    void processQueue();
    const std::string& lastUiError() const { return lastUiError_; }

    void dispatch(const std::string& command) override;
    std::vector<std::string> collectResponses() override;
    std::vector<CommandInfo> commandCatalog() const override;

    // GUI operations take the same queue and handlers as typed commands; the response is discarded (see UiCommand.h).
    void submitUi(const std::string& cmd) { dispatch(markUiCommand(cmd)); }

    // Runs one command immediately (tests, and processQueue()).
    std::string route(const std::string& cmd);

private:
    std::string cmdCheckCommandCatalog();

    MeshWorld* world_ = nullptr;
    CameraAccess camera_;
    CommandQueue queue_;
    std::string lastUiError_;
};

}  // namespace MeshView
