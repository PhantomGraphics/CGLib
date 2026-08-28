#pragma once

#include "VolumeScene.h"
#include "DenseVolumeScene.h"

#include "../VolumeRenderer/IPBVRDataSource.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace VkVolumeView {

struct PolygonMesh {
    std::string           name;
    std::vector<float>    positions; // x,y,z triplets (3 floats per vertex)
    std::vector<float>    colors;    // r,g,b,a quads   (4 floats per vertex)
    std::vector<uint32_t> indices;   // triangle index list
    bool visible = true;
};

class World : public Phantom::Volume::IPBVRDataSource {
public:
    SparseVolumeScene* addScene(const std::string& name);
    void         removeScene(int id);
    SparseVolumeScene* findById(int id);

    DenseVolumeScene* addDenseScene(const std::string& name);
    void              removeDenseScene(int id);
    DenseVolumeScene* findDenseById(int id);
    void              clearDenseScenes();

    void         clear();

    std::vector<Phantom::Volume::PBVRSceneEntry> getPBVREntries() const override;

    bool isEmpty() const { return scenes_.empty(); }

    const std::vector<std::unique_ptr<SparseVolumeScene>>& getScenes() const { return scenes_; }
    std::vector<std::unique_ptr<SparseVolumeScene>>&       getScenes()       { return scenes_; }

    const std::vector<std::unique_ptr<DenseVolumeScene>>& getDenseScenes() const { return denseScenes_; }
    std::vector<std::unique_ptr<DenseVolumeScene>>&       getDenseScenes()       { return denseScenes_; }

    void                            clearPolygons();
    void                            addPolygon(PolygonMesh mesh);
    const std::vector<PolygonMesh>& getPolygons() const { return polygons_; }

private:
    int nextId_ = 0;
    int nextDenseId_ = 0;
    std::vector<std::unique_ptr<SparseVolumeScene>> scenes_;
    std::vector<std::unique_ptr<DenseVolumeScene>> denseScenes_;
    std::vector<PolygonMesh>                  polygons_;
};

} // namespace VkVolumeView
