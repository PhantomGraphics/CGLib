//
// VkSpaceView - Vulkan-based spatial algorithm viewer
//
// Controls:
//   Left drag  : rotate camera
//   Scroll     : zoom
//
// Scenario mode:
//   VkSpaceView.exe --run-scenario <path.json>
//   Exit code: 0=passed / 1=failed
//

#include "VkSpaceApp.h"

#include <cstdio>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    std::string scenarioPath;
    bool        noExitOnComplete = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--run-scenario" && i + 1 < argc) {
            scenarioPath = argv[++i];
        } else if (arg == "--no-exit-on-complete") {
            noExitOnComplete = true;
        }
    }

    VKSpace::VkSpaceApp app(1280, 720, "Vk Space View");

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
