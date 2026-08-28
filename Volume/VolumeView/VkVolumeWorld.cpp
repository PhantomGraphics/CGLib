#include "VkVolumeWorld.h"

namespace VkVolumeView {

void VkVolumeWorld::add(std::unique_ptr<Phantom::Volume::SparseVolumef> sv) {
    auto scene = std::make_unique<Phantom::Volume::SparseVolumeScene>();
    scene->setShape(std::move(sv));

    sceneViews_.push_back(scene.get());
    scenes_.push_back(std::move(scene));
}

void VkVolumeWorld::clear() {
    scenes_.clear();
    sceneViews_.clear();
}

} // namespace VkVolumeView
