#include "DVFromSparseView.h"

#include "VolumeScene.h"

#include "../Volume/SparseVolumeTree/Coord.h"
#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/Volume.h"

#include "imgui.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <string>

namespace VkVolumeView {

void DVFromSparseView::onImGui(World& world, int activeSceneId,
							   const std::function<void()>& onRebuild)
{
	const auto* src = world.findById(activeSceneId);
	if (!src || !src->getShape()) {
		ImGui::TextDisabled("No active sparse scene selected");
		return;
	}

	if (voxelSize_ <= 0.0f) voxelSize_ = src->getShape()->getVoxelSize();
	ImGui::SliderFloat("Voxel Size", &voxelSize_, 0.05f, 5.0f);

	if (ImGui::Button("Convert To Dense")) {
		const auto* sparse = src->getShape();

		int minX = INT_MAX, minY = INT_MAX, minZ = INT_MAX;
		int maxX = INT_MIN, maxY = INT_MIN, maxZ = INT_MIN;

		sparse->forEachActive([&](const Phantom::Volume::Coord& c,
								  const Phantom::Math::Vector3df&, float) {
			minX = std::min(minX, (int)c.x); minY = std::min(minY, (int)c.y);
			minZ = std::min(minZ, (int)c.z);
			maxX = std::max(maxX, (int)c.x); maxY = std::max(maxY, (int)c.y);
			maxZ = std::max(maxZ, (int)c.z);
		});

		if (minX > maxX || minY > maxY || minZ > maxZ) {
			statusMsg_ = "Error: empty sparse volume";
			return;
		}

		const int ox = minX - 1, oy = minY - 1, oz = minZ - 1;
		const int dimX = maxX - minX + 3;
		const int dimY = maxY - minY + 3;
		const int dimZ = maxZ - minZ + 3;

		const float vs = std::max(voxelSize_, 0.05f);
		const Phantom::Math::Vector3df bMin(
			(static_cast<float>(ox) - 0.5f) * vs,
			(static_cast<float>(oy) - 0.5f) * vs,
			(static_cast<float>(oz) - 0.5f) * vs);
		const Phantom::Math::Vector3df bMax(
			bMin.x + static_cast<float>(dimX) * vs,
			bMin.y + static_cast<float>(dimY) * vs,
			bMin.z + static_cast<float>(dimZ) * vs);

		auto dense = std::make_unique<Phantom::Volume::Volumef>(
			Phantom::Math::Box3df(bMin, bMax),
			std::array<size_t, 3>{(size_t)dimX, (size_t)dimY, (size_t)dimZ});

		const float bg = sparse->getBackground();
		for (int i = 0; i < dimX; ++i)
			for (int j = 0; j < dimY; ++j)
				for (int k = 0; k < dimZ; ++k)
					dense->setValue({i, j, k}, bg);

		sparse->forEachActive([&](const Phantom::Volume::Coord& c,
								  const Phantom::Math::Vector3df&, float value) {
			dense->setValue({c.x - ox, c.y - oy, c.z - oz}, value);
		});

		const std::string name = "FromSV_" + src->getName() + "_" + std::to_string(count_++);
		auto* scene = world.addDenseScene(name);
		scene->setVolume(std::move(dense));

		statusMsg_ = "Created: " + name;
		if (onRebuild) onRebuild();
	}

	if (!statusMsg_.empty()) {
		ImGui::Spacing();
		ImGui::TextWrapped("%s", statusMsg_.c_str());
	}
}

} // namespace VkVolumeView
