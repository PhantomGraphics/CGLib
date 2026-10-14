#include "RendererApp.h"

#include "../../../CGLib/VulkanGraphics/SpvResolver.h"

#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace Phantom::VKRenderer {

RendererApp::RendererApp(int w, int h, const std::string& title)
    : ::Phantom::VKG::VkAppBase(w, h, title) {
    add(&renderer_);
}

void RendererApp::onInit() {
    static constexpr auto kSh = "shaders/";
    VkRendererSubRenderer::Shaders s;
    s.pointVert  = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "point.vert.spv");
    s.pointFrag  = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "point.frag.spv");
    s.lineVert   = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "line.vert.spv");
    s.lineFrag   = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "line.frag.spv");
    s.triVert    = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "triangle.vert.spv");
    s.triFrag    = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "triangle.frag.spv");
    s.texVert    = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "tex.vert.spv");
    s.texFrag    = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "tex.frag.spv");
    s.skyboxVert = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "skybox.vert.spv");
    s.skyboxFrag = ::Phantom::VKG::loadSPVRepo(std::string(kSh) + "skybox.frag.spv");
    renderer_.setShaders(std::move(s));

    ::Phantom::VKG::VkAppBase::onInit();
    renderer_.setExtent(getExtent());
    renderer_.setCamera(computeProjMatrix(), computeViewMatrix());
    setupWindowCallbacks();
}

void RendererApp::onUpdate(uint32_t frameIndex) {
    renderer_.setCamera(computeProjMatrix(), computeViewMatrix());
    ::Phantom::VKG::VkAppBase::onUpdate(frameIndex);
}

void RendererApp::onSwapChainCreated() {
    renderer_.setExtent(getExtent());
}

void RendererApp::onImGui() {
    ::Phantom::VKG::VkAppBase::onImGui();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Renderer")) {
            auto mode = renderer_.getMode();

            if (ImGui::MenuItem("Point", nullptr, mode == VkRendererSubRenderer::Mode::Point))
                renderer_.setMode(VkRendererSubRenderer::Mode::Point);
            if (ImGui::MenuItem("Line", nullptr, mode == VkRendererSubRenderer::Mode::Line))
                renderer_.setMode(VkRendererSubRenderer::Mode::Line);
            if (ImGui::MenuItem("Triangle", nullptr, mode == VkRendererSubRenderer::Mode::Triangle))
                renderer_.setMode(VkRendererSubRenderer::Mode::Triangle);
            if (ImGui::MenuItem("Tex", nullptr, mode == VkRendererSubRenderer::Mode::Tex))
                renderer_.setMode(VkRendererSubRenderer::Mode::Tex);
            if (ImGui::MenuItem("SkyBox", nullptr, mode == VkRendererSubRenderer::Mode::SkyBox))
                renderer_.setMode(VkRendererSubRenderer::Mode::SkyBox);

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void RendererApp::onCleanup() {
    ::Phantom::VKG::VkAppBase::onCleanup();
}

void RendererApp::setupWindowCallbacks() {
    auto& win = getWindow();

    win.onMouseButton = [this](int button, int action, int) {
        if (button == 0) {
            dragging_ = (action == 1);
        }
    };

    win.onCursorPos = [this](double x, double y) {
        if (dragging_) {
            const float dx = static_cast<float>(x - prevMouseX_);
            const float dy = static_cast<float>(y - prevMouseY_);
            azimuth_ += dx * 0.4f;
            elevation_ = std::clamp(elevation_ + dy * 0.3f, -85.0f, 85.0f);
        }
        prevMouseX_ = x;
        prevMouseY_ = y;
    };

    win.onScroll = [this](double, double dy) {
        distance_ = std::clamp(distance_ - static_cast<float>(dy) * 0.25f, 0.5f, 30.0f);
    };
}

glm::mat4 RendererApp::computeViewMatrix() const {
    const float az = glm::radians(azimuth_);
    const float el = glm::radians(elevation_);

    const glm::vec3 target(0.0f, 0.0f, 0.0f);
    const glm::vec3 eye(
        distance_ * std::cos(el) * std::sin(az),
        distance_ * std::sin(el),
        distance_ * std::cos(el) * std::cos(az));

    return glm::lookAt(eye, target, glm::vec3(0.f, 1.f, 0.f));
}

glm::mat4 RendererApp::computeProjMatrix() const {
    const auto ext = getExtent();
    const float aspect = (ext.height > 0)
        ? static_cast<float>(ext.width) / static_cast<float>(ext.height)
        : 1.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 100.0f);
    proj[1][1] *= -1.0f;
    return proj;
}

} // namespace VKRenderer
