#pragma once

#include "../Volume/Volume.h"

#include <memory>
#include <string>

namespace VkVolumeView {

class DenseVolumeScene {
public:
	int getId() const { return id_; }

	const std::string& getName() const { return name_; }
	void setName(const std::string& name) { name_ = name; }

	bool isVisible() const { return visible_; }
	void setVisible(bool v) { visible_ = v; }

	void setVolume(std::unique_ptr<Phantom::Volume::Volumef> vol) {
		vol_ = std::move(vol);
	}
	Phantom::Volume::Volumef*       getVolume()       { return vol_.get(); }
	const Phantom::Volume::Volumef* getVolume() const { return vol_.get(); }

private:
	friend class World;

	int         id_      = -1;
	std::string name_;
	bool        visible_ = true;
	std::unique_ptr<Phantom::Volume::Volumef> vol_;
};

} // namespace VkVolumeView
