#pragma once

#include "IVolumeProcessView.h"

#include <functional>
#include <string>

namespace VkVolumeView {

// Generate a signed-distance field from an STL triangle mesh.
class SVMeshView : public IVolumeProcessView {
public:
    const char* getName() const override { return "Mesh to SDF"; }
    void onImGui(World& world, int activeSceneId,
                 const std::function<void()>& onRebuild) override;

private:
    char  pathBuf_[512] = {};
    float cellSize_ = 1.0f;
    int   count_    = 0;
    std::string statusMsg_;
};

} // namespace VkVolumeView
