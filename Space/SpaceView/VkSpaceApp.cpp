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
    add(&menuPanel_);
    add(&scenarioBrowser_);
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

    if (runner_.isActive()) {
        auto responses = dispatcher_.collectResponses();
        if (runner_.tick(dispatcher_, responses)) {
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
        ImGui::EndMainMenuBar();
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
    win.onMouseButton = [this](int button, int action, int) {
        if (button == 0) renderer_.handleMouseButton(action == 1);
    };
    win.onCursorPos = [this](double x, double y) {
        renderer_.handleMouseMove(x, y);
    };
    win.onScroll = [this](double, double dy) {
        renderer_.handleScroll(dy);
    };
}

} // namespace VKSpace
