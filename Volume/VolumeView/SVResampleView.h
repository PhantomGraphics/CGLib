#pragma once

#include "IVolumeProcessView.h"

#include <functional>
#include <string>

namespace VkVolumeView {

// Resample a SparseVolume to a different cell size using trilinear interpolation.
class SVResampleView : public IVolumeProcessView {
public:
    const char* getName() const override { return "Resample"; }
    void onImGui(World& world, int activeSceneId,
                 const std::function<void()>& onRebuild) override;

private:
    float newCellSize_ = 1.0f;
    int   count_ = 0;
    std::string statusMsg_;
};

} // namespace VkVolumeView
