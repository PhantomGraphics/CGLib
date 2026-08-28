// AnimationView - CPU-skinned skeletal animation viewer
//
// Controls:
//   Left drag : rotate camera
//   Scroll    : zoom
//
// Scenario mode:
//   AnimationView.exe --run-scenario <path.json>
//   Exit code: 0=passed / 1=failed

#include "AnimationViewApp.h"

#include <iostream>
#include <string>
#include <string_view>

using namespace Phantom::Animation;

int main(int argc, char* argv[])
{
    std::string scenarioPath;
    bool        noExitOnComplete = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if ((arg == "--run-scenario" || arg == "--scenario") && i + 1 < argc) {
            scenarioPath = argv[++i];
        } else if (arg == "--no-exit-on-complete") {
            noExitOnComplete = true;
        }
    }

    AnimationViewApp app(1280, 720, "Animation View");

    if (!scenarioPath.empty()) {
        if (!app.loadScenario(scenarioPath)) {
            std::fprintf(stderr, "[Scenario] Failed to load: %s\n", scenarioPath.c_str());
            return 1;
        }
        app.setExitOnScenarioComplete(!noExitOnComplete);
    }

    app.run(argc, argv);
    return app.getExitCode();
}
