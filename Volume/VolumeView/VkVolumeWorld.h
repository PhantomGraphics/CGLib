#pragma once

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "SparseVolumeScene.h"

#include <memory>
#include <vector>

namespace VkVolumeView {

class VkVolumeWorld {
public:
    void add(std::unique_ptr<Phantom::Volume::SparseVolumef> sv);
    const std::vector<Phantom::Volume::SparseVolumeScene*>& getScenes() const { return sceneViews_; }
    void clear();

private:
    std::vector<std::unique_ptr<Phantom::Volume::SparseVolumeScene>> scenes_;
    std::vector<Phantom::Volume::SparseVolumeScene*> sceneViews_;
};

} // namespace VkVolumeView
