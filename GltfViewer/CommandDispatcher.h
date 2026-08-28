#pragma once

#include "../../CGLib/VkAppBase/ScenarioRunner/IScenarioDispatcher.h"
#include "../../CGLib/GltfRenderer/Gltf/GltfDocument.h"
#include "../GltfRenderer/Renderer/GltfSceneRenderer.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

namespace Phantom::Gltf {

    class App;

    class CommandDispatcher : public IScenarioDispatcher {
    public:
        void setDocument(const GltfDocument* doc) { doc_ = doc; }
        void setRenderer(GltfSceneRenderer* r) { renderer_ = r; }
        // Non-owning; only needed for the Get/SetVrm* commands (VRM metadata lives on the app,
        // not on GltfDocument -- see GltfVrmViewState.h). Forward-declared here to avoid a
        // circular include (GltfViewerApp.h already includes this header); route() defines its
        // body in the .cpp, which does include GltfViewerApp.h.
        void setApp(App* app) { app_ = app; }

        void processQueue();

        void dispatch(const std::string& command) override;
        std::vector<std::string> collectResponses() override;

        std::optional<std::filesystem::path> takePendingLoad();
        void signalLoaded(bool ok, const std::string& msg = {});

        std::optional<std::filesystem::path> takePendingScreenshot();
        void signalScreenshotDone(bool ok, const std::string& path);

    private:
        std::string route(const std::string& cmd);

        const GltfDocument* doc_ = nullptr;
        GltfSceneRenderer* renderer_ = nullptr;
        App* app_ = nullptr;

        std::optional<std::filesystem::path> pendingLoad_;
        std::optional<std::filesystem::path> pendingScreenshot_;

        std::mutex              mutex_;
        std::queue<std::string> inputQueue_;
        std::queue<std::string> outputQueue_;
    };

}