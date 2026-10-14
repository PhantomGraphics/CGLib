#include "MeshEditApp.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>

namespace Phantom::MeshView {

MeshEditApp::MeshEditApp(int w, int h, const std::string& title)
    : ::Phantom::VKG::VkAppBase(w, h, title)
    , renderer_(&world_)
{
    dispatcher_.setWorld(&world_);
    CameraAccess camera;
    camera.reset = [this] { renderer_.resetCamera(); };
    camera.set = [this](float t, float p, float d) { renderer_.setCamera(t, p, d); };
    camera.distance = [this] { return renderer_.getCameraDistance(); };
    camera.scroll = [this](double dy) { renderer_.handleScroll(dy); };
    camera.eye = [this] { return renderer_.eye(); };
    camera.pickView = [this] { return renderer_.pickView(); };
    dispatcher_.setCamera(std::move(camera));
    scenarioBrowser_.setHost(this);
    scenarioBrowser_.setDefaultFolder("scenarios");

    add(&renderer_);
    menuPanel_.init(&world_);

    // Standard screen: render area + menu + Command + Outliner; the rest is opened from the View menu / outliner.
    shell_.setDispatcher(&dispatcher_);
    shell_.registerPanel("Control", { 0.70f, 0.00f, 0.30f, 0.80f });
    shell_.registerPanel("Scenario Browser", { 0.30f, 0.05f, 0.40f, 0.55f });
    shell_.setOutlinerProvider([this] {
        std::vector<ViewShell::OutlinerItem> items;
        items.push_back({ 1, "Mesh: " + world_.describe(), "Control" });
        items.push_back({ 2, std::string("Mode: ") + (world_.editMode() ? "edit" : "object") + ", " +
                             Phantom::MeshEdit::elementKindName(world_.edit().kind) + " x" + std::to_string(world_.edit().ids.size()), "Control" });
        bool closed = false;
        const double volume = world_.signedVolume(&closed);
        items.push_back({ 3, closed ? std::string(volume >= 0 ? "Winding: outward (closed)" : "Winding: INSIDE OUT (closed)")
                                    : std::string("Winding: open surface"), "" });
        return items;
    });
    menuPanel_.setShell(&shell_);
    menuPanel_.setSubmit([this](const std::string& c) { dispatcher_.submitUi(c); });
}

void MeshEditApp::onImGuiReady() {
    // Context exists, imgui.ini is not read until the first frame.
    shell_.installSettings();
}

bool MeshEditApp::loadScenario(const std::string& jsonPath) { return runner_.load(jsonPath); }

void MeshEditApp::onInit() {
    {
        static constexpr auto kSS = "shaders/";
        Renderer::Shaders s;
        s.lineVert = ::Phantom::VKG::loadSPVRepo(std::string(kSS) + "line.vert.spv");
        s.lineFrag = ::Phantom::VKG::loadSPVRepo(std::string(kSS) + "line.frag.spv");
        s.pointVert = ::Phantom::VKG::loadSPVRepo(std::string(kSS) + "point.vert.spv");
        s.pointFrag = ::Phantom::VKG::loadSPVRepo(std::string(kSS) + "point.frag.spv");
        s.triVert = ::Phantom::VKG::loadSPVRepo(std::string(kSS) + "triangle.vert.spv");
        s.triFrag = ::Phantom::VKG::loadSPVRepo(std::string(kSS) + "triangle.frag.spv");
        renderer_.setShaders(std::move(s));
    }
    ::Phantom::VKG::VkAppBase::onInit();
    renderer_.setExtent(getExtent());
    setupWindowCallbacks();
}

void MeshEditApp::onSwapChainCreated() { renderer_.setExtent(getExtent()); }

void MeshEditApp::onUpdate(uint32_t frameIndex) {
    dispatcher_.processQueue();

    // Single place that collects responses: first the ones for commands typed into the Command window, the rest belong
    // to the running scenario.
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
    }

    ::Phantom::VKG::VkAppBase::onUpdate(frameIndex);
}

void MeshEditApp::onImGui() {
    ::Phantom::VKG::VkAppBase::onImGui();

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

void MeshEditApp::onCleanup() { ::Phantom::VKG::VkAppBase::onCleanup(); }

void MeshEditApp::setupWindowCallbacks() {
    auto& win = getWindow();
    // Camera and picking input is ignored while ImGui owns the mouse and while a scenario runs; a release is always
    // forwarded so a drag can end.
    win.onMouseButton = [this](int button, int action, int) {
        if (button != 0) return;
        if (action == 1 && (ImGui::GetIO().WantCaptureMouse || runner_.isActive())) return;
        renderer_.handleMouseButton(action == 1);
        if (action == 1) {
            pressed_ = true;
            pressX_ = lastX_;
            pressY_ = lastY_;
        } else if (pressed_) {
            pressed_ = false;
            const double moved = std::hypot(lastX_ - pressX_, lastY_ - pressY_);
            if (moved < 4.0 && world_.editMode()) {
                const ImGuiIO& io = ImGui::GetIO();
                const char* mode = io.KeyCtrl ? "toggle" : io.KeyShift ? "add" : "replace";
                char cmd[96];
                std::snprintf(cmd, sizeof(cmd), "Pick:%.1f,%.1f mode=%s", lastX_, lastY_, mode);
                dispatcher_.submitUi(cmd);
            }
        }
    };
    win.onCursorPos = [this](double x, double y) {
        lastX_ = x;
        lastY_ = y;
        renderer_.handleMouseMove(x, y);
    };
    win.onScroll = [this](double, double dy) {
        if (ImGui::GetIO().WantCaptureMouse || runner_.isActive()) return;
        renderer_.handleScroll(dy);
    };
}

}  // namespace MeshView
