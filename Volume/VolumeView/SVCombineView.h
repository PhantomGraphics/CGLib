#pragma once

#include "IVolumeProcessView.h"

#include <functional>
#include <string>

namespace VkVolumeView {

// CSG Union / Intersection / Difference of two SDF volumes.
class SVCombineView : public IVolumeProcessView {
public:
    const char* getName() const override { return "CSG Combine"; }
    void onImGui(World& world, int activeSceneId,
                 const std::function<void()>& onRebuild) override;

private:
    enum class CsgOp { Union = 0, Intersection = 1, Difference = 2 };

    CsgOp op_   = CsgOp::Union;
    int   idxA_ = 0;
    int   idxB_ = 1;
    int   count_ = 0;
    std::string statusMsg_;
};

} // namespace VkVolumeView
