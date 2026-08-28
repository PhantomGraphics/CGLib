#include "World.h"

#include <algorithm>

namespace VkVolumeView {

SparseVolumeScene* World::addScene(const std::string& name) {
    auto scene   = std::make_unique<SparseVolumeScene>();
    scene->id_   = nextId_++;
    scene->name_ = name;
    SparseVolumeScene* ptr = scene.get();
    scenes_.push_back(std::move(scene));
    return ptr;
}

void World::removeScene(int id) {
    scenes_.erase(
        std::remove_if(scenes_.begin(), scenes_.end(),
            [id](const std::unique_ptr<SparseVolumeScene>& s) { return s->getId() == id; }),
        scenes_.end());
}

SparseVolumeScene* World::findById(int id) {
    for (auto& s : scenes_) {
        if (s->getId() == id) return s.get();
    }
    return nullptr;
}

DenseVolumeScene* World::addDenseScene(const std::string& name) {
    auto scene   = std::make_unique<DenseVolumeScene>();
    scene->id_   = nextDenseId_++;
    scene->name_ = name;
    DenseVolumeScene* ptr = scene.get();
    denseScenes_.push_back(std::move(scene));
    return ptr;
}

void World::removeDenseScene(int id) {
    denseScenes_.erase(
        std::remove_if(denseScenes_.begin(), denseScenes_.end(),
            [id](const std::unique_ptr<DenseVolumeScene>& s) { return s->getId() == id; }),
        denseScenes_.end());
}

DenseVolumeScene* World::findDenseById(int id) {
    for (auto& s : denseScenes_) {
        if (s->getId() == id) return s.get();
    }
    return nullptr;
}

void World::clearDenseScenes() {
    denseScenes_.clear();
}

void World::clear() {
    scenes_.clear();
    denseScenes_.clear();
    polygons_.clear();
}

void World::clearPolygons() {
    polygons_.clear();
}

void World::addPolygon(PolygonMesh mesh) {
    polygons_.push_back(std::move(mesh));
}

std::vector<Phantom::Volume::PBVRSceneEntry> World::getPBVREntries() const {
    std::vector<Phantom::Volume::PBVRSceneEntry> result;
    result.reserve(scenes_.size());
    for (const auto& scene : scenes_) {
        if (scene && scene->getShape()) {
            result.push_back({scene->getShape(), scene->isVisible()});
        }
    }
    return result;
}

} // namespace VkVolumeView
