#pragma once

#include "IVolumeProcessView.h"

#include <string>

namespace VkVolumeView {

class SVSphereView : public IVolumeProcessView {
public:
    const char* getName() const override { return "Create Sphere SDF"; }
    void onImGui(World& world, int activeSceneId,
                 const std::function<void()>& onRebuild) override;

private:
    float center_[3] = {0.f, 0.f, 0.f};
    float radius_    = 10.f;
    float cell_      = 1.0f;
    int   count_     = 0;
    std::string statusMsg_;
};

} // namespace VkVolumeView
