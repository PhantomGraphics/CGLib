#include "SVBoxView.h"

#include "../Volume/LevelSet.h"

#include "imgui.h"

#include <algorithm>
#include <memory>
#include <string>

namespace VkVolumeView {

void SVBoxView::onImGui(World& world, int /*activeSceneId*/,
                         const std::function<void()>& onRebuild)
{
    ImGui::InputFloat3("Min", min_);
    ImGui::InputFloat3("Max", max_);
    ImGui::SliderFloat("Cell length", &cell_, 0.1f, 5.0f);

    if (ImGui::Button("Create")) {
        const float minX = std::min(min_[0], max_[0]);
        const float minY = std::min(min_[1], max_[1]);
        const float minZ = std::min(min_[2], max_[2]);
        const float maxX = std::max(min_[0], max_[0]);
        const float maxY = std::max(min_[1], max_[1]);
        const float maxZ = std::max(min_[2], max_[2]);

        const Phantom::Math::Box3df box(
            Phantom::Math::Vector3df(minX, minY, minZ),
            Phantom::Math::Vector3df(maxX, maxY, maxZ));

        const float cell = std::max(cell_, 0.05f);
        // Use large positive background so MC can extract the SDF zero-crossing.
        auto volume = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
        volume->setVoxelSize(cell);

        Phantom::Volume::LevelSet levelSet;
        levelSet.setSignedDistance(box, *volume, static_cast<double>(cell) * 3.0);

        const std::string name = "Box_" + std::to_string(count_++);
        auto* scene = world.addScene(name);
        scene->setShape(std::move(volume));
        statusMsg_ = "Created: " + name +
                     " (" + std::to_string(scene->getShape()->getActiveVoxelCount()) + " vx)";
        if (onRebuild) onRebuild();
    }

    if (!statusMsg_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", statusMsg_.c_str());
    }
}

} // namespace VkVolumeView
