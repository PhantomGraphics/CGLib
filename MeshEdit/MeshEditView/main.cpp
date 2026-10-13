//
// MeshEditView - Vulkan-based viewer for CGLib's MeshEdit module (Phantom::MeshEdit)
//
// Controls:
//   Left drag           : rotate camera
//   Scroll              : zoom
//   Click (edit mode)   : select the vertex / edge / face under the cursor (Shift: add, Ctrl: toggle)
//
// Every operation is a command (Mesh menu, Control window, Command window, scenarios all use the same path).
//
// Scenario mode:
//   MeshEditView.exe --run-scenario <path.json>
//   Exit code: 0=passed / 1=failed
//
// Screenshot (VkAppBase): MeshEditView.exe --screenshot out.png [--screenshot-frame N]
//

#include "VkMeshEditApp.h"

#include <cstdio>
#include <string>
#include <string_view>

int main(int argc, char* argv[]) {
    std::string scenarioPath;
    bool noExitOnComplete = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--run-scenario" && i + 1 < argc) {
            scenarioPath = argv[++i];
        } else if (arg == "--no-exit-on-complete") {
            noExitOnComplete = true;
        }
    }

    MeshView::VkMeshEditApp app(1280, 720, "Mesh Edit View");

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
