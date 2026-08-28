#include "VkVolumeViewPanel.h"

#include "imgui.h"

#include "../Volume/LevelSet.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

namespace VkVolumeView {

void VkVolumeViewPanel::onImGui() {
    ImGui::Begin("VolumeView Control");

    if (ImGui::CollapsingHeader("Create SVBox", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputFloat3("Min", minCorner_);
        ImGui::InputFloat3("Max", maxCorner_);
        ImGui::SliderFloat("Cell length", &cellLength_, 0.1f, 5.0f);

        if (ImGui::Button("Create") && world_) {
            const float minX = std::min(minCorner_[0], maxCorner_[0]);
            const float minY = std::min(minCorner_[1], maxCorner_[1]);
            const float minZ = std::min(minCorner_[2], maxCorner_[2]);
            const float maxX = std::max(minCorner_[0], maxCorner_[0]);
            const float maxY = std::max(minCorner_[1], maxCorner_[1]);
            const float maxZ = std::max(minCorner_[2], maxCorner_[2]);

            const Phantom::Math::Box3df box(
                Phantom::Math::Vector3df(minX, minY, minZ),
                Phantom::Math::Vector3df(maxX, maxY, maxZ));

            const float cell = std::max(cellLength_, 0.05f);
            auto volume = std::make_unique<Phantom::Volume::SparseVolumef>(0.0f);
            volume->setVoxelSize(cell);

            Phantom::Volume::LevelSet levelSet;
            levelSet.setSignedDistance(box, *volume, 1.0);

            std::vector<std::pair<Phantom::Volume::Coord, float>> writes;
            writes.reserve(static_cast<size_t>(volume->getActiveVoxelCount()));
            volume->forEachActive([&](const Phantom::Volume::Coord& idx, const Phantom::Math::Vector3df& worldPos, float) {
                writes.emplace_back(idx, glm::length(worldPos));
            });

            for (const auto& item : writes) {
                volume->setValue(item.first, item.second);
            }

            const std::string name = "Box_" + std::to_string(sceneCount_++);
            auto* scene = world_->addScene(name);
            scene->setShape(std::move(volume));
            if (onChange_) {
                onChange_();
            }
        }
    }

    if (ImGui::Button("Clear") && world_) {
        world_->clear();
        if (onChange_) {
            onChange_();
        }
    }

    ImGui::End();
}

} // namespace VkVolumeView
