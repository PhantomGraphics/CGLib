#pragma once

#include "../Volume/SparseVolumeTree/SparseVolume.h"

#include <memory>
#include <string>

namespace VkVolumeView {

class SparseVolumeScene {
public:
    int getId() const { return id_; }

    const std::string& getName() const { return name_; }
    void setName(const std::string& name) { name_ = name; }

    bool isVisible() const { return visible_; }
    void setVisible(bool v) { visible_ = v; }

    void setShape(std::unique_ptr<Phantom::Volume::SparseVolumef> shape) {
        shape_ = std::move(shape);
    }
    Phantom::Volume::SparseVolumef*       getShape()       { return shape_.get(); }
    const Phantom::Volume::SparseVolumef* getShape() const { return shape_.get(); }

private:
    friend class World;

    int         id_      = -1;
    std::string name_;
    bool        visible_ = true;
    std::unique_ptr<Phantom::Volume::SparseVolumef> shape_;
};

} // namespace VkVolumeView
