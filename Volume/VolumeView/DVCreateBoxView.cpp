#include "DVCreateBoxView.h"

#include "imgui.h"

#include "../Volume/Volume.h"

#include <algorithm>
#include <memory>
#include <string>

namespace VkVolumeView {

void DVCreateBoxView::onImGui(World& world, int /*activeSceneId*/,
							  const std::function<void()>& onRebuild)
{
	ImGui::InputFloat3("Min", min_);
	ImGui::InputFloat3("Max", max_);
	ImGui::InputInt3("Resolution", res_);

	if (ImGui::Button("Create Dense")) {
		const float minX = std::min(min_[0], max_[0]);
		const float minY = std::min(min_[1], max_[1]);
		const float minZ = std::min(min_[2], max_[2]);
		const float maxX = std::max(min_[0], max_[0]);
		const float maxY = std::max(min_[1], max_[1]);
		const float maxZ = std::max(min_[2], max_[2]);

		const int rx = std::clamp(res_[0], 1, 256);
		const int ry = std::clamp(res_[1], 1, 256);
		const int rz = std::clamp(res_[2], 1, 256);

		auto vol = std::make_unique<Phantom::Volume::Volumef>(
			Phantom::Math::Box3df(
				Phantom::Math::Vector3df(minX, minY, minZ),
				Phantom::Math::Vector3df(maxX, maxY, maxZ)),
			std::array<size_t, 3>{static_cast<size_t>(rx), static_cast<size_t>(ry), static_cast<size_t>(rz)});

		for (int i = 0; i < rx; ++i)
			for (int j = 0; j < ry; ++j)
				for (int k = 0; k < rz; ++k)
					vol->setValue({i, j, k}, 0.0f);

		const std::string name = "DenseBox_" + std::to_string(count_++);
		auto* scene = world.addDenseScene(name);
		scene->setVolume(std::move(vol));

		statusMsg_ = "Created: " + name +
					 " (" + std::to_string(rx) + "x" +
					 std::to_string(ry) + "x" + std::to_string(rz) + ")";
		if (onRebuild) onRebuild();
	}

	if (!statusMsg_.empty()) {
		ImGui::Spacing();
		ImGui::TextWrapped("%s", statusMsg_.c_str());
	}
}

} // namespace VkVolumeView
