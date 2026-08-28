#pragma once

#include "IVolumeProcessView.h"

namespace VkVolumeView {

class SVBoxView : public IVolumeProcessView {
public:
    const char* getName() const override { return "Create Box SDF"; }
    void onImGui(World& world, int activeSceneId,
                 const std::function<void()>& onRebuild) override;

private:
    float min_[3]  = {-10.f, -10.f, -10.f};
    float max_[3]  = { 10.f,  10.f,  10.f};
    float cell_    = 1.0f;
    int   count_   = 0;
    std::string statusMsg_;
};

} // namespace VkVolumeView
