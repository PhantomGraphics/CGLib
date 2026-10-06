#include "VkSpaceApp.h"

#include "imgui.h"

#include <cstdio>
#include <stdexcept>

namespace VKSpace {

// ============================================================
//  Construction
// ============================================================

VkSpaceApp::VkSpaceApp(int w, int h, const std::string& title)
    : ::VKG::VkAppBase(w, h, title)
    , renderer_(&world_)
{
    dispatcher_.setWorld(&world_);
    dispatcher_.setMenuPanel(&menuPanel_);
    dispatcher_.setRenderer(&renderer_);
    scenarioBrowser_.setHost(this);
    scenarioBrowser_.setDefaultFolder("scenarios");

    add(&renderer_);
    menuPanel_.init(&world_, &renderer_);

    // Standard screen: render area + menu + Command + Outliner; the rest is
    // opened from the View menu / outliner.
    shell_.setDispatcher(&dispatcher_);
    shell_.registerPanel("Control", {0.70f, 0.00f, 0.30f, 0.66f});
    shell_.registerPanel("Scenario Browser", {0.30f, 0.05f, 0.40f, 0.55f});
    shell_.setOutlinerProvider([this] {
        const auto& r = world_.getResult();
        std::vector<ViewShell::OutlinerItem> items;
        items.push_back({1, std::string("Algorithm: ") + menuPanel_.activeName(), "Control"});
        items.push_back({2, "Result: " + std::to_string(r.lineIndices.size() / 2) + " lines, " +
                            std::to_string(r.pointSizes.size()) + " points", ""});
        return items;
    });
    menuPanel_.setShell(&shell_);
    menuPanel_.setSubmit([this](const std::string& c) { dispatcher_.submitUi(c); });
    // Control and Scenario Browser are drawn through the shell in onImGui().
}

void VkSpaceApp::onImGuiReady() {
    // Context exists, imgui.ini is not read until the first frame.
    shell_.installSettings();
}

bool VkSpaceApp::loadScenario(const std::string& jsonPath) {
    return runner_.load(jsonPath);
}

// ============================================================
//  VkAppBase hooks
// ============================================================

void VkSpaceApp::onInit() {
    {
        static constexpr auto kSS = "shaders/";
        Renderer::Shaders s;
        s.lineVert  = ::VKG::loadSPVRepo(std::string(kSS) + "line.vert.spv");
        s.lineFrag  = ::VKG::loadSPVRepo(std::string(kSS) + "line.frag.spv");
        s.pointVert = ::VKG::loadSPVRepo(std::string(kSS) + "point.vert.spv");
        s.pointFrag = ::VKG::loadSPVRepo(std::string(kSS) + "point.frag.spv");
        renderer_.setShaders(std::move(s));
    }
    ::VKG::VkAppBase::onInit();
    renderer_.setExtent(getExtent());
    setupWindowCallbacks();
}

void VkSpaceApp::onSwapChainCreated() {
    renderer_.setExtent(getExtent());
}

void VkSpaceApp::onUpdate(uint32_t frameIndex) {
    dispatcher_.processQueue();

    // Single place that collects responses: first the ones for commands typed
    // into the Command window, the rest belong to the running scenario.
    auto responses = dispatcher_.collectResponses();
    shell_.consumeResponses(responses);
    shell_.setScenarioActive(runner_.isActive());
    menuPanel_.setLocked(runner_.isActive());

    if (runner_.isActive()) {
        if (runner_.tick(shell_.scenarioDispatcher(), responses)) {
            if (runner_.hasFailed()) {
                fprintf(stderr, "[Scenario] FAILED: %s\n", runner_.failMessage().c_str());
                exitCode_ = 1;
            } else {
                fprintf(stdout, "[Scenario] PASSED (%zu steps)\n", runner_.stepCount());
                exitCode_ = 0;
            }
            if (exitOnComplete_) getWindow().close();
        }
    } else {
    }

    ::VKG::VkAppBase::onUpdate(frameIndex);
}

void VkSpaceApp::onImGui() {
    ::VKG::VkAppBase::onImGui();

    if (ImGui::BeginMainMenuBar()) {
        menuPanel_.onImGuiMenuBar();
        shell_.drawViewMenu();
        ImGui::EndMainMenuBar();
    }
    shell_.drawWindows();
    menuPanel_.onImGui();
    scenarioBrowser_.pumpQueue();
    if (shell_.beginPanel("Scenario Browser")) {
        scenarioBrowser_.drawEmbedded();
        shell_.endPanel();
    }
}

void VkSpaceApp::onCleanup() {
    ::VKG::VkAppBase::onCleanup();
}

// ============================================================
//  Private
// ============================================================

void VkSpaceApp::setupWindowCallbacks() {
    auto& win = getWindow();
    // Camera input is ignored while ImGui owns the mouse and while a scenario
    // runs; a release is always forwarded so a drag can end.
    win.onMouseButton = [this](int button, int action, int) {
        if (button != 0) return;
        if (action == 1 && (ImGui::GetIO().WantCaptureMouse || runner_.isActive())) return;
        renderer_.handleMouseButton(action == 1);
    };
    win.onCursorPos = [this](double x, double y) {
        renderer_.handleMouseMove(x, y);
    };
    win.onScroll = [this](double, double dy) {
        if (ImGui::GetIO().WantCaptureMouse || runner_.isActive()) return;
        renderer_.handleScroll(dy);
    };
}

} // namespace VKSpace
