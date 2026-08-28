#pragma once

#include "IVolumeProcessView.h"

#include <string>

namespace VkVolumeView {

class DVMarchingCubesView : public IVolumeProcessView {
public:
	const char* getName() const override { return "Dense Marching Cubes"; }
	void onImGui(World& world, int activeSceneId,
				 const std::function<void()>& onRebuild) override;

private:
	float isoLevel_ = 0.0f;
	std::string statusMsg_;
};

} // namespace VkVolumeView
