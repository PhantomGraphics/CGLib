#pragma once

#include "IVolumeProcessView.h"

#include <string>

namespace VkVolumeView {

class DVFromSparseView : public IVolumeProcessView {
public:
	const char* getName() const override { return "Dense From Sparse"; }
	void onImGui(World& world, int activeSceneId,
				 const std::function<void()>& onRebuild) override;

private:
	float voxelSize_ = 1.0f;
	int   count_ = 0;
	std::string statusMsg_;
};

} // namespace VkVolumeView
