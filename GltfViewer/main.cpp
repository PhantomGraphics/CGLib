#include "App.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

using namespace Phantom::Gltf;

int main(int argc, char* argv[]) {
    std::filesystem::path modelPath;
    std::string           scenarioPath;
    bool                  noExitOnComplete = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--run-scenario" && i + 1 < argc) {
            scenarioPath = argv[++i];
        } else if (arg == "--no-exit-on-complete") {
            noExitOnComplete = true;
        } else if ((arg == "--screenshot" || arg == "--screenshot-frame") && i + 1 < argc) {
            // VkAppBase::parseScreenshotArgs (called from app.run() below) consumes
            // these itself -- skip their value here too, or a bare `--screenshot
            // path.png` with no model path ends up misparsed as modelPath=path.png.
            ++i;
        } else if (!arg.starts_with("--")) {
            modelPath = argv[i];
        }
    }

    App app(modelPath);

    if (!scenarioPath.empty()) {
        if (!app.loadScenario(scenarioPath)) {
            fprintf(stderr, "[Scenario] Failed to load: %s\n", scenarioPath.c_str());
            return 1;
        }
        app.setExitOnScenarioComplete(!noExitOnComplete);
    }

    app.run(argc, argv);
    return app.getExitCode();
}
