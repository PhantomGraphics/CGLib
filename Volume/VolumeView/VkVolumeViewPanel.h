#pragma once

#include "../../VkAppBase/IVkSubRenderer.h"
#include "VolumeWorld.h"

#include <functional>

namespace VkVolumeView {

class VkVolumeViewPanel : public ::VKG::IVkUIPanel {
public:
    void setWorld(VolumeWorld* world) { world_ = world; }
    void setOnChange(std::function<void()> cb) { onChange_ = std::move(cb); }

    void onImGui() override;

private:
    VolumeWorld* world_ = nullptr;
    std::function<void()> onChange_;

    float minCorner_[3] = {-10.0f, -10.0f, -10.0f};
    float maxCorner_[3] = {10.0f, 10.0f, 10.0f};
    float cellLength_ = 1.0f;
    int   sceneCount_ = 0;
};

} // namespace VkVolumeView
