#include "SVResampleView.h"

#include "World.h"
#include "VolumeScene.h"

#include "../Volume/SparseVolumeTree/SparseVolume.h"
#include "../Volume/SparseVolumeTree/Interpolator.h"
#include "../Volume/SparseVolumeTree/Coord.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace VkVolumeView {

void SVResampleView::onImGui(World& world, int activeSceneId,
                              const std::function<void()>& onRebuild)
{
    const auto* src = world.findById(activeSceneId);
    if (!src || !src->getShape()) {
        ImGui::TextDisabled("No active volume scene selected");
        return;
    }

    const float origCell = src->getShape()->getVoxelSize();
    ImGui::Text("Source: %s  (cell=%.3f)", src->getName().c_str(), origCell);

    if (newCellSize_ <= 0.f) newCellSize_ = origCell;
    ImGui::SliderFloat("New Cell Size", &newCellSize_, 0.05f, 10.0f);

    const float ratio = origCell / std::max(newCellSize_, 0.001f);
    const int origVx  = src->getShape()->getActiveVoxelCount();
    ImGui::Text("Est. voxels: ~%d",
                static_cast<int>(static_cast<float>(origVx) * ratio * ratio * ratio));

    if (ImGui::Button("Resample")) {
        const auto* shape = src->getShape();
        const float cell  = std::max(newCellSize_, 0.05f);

        auto result = std::make_unique<Phantom::Volume::SparseVolumef>(1e6f);
        result->setVoxelSize(cell);

        const auto bbox = shape->getBoundingBox();
        const auto wMin = bbox.getMin();
        const auto wMax = bbox.getMax();

        const Phantom::Volume::Coord iMin = result->worldToIndex(wMin);
        const Phantom::Volume::Coord iMax = result->worldToIndex(wMax);

        Phantom::Volume::TrilinearInterpolator<float> interp(*shape);
        const float bg = shape->getBackground();

        for (int i = iMin.x - 1; i <= iMax.x + 1; ++i) {
            for (int j = iMin.y - 1; j <= iMax.y + 1; ++j) {
                for (int k = iMin.z - 1; k <= iMax.z + 1; ++k) {
                    const Phantom::Volume::Coord idx(i, j, k);
                    const auto wp   = result->indexToWorld(idx);
                    const float val = interp.getValue(wp);
                    if (std::fabs(val) < bg * 0.99f) {
                        result->setValue(idx, val);
                    }
                }
            }
        }

        const std::string name = src->getName() + "_resamp_" + std::to_string(count_++);
        auto* scene = world.addScene(name);
        scene->setShape(std::move(result));
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
