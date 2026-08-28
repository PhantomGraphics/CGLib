#pragma once

#include "IVolumeProcessView.h"

#include <string>

namespace VkVolumeView {

class DVCreateBoxView : public IVolumeProcessView {
public:
	const char* getName() const override { return "Create Dense Box"; }
	void onImGui(World& world, int activeSceneId,
				 const std::function<void()>& onRebuild) override;

private:
	float min_[3] = {-10.f, -10.f, -10.f};
	float max_[3] = {10.f, 10.f, 10.f};
	int   res_[3] = {20, 20, 20};
	int   count_ = 0;
	std::string statusMsg_;
};

} // namespace VkVolumeView
