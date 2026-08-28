#pragma once

#include "IVolumeProcessView.h"

#include <string>

namespace VkVolumeView {

// Runs Marching Cubes on the active scene's SparseVolumef and adds the
// resulting mesh as a PolygonMesh overlay in VolumeWorld.
class MCMeshView : public IVolumeProcessView {
public:
    const char* getName() const override { return "Marching Cubes Mesh"; }
    void onImGui(World& world, int activeSceneId,
                 const std::function<void()>& onRebuild) override;

private:
    float isoLevel_ = 0.0f;
    std::string statusMsg_;
};

} // namespace VkVolumeView
