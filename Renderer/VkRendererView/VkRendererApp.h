#pragma once

#include "../../CGLib/VkAppBase/VkAppBase.h"
#include "VkRendererSubRenderer.h"

#include <glm/glm.hpp>

#include <string>

namespace VKRenderer {

class VkRendererApp : public ::VKG::VkAppBase {
public:
    VkRendererApp(int w, int h, const std::string& title);

protected:
    void onInit() override;
    void onUpdate(uint32_t frameIndex) override;
    void onSwapChainCreated() override;
    void onImGui() override;
    void onCleanup() override;

private:
    VkRendererSubRenderer renderer_;

    float azimuth_ = 0.0f;
    float elevation_ = 20.0f;
    float distance_ = 3.0f;
    double prevMouseX_ = 0.0;
    double prevMouseY_ = 0.0;
    bool dragging_ = false;

    void setupWindowCallbacks();
    glm::mat4 computeViewMatrix() const;
    glm::mat4 computeProjMatrix() const;
};

} // namespace VKRenderer
